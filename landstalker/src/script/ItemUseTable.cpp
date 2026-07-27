#include <landstalker/script/ItemUseTable.h>

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <sstream>

#include <landstalker/script/AsmText.h>

namespace Landstalker {

using namespace AsmText;

const std::string ItemUseTable::kNextFileMarker = "{{Next File}}";

namespace {

const char* const kDivider = "; ---------------------------------------------------------------------------";

// Parse a dispatch table into head + entries(handler,id_expr) + tail (the terminator onward).
bool ParseDispatch(const std::string& text, const std::string& eol,
	const std::map<std::string, std::string>& defines,
	std::string& head, std::vector<ItemUseTable::Entry>& entries, std::string& tail)
{
	const auto lines = SplitLines(text);
	static const std::regex bra_re("^\\s*bra\\.w\\s+(\\w+)");
	static const std::regex dcb_re("^\\s*dc\\.b\\s+(.+?),\\s*\\$FF\\s*$", std::regex::icase);
	auto skippable = [](const std::string& l)
	{
		std::size_t j = 0; while (j < l.size() && (l[j] == ' ' || l[j] == '\t')) ++j;
		return j >= l.size() || l[j] == ';' || l[j] == '\r';
	};
	std::size_t i = 0;
	// head = up to the first bra.w
	while (i < lines.size() && !std::regex_search(lines[i], bra_re)) ++i;
	if (i >= lines.size()) return false;
	head = Join(lines, 0, i, eol);
	std::smatch m;
	while (i < lines.size())
	{
		while (i < lines.size() && skippable(lines[i])) ++i;
		if (i >= lines.size() || !std::regex_search(lines[i], m, bra_re)) break; // terminator reached
		const std::string handler = m[1].str();
		++i;
		while (i < lines.size() && skippable(lines[i])) ++i;
		if (i >= lines.size() || !std::regex_search(lines[i], m, dcb_re)) break;
		ItemUseTable::Entry e;
		e.handler = handler;
		e.id_expr = m[1].str();
		// trim trailing ws from id_expr
		while (!e.id_expr.empty() && (e.id_expr.back() == ' ' || e.id_expr.back() == '\t')) e.id_expr.pop_back();
		const auto v = ResolveValue(e.id_expr, defines);
		if (v) e.item = *v & 0x7F;
		entries.push_back(e);
		++i;
	}
	tail = Join(lines, i, lines.size(), eol);
	return !entries.empty();
}

// Split a handler pool into (preamble, blocks) using `handlers` as the block-delimiting labels.
void ParsePool(const std::string& text, const std::set<std::string>& handlers, const std::string& eol,
	std::string& preamble, std::vector<ItemUseTable::Block>& blocks)
{
	const auto lines = SplitLines(text);
	std::vector<std::pair<std::size_t, std::string>> idx;
	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		std::string lab;
		if (ParseColumn0Label(lines[i], lab) && handlers.count(lab)) idx.emplace_back(i, lab);
	}
	if (idx.empty()) { preamble = text; return; }
	// Each segment keeps a trailing eol so concatenation reproduces the file line-for-line.
	preamble = (idx.front().first > 0) ? Join(lines, 0, idx.front().first, eol) + eol : std::string();
	// A handler block runs from its label to the start of the next handler's *lead* (the run of comment
	// and blank lines immediately above the next label). That lead describes the next handler, so it is
	// carried into that block rather than trailing onto this one. Splits stay contiguous, so the
	// concatenation - and the round-trip - is unchanged.
	std::size_t start = idx.front().first;
	for (std::size_t k = 0; k < idx.size(); ++k)
	{
		std::size_t end = (k + 1 < idx.size()) ? idx[k + 1].first : lines.size();
		if (k + 1 < idx.size())
		{
			while (end > idx[k].first + 1 && (IsCommentLine(lines[end - 1]) || IsBlank(lines[end - 1]))) --end;
		}
		ItemUseTable::Block b;
		b.label = idx[k].second;
		b.text = Join(lines, start, end, eol) + eol;
		blocks.push_back(std::move(b));
		start = end;
	}
}

// Split an itemuse2-style preamble into (file header, lead of the first handler). The lead is the run
// of comment lines directly attached to (immediately above) the first handler label; everything before
// it - the file/module header paragraph - stays with the file-split marker. Concatenation is preserved.
void SplitPreambleLead(const std::string& preamble, const std::string& eol, std::string& header, std::string& lead)
{
	const auto lines = SplitLines(preamble);
	std::size_t s = lines.size();
	while (s > 0 && IsCommentLine(lines[s - 1])) --s; // the attached comment paragraph (no blank gap)
	if (s == 0 || s == lines.size()) { header = preamble; lead.clear(); return; }
	header = Join(lines, 0, s, eol) + eol;
	lead = Join(lines, s, lines.size(), eol) + eol;
}

} // namespace

