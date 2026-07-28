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
	ParseFile(filename);
	if (m_good)
	{
		ResolveFixups();
	}
}

void Z80AsmFile::ParseFile(const std::filesystem::path& path)
{
	std::ifstream ifs(path);
	if (!ifs.is_open())
	{
		m_good = false;
		return;
	}
	const std::filesystem::path dir = path.parent_path();
	std::string line;
	while (std::getline(ifs, line))
	{
		ParseLine(dir, line);
	}
}

void Z80AsmFile::ParseLine(const std::filesystem::path& current_dir, std::string line)
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
	else if (!line.empty() && (std::isalpha(static_cast<unsigned char>(line[0])) || line[0] == '_') &&
		std::all_of(line.begin(), line.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; }))
	{
		// A bare identifier alone on its own line is also a label, colon optional - this
		// disassembly isn't fully consistent about including it (e.g. music/music11.asm).
		m_labels[line] = m_data.size();
		return;
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
			else
			{
				// Not a literal - treat it as a label reference and fix it up once every label in
				// this file (and everything it includes) is known.
				m_fixups.push_back({ m_data.size(), tok });
				m_data.push_back(0);
				m_data.push_back(0);
			}
		}
		return;
	}
	if (kw_lower == "include")
	{
		std::string path = Trim(operand);
		if (path.size() >= 2 && (path.front() == '"' || path.front() == '\'') && path.back() == path.front())
		{
			path = path.substr(1, path.size() - 2);
		}
		if (!path.empty())
		{
			ParseFile(current_dir / path);
		}
		return;
	}
	if (kw_lower == "org")
	{
		uint32_t addr = 0;
		if (ParseNumber(operand, addr))
		{
			if (!m_org_base_set)
			{
				m_org_base = addr;
				m_org_base_set = true;
			}
			else if (addr >= m_org_base)
			{
				const std::size_t target_offset = addr - m_org_base;
				if (target_offset > m_data.size())
				{
					m_data.resize(target_offset, 0);
				}
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
	// Anything else (cpu, phase, ds, instructions, ...) is not needed here and is skipped.
}

void Z80AsmFile::ResolveFixups()
{
	for (const auto& fixup : m_fixups)
	{
		std::size_t label_offset = 0;
		if (GetLabelOffset(fixup.label, label_offset))
		{
			m_data[fixup.data_offset] = static_cast<uint8_t>(label_offset & 0xFF);
			m_data[fixup.data_offset + 1] = static_cast<uint8_t>((label_offset >> 8) & 0xFF);
		}
	}
	m_fixups.clear();
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

bool Z80AsmFile::GetLabelOffset(const std::string& label, std::size_t& offset) const
{
	const auto it = m_labels.find(label);
	if (it == m_labels.end())
	{
		return false;
	}
	offset = it->second;
	return true;
}

std::size_t Z80AsmFile::NextLabelOffsetAfter(std::size_t offset) const
{
	std::size_t best = m_data.size();
	for (const auto& [name, label_offset] : m_labels)
	{
		if (label_offset > offset && label_offset < best)
		{
			best = label_offset;
		}
	}
	return best;
}

bool Z80AsmFile::Goto(const std::string& label)
{
	std::size_t offset = 0;
	if (!GetLabelOffset(label, offset))
	{
		return false;
	}
	m_readpos = offset;
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

std::vector<uint8_t> Z80AsmFile::ReadBytesAt(std::size_t offset, std::size_t count) const
{
	std::vector<uint8_t> out;
	out.reserve(count);
	for (std::size_t i = 0; i < count && offset + i < m_data.size(); ++i)
	{
		out.push_back(m_data[offset + i]);
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

void Z80AsmFile::WriteWordRefs(const std::vector<std::string>& labels)
{
	for (const auto& label : labels)
	{
		m_out_lines.push_back("\tdw " + label);
	}
}

void Z80AsmFile::WriteRaw(const std::string& line)
{
	m_out_lines.push_back(line);
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
