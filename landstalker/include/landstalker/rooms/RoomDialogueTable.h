#ifndef _ROOM_DIALOGUE_TABLE_H_
#define _ROOM_DIALOGUE_TABLE_H_

#include <cstdint>
#include <vector>
#include <map>

#include <landstalker/rooms/RoomIndexMap.h>

namespace Landstalker {

typedef uint16_t Character;

class RoomDialogueTable
{
public:
	// GetDialogueForRoom scans this table one entry at a time, extracting the run count
	// from each header word with `andi.w #$7800` - bits 11-14 only. A 16th run sets bit
	// 15, which the scan's `blt.s` reads as the end-of-table marker, so every room after
	// that one silently loses its dialogue. Runs, not characters: consecutive character
	// IDs are packed into a single run.
	static constexpr std::size_t MAX_RUNS_PER_ROOM = 15;

	RoomDialogueTable(const std::vector<uint16_t>& data);
	RoomDialogueTable();

	bool operator==(const RoomDialogueTable& rhs) const;
	bool operator!=(const RoomDialogueTable& rhs) const;

	std::vector<uint16_t> GetData() const;

	// The number of run-length words GetData would emit for this character list.
	static std::size_t CountRuns(const std::vector<Character>& chars);
	static bool IsValidRoomCharacterList(const std::vector<Character>& chars);

	std::vector<Character> GetRoomCharacters(uint16_t room) const;
	// Returns false and leaves the room untouched if the list needs more than
	// MAX_RUNS_PER_ROOM runs to encode.
	bool SetRoomCharacters(uint16_t room, const std::vector<Character>& chars);
	void RemapRooms(const RoomIndexMap& mapping);
private:
	static std::vector<uint16_t> EncodeRuns(const std::vector<Character>& chars);

	std::map<uint16_t, std::vector<Character>> m_dialogue_table;
};

} // namespace Landstalker

#endif // _ROOM_DIALOGUE_TABLE_H_
