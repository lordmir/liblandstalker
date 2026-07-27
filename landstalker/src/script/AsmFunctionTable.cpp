#include <landstalker/script/AsmFunctionTable.h>

#include <algorithm>
#include <set>

#include <landstalker/script/AsmText.h>

namespace Landstalker {

using namespace AsmText;

namespace {

std::size_t FirstNonSpace(const std::string& line)
{
	std::size_t i = 0;
	while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
	{
		++i;
	}
	return i;
}

// A "; -------" divider between blocks - a comment whose only content is dashes.
bool IsSeparator(const std::string& line)
{
	std::size_t i = FirstNonSpace(line);
	if (i >= line.size() || line[i] != ';')
	{
		return false;
	}
	++i;
	bool saw_dash = false;
	for (; i < line.size(); ++i)
	{
		const char c = line[i];
		if (c == '-')
		{
			saw_dash = true;
		}
		else if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
		{
			return false;
		}
	}
	return saw_dash;
}

std::string TrimmedFront(const std::string& line)
{
	return line.substr(FirstNonSpace(line));
}

// Parse a "bra.w <label>" jump-table line, capturing the exact surrounding text so it can be
// reproduced verbatim. Accepts bra / bra.w / bra.s / bra.b.
bool ParseBraLine(const std::string& line, std::string& prefix, std::string& label, std::string& suffix)
{
	std::size_t i = FirstNonSpace(line);
	if (line.compare(i, 3, "bra") != 0)
	{
		return false;
	}
	std::size_t j = i + 3;
	if (j < line.size() && line[j] == '.')
	{
		j += 2; // skip size suffix e.g. ".w"
	}
	if (j >= line.size() || !(line[j] == ' ' || line[j] == '\t'))
	{
		return false;
	}
	while (j < line.size() && (line[j] == ' ' || line[j] == '\t'))
	{
		++j;
	}
	const std::size_t op_start = j;
	while (j < line.size() && IsLabelChar(line[j]))
	{
		++j;
	}
	if (j == op_start)
	{
		return false;
	}
	prefix = line.substr(0, op_start);
	label = line.substr(op_start, j - op_start);
	suffix = line.substr(j);
	return true;
}

} // namespace

bool AsmFunctionTable::Parse(const std::string& dispatch_text, const std::string& actions_text,
	const std::string& table_label)
{
	*this = AsmFunctionTable();

	// --- dispatch file: head, bra.w slot run, tail ---
	const auto dlines = SplitLinesKeepEnds(dispatch_text);
	std::size_t t = dlines.size();
	for (std::size_t i = 0; i < dlines.size(); ++i)
	{
		std::string name;
		if (ParseColumn0Label(dlines[i], name) && name == table_label)
		{
			t = i;
			break;
		}
	}
	if (t == dlines.size())
	{
		return false; // table label not found
	}
	std::size_t u = t + 1;
	std::vector<Slot> slots;
	for (; u < dlines.size(); ++u)
	{
		Slot s;
		if (!ParseBraLine(dlines[u], s.prefix, s.label, s.suffix))
		{
			// The slot run is contiguous; the first non-`bra` line (blank line, modend, ...)
			// begins the tail, which is preserved verbatim.
			break;
		}
		slots.push_back(s);
	}
	if (slots.empty())
	{
		return false;
	}

	// --- code file: preamble, blocks, epilogue ---
	const auto alines = SplitLinesKeepEnds(actions_text);
	// Epilogue = trailing "modend" plus the blank lines immediately above it.
	std::size_t epilogue_start = alines.size();
	for (std::size_t i = alines.size(); i-- > 0;)
	{
		if (TrimmedFront(alines[i]).compare(0, 6, "modend") == 0)
		{
			epilogue_start = i;
			break;
		}
	}
	while (epilogue_start > 0 && IsBlank(alines[epilogue_start - 1]))
	{
		--epilogue_start;
	}

	// A handler's body runs from its jump-table entry label to the NEXT jump-table entry label, so
	// only the slot-target labels delimit blocks. Every other column-0 label - local branch targets
	// (_c0036Take), named sub-labels (CSA_00B2_MoveUp), inline data/helpers - is absorbed into the
	// enclosing handler's body rather than splitting it.
	std::set<std::string> slot_labels;
	for (const auto& s : slots)
	{
		slot_labels.insert(s.label);
	}
	std::vector<std::size_t> label_lines;
	std::vector<std::string> label_names;
	for (std::size_t i = 0; i < epilogue_start; ++i)
	{
		std::string name;
		if (ParseColumn0Label(alines[i], name) && slot_labels.count(name))
		{
			label_lines.push_back(i);
			label_names.push_back(name);
		}
	}
	if (label_lines.empty())
	{
		return false;
	}

	// Block start = the label line, extended up over its directly-attached comment lines
	// (stopping at a blank, a "; ----" divider, or code - those close the previous block).
	std::vector<std::size_t> starts(label_lines.size());
	for (std::size_t b = 0; b < label_lines.size(); ++b)
	{
		const std::size_t lower = (b == 0) ? 0 : label_lines[b - 1] + 1;
		std::size_t k = label_lines[b];
		while (k > lower && IsCommentLine(alines[k - 1]) && !IsSeparator(alines[k - 1]))
		{
			--k;
		}
		starts[b] = k;
	}

	m_dispatch_head = Concat(dlines, 0, t + 1);
	m_slots = std::move(slots);
	m_dispatch_tail = Concat(dlines, u, dlines.size());

	m_preamble = Concat(alines, 0, starts[0]);
	m_blocks.clear();
	for (std::size_t b = 0; b < label_lines.size(); ++b)
	{
		const std::size_t block_end = (b + 1 < label_lines.size()) ? starts[b + 1] : epilogue_start;
		Block blk;
		blk.label = label_names[b];
		blk.text = Concat(alines, starts[b], block_end);
		m_blocks.push_back(std::move(blk));
	}
	m_epilogue = Concat(alines, epilogue_start, alines.size());

	RebuildIndex();
	m_valid = true;
	return true;
}

void AsmFunctionTable::RebuildIndex()
{
	m_index.clear();
	for (std::size_t i = 0; i < m_blocks.size(); ++i)
	{
		m_index[m_blocks[i].label] = i;
	}
}

std::string AsmFunctionTable::EmitDispatch() const
{
	std::string out = m_dispatch_head;
	for (const auto& s : m_slots)
	{
		out += s.prefix + s.label + s.suffix;
	}
	out += m_dispatch_tail;
	return out;
}

std::string AsmFunctionTable::EmitActions() const
{
	std::string out = m_preamble;
	for (const auto& b : m_blocks)
	{
		out += b.text;
	}
	out += m_epilogue;
	return out;
}

const std::string& AsmFunctionTable::GetSlotLabel(std::size_t i) const
{
	static const std::string empty;
	return i < m_slots.size() ? m_slots[i].label : empty;
}

bool AsmFunctionTable::SwapSlots(std::size_t i, std::size_t j)
{
	if (i >= m_slots.size() || j >= m_slots.size())
	{
		return false;
	}
	// Swap only the target labels, leaving each line's captured prefix/suffix in place so the
	// file's formatting is preserved exactly.
	std::swap(m_slots[i].label, m_slots[j].label);
	return true;
}

bool AsmFunctionTable::SetSlotLabel(std::size_t i, const std::string& label)
{
	if (i >= m_slots.size())
	{
		return false;
	}
	m_slots[i].label = label;
	return true;
}

bool AsmFunctionTable::AppendSlot(const std::string& label)
{
	if (m_slots.empty())
	{
		return false; // no existing entry to copy the line format from
	}
	Slot s = m_slots.back();
	s.label = label;
	m_slots.push_back(s);
	return true;
}

bool AsmFunctionTable::PopSlot()
{
	if (m_slots.empty())
	{
		return false;
	}
	m_slots.pop_back();
	return true;
}

bool AsmFunctionTable::IsSlotTarget(const std::string& label) const
{
	for (const auto& s : m_slots)
	{
		if (s.label == label)
		{
			return true;
		}
	}
	return false;
}

const AsmFunctionTable::Block* AsmFunctionTable::FindBlock(const std::string& label) const
{
	const auto it = m_index.find(label);
	return it == m_index.end() ? nullptr : &m_blocks[it->second];
}

bool AsmFunctionTable::SetBlockText(const std::string& label, const std::string& text)
{
	const auto it = m_index.find(label);
	if (it == m_index.end())
	{
		return false;
	}
	m_blocks[it->second].text = text;
	return true;
}

bool AsmFunctionTable::MoveBlockAfter(const std::string& label, const std::string& after)
{
	const auto it = m_index.find(label);
	if (it == m_index.end() || label == after)
	{
		return false;
	}
	Block moved = m_blocks[it->second];
	m_blocks.erase(m_blocks.begin() + it->second);
	std::size_t pos = m_blocks.size();
	if (after.empty())
	{
		pos = 0;
	}
	else
	{
		const auto ait = std::find_if(m_blocks.begin(), m_blocks.end(),
			[&](const Block& b) { return b.label == after; });
		if (ait != m_blocks.end())
		{
			pos = std::distance(m_blocks.begin(), ait) + 1;
		}
	}
	m_blocks.insert(m_blocks.begin() + pos, std::move(moved));
	RebuildIndex();
	return true;
}

bool AsmFunctionTable::AddBlock(const std::string& label, const std::string& text, const std::string& after)
{
	if (m_index.find(label) != m_index.end())
	{
		return false;
	}
	std::size_t pos = m_blocks.size();
	if (!after.empty())
	{
		const auto ait = std::find_if(m_blocks.begin(), m_blocks.end(),
			[&](const Block& b) { return b.label == after; });
		if (ait != m_blocks.end())
		{
			pos = std::distance(m_blocks.begin(), ait) + 1;
		}
	}
	m_blocks.insert(m_blocks.begin() + pos, Block{ label, text });
	RebuildIndex();
	return true;
}

bool AsmFunctionTable::RemoveBlock(const std::string& label)
{
	const auto it = m_index.find(label);
	if (it == m_index.end())
	{
		return false;
	}
	m_blocks.erase(m_blocks.begin() + it->second);
	RebuildIndex();
	return true;
}

bool AsmFunctionTable::operator==(const AsmFunctionTable& rhs) const
{
	// Compare on the reconstructed text: the exact bytes we would write are what "modified"
	// means for dirty-tracking, and it is immune to incidental internal-representation drift.
	return m_valid == rhs.m_valid
		&& EmitDispatch() == rhs.EmitDispatch()
		&& EmitActions() == rhs.EmitActions();
}

} // namespace Landstalker
