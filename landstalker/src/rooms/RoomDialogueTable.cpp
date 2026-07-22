#include <landstalker/rooms/RoomDialogueTable.h>

namespace Landstalker {

RoomDialogueTable::RoomDialogueTable(const std::vector<uint16_t>& data)
{
	std::size_t i = 0;
	while(i < data.size())
	{
		if (data[i] == 0xFFFF)
		{
			break;
		}
		uint16_t room = data[i] & 0x07FF;
		int count = data[i++] >> 11;
		std::vector<Character> chars;
		for (int j = 0; j < count; ++j, ++i)
		{
			uint16_t chr = data[i] & 0x7FF;
			const uint16_t run_length = data[i] >> 11;
			for (uint16_t k = 0; k < run_length; ++k)
			{
				chars.push_back(chr + k);
			}
		}
		m_dialogue_table.insert({ room, chars });
	}
}

RoomDialogueTable::RoomDialogueTable()
{
}

bool RoomDialogueTable::operator==(const RoomDialogueTable& rhs) const
{
	return this->m_dialogue_table == rhs.m_dialogue_table;
}

bool RoomDialogueTable::operator!=(const RoomDialogueTable& rhs) const
{
	return !(*this == rhs);
}

std::vector<uint16_t> RoomDialogueTable::EncodeRuns(const std::vector<Character>& chars)
{
	std::vector<uint16_t> runs;
	for (const auto& c : chars)
	{
		// Extend the open run if this is the next consecutive character and the run
		// length (bits 11-15) has not saturated, otherwise start a new one.
		if (!runs.empty() && ((runs.back() & 0x7FF) == c - (runs.back() >> 11)) && runs.back() < 0xF800)
		{
			runs.back() += 0x0800;
		}
		else
		{
			runs.push_back(c | 0x0800);
		}
	}
	return runs;
}

std::size_t RoomDialogueTable::CountRuns(const std::vector<Character>& chars)
{
	return EncodeRuns(chars).size();
}

bool RoomDialogueTable::IsValidRoomCharacterList(const std::vector<Character>& chars)
{
	return CountRuns(chars) <= MAX_RUNS_PER_ROOM;
}

std::vector<uint16_t> RoomDialogueTable::GetData() const
{
	std::vector<uint16_t> data;
	for (const auto& d : m_dialogue_table)
	{
		auto runs = EncodeRuns(d.second);
		// SetRoomCharacters rejects lists that do not fit, so this should be unreachable.
		// Clamp anyway: emitting an over-long count sets bit 15 in the header word, which
		// the game reads as the end-of-table marker and drops every later room's
		// dialogue. Losing the tail of one room's list is the lesser failure.
		if (runs.size() > MAX_RUNS_PER_ROOM)
		{
			runs.resize(MAX_RUNS_PER_ROOM);
		}
		data.push_back(static_cast<uint16_t>(d.first | (runs.size() << 11)));
		data.insert(data.end(), runs.cbegin(), runs.cend());
	}
	data.push_back(0xFFFF);
	return data;
}

std::vector<Character> RoomDialogueTable::GetRoomCharacters(uint16_t room) const
{
	if (m_dialogue_table.count(room) == 0)
	{
		return {};
	}
	else
	{
		return m_dialogue_table.at(room);
	}
}

bool RoomDialogueTable::SetRoomCharacters(uint16_t room, const std::vector<Character>& chars)
{
	if (chars.empty())
	{
		m_dialogue_table.erase(room);
		return true;
	}
	if (!IsValidRoomCharacterList(chars))
	{
		return false;
	}
	m_dialogue_table[room] = chars;
	return true;
}

void RoomDialogueTable::RemapRooms(const RoomIndexMap& mapping)
{
	RemapRoomKeys(mapping, m_dialogue_table);
}

} // namespace Landstalker
