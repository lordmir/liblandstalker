#ifndef _ITEM_USE_TABLE_H_
#define _ITEM_USE_TABLE_H_

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Landstalker {

// Models the item pre-use / post-use handlers (itemuse1.asm + itemuse2.asm = one ItemUse module split
// across two files; itempostuse.asm = the ItemPostUse module) and their two dispatch tables
// (PreUseItemTable / PostUseItemTable, each a run of `bra.w handler / dc.b item_id,$FF` entries). Each
// handler is a verbatim named block delimited by the dispatch-target labels (helpers fold into the
// preceding handler). The pre-use pool spans two files with a data block between them in the ROM, so
// it is held as one block list with a `{{Next File}}` marker at the itemuse1/itemuse2 boundary (end of
// the last itemuse1 handler, ItemUseLantern), split back on emit. Handler blocks stay verbatim (the
// tables reference their labels); the tables are regenerated cleanly (only the bra.w targets, dc.b ids
// and terminator are byte-critical). See [[dialogueactions-asm-format]].
class ItemUseTable {
public:
	static const std::string kNextFileMarker;

	struct Block {
		std::string label; // the handler label (ItemUseEkeEke / PostUseGarlic / ...)
		std::string text;  // verbatim block: label line + body (+ folded helpers); may hold the marker
	};
	struct Entry {
		std::string id_expr;   // the dc.b operand verbatim ("ITM_EKEEKE" / "($80|ITM_GARLIC)")
		int item = -1;         // resolved item id (low 7 bits; -1 if unresolved)
		std::string handler;   // the bra.w target label
	};

	ItemUseTable() = default;

	bool Parse(const std::string& pre_table, const std::string& post_table,
		const std::string& itemuse1, const std::string& itemuse2, const std::string& itempostuse,
		const std::map<std::string, std::string>& defines);
	bool IsValid() const { return m_valid; }

	std::string EmitPreTable() const;
	std::string EmitPostTable() const;
	std::string EmitItemUse1(const std::string& header = std::string()) const;
	std::string EmitItemUse2(const std::string& header = std::string()) const;
	std::string EmitItemPostUse(const std::string& header = std::string()) const;

	// The handler label bound to `item` in the pre-/post-use table, or "" if none.
	std::string PreUseHandlerFor(int item) const;
	std::string PostUseHandlerFor(int item) const;
	// Every item that has a pre-/post-use handler, in table order.
	std::vector<int> PreUseItems() const;
	std::vector<int> PostUseItems() const;

	// Editable body of a handler block (pre or post), or "" if unknown.
	std::string GetBlockBody(const std::string& label) const;
	bool SetBlockBody(const std::string& label, const std::string& body);

	// Bind a new (empty stub) handler to an item; returns the new handler label, or nullopt if the
	// item already has one / the pool is unavailable.
	std::optional<std::string> AddPreUse(int item);
	std::optional<std::string> AddPostUse(int item);
	// Remove an item's handler + its table entry (and the handler block if nothing else uses it).
	bool RemovePreUse(int item);
	bool RemovePostUse(int item);

	bool operator==(const ItemUseTable& rhs) const;
	bool operator!=(const ItemUseTable& rhs) const { return !(*this == rhs); }

private:
	struct Dispatch { std::string head; std::string tail; std::vector<Entry> entries; };

	static std::string EmitTable(const Dispatch& d);
	std::size_t FindBlock(std::vector<Block>& pool, const std::string& label) const;
	const Block* FindBlockC(const std::string& label) const;
	std::optional<std::string> Add(bool pre, int item);
	bool Remove(bool pre, int item);

	bool m_valid = false;
	std::string m_eol = "\n";
	Dispatch m_pre, m_post;
	std::string m_pre_preamble;   // itemuse1 module header + lead comment
	std::vector<Block> m_pre_blocks; // itemuse1 then itemuse2 handler blocks (marker in the join block)
	std::string m_post_preamble;  // itempostuse module + RunItemPostUse dispatcher
	std::vector<Block> m_post_blocks;
	int m_add_seq = 0;
	std::map<int, std::string> m_item_names; // item value -> ITM_x token, for new entries
};

} // namespace Landstalker

#endif // _ITEM_USE_TABLE_H_
