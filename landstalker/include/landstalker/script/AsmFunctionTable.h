#ifndef _ASM_FUNCTION_TABLE_H_
#define _ASM_FUNCTION_TABLE_H_

#include <string>
#include <vector>
#include <map>

namespace Landstalker {

// Models a hand-written m68k dispatch module (e.g. dialogueactions.asm + its
// dialogueactionjumptable.asm) as VERBATIM TEXT, deliberately NOT routed through AsmFile:
// these files carry semantically important comments, `module`/`modend` framing and inline
// data that AsmFile would drop or reformat. The only structure interpreted is
//   - the jump table: an ordered list of `bra.w <label>` slots (index = position, a stable id)
//   - the code file: an ordered list of column-0-label-delimited blocks (verbatim bodies).
// An unmodified table re-emits byte-for-byte. See [[dialogueactions-asm-format]].
class AsmFunctionTable
{
public:
	struct Block
	{
		std::string label; // the column-0 label (without trailing ':')
		std::string text;  // verbatim source (attached lead comments + label line + body)

		bool operator==(const Block& rhs) const { return label == rhs.label && text == rhs.text; }
		bool operator!=(const Block& rhs) const { return !(*this == rhs); }
	};

	AsmFunctionTable() = default;

	// Parse the two source-file contents. `table_label` is the dispatch-table label whose
	// following `bra.w` run enumerates the slots (e.g. "CustomScriptActionTable"). Leaves
	// IsValid() false and the object empty if the structure is not recognised.
	bool Parse(const std::string& dispatch_text, const std::string& actions_text,
		const std::string& table_label);

	bool IsValid() const { return m_valid; }

	// Reconstruct the file contents - byte-identical to the parsed input when unmodified.
	std::string EmitDispatch() const;
	std::string EmitActions() const;

	// --- Dispatch table (stable index space) ---
	std::size_t SlotCount() const { return m_slots.size(); }
	// Target label of slot i (empty string if out of range).
	const std::string& GetSlotLabel(std::size_t i) const;
	// Swap the handlers that two indices dispatch to - the "move" operation. Indices stay
	// stable, so no external reference is ever renumbered (see [[reorder-semantics]]).
	bool SwapSlots(std::size_t i, std::size_t j);
	bool SetSlotLabel(std::size_t i, const std::string& label);
	// Append a new dispatch slot targeting `label` (a new highest cutscene index), reusing the
	// existing entries' line format. False if there is no existing slot to model the line on.
	bool AppendSlot(const std::string& label);
	// Remove the last dispatch slot (the highest index). False if there are none.
	bool PopSlot();
	// True if any slot dispatches to `label` - i.e. removing/blanking would orphan its block.
	bool IsSlotTarget(const std::string& label) const;

	// --- Code blocks (verbatim bodies) ---
	const std::vector<Block>& GetBlocks() const { return m_blocks; }
	const Block* FindBlock(const std::string& label) const;
	// Replace a block's verbatim text (the editor commits raw asm here). False if unknown.
	bool SetBlockText(const std::string& label, const std::string& text);
	// Reposition a block in file order (fall-through / readability); "" moves it to the front.
	bool MoveBlockAfter(const std::string& label, const std::string& after);
	// Insert a new block after `after` ("" = end). False if the label already exists.
	bool AddBlock(const std::string& label, const std::string& text, const std::string& after = std::string());
	bool RemoveBlock(const std::string& label);

	bool operator==(const AsmFunctionTable& rhs) const;
	bool operator!=(const AsmFunctionTable& rhs) const { return !(*this == rhs); }

private:
	struct Slot
	{
		std::string prefix; // line text up to the operand (indent + "bra.w" + whitespace)
		std::string label;  // the target label operand
		std::string suffix; // trailing text incl. line ending
	};

	void RebuildIndex();

	bool m_valid = false;

	// dispatch file
	std::string m_dispatch_head; // everything through the table label line (inclusive)
	std::vector<Slot> m_slots;   // ordered bra.w slots
	std::string m_dispatch_tail; // everything after the last bra.w line

	// code file
	std::string m_preamble;      // text before the first block
	std::vector<Block> m_blocks; // ordered verbatim blocks
	std::string m_epilogue;      // trailing text (blank lines + modend)

	std::map<std::string, std::size_t> m_index; // label -> block position
};

} // namespace Landstalker

#endif // _ASM_FUNCTION_TABLE_H_
