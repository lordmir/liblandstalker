#include <landstalker/script/RoomActionTable.h>

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

#include <landstalker/script/AsmText.h>

namespace Landstalker {

using namespace AsmText;

const std::string RoomActionTable::kNextFileMarker = "{{Next File}}";

namespace {

bool IsNextLabel(const std::string& l)
{
	static const std::regex re("^_Next[0-9]+$");
	return std::regex_match(l, re);
}
bool IsEpilogueLine(const std::string& line)
{
	std::string l;
	if (ParseColumn0Label(line, l)) return l == "EndCustomRoomAction" || l == "_Done";
	static const std::regex modend("^\\s*modend\\b");
	return std::regex_search(line, modend);
}

} // namespace

bool RoomActionTable::Parse(const std::string& file1, const std::string& file2,
	const std::map<std::string, std::string>& defines)
{
	m_valid = false;
	m_branches.clear();
	m_room_names.clear();
	m_add_seq = 0;
	m_eol = (file1.find("\r\n") != std::string::npos || file2.find("\r\n") != std::string::npos) ? "\r\n" : "\n";

	for (const auto& d : defines)
	{
		if (d.first.rfind("ROOM_", 0) == 0)
		{
			const auto v = ResolveValue(d.second, defines);
			if (v && m_room_names.find(*v) == m_room_names.end()) m_room_names[*v] = d.first;
		}
	}

	static const std::regex cmpi_re("^\\s*cmpi\\.([bw])\\s+#(\\S+?),\\((g_CurrentRoom|g_OriginalRoom|g_BGM)\\)\\.l");
	static const std::regex bne_re("^\\s*bne(\\.[bsw])?\\s+(\\w+)");
	static const std::regex exit_re("^\\s*bra(\\.[bsw])?\\s+(_Done|EndCustomRoomAction)\\s*$");

	std::vector<std::vector<Branch>> segs(2);
	const std::string* files[2] = { &file1, &file2 };
	for (int seg = 0; seg < 2; ++seg)
	{
		const auto lines = SplitLines(*files[seg]);
		if (lines.empty()) return false;
		std::vector<std::pair<std::size_t, std::string>> entries;
		std::size_t epi = lines.size();
		bool first = false;
		for (std::size_t i = 0; i < lines.size(); ++i)
		{
			if (!entries.empty() && IsEpilogueLine(lines[i])) { epi = i; break; }
			std::string lab;
			if (ParseColumn0Label(lines[i], lab))
			{
				if (!first) { entries.emplace_back(i, lab); first = true; }
				else if (IsNextLabel(lab)) entries.emplace_back(i, lab);
			}
		}
		if (entries.empty()) return false;

		// Preamble comment (after any leading `module` directive) becomes the first branch's lead.
		std::string prev_trailing;
		{
			std::vector<std::string> pre;
			for (std::size_t i = 0; i < entries.front().first; ++i)
			{
				if (IsCommentLine(lines[i]) || IsBlank(lines[i])) pre.push_back(lines[i]);
				else pre.clear(); // reset at the module directive so only the trailing comment survives
			}
			// Drop leading blanks.
			std::size_t s = 0; while (s < pre.size() && IsBlank(pre[s])) ++s;
			prev_trailing = Join(pre, s, pre.size(), m_eol);
		}

		for (std::size_t k = 0; k < entries.size(); ++k)
		{
			const std::size_t bstart = entries[k].first;
			const std::size_t bend = (k + 1 < entries.size()) ? entries[k + 1].first : epi;
			std::vector<std::string> content(lines.begin() + bstart + 1, lines.begin() + bend);

			Branch b;
			b.label = entries[k].second;
			b.segment = seg;
			b.is_head = (seg == 0 && k == 0);
			b.lead = prev_trailing;

			// Guard: first code line = cmpi, next code line = bne.
			std::size_t body_start = 0;
			std::smatch m;
			int ci = -1;
			for (std::size_t j = 0; j < content.size(); ++j) if (IsCode(content[j])) { ci = static_cast<int>(j); break; }
			if (ci >= 0 && std::regex_search(content[ci], m, cmpi_re))
			{
				b.cond_size = m[1].str()[0];
				b.cond_token = m[2].str();
				b.cond_reg = m[3].str();
				int bj = -1;
				for (std::size_t j = ci + 1; j < content.size(); ++j) if (IsCode(content[j])) { bj = static_cast<int>(j); break; }
				if (bj >= 0 && std::regex_search(content[bj], m, bne_re))
				{
					b.has_guard = true;
					b.guard_size = m[1].matched ? m[1].str()[1] : 'w';
					body_start = static_cast<std::size_t>(bj) + 1;
				}
				const auto v = ResolveValue(b.cond_token, defines);
				if (v) { b.value = *v; b.key = (b.cond_reg == "g_BGM") ? KeyType::BGM : KeyType::ROOM; }
			}

			// Exit: last code line = `bra _Done`/`bra EndCustomRoomAction`.
			int lastc = -1;
			for (std::size_t j = content.size(); j-- > 0; ) if (IsCode(content[j])) { lastc = static_cast<int>(j); break; }
			std::size_t body_end;
			if (lastc >= 0 && std::regex_search(content[lastc], m, exit_re))
			{
				b.has_exit = true;
				b.exit_size = m[1].matched ? m[1].str()[1] : 'w';
				body_end = static_cast<std::size_t>(lastc);
			}
			else
			{
				body_end = (lastc >= 0) ? static_cast<std::size_t>(lastc) + 1 : content.size();
			}
			const std::size_t trail_start = b.has_exit ? static_cast<std::size_t>(lastc) + 1 : body_end;
			prev_trailing = Join(content, trail_start, content.size(), m_eol);
			// Trim leading blanks of the next lead.
			{
				auto tl = SplitLines(prev_trailing);
				std::size_t s = 0; while (s < tl.size() && IsBlank(tl[s])) ++s;
				std::size_t e = tl.size(); while (e > s && IsBlank(tl[e - 1])) --e;
				prev_trailing = Join(tl, s, e, m_eol);
			}
			b.body = Join(content, body_start, body_end, m_eol);
			segs[seg].push_back(std::move(b));
		}
	}

	// Merge the tree-warp handler: seg0's last (BGM) branch continues into seg1's first (helper) block,
	// which is presented as one body split by the marker.
	if (!segs[0].empty() && !segs[1].empty()
		&& segs[0].back().key == KeyType::BGM && !segs[1].front().has_guard)
	{
		Branch& tw = segs[0].back();
		const Branch& cont = segs[1].front();
		tw.body += m_eol + kNextFileMarker + m_eol;
		if (!cont.lead.empty()) tw.body += cont.lead + m_eol;
		tw.body += cont.label + ":" + m_eol + cont.body;
		tw.spans_files = true;
		segs[1].erase(segs[1].begin());
	}

	m_branches = std::move(segs[0]);
	for (auto& b : segs[1]) m_branches.push_back(std::move(b));
	// The trailing guardless sentinel (a bare `nop`) is boilerplate, not a user action - it is still
	// emitted (the last real branch's guard targets it) but hidden from the editor.
	if (!m_branches.empty() && !m_branches.back().has_guard && !m_branches.back().is_head)
	{
		m_branches.back().is_boilerplate = true;
	}
	m_valid = true;
	return true;
}

std::string RoomActionTable::EmitBranchLines(std::size_t k, const std::vector<std::string>& emit_labels) const
{
	const Branch& b = m_branches[k];
	std::string out;
	if (!b.lead.empty()) out += b.lead + m_eol;
	out += emit_labels[k] + ":" + m_eol;
	if (b.has_guard)
	{
		out += "\t\tcmpi." + std::string(1, b.cond_size) + "\t#" + b.cond_token + ",(" + b.cond_reg + ").l" + m_eol;
		const std::string target = (k + 1 < m_branches.size()) ? emit_labels[k + 1] : "_Done";
		out += "\t\tbne." + std::string(1, b.guard_size) + "\t" + target + m_eol;
	}
	if (!b.body.empty()) out += b.body + m_eol;
	if (b.has_exit) out += "\t\tbra." + std::string(1, b.exit_size) + "\t_Done" + m_eol;
	return out;
}

std::string RoomActionTable::EmitFile1(const std::string& header) const
{
	std::vector<std::string> emit_labels(m_branches.size());
	int n = 0;
	for (std::size_t i = 0; i < m_branches.size(); ++i)
		emit_labels[i] = m_branches[i].is_head ? "DoCustomRoomActions" : "_Next" + std::to_string(n++);

	std::string out = header;
	out += "CustomRoomActions\tmodule" + m_eol + m_eol;
	for (std::size_t k = 0; k < m_branches.size(); ++k)
	{
		if (m_branches[k].segment != 0) continue;
		std::string lines = EmitBranchLines(k, emit_labels);
		const std::size_t marker = lines.find(kNextFileMarker);
		if (marker != std::string::npos)
		{
			// Tree-warp: only the part before the marker belongs to file 1.
			std::size_t line_start = lines.rfind(m_eol, marker);
			line_start = (line_start == std::string::npos) ? 0 : line_start + m_eol.size();
			out += lines.substr(0, line_start);
		}
		else
		{
			out += lines;
		}
	}
	return out;
}

std::string RoomActionTable::EmitFile2(const std::string& header) const
{
	std::vector<std::string> emit_labels(m_branches.size());
	int n = 0;
	for (std::size_t i = 0; i < m_branches.size(); ++i)
		emit_labels[i] = m_branches[i].is_head ? "DoCustomRoomActions" : "_Next" + std::to_string(n++);

	std::string out = header;
	for (std::size_t k = 0; k < m_branches.size(); ++k)
	{
		std::string lines = EmitBranchLines(k, emit_labels);
		const std::size_t marker = lines.find(kNextFileMarker);
		if (marker != std::string::npos)
		{
			// Tree-warp: the part after the marker line starts file 2.
			std::size_t after = lines.find(m_eol, marker);
			after = (after == std::string::npos) ? lines.size() : after + m_eol.size();
			out += lines.substr(after);
		}
		else if (m_branches[k].segment == 1)
		{
			out += lines;
		}
	}
	out += m_eol + "EndCustomRoomAction:" + m_eol + "_Done:" + m_eol + "\t\trts" + m_eol + m_eol + "\tmodend" + m_eol;
	return out;
}

std::size_t RoomActionTable::IndexOf(const std::string& label) const
{
	for (std::size_t i = 0; i < m_branches.size(); ++i) if (m_branches[i].label == label) return i;
	return static_cast<std::size_t>(-1);
}

int RoomActionTable::IndexOfLabel(const std::string& label) const
{
	const auto i = IndexOf(label);
	return i == static_cast<std::size_t>(-1) ? -1 : static_cast<int>(i);
}

const RoomActionTable::Branch* RoomActionTable::FindByLabel(const std::string& label) const
{
	const auto i = IndexOf(label);
	return i == static_cast<std::size_t>(-1) ? nullptr : &m_branches[i];
}

std::vector<std::size_t> RoomActionTable::FindByRoom(int room) const
{
	std::vector<std::size_t> out;
	for (std::size_t i = 0; i < m_branches.size(); ++i)
		if (m_branches[i].key == KeyType::ROOM && m_branches[i].value == room) out.push_back(i);
	return out;
}

std::vector<std::size_t> RoomActionTable::FindByBgm(int bgm) const
{
	std::vector<std::size_t> out;
	for (std::size_t i = 0; i < m_branches.size(); ++i)
		if (m_branches[i].key == KeyType::BGM && m_branches[i].value == bgm) out.push_back(i);
	return out;
}

std::string RoomActionTable::GetBody(const std::string& label) const
{
	const auto i = IndexOf(label);
	return i == static_cast<std::size_t>(-1) ? std::string() : m_branches[i].body;
}

bool RoomActionTable::SetBody(const std::string& label, const std::string& body)
{
	const auto i = IndexOf(label);
	if (i == static_cast<std::size_t>(-1)) return false;
	m_branches[i].body = body;
	return true;
}

bool RoomActionTable::IsRemovable(const std::string& label) const
{
	const Branch* b = FindByLabel(label);
	return b && (b->key == KeyType::ROOM || b->key == KeyType::BGM) && !b->spans_files && !b->is_head;
}

std::optional<std::string> RoomActionTable::AddBranch(KeyType key, int value, const std::string& body)
{
	if (!m_valid || m_branches.empty()) return std::nullopt;
	std::string new_label;
	do { new_label = "_RoomActionAdded" + std::to_string(m_add_seq++); } while (IndexOf(new_label) != static_cast<std::size_t>(-1));

	Branch b;
	b.label = new_label;
	b.key = key;
	b.value = value;
	b.has_guard = true;
	b.guard_size = 'w';
	b.cond_size = (key == KeyType::BGM) ? 'b' : 'w';
	b.cond_reg = (key == KeyType::BGM) ? "g_BGM" : "g_CurrentRoom";
	if (key == KeyType::ROOM)
	{
		const auto it = m_room_names.find(value);
		std::ostringstream num; num << "$" << std::hex << std::uppercase << value;
		b.cond_token = (it != m_room_names.end()) ? it->second : num.str();
	}
	else
	{
		std::ostringstream num; num << "$" << std::hex << std::uppercase << (value & 0xFF);
		b.cond_token = num.str();
	}
	b.has_exit = true;
	b.exit_size = 'w';
	b.body = body.empty() ? "\t\t; New room action - add code here" : body;
	b.segment = m_branches.back().segment; // join the tail (file 2)

	// Insert before the final sentinel branch so it becomes part of the chain.
	m_branches.insert(m_branches.end() - 1, std::move(b));
	return new_label;
}

std::optional<std::string> RoomActionTable::AddRoomBranch(int room, const std::string& body) { return AddBranch(KeyType::ROOM, room, body); }
std::optional<std::string> RoomActionTable::AddBgmBranch(int bgm, const std::string& body) { return AddBranch(KeyType::BGM, bgm, body); }

bool RoomActionTable::RemoveBranch(const std::string& label)
{
	if (!IsRemovable(label)) return false;
	m_branches.erase(m_branches.begin() + IndexOf(label)); // emit re-stitches the chain
	return true;
}

bool RoomActionTable::operator==(const RoomActionTable& rhs) const
{
	if (m_valid != rhs.m_valid) return false;
	return EmitFile1() == rhs.EmitFile1() && EmitFile2() == rhs.EmitFile2();
}

} // namespace Landstalker
