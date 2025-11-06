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
    auto get_room_name = [&](int idx) {
        if (idx >= 0 && idx < gd->GetRoomData()->GetRoomCount())
        {
            auto room_entry = gd->GetRoomData()->GetRoom(idx);
            return room_entry->name;
        }
        return std::string("none");
    };

    auto get_room_display_name = [&](int idx) {
        if (idx >= 0 && idx < gd->GetRoomData()->GetRoomCount())
        {
            auto room_entry = gd->GetRoomData()->GetRoom(idx);
            return std::to_string(idx) + ": " + Landstalker::wstr_to_utf8(room_entry->GetDisplayName());
        }
        return std::string("<NONE>");
    };

    auto get_system_string = [&](uint8_t str_idx)
    {
        if (str_idx > 0 && str_idx < 0xFF)
        {
            if(str_idx >= 0x40)
            {
                return Landstalker::wstr_to_utf8(gd->GetStringData()->GetMenuStr(str_idx - 0x40));
            }
            else
            {
                return Landstalker::wstr_to_utf8(gd->GetStringData()->GetItemName(str_idx));
            }
        }
        return std::string("<NONE>");
    };

    YAML::Emitter out;
    out << YAML::BeginMap << YAML::Key << name << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "index" << YAML::Value << index;
    out << YAML::Key << "name" << YAML::Value << name;
    out << YAML::Key << "display_name" << YAML::Value << Landstalker::wstr_to_utf8(GetDisplayName());
    out << YAML::Key << "map" << YAML::Value << map;
    out << YAML::Key << "tileset" << YAML::Value << gd->GetRoomData()->GetTileset(tileset)->GetName() << YAML::Comment("Index " + std::to_string(tileset));
    out << YAML::Key << "pri_blockset" << YAML::Value << gd->GetRoomData()->GetBlockset(tileset, pri_blockset, 0)->GetName() << YAML::Comment("Index " + std::to_string(pri_blockset));
    out << YAML::Key << "sec_blockset" << YAML::Value << gd->GetRoomData()->GetBlockset(tileset, pri_blockset, sec_blockset + 1)->GetName() << YAML::Comment("Index " + std::to_string(sec_blockset + 1));
    out << YAML::Key << "room_palette" << YAML::Value << gd->GetRoomData()->GetRoomPalette(room_palette)->GetName() << YAML::Comment("Index " + std::to_string(room_palette));
    out << YAML::Key << "bgm" << YAML::Value << static_cast<int>(bgm) << YAML::Comment(wstr_to_utf8(Labels::Get(Labels::C_BGMS, bgm).value_or(L"(none)")));
    out << YAML::Key << "room_z_begin" << YAML::Value << static_cast<int>(room_z_begin);
    out << YAML::Key << "room_z_end" << YAML::Value << static_cast<int>(room_z_end);
    out << YAML::Key << "unknown_param1" << YAML::Value << static_cast<int>(unknown_param1);
    out << YAML::Key << "unknown_param2" << YAML::Value << static_cast<int>(unknown_param2);
    int fall_dest = gd->GetRoomData()->GetFallDestination(index);
    fall_dest = fall_dest >= gd->GetRoomData()->GetRoomCount() ? -1 : fall_dest;
    int climb_dest = gd->GetRoomData()->GetClimbDestination(index);
    climb_dest = climb_dest >= gd->GetRoomData()->GetRoomCount() ? -1 : climb_dest;
    out << YAML::Key << "fall_destination" << YAML::Value << get_room_name(fall_dest) << YAML::Comment(get_room_display_name(fall_dest));
    out << YAML::Key << "climb_destination" << YAML::Value << get_room_name(climb_dest) << YAML::Comment(get_room_display_name(climb_dest));
    int room_visit_flag = gd->GetStringData()->GetRoomVisitFlag(index);
    room_visit_flag = room_visit_flag >= 0xFFFF ? -1 : room_visit_flag;
    out << YAML::Key << "room_visit_flag" << YAML::Value << room_visit_flag;
    int save_loc = gd->GetStringData()->GetSaveLocation(index);
    save_loc = save_loc >= 0xFF ? -1 : save_loc;
    int map_loc = gd->GetStringData()->GetMapLocation(index);
    map_loc = map_loc >= 0xFF ? -1 : map_loc;
    std::string save_loc_name = get_system_string(save_loc);
    std::string map_loc_name = get_system_string(map_loc);
    out << YAML::Key << "save_location_string_index" << YAML::Value << save_loc << YAML::Comment(save_loc_name);
    out << YAML::Key << "map_location_string_index" << YAML::Value << map_loc << YAML::Comment(map_loc_name);
    out << YAML::Key << "is_shop" << YAML::Value << gd->GetRoomData()->IsShop(index);
    out << YAML::Key << "has_warp_tree" << YAML::Value << gd->GetRoomData()->IsTree(index);
    out << YAML::Key << "map_position" << YAML::Value << (map_loc == -1 ? map_loc : gd->GetStringData()->GetMapPosition(index));
    int lantern_flag = gd->GetRoomData()->HasLanternFlag(index) ? gd->GetRoomData()->GetLanternFlag(index) : -1;
    int lifestock_flag = gd->GetRoomData()->HasLifestockSaleFlag(index) ? gd->GetRoomData()->GetLifestockSaleFlag(index) : -1;
    out << YAML::Key << "lantern_flag" << YAML::Value << lantern_flag;
    out << YAML::Key << "lifestock_flag" << YAML::Value << lifestock_flag;

    auto chars = gd->GetStringData()->GetRoomCharacters(index);
    auto entities = gd->GetSpriteData()->GetRoomEntities(index);
    auto warps = gd->GetRoomData()->GetWarpsForRoom(index);
    auto tileswaps = gd->GetRoomData()->GetTileSwaps(index);
    auto doors = gd->GetRoomData()->GetDoors(index);
    auto chests = gd->GetRoomData()->GetChestsForRoom(index);
    // Entities
    int chest_counter = 0;
    out << YAML::Key << "entities" << YAML::Value << YAML::BeginSeq;
    for(const auto& entity : entities)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "type" << YAML::Value << static_cast<int>(entity.GetType()) << YAML::Comment(Landstalker::wstr_to_utf8(entity.GetTypeName()));
        out << YAML::Key << "position" << YAML::Value << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "x" << YAML::Value << entity.GetXDbl();
        out << YAML::Key << "y" << YAML::Value << entity.GetYDbl();
        out << YAML::Key << "z" << YAML::Value << entity.GetZDbl() << YAML::EndMap;
        out << YAML::Key << "orientation" << YAML::Value << entity.GetOrientationName();
        out << YAML::Key << "palette" << YAML::Value << static_cast<int>(entity.GetPalette());
        out << YAML::Key << "speed" << YAML::Value << static_cast<int>(entity.GetSpeed());
        out << YAML::Key << "behaviour" << YAML::Value << entity.GetBehaviour() << YAML::Comment(Landstalker::wstr_to_utf8(Labels::Get(Labels::C_BEHAVIOURS, entity.GetBehaviour()).value_or(L"(unknown)")));
        int char_index = entity.GetDialogue() < chars.size() ? chars.at(entity.GetDialogue()) : -1;
        auto char_name = std::to_string(char_index) + ": " + Landstalker::wstr_to_utf8(gd->GetStringData()->GetCharacterDisplayName(char_index));
        out << YAML::Key << "dialogue" << YAML::Value << static_cast<int>(entity.GetDialogue()) << YAML::Comment(char_index == -1 ? "" : char_name);
        out << YAML::Key << "flags" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "hostile" << YAML::Value << entity.IsHostile();
        out << YAML::Key << "no_rotate" << YAML::Value << entity.NoRotate();
        out << YAML::Key << "no_pickup" << YAML::Value << entity.NoPickup();
        out << YAML::Key << "has_dialogue" << YAML::Value << entity.HasDialogue();
        out << YAML::Key << "invisible" << YAML::Value << entity.IsVisible();
        out << YAML::Key << "not_solid" << YAML::Value << entity.IsSolid();
        out << YAML::Key << "no_gravity" << YAML::Value << entity.HasGravity();
        out << YAML::Key << "no_friction" << YAML::Value << entity.HasFriction();
        out << YAML::Key << "reserved" << YAML::Value << entity.IsReservedSet();
        out << YAML::Key << "tile_copy" << YAML::Value << entity.IsTileCopySet() << YAML::EndMap;
        out << YAML::Key << "copy_source" << YAML::Value << static_cast<int>(entity.GetCopySource());
        // If chest, add chest data
        if(entity.GetType() == 0x12 && chest_counter < chests.size()) // Chest
        {
            int chest_contents = chests.at(chest_counter++);
            out << YAML::Key << "chest_contents" << YAML::Value << chest_contents << YAML::Comment(Landstalker::wstr_to_utf8(gd->GetStringData()->GetItemDisplayName(chest_contents)));
        }
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    // Warps
    out << YAML::Key << "warps" << YAML::Value << YAML::BeginSeq;
    for(const auto& warp : warps)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "type" << YAML::Value << static_cast<int>(warp.type) << YAML::Comment([&]()
        {
            switch(warp.type)
            {
                case WarpList::Warp::Type::NORMAL: return "Normal";
                case WarpList::Warp::Type::STAIR_SE: return "Stair SE";
                case WarpList::Warp::Type::STAIR_SW: return "Stair SW";
                default: return "Unknown";
            }
        }());
        uint16_t dest_room = warp.room1 == index ? warp.room2 : warp.room1;
        const auto& dest_room_data = gd->GetRoomData()->GetRoom(dest_room);
        out << YAML::Key << "size" << YAML::Value << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "width" << YAML::Value << static_cast<int>(warp.x_size);
        out << YAML::Key << "height" << YAML::Value << static_cast<int>(warp.y_size) << YAML::EndMap;
        out << YAML::Key << "target" << YAML::Value << dest_room_data->name;
        out << YAML::Comment("Index: " + std::to_string(dest_room) + " (" + Landstalker::wstr_to_utf8(dest_room_data->GetDisplayName()) + ")");
        out << YAML::Key << "source" << YAML::Value << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "x" << YAML::Value << static_cast<int>(warp.room1 == index ? warp.x1 : warp.x2);
        out << YAML::Key << "y" << YAML::Value << static_cast<int>(warp.room1 == index ? warp.y1 : warp.y2) << YAML::EndMap;
        out << YAML::Key << "destination" << YAML::Value << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "x" << YAML::Value << static_cast<int>(warp.room1 == index ? warp.x2 : warp.x1);
        out << YAML::Key << "y" << YAML::Value << static_cast<int>(warp.room1 == index ? warp.y2 : warp.y1) << YAML::EndMap;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    // Tileswaps
    out << YAML::Key << "tileswaps" << YAML::Value << YAML::BeginSeq;
    for(const auto& ts : tileswaps)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "trigger" << YAML::Value << static_cast<int>(ts.trigger);
        out << YAML::Key << "map" << YAML::Value << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "src_x" << YAML::Value << static_cast<int>(ts.map.src_x);
        out << YAML::Key << "src_y" << YAML::Value << static_cast<int>(ts.map.src_y);
        out << YAML::Key << "dst_x" << YAML::Value << static_cast<int>(ts.map.dst_x);
        out << YAML::Key << "dst_y" << YAML::Value << static_cast<int>(ts.map.dst_y);
        out << YAML::Key << "width" << YAML::Value << static_cast<int>(ts.map.width);
        out << YAML::Key << "height" << YAML::Value << static_cast<int>(ts.map.height) << YAML::EndMap;
        out << YAML::Key << "heightmap" << YAML::Value << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "src_x" << YAML::Value << static_cast<int>(ts.heightmap.src_x);
        out << YAML::Key << "src_y" << YAML::Value << static_cast<int>(ts.heightmap.src_y);
        out << YAML::Key << "dst_x" << YAML::Value << static_cast<int>(ts.heightmap.dst_x);
        out << YAML::Key << "dst_y" << YAML::Value << static_cast<int>(ts.heightmap.dst_y);
        out << YAML::Key << "width" << YAML::Value << static_cast<int>(ts.heightmap.width);
        out << YAML::Key << "height" << YAML::Value << static_cast<int>(ts.heightmap.height) << YAML::EndMap;
        out << YAML::Key << "mode" << YAML::Value << static_cast<int>(ts.mode) << YAML::Comment([&]()
        {
            switch(ts.mode)
            {
                case TileSwap::Mode::FLOOR: return "Floor";
                case TileSwap::Mode::WALL_NE: return "Wall NE";
                case TileSwap::Mode::WALL_NW: return "Wall NW";
                default: return "Unknown";
            }
        }());
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    // Doors
    out << YAML::Key << "doors" << YAML::Value << YAML::BeginSeq;
    for(const auto& door : doors)
    {
        out << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "x" << YAML::Value << static_cast<int>(door.x);
        out << YAML::Key << "y" << YAML::Value << static_cast<int>(door.y);
        out << YAML::Key << "size" << YAML::Value << static_cast<int>(door.size);
        out << YAML::EndMap << YAML::Comment(Door::SIZE_NAMES.at(door.size));
    }
    out << YAML::EndSeq;
    // Chests
    out << YAML::Key << "no_chests_flag" << YAML::Value << gd->GetRoomData()->GetNoChestFlagForRoom(index);
    out << YAML::Key << "chests" << YAML::Value << YAML::BeginSeq;
    for(const auto& chest : chests)
    {
        out << static_cast<int>(chest) << YAML::Comment(Landstalker::wstr_to_utf8(gd->GetStringData()->GetItemDisplayName(chest)));
    }
    out << YAML::EndSeq;
    // Characters
    out << YAML::Key << "characters" << YAML::Value << YAML::BeginSeq;
    for(const auto& ch : chars)
    {
        auto char_name = Landstalker::wstr_to_utf8(gd->GetStringData()->GetCharacterDisplayName(ch));
        out << YAML::Value << ch << YAML::Comment(char_name);
    }
    out << YAML::EndSeq;
    // Flags
    auto entity_visibility_flags = gd->GetSpriteData()->GetEntityVisibilityFlagsForRoom(index);
    auto one_time_event_flags = gd->GetSpriteData()->GetOneTimeEventFlagsForRoom(index);
    auto multiple_entity_hide_flags = gd->GetSpriteData()->GetMultipleEntityHideFlagsForRoom(index);
    auto locked_doors_flags = gd->GetSpriteData()->GetLockedDoorFlagsForRoom(index);
    auto sacred_tree_flags = gd->GetSpriteData()->GetSacredTreeFlagsForRoom(index);
    auto permanent_switch_flags = gd->GetSpriteData()->GetPermanentSwitchFlagsForRoom(index);
    auto room_transition_flags = gd->GetRoomData()->GetTransitions(index);
    auto tileswap_flags = gd->GetRoomData()->GetNormalTileSwaps(index);
    auto locked_door_tileswap_flags = gd->GetRoomData()->GetLockedDoorTileSwaps(index);
    auto tree_warp_flags = gd->GetRoomData()->GetTreeWarp(index);

    out << YAML::Key << "flags" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "entity_visibility" << YAML::Value << YAML::BeginSeq;
    for(const auto& flag : entity_visibility_flags)
    {
        out << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "flag" << YAML::Value << static_cast<int>(flag.entity);
        out << YAML::Key << "entity" << YAML::Value << static_cast<int>(flag.entity);
        out << YAML::Key << "hide_on_flag_set" << YAML::Value << flag.set << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::Key << "one_time_events" << YAML::Value << YAML::BeginSeq;
    for(const auto& flag : one_time_event_flags)
    {
        out << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "on_flag" << YAML::Value << static_cast<int>(flag.flag_on);
        out << YAML::Key << "on_flag_set" << YAML::Value << flag.flag_on_set;
        out << YAML::Key << "off_flag" << YAML::Value << static_cast<int>(flag.flag_off);
        out << YAML::Key << "off_flag_set" << YAML::Value << flag.flag_off_set;
        out << YAML::Key << "entity" << YAML::Value << static_cast<int>(flag.entity) << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::Key << "multiple_entity_hides" << YAML::Value << YAML::BeginSeq;
    for(const auto& flag : multiple_entity_hide_flags)
    {
        out << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "flag" << YAML::Value << static_cast<int>(flag.flag);
        out << YAML::Key << "min_entity" << YAML::Value << static_cast<int>(flag.entity);
    }
    out << YAML::EndSeq;
    out << YAML::Key << "locked_doors" << YAML::Value << YAML::BeginSeq;
    for(const auto& flag : locked_doors_flags)
    {
        out << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "flag" << YAML::Value << static_cast<int>(flag.flag);
        out << YAML::Key << "door_entity" << YAML::Value << static_cast<int>(flag.entity) << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::Key << "sacred_trees" << YAML::Value << YAML::BeginSeq;
    for(const auto& flag : sacred_tree_flags)
    {
        out << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "flag" << YAML::Value << static_cast<int>(flag.flag);
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::Key << "permanent_switches" << YAML::Value << YAML::BeginSeq;
    for(const auto& flag : permanent_switch_flags)
    {
        out << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "flag" << YAML::Value << static_cast<int>(flag.flag);
        out << YAML::Key << "switch_entity" << YAML::Value << static_cast<int>(flag.entity) << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::Key << "room_transitions" << YAML::Value << YAML::BeginSeq;
    for(const auto& flag : room_transition_flags)
    {
        out << YAML::Flow << YAML::BeginMap;
        uint16_t dest_room = flag.src_rm == index ? flag.src_rm : flag.dst_rm;
        bool on_set = flag.src_rm == index ? true : false;
        const auto& dest_room_data = gd->GetRoomData()->GetRoom(dest_room);
        out << YAML::Key << "flag" << YAML::Value << static_cast<int>(flag.flag);
        out << YAML::Key << "on_flag_set" << YAML::Value << on_set;
        out << YAML::Key << "destination" << YAML::Value << dest_room_data->name;
        out << YAML::EndMap << YAML::Comment("Index " + std::to_string(dest_room) + " (" + Landstalker::wstr_to_utf8(dest_room_data->GetDisplayName()) + ")");
    }
    out << YAML::EndSeq;
    out << YAML::Key << "tileswaps" << YAML::Value << YAML::BeginSeq;
    for(const auto& flag : tileswap_flags)
    {
        out << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "flag" << YAML::Value << static_cast<int>(flag.flag);
        out << YAML::Key << "always" << YAML::Value << flag.always;
        out << YAML::Key << "tileswap_index" << YAML::Value << static_cast<int>(flag.index + 1) << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::Key << "locked_door_tileswaps" << YAML::Value << YAML::BeginSeq;
    for(const auto& flag : locked_door_tileswap_flags)
    {
        out << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "flag" << YAML::Value << static_cast<int>(flag.flag);
        out << YAML::Key << "always" << YAML::Value << flag.always;
        out << YAML::Key << "tileswap_index" << YAML::Value << static_cast<int>(flag.index + 1) << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::Key << "tree_warps" << YAML::Value << YAML::BeginSeq;
    if(tree_warp_flags.flag != 0 || tree_warp_flags.room1 != 0 || tree_warp_flags.room2 != 0)
    {
        out << YAML::Flow << YAML::BeginMap;
        uint16_t dest_room = tree_warp_flags.room1 == index ? tree_warp_flags.room2 : tree_warp_flags.room1;
        const auto& dest_room_data = gd->GetRoomData()->GetRoom(dest_room);
        out << YAML::Key << "flag" << YAML::Value << static_cast<int>(tree_warp_flags.flag);
        out << YAML::Key << "destination" << YAML::Value << dest_room_data->name;
        out << YAML::EndMap << YAML::Comment("Index " + std::to_string(dest_room) + " (" + Landstalker::wstr_to_utf8(dest_room_data->GetDisplayName()) + ")");
    }
    out << YAML::EndSeq << YAML::EndMap << YAML::EndMap << YAML::EndMap;
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
