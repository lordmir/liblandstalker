#include <landstalker/rooms/RoomMetadata.h>
#include <landstalker/main/GameData.h>
#include <landstalker/main/RoomData.h>
#include <landstalker/main/SpriteData.h>
#include <landstalker/main/StringData.h>
#include <landstalker/misc/Labels.h>
#include <landstalker/misc/Utils.h>
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Landstalker {

RoomMetadata RoomMetadata::FromGameData(uint16_t roomnum, std::shared_ptr<GameData> gd)
{
    auto rd = gd->GetRoomData();
    auto sd = gd->GetSpriteData();
    auto room = rd->GetRoom(roomnum);
    if (!room)
    {
        return RoomMetadata();
    }

    RoomMetadata meta;
    meta.room_index = roomnum;
    meta.map = room->map;
    meta.name = room->name;
    meta.tileset = room->tileset;
    meta.pri_blockset = room->pri_blockset;
    meta.sec_blockset = room->sec_blockset;
    meta.palette = room->room_palette;
    meta.bgm = room->bgm;
    meta.z_begin = room->room_z_begin;
    meta.z_end = room->room_z_end;
    meta.unknown_param1 = room->unknown_param1;
    meta.unknown_param2 = room->unknown_param2;

    meta.warps = rd->GetWarpsForRoom(roomnum);
    meta.entities = sd->GetRoomEntities(roomnum);
    meta.chests = rd->GetChestsForRoom(roomnum);
    meta.characters = gd->GetStringData()->GetRoomCharacters(roomnum);
    meta.tileswaps = rd->GetTileSwaps(roomnum);
    meta.doors = rd->GetDoors(roomnum);

    meta.fall_destination = rd->GetFallDestination(roomnum);
    meta.climb_destination = rd->GetClimbDestination(roomnum);
    meta.visit_flag = gd->GetStringData()->GetRoomVisitFlag(roomnum);
    meta.save_location = gd->GetStringData()->GetSaveLocation(roomnum);
    meta.map_location = gd->GetStringData()->GetMapLocation(roomnum);
    meta.map_position = gd->GetStringData()->GetMapPosition(roomnum);
    meta.is_shop = rd->IsShop(roomnum);
    meta.is_tree = rd->IsTree(roomnum);
    meta.has_tree_warp = rd->HasTreeWarpFlag(roomnum);
    if (meta.has_tree_warp)
    {
        auto tree = rd->GetTreeWarp(roomnum);
        meta.tree_warp_flag = tree.flag;
        meta.tree_warp_room = tree.room2;
    }
    else
    {
        meta.tree_warp_flag = 0xFFFF;
        meta.tree_warp_room = 0xFFFF;
    }
    meta.has_lantern_flag = rd->HasLanternFlag(roomnum);
    meta.lantern_flag = meta.has_lantern_flag ? rd->GetLanternFlag(roomnum) : 0xFFFF;
    meta.has_lifestock_flag = rd->HasLifestockSaleFlag(roomnum);
    meta.lifestock_flag = meta.has_lifestock_flag ? rd->GetLifestockSaleFlag(roomnum) : 0xFFFF;

    meta.entity_visibility_flags = sd->GetEntityVisibilityFlagsForRoom(roomnum);
    meta.one_time_event_flags = sd->GetOneTimeEventFlagsForRoom(roomnum);
    meta.multiple_entity_hide_flags = sd->GetMultipleEntityHideFlagsForRoom(roomnum);
    meta.locked_doors_flags = sd->GetLockedDoorFlagsForRoom(roomnum);
    meta.sacred_tree_flags = sd->GetSacredTreeFlagsForRoom(roomnum);
    meta.permanent_switch_flags = sd->GetPermanentSwitchFlagsForRoom(roomnum);
    meta.room_transition_flags = rd->GetTransitions(roomnum);
    meta.tileswap_flags = rd->GetNormalTileSwaps(roomnum);
    meta.locked_door_tileswap_flags = rd->GetLockedDoorTileSwaps(roomnum);

    return meta;
}

void RoomMetadata::ApplyToGameData(std::shared_ptr<GameData> gd) const
{
    auto rd = gd->GetRoomData();
    auto sd = gd->GetSpriteData();
    auto room = rd->GetRoom(room_index);
    if (!room)
    {
        return;
    }

    room->map = map;
    room->name = name;
    room->tileset = tileset;
    room->pri_blockset = pri_blockset;
    room->sec_blockset = sec_blockset;
    room->room_palette = palette;
    room->bgm = bgm;
    room->room_z_begin = z_begin;
    room->room_z_end = z_end;
    room->unknown_param1 = unknown_param1;
    room->unknown_param2 = unknown_param2;

    rd->SetWarpsForRoom(room_index, warps);
    sd->SetRoomEntities(room_index, entities);
    rd->SetChestsForRoom(room_index, chests);
    gd->GetStringData()->SetRoomCharacters(room_index, characters);
    rd->SetTileSwaps(room_index, tileswaps);
    rd->SetDoors(room_index, doors);

    rd->SetFallDestination(room_index, fall_destination);
    rd->SetClimbDestination(room_index, climb_destination);
    gd->GetStringData()->SetRoomVisitFlag(room_index, visit_flag);
    gd->GetStringData()->SetSaveLocation(room_index, save_location);
    gd->GetStringData()->SetMapLocation(room_index, map_location, map_position);
    rd->SetShop(room_index, is_shop);
    rd->SetTree(room_index, is_tree);
    if (has_tree_warp)
    {
        rd->SetTreeWarp({ room_index, tree_warp_room, tree_warp_flag });
    }
    else
    {
        rd->ClearTreeWarp(room_index);
    }
    
    if (has_lantern_flag) {
        rd->SetLanternFlag(room_index, lantern_flag);
    } else {
        rd->ClearLanternFlag(room_index);
    }

    if (has_lifestock_flag) {
        rd->SetLifestockSaleFlag(room_index, lifestock_flag);
    } else {
        rd->ClearLifestockSaleFlag(room_index);
    }

    sd->SetEntityVisibilityFlagsForRoom(room_index, entity_visibility_flags);
    sd->SetOneTimeEventFlagsForRoom(room_index, one_time_event_flags);
    sd->SetMultipleEntityHideFlagsForRoom(room_index, multiple_entity_hide_flags);
    sd->SetLockedDoorFlagsForRoom(room_index, locked_doors_flags);
    sd->SetSacredTreeFlagsForRoom(room_index, sacred_tree_flags);
    sd->SetPermanentSwitchFlagsForRoom(room_index, permanent_switch_flags);
    rd->SetSrcTransitions(room_index, room_transition_flags);
    rd->SetNormalTileSwaps(room_index, tileswap_flags);
    rd->SetLockedDoorTileSwaps(room_index, locked_door_tileswap_flags);
}

std::string RoomMetadata::ToYaml(std::shared_ptr<GameData> gd) const
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
    out << YAML::Key << "index" << YAML::Value << room_index;
    out << YAML::Key << "name" << YAML::Value << name;
    out << YAML::Key << "display_name" << YAML::Value << Landstalker::wstr_to_utf8(gd->GetRoomData()->GetRoom(room_index)->GetDisplayName());
    out << YAML::Key << "map" << YAML::Value << map;
    out << YAML::Key << "tileset" << YAML::Value << gd->GetRoomData()->GetTileset(tileset)->GetName() << YAML::Comment("Index " + std::to_string(tileset));
    out << YAML::Key << "pri_blockset" << YAML::Value << gd->GetRoomData()->GetBlockset(tileset, pri_blockset, 0)->GetName() << YAML::Comment("Index " + std::to_string(pri_blockset));
    out << YAML::Key << "sec_blockset" << YAML::Value << gd->GetRoomData()->GetBlockset(tileset, pri_blockset, sec_blockset + 1)->GetName() << YAML::Comment("Index " + std::to_string(sec_blockset + 1));
    out << YAML::Key << "room_palette" << YAML::Value << gd->GetRoomData()->GetRoomPalette(palette)->GetName() << YAML::Comment("Index " + std::to_string(palette));
    out << YAML::Key << "bgm" << YAML::Value << static_cast<int>(bgm) << YAML::Comment(wstr_to_utf8(Labels::Get(Labels::C_BGMS, bgm).value_or(L"(none)")));
    out << YAML::Key << "room_z_begin" << YAML::Value << static_cast<int>(z_begin);
    out << YAML::Key << "room_z_end" << YAML::Value << static_cast<int>(z_end);
    out << YAML::Key << "unknown_param1" << YAML::Value << static_cast<int>(unknown_param1);
    out << YAML::Key << "unknown_param2" << YAML::Value << static_cast<int>(unknown_param2);
    out << YAML::Key << "fall_destination" << YAML::Value << get_room_name(fall_destination) << YAML::Comment(get_room_display_name(fall_destination));
    out << YAML::Key << "climb_destination" << YAML::Value << get_room_name(climb_destination) << YAML::Comment(get_room_display_name(climb_destination));
    out << YAML::Key << "room_visit_flag" << YAML::Value << static_cast<int>(visit_flag);
    out << YAML::Key << "save_location_string_index" << YAML::Value << static_cast<int>(save_location) << YAML::Comment(get_system_string(save_location));
    out << YAML::Key << "map_location_string_index" << YAML::Value << static_cast<int>(map_location) << YAML::Comment(get_system_string(map_location));
    out << YAML::Key << "is_shop" << YAML::Value << is_shop;
    out << YAML::Key << "has_warp_tree" << YAML::Value << is_tree;
    out << YAML::Key << "map_position" << YAML::Value << static_cast<int>(map_position);
    out << YAML::Key << "lantern_flag" << YAML::Value << static_cast<int>(lantern_flag);
    out << YAML::Key << "lifestock_flag" << YAML::Value << static_cast<int>(lifestock_flag);

    // Entities
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
        out << YAML::Key << "dialogue" << YAML::Value << static_cast<int>(entity.GetDialogue());
        out << YAML::Key << "flags" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "hostile" << YAML::Value << entity.IsHostile();
        out << YAML::Key << "no_rotate" << YAML::Value << entity.NoRotate();
        out << YAML::Key << "no_pickup" << YAML::Value << entity.NoPickup();
        out << YAML::Key << "has_dialogue" << YAML::Value << entity.HasDialogue();
        out << YAML::Key << "invisible" << YAML::Value << !entity.IsVisible();
        out << YAML::Key << "not_solid" << YAML::Value << !entity.IsSolid();
        out << YAML::Key << "no_gravity" << YAML::Value << !entity.HasGravity();
        out << YAML::Key << "no_friction" << YAML::Value << !entity.HasFriction();
        out << YAML::Key << "reserved" << YAML::Value << entity.IsReservedSet();
        out << YAML::Key << "tile_copy" << YAML::Value << entity.IsTileCopySet() << YAML::EndMap;
        out << YAML::Key << "copy_source" << YAML::Value << static_cast<int>(entity.GetCopySource());
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    // Warps
    out << YAML::Key << "warps" << YAML::Value << YAML::BeginSeq;
    for(const auto& warp : warps)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "type" << YAML::Value << static_cast<int>(warp.type);
        uint16_t dest_room = warp.room1 == room_index ? warp.room2 : warp.room1;
        out << YAML::Key << "size" << YAML::Value << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "width" << YAML::Value << static_cast<int>(warp.x_size);
        out << YAML::Key << "height" << YAML::Value << static_cast<int>(warp.y_size) << YAML::EndMap;
        out << YAML::Key << "target" << YAML::Value << get_room_name(dest_room);
        out << YAML::Key << "source" << YAML::Value << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "x" << YAML::Value << static_cast<int>(warp.room1 == room_index ? warp.x1 : warp.x2);
        out << YAML::Key << "y" << YAML::Value << static_cast<int>(warp.room1 == room_index ? warp.y1 : warp.y2) << YAML::EndMap;
        out << YAML::Key << "destination" << YAML::Value << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "x" << YAML::Value << static_cast<int>(warp.room1 == room_index ? warp.x2 : warp.x1);
        out << YAML::Key << "y" << YAML::Value << static_cast<int>(warp.room1 == room_index ? warp.y2 : warp.y1) << YAML::EndMap;
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
        out << YAML::Key << "mode" << YAML::Value << static_cast<int>(ts.mode);
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
        out << YAML::Key << "size" << YAML::Value << static_cast<int>(door.size) << YAML::EndMap;
    }
    out << YAML::EndSeq;

    // Chests
    out << YAML::Key << "chests" << YAML::Value << YAML::BeginSeq;
    for(const auto& chest : chests)
    {
        out << static_cast<int>(chest);
    }
    out << YAML::EndSeq;

    // Characters
    out << YAML::Key << "characters" << YAML::Value << YAML::BeginSeq;
    for(const auto& ch : characters)
    {
        out << static_cast<int>(ch);
    }
    out << YAML::EndSeq;

    // Flags
    out << YAML::Key << "flags" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "entity_visibility" << YAML::Value << YAML::BeginSeq;
    for(const auto& flag : entity_visibility_flags)
    {
        out << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "flag" << YAML::Value << static_cast<int>(flag.flag);
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
        out << YAML::Key << "min_entity" << YAML::Value << static_cast<int>(flag.entity) << YAML::EndMap;
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
        out << YAML::Key << "flag" << YAML::Value << static_cast<int>(flag.flag) << YAML::EndMap;
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
        bool on_set = flag.src_rm == room_index ? true : false;
        out << YAML::Key << "flag" << YAML::Value << static_cast<int>(flag.flag);
        out << YAML::Key << "on_flag_set" << YAML::Value << on_set;
        out << YAML::Key << "destination" << YAML::Value << get_room_name(on_set ? flag.dst_rm : flag.src_rm) << YAML::EndMap;
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
    if(has_tree_warp)
    {
        out << YAML::Flow << YAML::BeginMap;
        out << YAML::Key << "flag" << YAML::Value << static_cast<int>(tree_warp_flag);
        out << YAML::Key << "destination" << YAML::Value << get_room_name(tree_warp_room) << YAML::EndMap;
    }
    out << YAML::EndSeq << YAML::EndMap << YAML::EndMap;
    return out.c_str();
}

