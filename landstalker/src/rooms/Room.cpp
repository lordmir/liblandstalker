#include <landstalker/rooms/Room.h>
#include <landstalker/misc/Utils.h>
#include <landstalker/main/GameData.h>

#include <cassert>

#include <yaml-cpp/yaml.h>

namespace Landstalker {

Room::Room(const std::string& name_, const std::string& map_name, uint16_t index_, const std::vector<uint8_t>& params)
    : map(map_name),
      name(name_),
      index(index_)
{
    assert(params.size() == 4);
    SetParams(params[0], params[1], params[2], params[3]);
}

Room::Room(const std::string& name_, const std::string& map_name, uint16_t index_, uint8_t params[4])
    : map(map_name),
      name(name_),
      index(index_)
{
    SetParams(params[0], params[1], params[2], params[3]);
}

std::string Room::ToYaml(std::shared_ptr<GameData> gd) const
{
    YAML::Emitter out;
    out << YAML::BeginMap << YAML::Key << name << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "map" << YAML::Value << map;
    out << YAML::Key << "name" << YAML::Value << name << YAML::Comment(Landstalker::wstr_to_utf8(GetDisplayName()));
    out << YAML::Key << "index" << YAML::Value << index;
    out << YAML::Key << "tileset" << YAML::Value << gd->GetRoomData()->GetTileset(tileset)->GetName() << YAML::Comment(std::to_string(tileset));
    out << YAML::Key << "pri_blockset" << YAML::Value << gd->GetRoomData()->GetBlockset(tileset, pri_blockset, 0)->GetName() << YAML::Comment(std::to_string(pri_blockset));
    out << YAML::Key << "sec_blockset" << YAML::Value << gd->GetRoomData()->GetBlockset(tileset, pri_blockset, sec_blockset + 1)->GetName() << YAML::Comment(std::to_string(sec_blockset + 1));
    out << YAML::Key << "room_palette" << YAML::Value << gd->GetRoomData()->GetRoomPalette(room_palette)->GetName() << YAML::Comment(std::to_string(room_palette));
    out << YAML::Key << "bgm" << YAML::Value << static_cast<int>(bgm) << YAML::Comment(wstr_to_utf8(Labels::Get(Labels::C_BGMS, bgm).value_or(L"(none)")));
    out << YAML::Key << "room_z_begin" << YAML::Value << static_cast<int>(room_z_begin);
    out << YAML::Key << "room_z_end" << YAML::Value << static_cast<int>(room_z_end);
    // Entities
    // Warps
    // Tileswaps
    // Doors
    // Chests
    // Characters
    // Flags
    // Properties
    out << YAML::EndMap << YAML::EndMap;
    return out.c_str();
}

Room Room::FromYaml(const std::string& yaml_data, std::shared_ptr<GameData> gd)
{
    return Room("", "", 0, std::vector<uint8_t>{0,0,0,0});
}

bool Room::operator==(const Room& rhs) const
{
    return ((this->bgm == rhs.bgm) &&
            (this->map == rhs.map) &&
            (this->pri_blockset == rhs.pri_blockset) &&
            (this->room_palette == rhs.room_palette) &&
            (this->room_z_begin == rhs.room_z_begin) &&
            (this->room_z_end == rhs.room_z_end) &&
            (this->sec_blockset == rhs.sec_blockset) &&
            (this->tileset == rhs.tileset) &&
            (this->unknown_param1 == rhs.unknown_param1) &&
            (this->unknown_param2 == rhs.unknown_param2));
}

bool Room::operator!=(const Room& rhs) const
{
    return !(*this == rhs);
}

void Room::SetParams(uint8_t param0, uint8_t param1, uint8_t param2, uint8_t param3)
{
    unknown_param1 = param0 >> 6;
    pri_blockset = (param0 >> 5) & 0x01;
    tileset = param0 & 0x1F;
    unknown_param2 = param1 >> 6;
    room_palette = param1 & 0x3F;
    room_z_end = param2 >> 4;
    room_z_begin = param2 & 0x0F;
    sec_blockset = param3 >> 5;
    bgm = param3 & 0x1F;
}

std::array<uint8_t, 4> Room::GetParams() const
{
    std::array<uint8_t, 4> ret{};
    ret[0] = ((unknown_param1 & 0x03) << 6) | ((pri_blockset & 0x01) << 5) | (tileset & 0x1F);
    ret[1] = ((unknown_param2 & 0x03) << 6) | (room_palette & 0x3F);
    ret[2] = ((room_z_end & 0x0F) << 4) | (room_z_begin & 0x0F);
    ret[3] = ((sec_blockset & 0x07) << 5) | (bgm & 0x1F);
    return ret;
}

uint8_t Room::GetBlocksetId() const
{
    return pri_blockset << 5 | tileset;
}

std::wstring Room::GetDisplayName() const
{
    return Labels::Get(Labels::C_ROOMS, index).value_or(std::wstring(name.cbegin(), name.cend()));
}

} // namespace Landstalker
