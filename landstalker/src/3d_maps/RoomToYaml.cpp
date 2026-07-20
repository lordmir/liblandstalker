#include <landstalker/3d_maps/RoomToYaml.h>
#include <landstalker/main/GameData.h>
#include <landstalker/misc/Utils.h>
#include <landstalker/rooms/Room.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

namespace Landstalker {

namespace {

YAML::Node RequireMap(const YAML::Node& parent, const char* field, const std::string& context)
{
    const auto node = parent[field];
    if (!node.IsMap())
    {
        throw std::runtime_error(context + "." + field + " must be a map.");
    }
    return node;
}

int ReadInt(const YAML::Node& parent, const char* field, int minimum, int maximum,
    const std::string& context)
{
    try
    {
        const int value = parent[field].as<int>();
        if (value < minimum || value > maximum)
        {
            throw std::runtime_error(context + "." + field + " must be between " +
                std::to_string(minimum) + " and " + std::to_string(maximum) + ".");
        }
        return value;
    }
    catch (const YAML::Exception& e)
    {
        throw std::runtime_error("Invalid " + context + "." + field + ": " + e.what());
    }
}

int ReadInt(const YAML::Node& node, int minimum, int maximum, const std::string& context)
{
    try
    {
        const int value = node.as<int>();
        if (value < minimum || value > maximum)
        {
            throw std::runtime_error(context + " must be between " +
                std::to_string(minimum) + " and " + std::to_string(maximum) + ".");
        }
        return value;
    }
    catch (const YAML::Exception& e)
    {
        throw std::runtime_error("Invalid " + context + ": " + e.what());
    }
}

double ReadDouble(const YAML::Node& parent, const char* field, const std::string& context)
{
    try
    {
        const double value = parent[field].as<double>();
        if (!std::isfinite(value))
        {
            throw std::runtime_error(context + "." + field + " must be finite.");
        }
        return value;
    }
    catch (const YAML::Exception& e)
    {
        throw std::runtime_error("Invalid " + context + "." + field + ": " + e.what());
    }
}

bool ReadBool(const YAML::Node& parent, const char* field, const std::string& context)
{
    try
    {
        return parent[field].as<bool>();
    }
    catch (const YAML::Exception& e)
    {
        throw std::runtime_error("Invalid " + context + "." + field + ": " + e.what());
    }
}

std::string ReadString(const YAML::Node& parent, const char* field, const std::string& context)
{
    try
    {
        return parent[field].as<std::string>();
    }
    catch (const YAML::Exception& e)
    {
        throw std::runtime_error("Invalid " + context + "." + field + ": " + e.what());
    }
}

} // namespace

bool RoomToYaml::ExportToYaml(const std::string& filename, uint16_t roomnum,
    const std::shared_ptr<GameData>& game_data)
{
    if (!game_data || roomnum >= game_data->GetRoomData()->GetRoomCount())
    {
        return false;
    }

    std::ofstream output(filename, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!output)
    {
        return false;
    }

    YAML::Emitter out;
    EmitYaml(out, *game_data->GetRoomData()->GetRoom(roomnum), game_data);
    if (!out.good())
    {
        return false;
    }
    output << out.c_str() << '\n';
    return output.good();
}

bool RoomToYaml::ImportFromYaml(const std::string& filename, const RoomKey& key,
    const std::shared_ptr<GameData>& game_data)
{
    std::ifstream input(filename, std::ios::binary | std::ios::in);
    if (!input)
    {
        return false;
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof())
    {
        return false;
    }
    return ImportFromYamlText(contents.str(), key, game_data);
}

bool RoomToYaml::ImportFromYamlText(const std::string& yaml, const RoomKey& key,
    const std::shared_ptr<GameData>& game_data)
{
    if (!game_data)
    {
        return false;
    }

    const YAML::Node root = YAML::Load(yaml);
    if (!root.IsMap())
    {
        throw std::runtime_error("Room metadata must be a YAML map.");
    }

    const auto room_data = game_data->GetRoomData();
    YAML::Node node;
    const bool unwrapped = root["index"].IsDefined() || root["name"].IsDefined() ||
        root["map"].IsDefined() || root["display_name"].IsDefined();
    if (unwrapped)
    {
        // Also accept an unwrapped room map for hand-authored metadata.
        node = root;
    }
    else if (const auto name = std::get_if<std::string>(&key); name && root[*name].IsDefined())
    {
        node = root[*name];
    }
    else if (root.size() == 1 && root.begin()->second.IsMap())
    {
        node = root.begin()->second;
    }
    else if (const auto index = std::get_if<uint16_t>(&key))
    {
        for (const auto& entry : root)
        {
            if (!entry.second.IsMap() || !entry.second["index"].IsDefined())
            {
                continue;
            }
            try
            {
                if (entry.second["index"].as<uint16_t>() == *index)
                {
                    node = entry.second;
                    break;
                }
            }
            catch (const YAML::Exception&)
            {
                // A selected malformed index is reported below with its field name.
            }
        }
    }

    if (!node || !node.IsMap())
    {
        throw std::runtime_error("The YAML file does not contain room metadata.");
    }

    uint16_t roomnum = 0;
    const bool target_by_id = !std::holds_alternative<std::string>(key);
    if (const auto index = std::get_if<uint16_t>(&key))
    {
        roomnum = *index;
    }
    else if (const auto name = std::get_if<std::string>(&key))
    {
        const auto& rooms = room_data->GetRoomlist();
        const auto room = std::find_if(rooms.cbegin(), rooms.cend(), [&](const auto& candidate)
        {
            return candidate->name == *name;
        });
        if (room == rooms.cend())
        {
            throw std::runtime_error("Unknown room name '" + *name + "'.");
        }
        roomnum = (*room)->index;
    }
    else
    {
        if (!node["index"].IsDefined())
        {
            throw std::runtime_error("Room metadata does not contain an index.");
        }
        try
        {
            const auto file_index = node["index"].as<unsigned int>();
            if (file_index > 0xFFFF)
            {
                throw std::runtime_error("Room metadata index is out of range.");
            }
            roomnum = static_cast<uint16_t>(file_index);
        }
        catch (const YAML::Exception& e)
        {
            throw std::runtime_error(std::string("Invalid 'index': ") + e.what());
        }
    }
    if (roomnum >= room_data->GetRoomCount())
    {
        throw std::runtime_error("Room metadata index is out of range.");
    }
    const auto room = room_data->GetRoom(roomnum);

    auto read_string = [&](const char* field)
    {
        try
        {
            return node[field].as<std::string>();
        }
        catch (const YAML::Exception& e)
        {
            throw std::runtime_error(std::string("Invalid '") + field + "': " + e.what());
        }
    };
    auto read_bounded = [&](const char* field, int current, int minimum, int maximum)
    {
        if (!node[field].IsDefined())
        {
            return current;
        }
        int value = 0;
        try
        {
            value = node[field].as<int>();
        }
        catch (const YAML::Exception& e)
        {
            throw std::runtime_error(std::string("Invalid '") + field + "': " + e.what());
        }
        if (value < minimum || value > maximum)
        {
            throw std::runtime_error(std::string("'") + field + "' must be between " +
                std::to_string(minimum) + " and " + std::to_string(maximum) + ".");
        }
        return value;
    };

    std::string room_name = room->name;
    if (target_by_id && node["name"].IsDefined())
    {
        room_name = read_string("name");
        if (!RoomData::IsValidRoomName(room_name))
        {
            throw std::runtime_error("The imported room name is not a valid assembly label.");
        }
        const auto& rooms = room_data->GetRoomlist();
        const auto duplicate = std::find_if(rooms.cbegin(), rooms.cend(), [&](const auto& candidate)
        {
            return candidate != room && candidate->name == room_name;
        });
        if (duplicate != rooms.cend())
        {
            throw std::runtime_error("The imported room name is already in use.");
        }
    }

    const auto resolve_room = [&](const std::string& name, const std::string& context)
    {
        if (name == room_name)
        {
            return roomnum;
        }
        const auto& rooms = room_data->GetRoomlist();
        const auto target = std::find_if(rooms.cbegin(), rooms.cend(), [&](const auto& candidate)
        {
            return candidate->name == name;
        });
        if (target == rooms.cend())
        {
            throw std::runtime_error(context + " references unknown room '" + name + "'.");
        }
        return (*target)->index;
    };

    std::string map = room->map;
    uint8_t tileset = room->tileset;
    uint8_t pri_blockset = room->pri_blockset;
    uint8_t sec_blockset = room->sec_blockset;
    uint8_t room_palette = room->room_palette;
    std::wstring display_name = room->GetDisplayName();
    const bool has_display_name = node["display_name"].IsDefined();

    if (node["map"].IsDefined())
    {
        map = read_string("map");
        if (room_data->GetMaps().count(map) == 0)
        {
            throw std::runtime_error("Unknown room map '" + map + "'.");
        }
    }

    if (node["tileset"].IsDefined())
    {
        const auto name = read_string("tileset");
        const auto tilesets = room_data->GetAllTilesets();
        const auto entry = tilesets.find(name);
        if (entry == tilesets.end())
        {
            throw std::runtime_error("Unknown room tileset '" + name + "'.");
        }
        tileset = static_cast<uint8_t>(entry->second->GetIndex());
    }

    const auto blocksets = room_data->GetAllBlocksets();
    if (node["pri_blockset"].IsDefined())
    {
        const auto name = read_string("pri_blockset");
        const auto entry = blocksets.find(name);
        if (entry == blocksets.end())
        {
            throw std::runtime_error("Unknown primary blockset '" + name + "'.");
        }
        if (entry->second->GetTileset() != tileset || entry->second->GetSecondary() != 0)
        {
            throw std::runtime_error("Primary blockset '" + name + "' is not valid for the selected tileset.");
        }
        pri_blockset = entry->second->GetPrimary();
    }
    if (node["sec_blockset"].IsDefined())
    {
        const auto name = read_string("sec_blockset");
        const auto entry = blocksets.find(name);
        if (entry == blocksets.end())
        {
            throw std::runtime_error("Unknown secondary blockset '" + name + "'.");
        }
        if (entry->second->GetTileset() != tileset ||
            entry->second->GetPrimary() != pri_blockset || entry->second->GetSecondary() == 0)
        {
            throw std::runtime_error("Secondary blockset '" + name + "' is not valid for the selected tileset and primary blockset.");
        }
        sec_blockset = static_cast<uint8_t>(entry->second->GetSecondary() - 1);
    }
    if (!room_data->GetBlockset(tileset, pri_blockset, 0) ||
        !room_data->GetBlockset(tileset, pri_blockset, static_cast<uint8_t>(sec_blockset + 1)))
    {
        throw std::runtime_error("The imported tileset and blockset combination is not valid.");
    }

    if (node["room_palette"].IsDefined())
    {
        const auto name = read_string("room_palette");
        const auto& palettes = room_data->GetRoomPalettes();
        const auto entry = std::find_if(palettes.cbegin(), palettes.cend(), [&](const auto& palette)
        {
            return palette->GetName() == name;
        });
        if (entry == palettes.cend())
        {
            throw std::runtime_error("Unknown room palette '" + name + "'.");
        }
        room_palette = static_cast<uint8_t>((*entry)->GetIndex());
    }

    if (has_display_name)
    {
        try
        {
            display_name = utf8_to_wstr(read_string("display_name"));
        }
        catch (const std::exception&)
        {
            throw std::runtime_error("The imported room display name is not valid UTF-8.");
        }
        const auto normalized_display_name = Labels::NormalizePath(display_name);
        if (!normalized_display_name)
        {
            throw std::runtime_error("The imported room display name contains invalid text.");
        }
        display_name = *normalized_display_name;
        if (display_name != room->GetDisplayName() &&
            !Labels::IsValid(display_name, Labels::C_ROOMS, roomnum))
        {
            throw std::runtime_error("The imported room display name is invalid or already in use.");
        }
    }

    const auto bgm = static_cast<uint8_t>(read_bounded("bgm", room->bgm, 0, 0x1F));
    const auto room_z_begin = static_cast<uint8_t>(read_bounded("room_z_begin", room->room_z_begin, 0, 0x0F));
    const auto room_z_end = static_cast<uint8_t>(read_bounded("room_z_end", room->room_z_end, 0, 0x0F));
    const auto unknown_param1 = static_cast<uint8_t>(read_bounded("unknown_param1", room->unknown_param1, 0, 0x03));
    const auto unknown_param2 = static_cast<uint8_t>(read_bounded("unknown_param2", room->unknown_param2, 0, 0x03));

    struct Destination
    {
        bool enabled;
        uint16_t room;
    };
    const auto read_destination = [&](const char* field) -> std::optional<Destination>
    {
        if (!node[field].IsDefined())
        {
            return std::nullopt;
        }
        const auto name = ReadString(node, field, "room");
        if (name == "none")
        {
            return Destination{ false, 0 };
        }
        return Destination{ true, resolve_room(name, field) };
    };
    const auto fall_destination = read_destination("fall_destination");
    const auto climb_destination = read_destination("climb_destination");

    std::optional<uint16_t> room_visit_flag;
    if (node["room_visit_flag"].IsDefined())
    {
        const int value = ReadInt(node, "room_visit_flag", -1, 0x7FFF, "room");
        room_visit_flag = value < 0 ? 0xFFFF : static_cast<uint16_t>(value);
    }

    std::optional<uint8_t> save_location;
    if (node["save_location_string_index"].IsDefined())
    {
        const int value = ReadInt(node, "save_location_string_index", -1, 0xFE, "room");
        save_location = value < 0 ? 0xFF : static_cast<uint8_t>(value);
    }

    const auto validate_system_string = [&](uint8_t value, const char* field)
    {
        if (value == 0xFF)
        {
            return;
        }
        const auto string_data = game_data->GetStringData();
        const bool valid_item = value < 0x40 && value < string_data->GetItemNameCount();
        const bool valid_menu = value >= 0x40 &&
            static_cast<std::size_t>(value - 0x40) < string_data->GetMenuStrCount();
        if (!valid_item && !valid_menu)
        {
            throw std::runtime_error(std::string("'") + field + "' does not reference a valid system string.");
        }
    };
    if (save_location)
    {
        validate_system_string(*save_location, "save_location_string_index");
    }

    std::optional<std::pair<uint8_t, uint8_t>> map_location;
    if (node["map_location_string_index"].IsDefined() || node["map_position"].IsDefined())
    {
        const auto string_data = game_data->GetStringData();
        int location = string_data->GetMapLocation(roomnum);
        int position = string_data->GetMapPosition(roomnum);
        location = location == 0xFF ? -1 : location;
        position = position == 0xFF ? -1 : position;
        if (node["map_location_string_index"].IsDefined())
        {
            location = ReadInt(node, "map_location_string_index", -1, 0xFE, "room");
        }
        if (node["map_position"].IsDefined())
        {
            position = ReadInt(node, "map_position", -1, 0xFE, "room");
        }
        if (location < 0)
        {
            if (node["map_position"].IsDefined() && position >= 0)
            {
                throw std::runtime_error("'map_position' must be -1 when no map location is selected.");
            }
            map_location = std::make_pair<uint8_t, uint8_t>(0xFF, 0xFF);
        }
        else
        {
            if (position < 0)
            {
                throw std::runtime_error("'map_position' is required when a map location is selected.");
            }
            validate_system_string(static_cast<uint8_t>(location), "map_location_string_index");
            map_location = std::make_pair(static_cast<uint8_t>(location), static_cast<uint8_t>(position));
        }
    }

    std::optional<bool> is_shop;
    if (node["is_shop"].IsDefined())
    {
        is_shop = ReadBool(node, "is_shop", "room");
    }
    std::optional<bool> has_warp_tree;
    if (node["has_warp_tree"].IsDefined())
    {
        has_warp_tree = ReadBool(node, "has_warp_tree", "room");
    }

    const auto read_optional_flag = [&](const char* field) -> std::optional<uint16_t>
    {
        if (!node[field].IsDefined())
        {
            return std::nullopt;
        }
        const int value = ReadInt(node, field, -1, 0x7FF, "room");
        return value < 0 ? 0xFFFF : static_cast<uint16_t>(value);
    };
    const auto lantern_flag = read_optional_flag("lantern_flag");
    const auto lifestock_flag = read_optional_flag("lifestock_flag");

    std::optional<std::vector<Entity>> entities;
    std::optional<std::vector<uint8_t>> entity_chests;
    if (node["entities"].IsDefined())
    {
        const auto entity_nodes = node["entities"];
        if (!entity_nodes.IsSequence())
        {
            throw std::runtime_error("'entities' must be a sequence.");
        }
        if (entity_nodes.size() > 15)
        {
            throw std::runtime_error("A room cannot contain more than 15 entities.");
        }

        entities.emplace();
        entities->reserve(entity_nodes.size());
        entity_chests.emplace();
        const auto existing_chests = room_data->GetChestsForRoom(roomnum);
        for (std::size_t i = 0; i < entity_nodes.size(); ++i)
        {
            const auto entity_node = entity_nodes[i];
            const auto context = "entities[" + std::to_string(i) + "]";
            if (!entity_node.IsMap())
            {
                throw std::runtime_error(context + " must be a map.");
            }

            Entity entity;
            entity.SetType(static_cast<uint8_t>(ReadInt(entity_node, "type", 0, 0xFF, context)));
            const auto position = RequireMap(entity_node, "position", context);
            const double x = ReadDouble(position, "x", context + ".position");
            const double y = ReadDouble(position, "y", context + ".position");
            const double z = ReadDouble(position, "z", context + ".position");
            if (!entity.SetXDbl(x) || std::abs(entity.GetXDbl() - x) > 0.0001)
            {
                throw std::runtime_error(context + ".position.x must be a half-integer between 0.5 and 64.");
            }
            if (!entity.SetYDbl(y) || std::abs(entity.GetYDbl() - y) > 0.0001)
            {
                throw std::runtime_error(context + ".position.y must be a half-integer between 0.5 and 64.");
            }
            if (!entity.SetZDbl(z) || std::abs(entity.GetZDbl() - z) > 0.0001)
            {
                throw std::runtime_error(context + ".position.z must be a half-integer between 0 and 15.5.");
            }

            const auto orientation = ReadString(entity_node, "orientation", context);
            if (orientation == "NE") entity.SetOrientation(Orientation::NE);
            else if (orientation == "SE") entity.SetOrientation(Orientation::SE);
            else if (orientation == "SW") entity.SetOrientation(Orientation::SW);
            else if (orientation == "NW") entity.SetOrientation(Orientation::NW);
            else throw std::runtime_error(context + ".orientation must be NE, SE, SW, or NW.");

            entity.SetPalette(static_cast<uint8_t>(ReadInt(entity_node, "palette", 0, 3, context)));
            entity.SetSpeed(static_cast<uint8_t>(ReadInt(entity_node, "speed", 0, 7, context)));
            entity.SetBehaviour(static_cast<uint16_t>(ReadInt(entity_node, "behaviour", 0, 0x3FF, context)));
            entity.SetDialogue(static_cast<uint8_t>(ReadInt(entity_node, "dialogue", 0, 0x3F, context)));
            entity.SetCopySource(static_cast<uint8_t>(ReadInt(entity_node, "copy_source", 0, 0x0F, context)));

            const auto flags = RequireMap(entity_node, "flags", context);
            entity.SetHostile(ReadBool(flags, "hostile", context + ".flags"));
            entity.SetNoRotate(ReadBool(flags, "no_rotate", context + ".flags"));
            entity.SetNoPickup(ReadBool(flags, "no_pickup", context + ".flags"));
            entity.SetHasDialogue(ReadBool(flags, "has_dialogue", context + ".flags"));
            // These legacy YAML keys contain the corresponding positive getter values.
            entity.SetVisible(ReadBool(flags, "invisible", context + ".flags"));
            entity.SetSolid(ReadBool(flags, "not_solid", context + ".flags"));
            entity.SetGravity(ReadBool(flags, "no_gravity", context + ".flags"));
            entity.SetFriction(ReadBool(flags, "no_friction", context + ".flags"));
            entity.SetReserved(ReadBool(flags, "reserved", context + ".flags"));
            entity.SetTileCopy(ReadBool(flags, "tile_copy", context + ".flags"));
            if (entity.IsChest())
            {
                if (entity_node["chest_contents"].IsDefined())
                {
                    entity_chests->push_back(static_cast<uint8_t>(
                        ReadInt(entity_node, "chest_contents", 0, 0xFF, context)));
                }
                else if (entity_chests->size() < existing_chests.size())
                {
                    entity_chests->push_back(existing_chests[entity_chests->size()]);
                }
                else
                {
                    entity_chests->push_back(0);
                }
            }
            entities->push_back(entity);
        }
    }

    std::optional<std::vector<WarpList::Warp>> warps;
    if (node["warps"].IsDefined())
    {
        const auto warp_nodes = node["warps"];
        if (!warp_nodes.IsSequence())
        {
            throw std::runtime_error("'warps' must be a sequence.");
        }

        warps.emplace();
        warps->reserve(warp_nodes.size());
        for (std::size_t i = 0; i < warp_nodes.size(); ++i)
        {
            const auto warp_node = warp_nodes[i];
            const auto context = "warps[" + std::to_string(i) + "]";
            if (!warp_node.IsMap())
            {
                throw std::runtime_error(context + " must be a map.");
            }

            WarpList::Warp warp;
            warp.room1 = roomnum;
            warp.type = static_cast<WarpList::Warp::Type>(ReadInt(warp_node, "type", 0, 3, context));
            const auto size = RequireMap(warp_node, "size", context);
            warp.x_size = static_cast<uint8_t>(ReadInt(size, "width", 1, 3, context + ".size"));
            warp.y_size = static_cast<uint8_t>(ReadInt(size, "height", 1, 3, context + ".size"));
            if ((warp.x_size == 3 && warp.y_size == 2) ||
                (warp.x_size == 2 && warp.y_size == 3))
            {
                throw std::runtime_error(context + ".size cannot encode a 2-by-3 or 3-by-2 warp.");
            }

            const auto source = RequireMap(warp_node, "source", context);
            warp.x1 = static_cast<uint8_t>(ReadInt(source, "x", 0, 0xFF, context + ".source"));
            warp.y1 = static_cast<uint8_t>(ReadInt(source, "y", 0, 0xFF, context + ".source"));
            const auto destination = RequireMap(warp_node, "destination", context);
            warp.x2 = static_cast<uint8_t>(ReadInt(destination, "x", 0, 0xFF, context + ".destination"));
            warp.y2 = static_cast<uint8_t>(ReadInt(destination, "y", 0, 0xFF, context + ".destination"));

            const auto target_name = ReadString(warp_node, "target", context);
            if (target_name == room_name)
            {
                warp.room2 = roomnum;
            }
            else
            {
                const auto& rooms = room_data->GetRoomlist();
                const auto target = std::find_if(rooms.cbegin(), rooms.cend(), [&](const auto& candidate)
                {
                    return candidate->name == target_name;
                });
                if (target == rooms.cend())
                {
                    throw std::runtime_error(context + ".target references unknown room '" + target_name + "'.");
                }
                warp.room2 = (*target)->index;
            }
            if (warp.room1 >= 0x400 || warp.room2 >= 0x400)
            {
                throw std::runtime_error(context + " references a room outside the encodable range.");
            }
            warps->push_back(warp);
        }
        if (WarpList::HasDuplicateWarps(*warps))
        {
            throw std::runtime_error("'warps' contains the same warp more than once.");
        }
    }

    std::optional<std::vector<uint16_t>> characters;
    if (node["characters"].IsDefined())
    {
        const auto character_nodes = node["characters"];
        if (!character_nodes.IsSequence())
        {
            throw std::runtime_error("'characters' must be a sequence.");
        }
        if (character_nodes.size() > 64)
        {
            throw std::runtime_error("A room cannot contain more than 64 character mappings.");
        }

        characters.emplace();
        characters->reserve(character_nodes.size());
        for (std::size_t i = 0; i < character_nodes.size(); ++i)
        {
            const auto context = "characters[" + std::to_string(i) + "]";
            characters->push_back(static_cast<uint16_t>(ReadInt(character_nodes[i], 0, 0x7FF, context)));
        }
    }

    std::optional<bool> no_chests_flag;
    if (node["no_chests_flag"].IsDefined())
    {
        no_chests_flag = ReadBool(node, "no_chests_flag", "room");
    }

    std::optional<std::vector<uint8_t>> chests;
    if (node["chests"].IsDefined())
    {
        const auto chest_nodes = node["chests"];
        if (!chest_nodes.IsSequence())
        {
            throw std::runtime_error("'chests' must be a sequence.");
        }
        chests.emplace();
        chests->reserve(chest_nodes.size());
        for (std::size_t i = 0; i < chest_nodes.size(); ++i)
        {
            const auto context = "chests[" + std::to_string(i) + "]";
            chests->push_back(static_cast<uint8_t>(ReadInt(chest_nodes[i], 0, 0xFF, context)));
        }
    }

    const bool effective_no_chests = no_chests_flag.value_or(room_data->GetNoChestFlagForRoom(roomnum));
    if (chests && effective_no_chests && !chests->empty())
    {
        throw std::runtime_error("'chests' must be empty when 'no_chests_flag' is true.");
    }

    std::optional<std::vector<Door>> doors;
    if (node["doors"].IsDefined())
    {
        const auto door_nodes = node["doors"];
        if (!door_nodes.IsSequence())
        {
            throw std::runtime_error("'doors' must be a sequence.");
        }
        doors.emplace();
        doors->reserve(door_nodes.size());
        for (std::size_t i = 0; i < door_nodes.size(); ++i)
        {
            const auto door_node = door_nodes[i];
            const auto context = "doors[" + std::to_string(i) + "]";
            if (!door_node.IsMap())
            {
                throw std::runtime_error(context + " must be a map.");
            }
            Door door;
            door.x = static_cast<uint8_t>(ReadInt(door_node, "x", 0, 0x3F, context));
            door.y = static_cast<uint8_t>(ReadInt(door_node, "y", 0, 0x3F, context));
            door.size = static_cast<Door::Size>(ReadInt(door_node, "size", 0, 3, context));
            doors->push_back(door);
        }
    }

    std::optional<std::vector<TileSwap>> tileswaps;
    if (node["tileswaps"].IsDefined())
    {
        const auto tileswap_nodes = node["tileswaps"];
        if (!tileswap_nodes.IsSequence())
        {
            throw std::runtime_error("'tileswaps' must be a sequence.");
        }
        tileswaps.emplace();
        tileswaps->reserve(tileswap_nodes.size());
        for (std::size_t i = 0; i < tileswap_nodes.size(); ++i)
        {
            const auto tileswap_node = tileswap_nodes[i];
            const auto context = "tileswaps[" + std::to_string(i) + "]";
            if (!tileswap_node.IsMap())
            {
                throw std::runtime_error(context + " must be a map.");
            }

            TileSwap tileswap;
            tileswap.trigger = static_cast<uint8_t>(ReadInt(tileswap_node, "trigger", 0, 0x1F, context));
            tileswap.mode = static_cast<TileSwap::Mode>(ReadInt(tileswap_node, "mode", 0, 2, context));
            tileswap.active = tileswap_node["active"].IsDefined()
                ? ReadBool(tileswap_node, "active", context)
                : true;

            const auto read_copy_op = [&](const char* field)
            {
                const auto copy_node = RequireMap(tileswap_node, field, context);
                const auto copy_context = context + "." + field;
                TileSwap::CopyOp copy;
                copy.src_x = static_cast<uint8_t>(ReadInt(copy_node, "src_x", 0, 0x3F, copy_context));
                copy.src_y = static_cast<uint8_t>(ReadInt(copy_node, "src_y", 0, 0x3F, copy_context));
                copy.dst_x = static_cast<uint8_t>(ReadInt(copy_node, "dst_x", 0, 0x3F, copy_context));
                copy.dst_y = static_cast<uint8_t>(ReadInt(copy_node, "dst_y", 0, 0x3F, copy_context));
                copy.width = static_cast<uint8_t>(ReadInt(copy_node, "width", 1, 0x40, copy_context));
                copy.height = static_cast<uint8_t>(ReadInt(copy_node, "height", 1, 0x40, copy_context));
                return copy;
            };
            tileswap.map = read_copy_op("map");
            tileswap.heightmap = read_copy_op("heightmap");
            tileswaps->push_back(tileswap);
        }
    }

    std::optional<std::vector<EntityFlag>> entity_visibility_flags;
    std::optional<std::vector<OneTimeEventFlag>> one_time_event_flags;
    std::optional<std::vector<RoomClearFlag>> multiple_entity_hide_flags;
    std::optional<std::vector<RoomClearFlag>> locked_door_flags;
    std::optional<std::vector<SacredTreeFlag>> sacred_tree_flags;
    std::optional<std::vector<RoomClearFlag>> permanent_switch_flags;
    std::optional<std::vector<WarpList::Transition>> room_transition_flags;
    std::optional<std::vector<TileSwapFlag>> normal_tileswap_flags;
    std::optional<std::vector<TileSwapFlag>> locked_door_tileswap_flags;
    std::optional<std::vector<TreeWarpFlag>> tree_warp_flags;
    if (node["flags"].IsDefined())
    {
        const auto flags = node["flags"];
        if (!flags.IsMap())
        {
            throw std::runtime_error("'flags' must be a map.");
        }

        const auto read_entries = [&](const char* field)
        {
            const auto entries = flags[field];
            if (!entries.IsSequence())
            {
                throw std::runtime_error(std::string("'flags.") + field + "' must be a sequence.");
            }
            return entries;
        };
        const auto require_entry_map = [](const YAML::Node& entry, const std::string& context)
        {
            if (!entry.IsMap())
            {
                throw std::runtime_error(context + " must be a map.");
            }
        };
        if (flags["entity_visibility"].IsDefined())
        {
            const auto entries = read_entries("entity_visibility");
            entity_visibility_flags.emplace();
            entity_visibility_flags->reserve(entries.size());
            for (std::size_t i = 0; i < entries.size(); ++i)
            {
                const auto entry = entries[i];
                const auto context = "flags.entity_visibility[" + std::to_string(i) + "]";
                require_entry_map(entry, context);
                EntityFlag flag(roomnum);
                flag.flag = static_cast<uint16_t>(ReadInt(entry, "flag", 0, 0x3FF, context));
                flag.entity = static_cast<uint8_t>(ReadInt(entry, "entity", 0, 0x1F, context));
                flag.set = ReadBool(entry, "hide_on_flag_set", context);
                entity_visibility_flags->push_back(flag);
            }
        }

        if (flags["one_time_events"].IsDefined())
        {
            const auto entries = read_entries("one_time_events");
            one_time_event_flags.emplace();
            one_time_event_flags->reserve(entries.size());
            for (std::size_t i = 0; i < entries.size(); ++i)
            {
                const auto entry = entries[i];
                const auto context = "flags.one_time_events[" + std::to_string(i) + "]";
                require_entry_map(entry, context);
                OneTimeEventFlag flag(roomnum);
                flag.flag_on = static_cast<uint16_t>(ReadInt(entry, "on_flag", 0, 0x3FF, context));
                flag.flag_on_set = ReadBool(entry, "on_flag_set", context);
                flag.flag_off = static_cast<uint16_t>(ReadInt(entry, "off_flag", 0, 0x3FF, context));
                flag.flag_off_set = ReadBool(entry, "off_flag_set", context);
                flag.entity = static_cast<uint8_t>(ReadInt(entry, "entity", 0, 0x1F, context));
                one_time_event_flags->push_back(flag);
            }
        }

        const auto read_room_clear_flags = [&](const char* section, const char* entity_field,
            std::optional<std::vector<RoomClearFlag>>& output)
        {
            if (!flags[section].IsDefined())
            {
                return;
            }
            const auto entries = read_entries(section);
            output.emplace();
            output->reserve(entries.size());
            for (std::size_t i = 0; i < entries.size(); ++i)
            {
                const auto entry = entries[i];
                const auto context = std::string("flags.") + section + "[" + std::to_string(i) + "]";
                require_entry_map(entry, context);
                RoomClearFlag flag(roomnum);
                flag.flag = static_cast<uint16_t>(ReadInt(entry, "flag", 0, 0x7FF, context));
                flag.entity = static_cast<uint8_t>(ReadInt(entry, entity_field, 0, 0x1F, context));
                output->push_back(flag);
            }
        };
        read_room_clear_flags("multiple_entity_hides", "min_entity", multiple_entity_hide_flags);
        read_room_clear_flags("locked_doors", "door_entity", locked_door_flags);
        read_room_clear_flags("permanent_switches", "switch_entity", permanent_switch_flags);

        if (flags["sacred_trees"].IsDefined())
        {
            const auto entries = read_entries("sacred_trees");
            sacred_tree_flags.emplace();
            sacred_tree_flags->reserve(entries.size());
            for (std::size_t i = 0; i < entries.size(); ++i)
            {
                const auto entry = entries[i];
                const auto context = "flags.sacred_trees[" + std::to_string(i) + "]";
                require_entry_map(entry, context);
                SacredTreeFlag flag(roomnum);
                flag.flag = static_cast<uint16_t>(ReadInt(entry, "flag", 0, 0x7FF, context));
                sacred_tree_flags->push_back(flag);
            }
        }

        if (flags["room_transitions"].IsDefined())
        {
            const auto entries = read_entries("room_transitions");
            room_transition_flags.emplace();
            room_transition_flags->reserve(entries.size());
            for (std::size_t i = 0; i < entries.size(); ++i)
            {
                const auto entry = entries[i];
                const auto context = "flags.room_transitions[" + std::to_string(i) + "]";
                require_entry_map(entry, context);
                const auto flag = static_cast<uint16_t>(ReadInt(entry, "flag", 0, 0x7FF, context));
                const bool on_set = ReadBool(entry, "on_flag_set", context);
                const auto destination_name = ReadString(entry, "destination", context);
                const auto destination = resolve_room(destination_name, context + ".destination");
                room_transition_flags->emplace_back(
                    on_set ? roomnum : destination,
                    on_set ? destination : roomnum,
                    flag);
            }
        }

        const auto read_tileswap_flags = [&](const char* section,
            std::optional<std::vector<TileSwapFlag>>& output)
        {
            if (!flags[section].IsDefined())
            {
                return;
            }
            const auto entries = read_entries(section);
            output.emplace();
            output->reserve(entries.size());
            for (std::size_t i = 0; i < entries.size(); ++i)
            {
                const auto entry = entries[i];
                const auto context = std::string("flags.") + section + "[" + std::to_string(i) + "]";
                require_entry_map(entry, context);
                const auto index = static_cast<uint8_t>(ReadInt(entry, "tileswap_index", 1, 32, context) - 1);
                TileSwapFlag flag(roomnum, index);
                flag.flag = static_cast<uint16_t>(ReadInt(entry, "flag", 0, 0x7FF, context));
                flag.always = ReadBool(entry, "always", context);
                output->push_back(flag);
            }
        };
        read_tileswap_flags("tileswaps", normal_tileswap_flags);
        read_tileswap_flags("locked_door_tileswaps", locked_door_tileswap_flags);

        if (flags["tree_warps"].IsDefined())
        {
            const auto entries = read_entries("tree_warps");
            if (entries.size() > 1)
            {
                throw std::runtime_error("'flags.tree_warps' cannot contain more than one entry.");
            }
            tree_warp_flags.emplace();
            if (entries.size() != 0)
            {
                const auto entry = entries[0];
                const std::string context = "flags.tree_warps[0]";
                require_entry_map(entry, context);
                const auto flag = static_cast<uint16_t>(ReadInt(entry, "flag", 0, 0x7FE, context));
                if ((flag & 1) != 0)
                {
                    throw std::runtime_error(context + ".flag must be even.");
                }
                const auto destination_name = ReadString(entry, "destination", context);
                const auto destination = resolve_room(destination_name, context + ".destination");
                tree_warp_flags->emplace_back(roomnum, destination, flag);
            }
        }
    }

    if (has_display_name && !Labels::Update(Labels::C_ROOMS, roomnum, display_name))
    {
        throw std::runtime_error("Unable to update the room display name.");
    }
    if (target_by_id && !room_data->RenameRoom(roomnum, room_name))
    {
        throw std::runtime_error("Unable to update the room name.");
    }
    room->map = map;
    room->tileset = tileset;
    room->pri_blockset = pri_blockset;
    room->sec_blockset = sec_blockset;
    room->room_palette = room_palette;
    room->bgm = bgm;
    room->room_z_begin = room_z_begin;
    room->room_z_end = room_z_end;
    room->unknown_param1 = unknown_param1;
    room->unknown_param2 = unknown_param2;
    if (fall_destination)
    {
        room_data->SetHasFallDestination(roomnum, fall_destination->enabled);
        if (fall_destination->enabled)
        {
            room_data->SetFallDestination(roomnum, fall_destination->room);
        }
    }
    if (climb_destination)
    {
        room_data->SetHasClimbDestination(roomnum, climb_destination->enabled);
        if (climb_destination->enabled)
        {
            room_data->SetClimbDestination(roomnum, climb_destination->room);
        }
    }
    const auto string_data = game_data->GetStringData();
    if (room_visit_flag)
    {
        string_data->SetRoomVisitFlag(roomnum, *room_visit_flag);
    }
    if (save_location)
    {
        string_data->SetSaveLocation(roomnum, *save_location);
    }
    if (map_location)
    {
        string_data->SetMapLocation(roomnum, map_location->first, map_location->second);
    }
    if (is_shop)
    {
        room_data->SetShop(roomnum, *is_shop);
    }
    if (has_warp_tree)
    {
        room_data->SetTree(roomnum, *has_warp_tree);
    }
    if (lantern_flag)
    {
        room_data->SetLanternFlag(roomnum, *lantern_flag);
    }
    if (lifestock_flag)
    {
        room_data->SetLifestockSaleFlag(roomnum, *lifestock_flag);
    }
    if (entities)
    {
        game_data->GetSpriteData()->SetRoomEntities(roomnum, *entities);
        if (!chests)
        {
            room_data->SetChestsForRoom(roomnum, *entity_chests);
        }
    }
    if (warps)
    {
        room_data->SetWarpsForRoom(roomnum, *warps);
    }
    if (characters)
    {
        game_data->GetStringData()->SetRoomCharacters(roomnum, *characters);
    }
    if (chests)
    {
        room_data->SetChestsForRoom(roomnum, *chests);
    }
    if (no_chests_flag)
    {
        room_data->SetNoChestFlagForRoom(roomnum, *no_chests_flag);
    }
    if (doors)
    {
        room_data->SetDoors(roomnum, *doors);
    }
    if (tileswaps)
    {
        room_data->SetTileSwaps(roomnum, *tileswaps);
    }
    const auto sprite_data = game_data->GetSpriteData();
    if (entity_visibility_flags)
    {
        sprite_data->SetEntityVisibilityFlagsForRoom(roomnum, *entity_visibility_flags);
    }
    if (one_time_event_flags)
    {
        sprite_data->SetOneTimeEventFlagsForRoom(roomnum, *one_time_event_flags);
    }
    if (multiple_entity_hide_flags)
    {
        sprite_data->SetMultipleEntityHideFlagsForRoom(roomnum, *multiple_entity_hide_flags);
    }
    if (locked_door_flags)
    {
        sprite_data->SetLockedDoorFlagsForRoom(roomnum, *locked_door_flags);
    }
    if (sacred_tree_flags)
    {
        sprite_data->SetSacredTreeFlagsForRoom(roomnum, *sacred_tree_flags);
    }
    if (permanent_switch_flags)
    {
        sprite_data->SetPermanentSwitchFlagsForRoom(roomnum, *permanent_switch_flags);
    }
    if (room_transition_flags)
    {
        room_data->SetTransitions(roomnum, *room_transition_flags);
    }
    if (normal_tileswap_flags)
    {
        room_data->SetNormalTileSwaps(roomnum, *normal_tileswap_flags);
    }
    if (locked_door_tileswap_flags)
    {
        room_data->SetLockedDoorTileSwaps(roomnum, *locked_door_tileswap_flags);
    }
    if (tree_warp_flags)
    {
        if (tree_warp_flags->empty())
        {
            room_data->ClearTreeWarp(roomnum);
        }
        else
        {
            room_data->SetTreeWarp(tree_warp_flags->front());
        }
    }
    return true;
}

void RoomToYaml::EmitYaml(YAML::Emitter& out, const Room& room,
    const std::shared_ptr<GameData>& gd)
{
    const auto& map = room.map;
    const auto& name = room.name;
    const auto index = room.index;
    const auto tileset = room.tileset;
    const auto pri_blockset = room.pri_blockset;
    const auto sec_blockset = room.sec_blockset;
    const auto room_palette = room.room_palette;
    const auto bgm = room.bgm;
    const auto room_z_begin = room.room_z_begin;
    const auto room_z_end = room.room_z_end;
    const auto unknown_param1 = room.unknown_param1;
    const auto unknown_param2 = room.unknown_param2;

    auto get_room_name = [&](int idx) {
        if (idx >= 0 && idx < static_cast<int>(gd->GetRoomData()->GetRoomCount()))
        {
            auto room_entry = gd->GetRoomData()->GetRoom(static_cast<uint16_t>(idx));
            return room_entry->name;
        }
        return std::string("none");
    };

    auto get_room_display_name = [&](int idx) {
        if (idx >= 0 && idx < static_cast<int>(gd->GetRoomData()->GetRoomCount()))
        {
            auto room_entry = gd->GetRoomData()->GetRoom(static_cast<uint16_t>(idx));
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

    out << YAML::BeginMap << YAML::Key << name << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "index" << YAML::Value << index;
    out << YAML::Key << "name" << YAML::Value << name;
    out << YAML::Key << "display_name" << YAML::Value << Landstalker::wstr_to_utf8(room.GetDisplayName());
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
    fall_dest = fall_dest >= static_cast<int>(gd->GetRoomData()->GetRoomCount()) ? -1 : fall_dest;
    int climb_dest = gd->GetRoomData()->GetClimbDestination(index);
    climb_dest = climb_dest >= static_cast<int>(gd->GetRoomData()->GetRoomCount()) ? -1 : climb_dest;
    out << YAML::Key << "fall_destination" << YAML::Value << get_room_name(fall_dest) << YAML::Comment(get_room_display_name(fall_dest));
    out << YAML::Key << "climb_destination" << YAML::Value << get_room_name(climb_dest) << YAML::Comment(get_room_display_name(climb_dest));
    int room_visit_flag = gd->GetStringData()->GetRoomVisitFlag(index);
    room_visit_flag = room_visit_flag >= 0xFFFF ? -1 : room_visit_flag;
    out << YAML::Key << "room_visit_flag" << YAML::Value << room_visit_flag;
    int save_loc = gd->GetStringData()->GetSaveLocation(index);
    save_loc = save_loc >= 0xFF ? -1 : save_loc;
    int map_loc = gd->GetStringData()->GetMapLocation(index);
    map_loc = map_loc >= 0xFF ? -1 : map_loc;
    std::string save_loc_name = get_system_string(static_cast<uint8_t>(save_loc));
    std::string map_loc_name = get_system_string(static_cast<uint8_t>(map_loc));
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
    std::size_t chest_counter = 0;
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
        out << YAML::Key << "active" << YAML::Value << ts.active;
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
        uint16_t dest_room = flag.src_rm == index ? flag.dst_rm : flag.src_rm;
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
}

} // namespace Landstalker
