#ifndef _ROOM_INDEX_MAP_H_
#define _ROOM_INDEX_MAP_H_

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <set>
#include <vector>

namespace Landstalker {

// Translation table for a room renumber: mapping[old_room_number] = new_room_number, or
// ROOM_DELETED if that room is going away.
//
// Room numbers must stay dense - the game indexes straight into the room table with no
// bounds check or validity marker, so a gap is not representable. A move mapping is
// therefore a permutation of [0, size); a delete mapping is a permutation of [0, size)
// with exactly one number removed and everything above it pulled down by one. Either way
// the result is a contiguous 0..N-1.
//
// Every structure in the game that stores a room number - warps, chests, doors, tile
// swaps, entity and flag tables, visit flags, shops, rooms.inc constants - has to be
// rewritten through the same table, or it silently ends up pointing at whatever room
// happens to now occupy the old slot. On a delete, records naming the removed room are
// dropped rather than renumbered: there is nothing left for them to point at.
using RoomIndexMap = std::vector<uint16_t>;

// Marks a room that is being removed. Only ever meaningful for a room number that is in
// range - see IsRoomDeleted, which is what callers should test. Out-of-range room numbers
// are "none" sentinels and are deliberately left alone.
constexpr uint16_t ROOM_DELETED = 0xFFFF;

// Builds the mapping for moving a single room to a new position, shuffling everything
// between the two positions along by one. Returns an empty map if the arguments are out
// of range.
RoomIndexMap MakeRoomMoveMap(std::size_t room_count, uint16_t from, uint16_t to);

// Builds the mapping for removing a room: every room above it drops by one. Returns an
// empty map if the room is out of range, or if it is the last room in the project.
RoomIndexMap MakeRoomDeleteMap(std::size_t room_count, uint16_t room);

// True if the mapping is a permutation of [0, size), i.e. a pure reorder. Rejects any
// mapping that would leave a room number unused (a gap) or reuse one (a collision):
// N entries, all distinct and all in range, can only be a bijection onto [0, N).
bool IsValidRoomIndexMap(const RoomIndexMap& mapping);

// True if the mapping is a valid renumbering, allowing for deleted rooms: the surviving
// rooms must map onto a contiguous 0..M-1 with no gaps or collisions.
bool IsValidRoomRenumbering(const RoomIndexMap& mapping);

// Number of rooms marked deleted by this mapping.
std::size_t CountDeletedRooms(const RoomIndexMap& mapping);

// Room numbers outside the mapping are left alone: tables use out-of-range values such as
// 0xFFFF as "none" sentinels, and those must survive untouched.
inline uint16_t RemapRoom(const RoomIndexMap& mapping, uint16_t room)
{
	return room < mapping.size() ? mapping[room] : room;
}

// Only an in-range room can be deleted. This is what distinguishes "this record points at
// a room that is going away" from "this field was already an out-of-range none sentinel
// that happens to equal ROOM_DELETED".
inline bool IsRoomDeleted(const RoomIndexMap& mapping, uint16_t room)
{
	return room < mapping.size() && mapping[room] == ROOM_DELETED;
}

// Rebuilds a room-keyed map under the new numbering, dropping entries belonging to a
// deleted room. Keys cannot be mutated in place, and a partial rewrite could transiently
// collide, so this always builds a fresh container.
template <typename T>
void RemapRoomKeys(const RoomIndexMap& mapping, std::map<uint16_t, T>& container)
{
	std::map<uint16_t, T> remapped;
	for (const auto& entry : container)
	{
		if (!IsRoomDeleted(mapping, entry.first))
		{
			remapped.emplace(RemapRoom(mapping, entry.first), entry.second);
		}
	}
	container.swap(remapped);
}

inline void RemapRoomValues(const RoomIndexMap& mapping, std::set<uint16_t>& container)
{
	std::set<uint16_t> remapped;
	for (const auto& room : container)
	{
		if (!IsRoomDeleted(mapping, room))
		{
			remapped.insert(RemapRoom(mapping, room));
		}
	}
	container.swap(remapped);
}

// Rewrites the room-number members of a list of records (flags, warps, shops...), erasing
// any record that names a deleted room. Records carrying two room numbers - a warp, a
// transition, a tree warp - go if *either* end is deleted, since a half-connected one
// would be worse than none.
template <typename T>
void RemapRoomRecords(const RoomIndexMap& mapping, std::vector<T>& items,
	std::initializer_list<uint16_t T::*> fields)
{
	items.erase(std::remove_if(items.begin(), items.end(), [&](const T& item)
		{
			return std::any_of(fields.begin(), fields.end(), [&](uint16_t T::* field)
				{
					return IsRoomDeleted(mapping, item.*field);
				});
		}), items.end());
	for (auto& item : items)
	{
		for (const auto& field : fields)
		{
			item.*field = RemapRoom(mapping, item.*field);
		}
	}
}

} // namespace Landstalker

#endif // _ROOM_INDEX_MAP_H_
