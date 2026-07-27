#include <landstalker/rooms/RoomIndexMap.h>

#include <algorithm>
#include <numeric>

namespace Landstalker {

RoomIndexMap MakeRoomMoveMap(std::size_t room_count, uint16_t from, uint16_t to)
{
	if (room_count == 0 || from >= room_count || to >= room_count)
	{
		return {};
	}
	RoomIndexMap mapping(room_count);
	std::iota(mapping.begin(), mapping.end(), static_cast<uint16_t>(0));
	if (from == to)
	{
		return mapping;
	}
	mapping[from] = to;
	if (from < to)
	{
		// Everything the room passes on its way down moves up one slot.
		for (uint16_t i = static_cast<uint16_t>(from + 1); i <= to; ++i)
		{
			mapping[i] = static_cast<uint16_t>(i - 1);
		}
	}
	else
	{
		for (uint16_t i = to; i < from; ++i)
		{
			mapping[i] = static_cast<uint16_t>(i + 1);
		}
	}
	return mapping;
}

RoomIndexMap MakeRoomDeleteMap(std::size_t room_count, uint16_t room)
{
	// Refuse to empty the project: a room list of zero has no valid room 0 for anything
	// to fall back to, and every table that indexes rooms would be left dangling.
	if (room_count < 2 || room >= room_count)
	{
		return {};
	}
	RoomIndexMap mapping(room_count);
	for (uint16_t i = 0; i < static_cast<uint16_t>(room_count); ++i)
	{
		mapping[i] = (i < room) ? i : (i == room ? ROOM_DELETED : static_cast<uint16_t>(i - 1));
	}
	return mapping;
}

bool IsValidRoomIndexMap(const RoomIndexMap& mapping)
{
	return IsValidRoomRenumbering(mapping) && CountDeletedRooms(mapping) == 0;
}

bool IsValidRoomRenumbering(const RoomIndexMap& mapping)
{
	if (mapping.empty())
	{
		return false;
	}
	const auto survivors = mapping.size() - CountDeletedRooms(mapping);
	if (survivors == 0)
	{
		return false;
	}
	// The surviving rooms must land on 0..survivors-1, each exactly once.
	std::vector<bool> seen(survivors, false);
	for (const auto& target : mapping)
	{
		if (target == ROOM_DELETED)
		{
			continue;
		}
		if (target >= survivors || seen[target])
		{
			return false;
		}
		seen[target] = true;
	}
	return true;
}

std::size_t CountDeletedRooms(const RoomIndexMap& mapping)
{
	return static_cast<std::size_t>(std::count(mapping.cbegin(), mapping.cend(), ROOM_DELETED));
}

} // namespace Landstalker
