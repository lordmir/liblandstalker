#ifndef _ROOM_METADATA_H_
#define _ROOM_METADATA_H_

#include <vector>
#include <string>
#include <memory>
#include <landstalker/rooms/Room.h>
#include <landstalker/rooms/Entity.h>
#include <landstalker/rooms/WarpList.h>
#include <landstalker/rooms/Flags.h>
#include <landstalker/3d_maps/TileSwaps.h>
#include <landstalker/3d_maps/Doors.h>

namespace Landstalker {

class GameData;

struct RoomMetadata {
    RoomMetadata() : room_index(0xFFFF), tileset(0), pri_blockset(0), sec_blockset(0), palette(0), bgm(0), z_begin(0), z_end(0),
        unknown_param1(0), unknown_param2(0), fall_destination(0xFFFF), climb_destination(0xFFFF), visit_flag(0xFFFF),
        save_location(0), map_location(0), map_position(0), is_shop(false), is_tree(false), has_tree_warp(false),
        tree_warp_flag(0xFFFF), tree_warp_room(0xFFFF), has_lantern_flag(false), lantern_flag(0xFFFF),
        has_lifestock_flag(false), lifestock_flag(0xFFFF) {}

    uint16_t room_index;

    // Parameters from Room class
    std::string map;
    std::string name;
    uint8_t tileset;
    uint8_t pri_blockset;
    uint8_t sec_blockset;
    uint8_t palette;
    uint8_t bgm;
    uint8_t z_begin;
    uint8_t z_end;
    uint8_t unknown_param1;
    uint8_t unknown_param2;

    // Associated Data
    std::vector<WarpList::Warp> warps;
    std::vector<Entity> entities;
    std::vector<uint8_t> chests;
    std::vector<uint16_t> characters;
    std::vector<TileSwap> tileswaps;
    std::vector<Door> doors;

    // Flags & Destinations
    uint16_t fall_destination;
    uint16_t climb_destination;
    uint16_t visit_flag;
    uint8_t save_location;
    uint8_t map_location;
    uint8_t map_position;
    bool is_shop;
    bool is_tree;
    bool has_tree_warp;
    uint16_t tree_warp_flag;
    uint16_t tree_warp_room;
    bool has_lantern_flag;
    uint16_t lantern_flag;
    bool has_lifestock_flag;
    uint16_t lifestock_flag;

    // Detailed Flags
    std::vector<EntityFlag> entity_visibility_flags;
    std::vector<OneTimeEventFlag> one_time_event_flags;
    std::vector<RoomClearFlag> multiple_entity_hide_flags;
    std::vector<RoomClearFlag> locked_doors_flags;
    std::vector<SacredTreeFlag> sacred_tree_flags;
    std::vector<RoomClearFlag> permanent_switch_flags;
    std::vector<WarpList::Transition> room_transition_flags;
    std::vector<TileSwapFlag> tileswap_flags;
    std::vector<TileSwapFlag> locked_door_tileswap_flags;

    static RoomMetadata FromGameData(uint16_t roomnum, std::shared_ptr<GameData> gd);
    void ApplyToGameData(std::shared_ptr<GameData> gd) const;

    std::string ToYaml(std::shared_ptr<GameData> gd) const;
    static RoomMetadata FromYaml(const std::string& yaml_data, std::shared_ptr<GameData> gd);
};

} // namespace Landstalker

#endif // _ROOM_METADATA_H_
