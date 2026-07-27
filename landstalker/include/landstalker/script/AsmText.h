#ifndef _ASM_TEXT_H_
#define _ASM_TEXT_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include <landstalker/main/AsmFile.h>

// Small line-oriented lexing helpers shared by the verbatim-asm table models (AsmFunctionTable,
// RoomActionTable, ItemUseTable). Header-only (inline) so each keeps its own private helpers alongside.
// Two line-splitting flavours are offered: SplitLines strips terminators (rejoin with Join + an eol),
// SplitLinesKeepEnds keeps them (rejoin with Concat) - the byte-exact-preserving form.
namespace Landstalker {
namespace AsmText {

// True for characters allowed in an m68k label/symbol after the first character.
inline bool IsLabelChar(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

// Split on '\n', dropping a trailing '\r' from each line; lines keep no terminator, and a final
// trailing '\n' does not yield a phantom empty line. Rejoining with Join(.., eol) reproduces the input.
inline std::vector<std::string> SplitLines(const std::string& s)
{
	std::vector<std::string> out;
	std::size_t start = 0;
	for (std::size_t i = 0; i <= s.size(); ++i)
	{
		if (i == s.size() || s[i] == '\n')
		{
			std::string line = s.substr(start, i - start);
			if (!line.empty() && line.back() == '\r') line.pop_back();
			out.push_back(line);
			start = i + 1;
		}
	}
	if (!s.empty() && s.back() == '\n' && !out.empty()) out.pop_back();
	return out;
}

// Split into lines, each KEEPING its trailing '\n' (and any '\r'), so concatenating them (Concat)
// reproduces the input byte-for-byte, CRLF or LF alike.
inline std::vector<std::string> SplitLinesKeepEnds(const std::string& s)
{
	std::vector<std::string> lines;
	std::size_t start = 0;
	for (std::size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] == '\n')
		{
			lines.push_back(s.substr(start, i - start + 1));
			start = i + 1;
		}
	}
	if (start < s.size()) lines.push_back(s.substr(start));
	return lines;
}

// Join lines [a,b) with `eol` BETWEEN them (no trailing eol). Pairs with SplitLines.
inline std::string Join(const std::vector<std::string>& v, std::size_t a, std::size_t b, const std::string& eol)
{
	std::string out;
	for (std::size_t i = a; i < b && i < v.size(); ++i) { out += v[i]; if (i + 1 < b) out += eol; }
	return out;
}

// Concatenate lines [begin,end) verbatim (each already carries its terminator). Pairs with SplitLinesKeepEnds.
inline std::string Concat(const std::vector<std::string>& v, std::size_t begin, std::size_t end)
{
	std::string out;
	for (std::size_t i = begin; i < end && i < v.size(); ++i) out += v[i];
	return out;
}

// The code part of a line: everything before a ';' comment (terminator/comment excluded).
inline std::string CodeOf(const std::string& line) { return line.substr(0, line.find(';')); }

// True if the line holds no code and no comment (whitespace only; '\n'/'\r' count as whitespace, so
// this is correct for both the stripped and the terminator-keeping line forms).
inline bool IsBlank(const std::string& line)
{
	for (char c : line) if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return false;
	return true;
}

// True if the first non-whitespace character is ';'.
inline bool IsCommentLine(const std::string& line)
{
	std::size_t j = 0;
	while (j < line.size() && (line[j] == ' ' || line[j] == '\t')) ++j;
	return j < line.size() && line[j] == ';';
}

// True if the line carries actual code (not blank, not a whole-line comment).
inline bool IsCode(const std::string& line) { return !IsBlank(CodeOf(line)) && !IsCommentLine(line); }

// A column-0 label: leading `Name:` (Name may be followed by code, e.g. "Foo:\tdc.b 1"). On success
// returns the bare label (without the colon) in `out`.
inline bool ParseColumn0Label(const std::string& line, std::string& out)
{
	if (line.empty()) return false;
	const char c = line[0];
	if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')) return false;
	std::size_t i = 1;
	while (i < line.size() && IsLabelChar(line[i])) ++i;
	if (i < line.size() && line[i] == ':') { out = line.substr(0, i); return true; }
	return false;
}

// Resolve an asm value token - a constant name, $hex, decimal, or the "($80|ITM_x)" flag-masked form -
// to an integer via the shared evaluator (which folds "(A-B)" arithmetic and chases define chains).
// nullopt if it cannot be evaluated.
inline std::optional<int> ResolveValue(std::string token, const std::map<std::string, std::string>& defines)
{
	std::smatch m;
	static const std::regex orred("\\(\\s*\\$?[0-9A-Fa-f]+\\s*\\|\\s*(\\w+)\\s*\\)");
	if (std::regex_search(token, m, orred)) token = m[1].str();
	const int64_t v = AsmFile::ParseValue(token, defines);
	if (v < 0) return std::nullopt;
	return static_cast<int>(v);
}

} // namespace AsmText
} // namespace Landstalker

#endif // _ASM_TEXT_H_