RoomMetadata RoomMetadata::FromYaml(const std::string& yaml_data, std::shared_ptr<GameData> gd)
{
    YAML::Node config = YAML::Load(yaml_data);
    if (!config.IsMap() || config.size() == 0) return RoomMetadata();

    auto root = config.begin()->second;
    RoomMetadata meta;

    meta.room_index = root["index"].as<uint16_t>();
    meta.name = root["name"].as<std::string>();
    meta.map = root["map"].as<std::string>();
    
    std::string ts_name = root["tileset"].as<std::string>();
    meta.tileset = gd->GetRoomData()->GetTileset(ts_name)->GetIndex();
    
    std::string p_name = root["room_palette"].as<std::string>();
    meta.palette = gd->GetRoomData()->GetRoomPalette(p_name)->GetIndex();
    
    std::string pb_name = root["pri_blockset"].as<std::string>();
    meta.pri_blockset = gd->GetRoomData()->GetBlockset(pb_name)->GetPrimary();
    
    std::string sb_name = root["sec_blockset"].as<std::string>();
    meta.sec_blockset = gd->GetRoomData()->GetBlockset(sb_name)->GetSecondary() - 1;

    meta.bgm = root["bgm"].as<int>();
    meta.z_begin = root["room_z_begin"].as<int>();
    meta.z_end = root["room_z_end"].as<int>();
    meta.unknown_param1 = root["unknown_param1"].as<int>();
    meta.unknown_param2 = root["unknown_param2"].as<int>();

    auto get_room_idx = [&](const std::string& name) -> uint16_t {
        if (name == "none") return 0xFFFF;
        auto room = gd->GetRoomData()->GetRoom(name);
        return room ? room->index : 0xFFFF;
    };

    meta.fall_destination = get_room_idx(root["fall_destination"].as<std::string>());
    meta.climb_destination = get_room_idx(root["climb_destination"].as<std::string>());
    meta.visit_flag = root["room_visit_flag"].as<int>();
    meta.save_location = root["save_location_string_index"].as<int>();
    meta.map_location = root["map_location_string_index"].as<int>();
    meta.is_shop = root["is_shop"].as<bool>();
    meta.is_tree = root["has_warp_tree"].as<bool>();
    meta.map_position = root["map_position"].as<int>();
    meta.lantern_flag = root["lantern_flag"].as<int>();
    meta.has_lantern_flag = meta.lantern_flag != 0xFFFF;
    meta.lifestock_flag = root["lifestock_flag"].as<int>();
    meta.has_lifestock_flag = meta.lifestock_flag != 0xFFFF;

    // Entities
    if (root["entities"])
    {
        for (auto ent_node : root["entities"])
        {
            Entity ent;
            ent.SetType(ent_node["type"].as<int>());
            ent.SetXDbl(ent_node["position"]["x"].as<double>());
            ent.SetYDbl(ent_node["position"]["y"].as<double>());
            ent.SetZDbl(ent_node["position"]["z"].as<double>());
            
            std::string orient = ent_node["orientation"].as<std::string>();
            if (orient == "NE") ent.SetOrientation(Orientation::NE);
            else if (orient == "SE") ent.SetOrientation(Orientation::SE);
            else if (orient == "SW") ent.SetOrientation(Orientation::SW);
            else if (orient == "NW") ent.SetOrientation(Orientation::NW);

            ent.SetPalette(ent_node["palette"].as<int>());
            ent.SetSpeed(ent_node["speed"].as<int>());
            ent.SetBehaviour(ent_node["behaviour"].as<int>());
            ent.SetDialogue(ent_node["dialogue"].as<int>());
            
            auto flags = ent_node["flags"];
            ent.SetHostile(flags["hostile"].as<bool>());
            ent.SetNoRotate(flags["no_rotate"].as<bool>());
            ent.SetNoPickup(flags["no_pickup"].as<bool>());
            ent.SetHasDialogue(flags["has_dialogue"].as<bool>());
            ent.SetVisible(!flags["invisible"].as<bool>());
            ent.SetSolid(!flags["not_solid"].as<bool>());
            ent.SetGravity(!flags["no_gravity"].as<bool>());
            ent.SetFriction(!flags["no_friction"].as<bool>());
            ent.SetReserved(flags["reserved"].as<bool>());
            ent.SetTileCopy(flags["tile_copy"].as<bool>());
            ent.SetCopySource(ent_node["copy_source"].as<int>());
            
            meta.entities.push_back(ent);
        }
    }

    // Warps
    if (root["warps"])
    {
        for (auto warp_node : root["warps"])
        {
            WarpList::Warp warp;
            warp.type = static_cast<WarpList::Warp::Type>(warp_node["type"].as<int>());
            warp.x_size = warp_node["size"]["width"].as<int>();
            warp.y_size = warp_node["size"]["height"].as<int>();
            uint16_t target = get_room_idx(warp_node["target"].as<std::string>());
            warp.room1 = meta.room_index;
            warp.room2 = target;
            warp.x1 = warp_node["source"]["x"].as<int>();
            warp.y1 = warp_node["source"]["y"].as<int>();
            warp.x2 = warp_node["destination"]["x"].as<int>();
            warp.y2 = warp_node["destination"]["y"].as<int>();
            meta.warps.push_back(warp);
        }
    }

    // Tileswaps
    if (root["tileswaps"])
    {
        for (auto ts_node : root["tileswaps"])
        {
            TileSwap ts;
            ts.trigger = ts_node["trigger"].as<int>();
            ts.map.src_x = ts_node["map"]["src_x"].as<int>();
            ts.map.src_y = ts_node["map"]["src_y"].as<int>();
            ts.map.dst_x = ts_node["map"]["dst_x"].as<int>();
            ts.map.dst_y = ts_node["map"]["dst_y"].as<int>();
            ts.map.width = ts_node["map"]["width"].as<int>();
            ts.map.height = ts_node["map"]["height"].as<int>();
            ts.heightmap.src_x = ts_node["heightmap"]["src_x"].as<int>();
            ts.heightmap.src_y = ts_node["heightmap"]["src_y"].as<int>();
            ts.heightmap.dst_x = ts_node["heightmap"]["dst_x"].as<int>();
            ts.heightmap.dst_y = ts_node["heightmap"]["dst_y"].as<int>();
            ts.heightmap.width = ts_node["heightmap"]["width"].as<int>();
            ts.heightmap.height = ts_node["heightmap"]["height"].as<int>();
            ts.mode = static_cast<TileSwap::Mode>(ts_node["mode"].as<int>());
            meta.tileswaps.push_back(ts);
        }
    }

    // Doors
    if (root["doors"])
    {
        for (auto door_node : root["doors"])
        {
            Door door;
            door.x = door_node["x"].as<int>();
            door.y = door_node["y"].as<int>();
            door.size = static_cast<Door::Size>(door_node["size"].as<int>());
            meta.doors.push_back(door);
        }
    }

    // Chests
    if (root["chests"])
    {
        for (auto chest_node : root["chests"])
        {
            meta.chests.push_back(chest_node.as<int>());
        }
    }

    // Characters
    if (root["characters"])
    {
        for (auto char_node : root["characters"])
        {
            meta.characters.push_back(char_node.as<int>());
        }
    }

    // Flags
    if (root["flags"])
    {
        auto flags = root["flags"];
        if (flags["entity_visibility"])
        {
            for (auto f : flags["entity_visibility"])
            {
                EntityFlag ef(meta.room_index);
                ef.flag = f["flag"].as<int>();
                ef.entity = f["entity"].as<int>();
                ef.set = f["hide_on_flag_set"].as<bool>();
                meta.entity_visibility_flags.push_back(ef);
            }
        }
        if (flags["one_time_events"])
        {
            for (auto f : flags["one_time_events"])
            {
                OneTimeEventFlag ef(meta.room_index);
                ef.flag_on = f["on_flag"].as<int>();
                ef.flag_on_set = f["on_flag_set"].as<bool>();
                ef.flag_off = f["off_flag"].as<int>();
                ef.flag_off_set = f["off_flag_set"].as<bool>();
                ef.entity = f["entity"].as<int>();
                meta.one_time_event_flags.push_back(ef);
            }
        }
        if (flags["multiple_entity_hides"])
        {
            for (auto f : flags["multiple_entity_hides"])
            {
                RoomClearFlag ef(meta.room_index);
                ef.flag = f["flag"].as<int>();
                ef.entity = f["min_entity"].as<int>();
                meta.multiple_entity_hide_flags.push_back(ef);
            }
        }
        if (flags["locked_doors"])
        {
            for (auto f : flags["locked_doors"])
            {
                RoomClearFlag ef(meta.room_index);
                ef.flag = f["flag"].as<int>();
                ef.entity = f["door_entity"].as<int>();
                meta.locked_doors_flags.push_back(ef);
            }
        }
        if (flags["sacred_trees"])
        {
            for (auto f : flags["sacred_trees"])
            {
                SacredTreeFlag ef(meta.room_index);
                ef.flag = f["flag"].as<int>();
                meta.sacred_tree_flags.push_back(ef);
            }
        }
        if (flags["permanent_switches"])
        {
            for (auto f : flags["permanent_switches"])
            {
                RoomClearFlag ef(meta.room_index);
                ef.flag = f["flag"].as<int>();
                ef.entity = f["switch_entity"].as<int>();
                meta.permanent_switch_flags.push_back(ef);
            }
        }
        if (flags["room_transitions"])
        {
            for (auto f : flags["room_transitions"])
            {
                uint16_t dest = get_room_idx(f["destination"].as<std::string>());
                uint16_t flag = f["flag"].as<int>();
                bool on_set = f["on_flag_set"].as<bool>();
                if (on_set) meta.room_transition_flags.emplace_back(meta.room_index, dest, flag);
                else meta.room_transition_flags.emplace_back(dest, meta.room_index, flag);
            }
        }
        if (flags["tileswaps"])
        {
            for (auto f : flags["tileswaps"])
            {
                TileSwapFlag ef(meta.room_index, f["tileswap_index"].as<int>() - 1);
                ef.flag = f["flag"].as<int>();
                ef.always = f["always"].as<bool>();
                meta.tileswap_flags.push_back(ef);
            }
        }
        if (flags["locked_door_tileswaps"])
        {
            for (auto f : flags["locked_door_tileswaps"])
            {
                TileSwapFlag ef(meta.room_index, f["tileswap_index"].as<int>() - 1);
                ef.flag = f["flag"].as<int>();
                ef.always = f["always"].as<bool>();
                meta.locked_door_tileswap_flags.push_back(ef);
            }
        }
        if (flags["tree_warps"] && flags["tree_warps"].size() > 0)
        {
            auto f = flags["tree_warps"][0];
            meta.has_tree_warp = true;
            meta.tree_warp_flag = f["flag"].as<int>();
            meta.tree_warp_room = get_room_idx(f["destination"].as<std::string>());
        }
    }

    return meta;
}

} // namespace Landstalker
