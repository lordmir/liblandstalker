#ifndef _ROOM_ACTION_TABLE_H_
#define _ROOM_ACTION_TABLE_H_

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Landstalker {

// Models the hand-written per-room fixup chain (customroomactions1.asm + customroomactions2.asm) as
// a list of structured branches. Each branch is `cmpi.[bw] #<ROOM_x|bgm>,(g_CurrentRoom|g_OriginalRoom
// |g_BGM).l / bne <next> / <body> [/ bra _Done]`; the editor edits only the body, and the label,
// guard and trailing `bra` are regenerated on emit (labels renumbered _Next0..N). The bar is a
// binary-exact assemble, NOT source-exact: the emitted files are regenerated cleanly (standard header,
// renumbered labels) but keep every instruction encoding and branch size so the assembled ROM is
// identical. The BGM tree-warp handler spans both files (a data block sits between them in the ROM);
// it is presented as one body with a `{{Next File}}` marker separating the two file-halves, split
// back on emit. See [[dialogueactions-asm-format]].
class RoomActionTable {
public:
	enum class KeyType { ROOM, BGM, OTHER };

	// The editor-visible marker separating the two file-halves of the tree-warp handler's body.
	static const std::string kNextFileMarker;

	struct Branch {
		std::string label;          // in-memory id (parsed label); stable for a session, renumbered on emit
		bool is_head = false;       // DoCustomRoomActions - the entry point, keeps its label
		KeyType key = KeyType::OTHER;
		int value = -1;             // resolved room number / BGM id; -1 for OTHER
		std::string cond_token;     // the cmpi operand after '#' (ROOM_GUMI / $1D)
		std::string cond_reg;       // g_CurrentRoom / g_OriginalRoom / g_BGM ("" if no guard)
		char cond_size = 'w';       // the cmpi size
		bool has_guard = false;     // has a cmpi+bne guard (a sentinel/helper may not)
		char guard_size = 's';      // the bne size (s/w) - preserved so the encoding is unchanged
		bool has_exit = false;      // ends with a `bra _Done`/`bra EndCustomRoomAction` (regenerated)
		char exit_size = 'w';       // that bra's size
		bool spans_files = false;   // the tree-warp branch: body contains kNextFileMarker
		bool is_boilerplate = false;// the trailing `nop` sentinel - emitted, but not an editable action
		int segment = 0;            // 0 = customroomactions1, 1 = customroomactions2
		std::string lead;           // descriptive comment lines emitted before the branch (verbatim)
		std::string body;           // the editable body (verbatim instructions), may hold the marker
	};

	RoomActionTable() = default;

	bool Parse(const std::string& file1, const std::string& file2,
		const std::map<std::string, std::string>& defines);
	bool IsValid() const { return m_valid; }

	// Regenerated file contents (labels renumbered, standard boilerplate). `header1`/`header2` are the
	// AsmFile file-header comment blocks to prepend (empty for none).
	std::string EmitFile1(const std::string& header = std::string()) const;
	std::string EmitFile2(const std::string& header = std::string()) const;

	const std::vector<Branch>& GetBranches() const { return m_branches; }
	std::vector<std::size_t> FindByRoom(int room) const;
	std::vector<std::size_t> FindByBgm(int bgm) const;
	const Branch* FindByLabel(const std::string& label) const;
	// The chain position of a branch (used to key its C_ROOM_ACTION name), or -1.
	int IndexOfLabel(const std::string& label) const;

	// The editable body for a branch (what the code editor shows - scaffold hidden). "" if unknown.
	std::string GetBody(const std::string& label) const;
	bool SetBody(const std::string& label, const std::string& body);

	// Append a new room-/BGM-keyed branch (empty stub body if `body` empty). Returns the new label.
	std::optional<std::string> AddRoomBranch(int room, const std::string& body = std::string());
	std::optional<std::string> AddBgmBranch(int bgm, const std::string& body = std::string());
	// Remove a ROOM/BGM branch (the chain is re-stitched automatically on emit). False if not removable.
	bool RemoveBranch(const std::string& label);
	bool IsRemovable(const std::string& label) const;

	bool operator==(const RoomActionTable& rhs) const;
	bool operator!=(const RoomActionTable& rhs) const { return !(*this == rhs); }

private:
	std::string EmitBranchLines(std::size_t k, const std::vector<std::string>& emit_labels) const;
	std::size_t IndexOf(const std::string& label) const;
	std::optional<std::string> AddBranch(KeyType key, int value, const std::string& body);

	bool m_valid = false;
	std::string m_eol = "\n";
	std::vector<Branch> m_branches;
	std::map<int, std::string> m_room_names; // room number -> ROOM_x token, for new branches
	int m_add_seq = 0;
};

} // namespace Landstalker

#endif // _ROOM_ACTION_TABLE_H_