bool ItemUseTable::Parse(const std::string& pre_table, const std::string& post_table,
	const std::string& itemuse1, const std::string& itemuse2, const std::string& itempostuse,
	const std::map<std::string, std::string>& defines)
{
	m_valid = false;
	*this = ItemUseTable();
	m_eol = (pre_table.find("\r\n") != std::string::npos) ? "\r\n" : "\n";

	for (const auto& d : defines)
	{
		if (d.first.rfind("ITM_", 0) == 0)
		{
			const auto v = ResolveValue(d.first, defines);
			if (v && m_item_names.find(*v & 0x7F) == m_item_names.end()) m_item_names[*v & 0x7F] = d.first;
		}
	}

	if (!ParseDispatch(pre_table, m_eol, defines, m_pre.head, m_pre.entries, m_pre.tail)) return false;
	if (!ParseDispatch(post_table, m_eol, defines, m_post.head, m_post.entries, m_post.tail)) return false;

	std::set<std::string> pre_h, post_h;
	for (const auto& e : m_pre.entries) pre_h.insert(e.handler);
	for (const auto& e : m_post.entries) post_h.insert(e.handler);

	std::string pre2_preamble;
	std::vector<Block> blocks1, blocks2;
	ParsePool(itemuse1, pre_h, m_eol, m_pre_preamble, blocks1);
	ParsePool(itemuse2, pre_h, m_eol, pre2_preamble, blocks2);
	if (blocks1.empty() || blocks2.empty()) return false;
	// The file boundary lives at the end of the last itemuse1 block; carry the marker + itemuse2's file
	// header there so a clean split restores both files. Itemuse2's own header comment describes the
	// *first* itemuse2 handler, so hand that lead to its block rather than trailing it onto Lantern.
	blocks1.back().text += m_eol + kNextFileMarker + m_eol;
	std::string header2, lead2;
	SplitPreambleLead(pre2_preamble, m_eol, header2, lead2);
	if (!header2.empty()) blocks1.back().text += header2;
	if (!lead2.empty()) blocks2.front().text = lead2 + blocks2.front().text;
	m_pre_blocks = std::move(blocks1);
	for (auto& b : blocks2) m_pre_blocks.push_back(std::move(b));

	ParsePool(itempostuse, post_h, m_eol, m_post_preamble, m_post_blocks);
	if (m_post_blocks.empty()) return false;

	m_valid = true;
	return true;
}

std::string ItemUseTable::EmitTable(const Dispatch& d)
{
	// eol is fixed to '\n' for regenerated tables (assembles identically); host preserves it on write.
	const std::string eol = "\n";
	std::string out = d.head.empty() ? std::string() : d.head + eol;
	for (const auto& e : d.entries)
	{
		out += "\t\tbra.w\t" + e.handler + eol;
		out += "\t\tdc.b \t" + e.id_expr + ", $FF" + eol;
		out += std::string(kDivider) + eol;
	}
	out += d.tail;
	if (!out.empty() && out.back() != '\n') out += eol;
	return out;
}

std::string ItemUseTable::EmitPreTable() const { return EmitTable(m_pre); }
std::string ItemUseTable::EmitPostTable() const { return EmitTable(m_post); }

std::string ItemUseTable::EmitItemUse1(const std::string& header) const
{
	std::string pool = m_pre_preamble;
	for (const auto& b : m_pre_blocks) pool += b.text;
	const std::size_t marker = pool.find(kNextFileMarker);
	std::string first = (marker == std::string::npos) ? pool : pool.substr(0, pool.rfind(m_eol, marker) + m_eol.size());
	return header + first;
}

std::string ItemUseTable::EmitItemUse2(const std::string& header) const
{
	std::string pool = m_pre_preamble;
	for (const auto& b : m_pre_blocks) pool += b.text;
	const std::size_t marker = pool.find(kNextFileMarker);
	if (marker == std::string::npos) return header;
	std::size_t after = pool.find(m_eol, marker);
	after = (after == std::string::npos) ? pool.size() : after + m_eol.size();
	return header + pool.substr(after);
}

std::string ItemUseTable::EmitItemPostUse(const std::string& header) const
{
	std::string out = header + m_post_preamble;
	for (const auto& b : m_post_blocks) out += b.text;
	return out;
}

