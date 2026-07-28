#include <landstalker/main/Z80AsmFile.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include <landstalker/misc/Utils.h>

namespace
{

std::string ToLowerStr(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
	return s;
}

// Splits a comma-separated operand list ("01h, 00h, 0AFh") into trimmed tokens.
std::vector<std::string> SplitOperands(const std::string& operand)
{
	std::vector<std::string> tokens;
	std::stringstream ss(operand);
	std::string tok;
	while (std::getline(ss, tok, ','))
	{
		tok = Landstalker::Trim(tok);
		if (!tok.empty())
		{
			tokens.push_back(tok);
		}
	}
	return tokens;
}

std::string FormatHexByte(uint8_t value)
{
	static const char DIGITS[] = "0123456789ABCDEF";
	std::string s;
	s += DIGITS[(value >> 4) & 0xF];
	s += DIGITS[value & 0xF];
	// A leading letter digit would be ambiguous with an identifier - prefix a 0, matching the
	// disassembly's own convention (e.g. "0AFh").
	if (s[0] >= 'A' && s[0] <= 'F')
	{
		s = "0" + s;
	}
	return s + "h";
}

} // namespace

namespace Landstalker {

Z80AsmFile::Z80AsmFile(const std::filesystem::path& filename)
{
	std::ifstream ifs(filename);
	if (!ifs.is_open())
	{
		m_good = false;
		return;
	}
	std::string line;
	while (std::getline(ifs, line))
	{
		ParseLine(line);
	}
}

void Z80AsmFile::ParseLine(std::string line)
{
	const auto semi = line.find(';');
	if (semi != std::string::npos)
	{
		line = line.substr(0, semi);
	}
	line = Trim(line);
	if (line.empty())
	{
		return;
	}

	std::string rest = line;
	const auto colon = line.find(':');
	if (colon != std::string::npos)
	{
		const std::string label = Trim(line.substr(0, colon));
		if (!label.empty() && label.find_first_of(" \t") == std::string::npos)
		{
			m_labels[label] = m_data.size();
			rest = Trim(line.substr(colon + 1));
		}
	}
	if (rest.empty())
	{
		return;
	}

	const auto sp = rest.find_first_of(" \t");
	const std::string keyword = (sp == std::string::npos) ? rest : rest.substr(0, sp);
	const std::string operand = (sp == std::string::npos) ? std::string() : Trim(rest.substr(sp + 1));
	const std::string kw_lower = ToLowerStr(keyword);

	if (kw_lower == "db" || kw_lower == "defb")
	{
		for (const auto& tok : SplitOperands(operand))
		{
			uint32_t v = 0;
			if (ParseNumber(tok, v))
			{
				m_data.push_back(static_cast<uint8_t>(v & 0xFF));
			}
		}
		return;
	}
	if (kw_lower == "dw" || kw_lower == "defw")
	{
		for (const auto& tok : SplitOperands(operand))
		{
			uint32_t v = 0;
			if (ParseNumber(tok, v))
			{
				m_data.push_back(static_cast<uint8_t>(v & 0xFF));
				m_data.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
			}
		}
		return;
	}

	// `NAME equ VALUE` - the constant name is `keyword`, and "equ" is the operand's own first token.
	const auto sp2 = operand.find_first_of(" \t");
	const std::string second = ToLowerStr((sp2 == std::string::npos) ? operand : operand.substr(0, sp2));
	if (second == "equ")
	{
		const std::string value_str = (sp2 == std::string::npos) ? std::string() : Trim(operand.substr(sp2 + 1));
		uint32_t v = 0;
		if (ParseNumber(value_str, v))
		{
			m_constants[keyword] = v;
		}
		return;
	}
	// Anything else (org, cpu, phase, include, ds, instructions, ...) is not needed here and is skipped.
}

bool Z80AsmFile::ParseNumber(const std::string& token, uint32_t& value)
{
	const std::string t = Trim(token);
	if (t.empty())
	{
		return false;
	}
	try
	{
		std::size_t consumed = 0;
		if (t.back() == 'h' || t.back() == 'H')
		{
			value = static_cast<uint32_t>(std::stoul(t.substr(0, t.size() - 1), &consumed, 16));
		}
		else
		{
			value = static_cast<uint32_t>(std::stoul(t, &consumed, 10));
		}
	}
	catch (const std::exception&)
	{
		return false;
	}
	return true;
}

bool Z80AsmFile::ConstantExists(const std::string& name) const
{
	return m_constants.find(name) != m_constants.end();
}

uint32_t Z80AsmFile::GetConstant(const std::string& name) const
{
	const auto it = m_constants.find(name);
	return (it == m_constants.end()) ? 0 : it->second;
}

bool Z80AsmFile::LabelExists(const std::string& label) const
{
	return m_labels.find(label) != m_labels.end();
}

bool Z80AsmFile::Goto(const std::string& label)
{
	const auto it = m_labels.find(label);
	if (it == m_labels.end())
	{
		return false;
	}
	m_readpos = it->second;
	return true;
}

std::vector<uint8_t> Z80AsmFile::ReadBytes(std::size_t count)
{
	std::vector<uint8_t> out;
	out.reserve(count);
	for (std::size_t i = 0; i < count && m_readpos < m_data.size(); ++i, ++m_readpos)
	{
		out.push_back(m_data[m_readpos]);
	}
	return out;
}

void Z80AsmFile::WriteComment(const std::string& comment)
{
	m_out_lines.push_back("; " + comment);
}

void Z80AsmFile::WriteLabel(const std::string& label)
{
	m_out_lines.push_back(label + ":");
}

void Z80AsmFile::WriteBytes(const std::vector<uint8_t>& bytes, std::size_t per_line)
{
	if (per_line == 0)
	{
		per_line = 1;
	}
	for (std::size_t i = 0; i < bytes.size(); i += per_line)
	{
		std::string line = "\tdb ";
		const std::size_t end = std::min(bytes.size(), i + per_line);
		for (std::size_t j = i; j < end; ++j)
		{
			if (j != i)
			{
				line += ", ";
			}
			line += FormatHexByte(bytes[j]);
		}
		m_out_lines.push_back(line);
	}
}

bool Z80AsmFile::WriteFile(const std::filesystem::path& filename) const
{
	std::ofstream ofs(filename);
	if (!ofs.is_open())
	{
		return false;
	}
	for (const auto& line : m_out_lines)
	{
		ofs << line << "\n";
	}
	return true;
}

} // namespace Landstalker