std::string ItemUseTable::PreUseHandlerFor(int item) const
{
	for (const auto& e : m_pre.entries) if (e.item == (item & 0x7F)) return e.handler;
	return {};
}
std::string ItemUseTable::PostUseHandlerFor(int item) const
{
	for (const auto& e : m_post.entries) if (e.item == (item & 0x7F)) return e.handler;
	return {};
}
std::vector<int> ItemUseTable::PreUseItems() const
{
	std::vector<int> out;
	for (const auto& e : m_pre.entries) if (e.item >= 0) out.push_back(e.item);
	return out;
}
std::vector<int> ItemUseTable::PostUseItems() const
{
	std::vector<int> out;
	for (const auto& e : m_post.entries) if (e.item >= 0) out.push_back(e.item);
	return out;
}

const ItemUseTable::Block* ItemUseTable::FindBlockC(const std::string& label) const
{
	for (const auto& b : m_pre_blocks) if (b.label == label) return &b;
	for (const auto& b : m_post_blocks) if (b.label == label) return &b;
	return nullptr;
}

std::size_t ItemUseTable::FindBlock(std::vector<Block>& pool, const std::string& label) const
{
	for (std::size_t i = 0; i < pool.size(); ++i) if (pool[i].label == label) return i;
	return static_cast<std::size_t>(-1);
}

std::string ItemUseTable::GetBlockBody(const std::string& label) const
{
	const Block* b = FindBlockC(label);
	return b ? b->text : std::string();
}

bool ItemUseTable::SetBlockBody(const std::string& label, const std::string& body)
{
	for (auto& b : m_pre_blocks) if (b.label == label) { b.text = body; return true; }
	for (auto& b : m_post_blocks) if (b.label == label) { b.text = body; return true; }
	return false;
}

std::optional<std::string> ItemUseTable::Add(bool pre, int item)
{
	if (!m_valid) return std::nullopt;
	item &= 0x7F;
	Dispatch& disp = pre ? m_pre : m_post;
	std::vector<Block>& pool = pre ? m_pre_blocks : m_post_blocks;
	for (const auto& e : disp.entries) if (e.item == item) return std::nullopt; // already bound
	if (pool.empty()) return std::nullopt;

	std::string label;
	do { label = (pre ? "ItemUseAdded" : "PostUseAdded") + std::to_string(m_add_seq++); }
	while (FindBlockC(label) != nullptr);

	const auto it = m_item_names.find(item);
	std::ostringstream num; num << "$" << std::hex << std::uppercase << item;
	const std::string itm = (it != m_item_names.end()) ? it->second : num.str();

	Entry e;
	e.handler = label;
	e.item = item;
	e.id_expr = pre ? itm : ("($80|" + itm + ")");
	disp.entries.push_back(e);

	Block b;
	b.label = label;
	b.text = label + ":" + m_eol + "\t\t; New " + std::string(pre ? "pre" : "post") + "-use handler" + m_eol
		+ "\t\tbra.w\t" + std::string(pre ? "ReturnSuccess" : "_puDone") + m_eol + m_eol;
	// Insert before the final block (which carries the module's shared exits / modend).
	pool.insert(pool.end() - 1, std::move(b));
	return label;
}

std::optional<std::string> ItemUseTable::AddPreUse(int item) { return Add(true, item); }
std::optional<std::string> ItemUseTable::AddPostUse(int item) { return Add(false, item); }

bool ItemUseTable::Remove(bool pre, int item)
{
	if (!m_valid) return false;
	item &= 0x7F;
	Dispatch& disp = pre ? m_pre : m_post;
	std::vector<Block>& pool = pre ? m_pre_blocks : m_post_blocks;
	std::string handler;
	bool removed = false;
	for (std::size_t i = 0; i < disp.entries.size(); ++i)
	{
		if (disp.entries[i].item == item) { handler = disp.entries[i].handler; disp.entries.erase(disp.entries.begin() + i); removed = true; break; }
	}
	if (!removed) return false;
	// Drop the handler block too, unless it is the pool's last block (which holds the shared exits /
	// modend) or something else still references it.
	bool referenced = false;
	for (const auto& e : disp.entries) if (e.handler == handler) referenced = true;
	const std::size_t bi = FindBlock(pool, handler);
	if (!referenced && bi != static_cast<std::size_t>(-1) && bi + 1 < pool.size())
	{
		pool.erase(pool.begin() + bi);
	}
	return true;
}

bool ItemUseTable::RemovePreUse(int item) { return Remove(true, item); }
bool ItemUseTable::RemovePostUse(int item) { return Remove(false, item); }

bool ItemUseTable::operator==(const ItemUseTable& rhs) const
{
	if (m_valid != rhs.m_valid) return false;
	return EmitPreTable() == rhs.EmitPreTable() && EmitPostTable() == rhs.EmitPostTable()
		&& EmitItemUse1() == rhs.EmitItemUse1() && EmitItemUse2() == rhs.EmitItemUse2()
		&& EmitItemPostUse() == rhs.EmitItemPostUse();
}

} // namespace Landstalker
