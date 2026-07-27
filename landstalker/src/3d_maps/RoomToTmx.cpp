#include <landstalker/3d_maps/RoomToTmx.h>
#include <landstalker/3d_maps/MapToTmx.h>
#include <landstalker/rooms/Room.h>
#include <landstalker/rooms/Entity.h>
#include <landstalker/rooms/WarpList.h>
#include <landstalker/misc/Labels.h>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <stdexcept>
#include <type_traits>
#include <pugixml.hpp>
#include <yaml-cpp/yaml.h>
#include <landstalker/misc/Utils.h>

template<class> constexpr bool ALWAYS_FALSE = false;

namespace Landstalker {

static std::string GetBlocksetData(const std::shared_ptr<BlocksetEntry>& blockset)
{
	std::ostringstream ss;
	for (auto it = blockset->GetData()->cbegin(); it != blockset->GetData()->cend(); ++it)
	{
		for(unsigned int i = 0; i < MapBlock::GetBlockSize(); ++i)
		{
			const auto& tile = it->GetTile(i);
			ss << std::setw(8) << StrPrintf("%s%s%s%04X",
				tile.Attributes().getAttribute(Landstalker::TileAttributes::Attribute::ATTR_PRIORITY) ? "P" : "",
				tile.Attributes().getAttribute(Landstalker::TileAttributes::Attribute::ATTR_VFLIP) ? "V" : "",
				tile.Attributes().getAttribute(Landstalker::TileAttributes::Attribute::ATTR_HFLIP) ? "H" : "",
			 	tile.GetIndex());
			if(i < MapBlock::GetBlockSize() - 1 && it != std::prev(blockset->GetData()->cend()))
			{
				ss << ",";
			}
		}
		ss << "\n";
	}
	return ss.str();
}

namespace {

std::optional<std::string> GetProperty(const pugi::xml_node& properties, const char* name)
{
	for (const auto property : properties.children("property"))
	{
		if (std::string(property.attribute("name").as_string()) != name)
		{
			continue;
		}
		if (const auto value = property.attribute("value"))
		{
			return value.as_string();
		}
		return property.child_value();
	}
	return std::nullopt;
}

void CopyProperty(YAML::Node& output, const char* output_name,
	const pugi::xml_node& properties, const char* property_name)
{
	if (const auto value = GetProperty(properties, property_name))
	{
		output[output_name] = *value;
	}
}

std::string RoomReference(const pugi::xml_node& properties, const char* label_property,
	const char* index_property, const std::shared_ptr<GameData>& game_data)
{
	if (const auto label = GetProperty(properties, label_property))
	{
		return *label;
	}
	const auto index_text = GetProperty(properties, index_property);
	if (!index_text)
	{
		return {};
	}
	std::size_t used = 0;
	int index = 0;
	try
	{
		index = std::stoi(*index_text, &used, 0);
	}
	catch (const std::exception&)
	{
		throw std::runtime_error(std::string("Invalid TMX property '") + index_property + "'.");
	}
	if (used != index_text->size())
	{
		throw std::runtime_error(std::string("Invalid TMX property '") + index_property + "'.");
	}
	const auto room_data = game_data->GetRoomData();
	return index >= 0 && index < static_cast<int>(room_data->GetRoomCount())
		? room_data->GetRoom(static_cast<uint16_t>(index))->name
		: "none";
}

pugi::xml_node FindObjectGroup(const pugi::xml_node& map, const char* name)
{
	for (const auto group : map.children("objectgroup"))
	{
		if (std::string(group.attribute("name").as_string()) == name)
		{
			return group;
		}
	}
	return {};
}

YAML::Node MakeSequence()
{
	return YAML::Node(YAML::NodeType::Sequence);
}

} // namespace

bool RoomToTmx::ImportFromTmx(const std::string& filename, const RoomToYaml::RoomKey& key,
	const std::shared_ptr<GameData>& game_data)
{
	if (!game_data)
	{
		return false;
	}

	pugi::xml_document document;
	const auto result = document.load_file(filename.c_str());
	if (!result)
	{
		throw std::runtime_error(std::string("Unable to read TMX XML: ") + result.description());
	}
	const auto map = document.child("map");
	if (!map)
	{
		throw std::runtime_error("TMX file does not contain a map element.");
	}

	YAML::Node room;
	bool has_metadata = false;
	const auto properties = map.child("properties");
	const auto copy = [&](const char* yaml_name, const char* tmx_name)
	{
		if (GetProperty(properties, tmx_name))
		{
			CopyProperty(room, yaml_name, properties, tmx_name);
			has_metadata = true;
		}
	};
	copy("index", "RoomNumber");
	copy("name", "RoomLabel");
	copy("display_name", "RoomName");
	copy("map", "RoomMap");
	copy("tileset", "RoomTilesetName");
	copy("pri_blockset", "RoomPrimaryBlocksetName");
	copy("sec_blockset", "RoomSecondaryBlocksetName");
	copy("room_palette", "RoomPaletteName");
	copy("bgm", "RoomBGM");
	copy("room_z_begin", "RoomZBegin");
	copy("room_z_end", "RoomZEnd");
	copy("unknown_param1", "RoomUnknownParam1");
	copy("unknown_param2", "RoomUnknownParam2");
	copy("room_visit_flag", "FlagRoomVisit");
	copy("save_location_string_index", "MiscSaveLocationStringIndex");
	copy("map_location_string_index", "MiscMapLocationStringIndex");
	copy("map_position", "MiscMapPosition");
	copy("is_shop", "FlagIsShopChurchInn");
	copy("has_warp_tree", "WarpHasTree");
	copy("no_chests_flag", "MiscNoChestsFlag");
	copy("lantern_flag", "FlagLantern");
	copy("lifestock_flag", "MiscLifestockSaleFlag");

	if (GetProperty(properties, "WarpFallDestinationLabel") ||
		GetProperty(properties, "WarpFallDestination"))
	{
		room["fall_destination"] = RoomReference(properties, "WarpFallDestinationLabel",
			"WarpFallDestination", game_data);
		has_metadata = true;
	}
	if (GetProperty(properties, "WarpClimbDestinationLabel") ||
		GetProperty(properties, "WarpClimbDestination"))
	{
		room["climb_destination"] = RoomReference(properties, "WarpClimbDestinationLabel",
			"WarpClimbDestination", game_data);
		has_metadata = true;
	}

	if (const auto group = FindObjectGroup(map, "Entities"))
	{
		has_metadata = true;
		auto entries = MakeSequence();
		for (const auto object : group.children("object"))
		{
			const auto props = object.child("properties");
			YAML::Node entry;
			CopyProperty(entry, "type", props, "Type");
			YAML::Node position;
			CopyProperty(position, "x", props, "X");
			CopyProperty(position, "y", props, "Y");
			CopyProperty(position, "z", props, "Z");
			entry["position"] = position;
			CopyProperty(entry, "orientation", props, "Orientation");
			CopyProperty(entry, "palette", props, "Palette");
			CopyProperty(entry, "speed", props, "Speed");
			CopyProperty(entry, "behaviour", props, "Behaviour");
			CopyProperty(entry, "dialogue", props, "Dialogue");
			CopyProperty(entry, "copy_source", props, "TileSource");
			CopyProperty(entry, "chest_contents", props, "ChestContents");
			YAML::Node flags;
			CopyProperty(flags, "hostile", props, "Hostile");
			CopyProperty(flags, "no_rotate", props, "NoRotate");
			CopyProperty(flags, "no_pickup", props, "NoPickup");
			CopyProperty(flags, "has_dialogue", props, "HasDialogue");
			CopyProperty(flags, "invisible", props, "Visible");
			CopyProperty(flags, "not_solid", props, "Solid");
			CopyProperty(flags, "no_gravity", props, "Gravity");
			CopyProperty(flags, "no_friction", props, "Friction");
			CopyProperty(flags, "reserved", props, "Reserved");
			CopyProperty(flags, "tile_copy", props, "TileCopy");
			entry["flags"] = flags;
			entries.push_back(entry);
		}
		room["entities"] = entries;
	}

	if (const auto group = FindObjectGroup(map, "Warps"))
	{
		has_metadata = true;
		auto entries = MakeSequence();
		for (const auto object : group.children("object"))
		{
			const auto props = object.child("properties");
			YAML::Node entry;
			const auto type = GetProperty(props, "warpType").value_or("UNKNOWN");
			entry["type"] = type == "NORMAL" ? 0 : type == "STAIR_SE" ? 1 : type == "STAIR_SW" ? 2 : 3;
			YAML::Node size;
			size["width"] = object.attribute("width").as_string();
			size["height"] = object.attribute("height").as_string();
			entry["size"] = size;
			const auto target = RoomReference(props, "destinationRoomLabel", "destinationRoom", game_data);
			if (!target.empty()) entry["target"] = target;
			YAML::Node source;
			source["x"] = object.attribute("x").as_string();
			source["y"] = object.attribute("y").as_string();
			const auto file_room = GetProperty(properties, "RoomNumber");
			if (file_room && GetProperty(props, "room1") != file_room)
			{
				CopyProperty(source, "x", props, "x2");
				CopyProperty(source, "y", props, "y2");
			}
			entry["source"] = source;
			YAML::Node destination;
			CopyProperty(destination, "x", props, "destinationX");
			CopyProperty(destination, "y", props, "destinationY");
			entry["destination"] = destination;
			entries.push_back(entry);
		}
		room["warps"] = entries;
	}

	if (const auto group = FindObjectGroup(map, "TileSwaps"))
	{
		has_metadata = true;
		auto entries = MakeSequence();
		for (const auto object : group.children("object"))
		{
			const auto props = object.child("properties");
			YAML::Node entry;
			CopyProperty(entry, "trigger", props, "Trigger");
			CopyProperty(entry, "active", props, "Active");
			CopyProperty(entry, "mode", props, "Mode");
			const auto copy_op = [&](const char* prefix)
			{
				YAML::Node operation;
				CopyProperty(operation, "src_x", props, (std::string(prefix) + "SourceX").c_str());
				CopyProperty(operation, "src_y", props, (std::string(prefix) + "SourceY").c_str());
				CopyProperty(operation, "dst_x", props, (std::string(prefix) + "DestinationX").c_str());
				CopyProperty(operation, "dst_y", props, (std::string(prefix) + "DestinationY").c_str());
				CopyProperty(operation, "width", props, (std::string(prefix) + "Width").c_str());
				CopyProperty(operation, "height", props, (std::string(prefix) + "Height").c_str());
				return operation;
			};
			entry["map"] = copy_op("Map");
			entry["heightmap"] = copy_op("Heightmap");
			entries.push_back(entry);
		}
		room["tileswaps"] = entries;
	}

	if (const auto group = FindObjectGroup(map, "Doors"))
	{
		has_metadata = true;
		auto entries = MakeSequence();
		for (const auto object : group.children("object"))
		{
			const auto props = object.child("properties");
			YAML::Node entry;
			CopyProperty(entry, "x", props, "X");
			CopyProperty(entry, "y", props, "Y");
			CopyProperty(entry, "size", props, "Size");
			entries.push_back(entry);
		}
		room["doors"] = entries;
	}

	if (const auto group = FindObjectGroup(map, "Chests"))
	{
		has_metadata = true;
		auto entries = MakeSequence();
		for (const auto object : group.children("object"))
		{
			if (const auto value = GetProperty(object.child("properties"), "Contents")) entries.push_back(*value);
		}
		room["chests"] = entries;
	}

	if (const auto group = FindObjectGroup(map, "Characters"))
	{
		has_metadata = true;
		auto entries = MakeSequence();
		for (const auto object : group.children("object"))
		{
			if (const auto value = GetProperty(object.child("properties"), "Index")) entries.push_back(*value);
		}
		room["characters"] = entries;
	}

	if (const auto group = FindObjectGroup(map, "Flags"))
	{
		has_metadata = true;
		YAML::Node flags;
		const char* sections[] = { "entity_visibility", "one_time_events", "multiple_entity_hides",
			"locked_doors", "sacred_trees", "permanent_switches", "room_transitions",
			"tileswaps", "locked_door_tileswaps", "tree_warps" };
		for (const auto section : sections) flags[section] = MakeSequence();
		for (const auto object : group.children("object"))
		{
			const auto props = object.child("properties");
			std::string type = object.attribute("class").as_string();
			if (type.empty()) type = object.attribute("type").as_string();
			YAML::Node entry;
			const auto fields = [&](std::initializer_list<std::pair<const char*, const char*>> values)
			{
				for (const auto& value : values) CopyProperty(entry, value.first, props, value.second);
			};
			if (type == "EntityVisibilityFlag")
			{
				fields({ {"flag", "Flag"}, {"entity", "Entity"}, {"hide_on_flag_set", "HideOnFlagSet"} });
				flags["entity_visibility"].push_back(entry);
			}
			else if (type == "OneTimeEventFlag")
			{
				fields({ {"on_flag", "OnFlag"}, {"on_flag_set", "OnFlagSet"}, {"off_flag", "OffFlag"},
					{"off_flag_set", "OffFlagSet"}, {"entity", "Entity"} });
				flags["one_time_events"].push_back(entry);
			}
			else if (type == "MultipleEntityHideFlag")
			{
				fields({ {"flag", "Flag"}, {"min_entity", "MinEntity"} });
				flags["multiple_entity_hides"].push_back(entry);
			}
			else if (type == "LockedDoorFlag")
			{
				fields({ {"flag", "Flag"}, {"door_entity", "DoorEntity"} });
				flags["locked_doors"].push_back(entry);
			}
			else if (type == "SacredTreeFlag")
			{
				fields({ {"flag", "Flag"} });
				flags["sacred_trees"].push_back(entry);
			}
			else if (type == "PermanentSwitchFlag")
			{
				fields({ {"flag", "Flag"}, {"switch_entity", "SwitchEntity"} });
				flags["permanent_switches"].push_back(entry);
			}
			else if (type == "RoomTransitionFlag")
			{
				fields({ {"flag", "Flag"}, {"on_flag_set", "OnFlagSet"} });
				entry["destination"] = RoomReference(props, "DestinationRoomLabel", "DestinationRoom", game_data);
				flags["room_transitions"].push_back(entry);
			}
			else if (type == "TileSwapFlag" || type == "LockedDoorTileSwapFlag")
			{
				fields({ {"flag", "Flag"}, {"always", "Always"}, {"tileswap_index", "TileSwapIndex"} });
				flags[type == "TileSwapFlag" ? "tileswaps" : "locked_door_tileswaps"].push_back(entry);
			}
			else if (type == "TreeWarpFlag")
			{
				fields({ {"flag", "Flag"} });
				entry["destination"] = RoomReference(props, "DestinationRoomLabel", "DestinationRoom", game_data);
				flags["tree_warps"].push_back(entry);
			}
		}
		room["flags"] = flags;
	}

	if (!has_metadata)
	{
		return true;
	}
	YAML::Emitter yaml;
	yaml << room;
	if (!yaml.good())
	{
		throw std::runtime_error("Unable to convert TMX room metadata.");
	}
	return RoomToYaml::ImportFromYamlText(yaml.c_str(), key, game_data);
}

bool RoomToTmx::ExportToTmx(const std::string& fname, int roomnum, std::shared_ptr<GameData> gameData, const std::string& blockset_filename)
{
	std::shared_ptr<RoomData> roomData = gameData->GetRoomData();
	pugi::xml_document tmx = MapToTmx::GenerateXmlDocument(fname, *(roomData->GetMapForRoom(static_cast<uint16_t>(roomnum))->GetData()), blockset_filename);

	auto add_property = [&](pugi::xml_node& parent, const std::string& name, auto value)
	{
		auto property = parent.append_child("property");
		property.append_attribute("name") = name.c_str();
		if constexpr (std::is_same_v<decltype(value), std::string>)
		{
			property.append_attribute("type") = "string";
			property.append_attribute("value") = value.c_str();
		}
		else if constexpr (std::is_same_v<decltype(value), std::wstring>)
		{
			property.append_attribute("type") = "string";
			property.append_attribute("value") = wstr_to_utf8(value).c_str();
		}
		else if constexpr (std::is_same_v<decltype(value), const char*>)
		{
			property.append_attribute("type") = "string";
			property.append_attribute("value") = value;
		}
		else if constexpr (std::is_same_v<decltype(value), bool>)
		{
			property.append_attribute("type") = "bool";
			property.append_attribute("value") = value ? "true" : "false";
		}
		else if constexpr (std::is_integral_v<decltype(value)>)
		{
			property.append_attribute("type") = "int";
			property.append_attribute("value") = value;
		}
		else if constexpr (std::is_floating_point_v<decltype(value)>)
		{
			property.append_attribute("type") = "float";
			property.append_attribute("value") = value;
		}
		else if constexpr (std::is_same_v<decltype(value), Landstalker::Palette::Colour>)
		{
			property.append_attribute("type") = "color";
			property.append_attribute("value") = StrPrintf("#%08X", value.GetRGB(true)).c_str();
		}
		else
		{
			static_assert(ALWAYS_FALSE<decltype(value)>, "Unsupported property value type");
		}
	};

	// Properties
	auto room = roomData->GetRoom(static_cast<uint16_t>(roomnum));
	auto map_node = tmx.child("map");
	auto properties = map_node.child("properties");
	if (!properties)
	{
		properties = map_node.prepend_child("properties");
	}
	const auto room_index = static_cast<uint16_t>(roomnum);
	const auto stringData = gameData->GetStringData();
	const auto spriteData = gameData->GetSpriteData();

	auto room_name = [&](uint16_t index)
	{
		return index < roomData->GetRoomCount() ? roomData->GetRoom(index)->name : std::string("none");
	};
	auto room_display_name = [&](uint16_t index)
	{
		return index < roomData->GetRoomCount() ? roomData->GetRoom(index)->GetDisplayName() : std::wstring(L"<NONE>");
	};
	auto system_string = [&](int index)
	{
		if (index <= 0 || index >= 0xFF)
		{
			return std::wstring(L"<NONE>");
		}
		return index >= 0x40 ? stringData->GetMenuStr(static_cast<std::size_t>(index - 0x40))
		                     : stringData->GetItemName(static_cast<std::size_t>(index));
	};

	auto tileset_properties = tmx.child("map").child("tileset").append_child("properties");
	add_property(tileset_properties, "Palette", roomData->GetRoomPaletteDisplayName(room->room_palette));
	for(unsigned int i = 0; i < static_cast<unsigned int>(roomData->GetPaletteForRoom(static_cast<uint16_t>(roomnum))->GetData()->GetSize()); i++)
	{
		std::string palette_name = "PaletteColour" + std::to_string(i);
		add_property(tileset_properties, palette_name, roomData->GetPaletteForRoom(static_cast<uint16_t>(roomnum))->GetData()->GetColour(static_cast<uint8_t>(i)));
	}
	auto blocksets = roomData->GetBlocksetsForRoom(static_cast<uint16_t>(roomnum));
	if(blocksets.size() > 0)
	{
		add_property(tileset_properties, "PrimaryBlocksetName", blocksets.front()->GetName());
		add_property(tileset_properties, "PrimaryBlocksetData", GetBlocksetData(blocksets.front()));
	}
	if(blocksets.size() > 1)
	{
		add_property(tileset_properties, "SecondaryBlocksetName", (*std::next(blocksets.begin()))->GetName());
		add_property(tileset_properties, "SecondaryBlocksetData", GetBlocksetData(*std::next(blocksets.begin())));
	}
	add_property(tileset_properties, "TilesetName", roomData->GetTilesetDisplayName(room->tileset));
	
	// Room Properties
	add_property(properties, "RoomName", room->GetDisplayName());
	add_property(properties, "RoomLabel", room->name);
	add_property(properties, "RoomNumber", roomnum);
	add_property(properties, "RoomTileset", room->tileset);
	add_property(properties, "RoomTilesetName", roomData->GetTileset(room->tileset)->GetName());
	add_property(properties, "RoomPalette", room->room_palette);
	add_property(properties, "RoomPaletteName", roomData->GetRoomPalette(room->room_palette)->GetName());
	add_property(properties, "RoomPrimaryBlockset", room->pri_blockset);
	add_property(properties, "RoomPrimaryBlocksetName", roomData->GetBlockset(room->tileset, room->pri_blockset, 0)->GetName());
	add_property(properties, "RoomSecondaryBlockset", room->sec_blockset);
	add_property(properties, "RoomSecondaryBlocksetName", roomData->GetBlockset(room->tileset, room->pri_blockset, room->sec_blockset + 1)->GetName());
	add_property(properties, "RoomBGM", room->bgm);
	add_property(properties, "RoomBGMName", Labels::Get(Labels::C_BGMS, room->bgm).value_or(L"(none)"));
	add_property(properties, "RoomMap", room->map);
	add_property(properties, "RoomUnknownParam1", room->unknown_param1);
	add_property(properties, "RoomUnknownParam2", room->unknown_param2);
	add_property(properties, "RoomZBegin", room->room_z_begin);
	add_property(properties, "RoomZEnd", room->room_z_end);
	// Warps Properties
	const auto fall_destination_index = roomData->GetFallDestination(room_index);
	const auto climb_destination_index = roomData->GetClimbDestination(room_index);
	const int fall_destination = fall_destination_index < roomData->GetRoomCount() ? fall_destination_index : -1;
	const int climb_destination = climb_destination_index < roomData->GetRoomCount() ? climb_destination_index : -1;
	add_property(properties, "WarpFallDestination", fall_destination);
	add_property(properties, "WarpFallDestinationLabel", room_name(fall_destination_index));
	add_property(properties, "WarpFallDestinationName", room_display_name(fall_destination_index));
	add_property(properties, "WarpClimbDestination", climb_destination);
	add_property(properties, "WarpClimbDestinationLabel", room_name(climb_destination_index));
	add_property(properties, "WarpClimbDestinationName", room_display_name(climb_destination_index));
	// Flags Properties
	const auto room_visit_flag = stringData->GetRoomVisitFlag(room_index);
	add_property(properties, "FlagRoomVisit", room_visit_flag == 0xFFFF ? -1 : static_cast<int>(room_visit_flag));
	add_property(properties, "FlagHasLantern", roomData->HasLanternFlag(room_index));
	add_property(properties, "FlagLantern", roomData->HasLanternFlag(room_index) ? static_cast<int>(roomData->GetLanternFlag(room_index)) : -1);
	// Misc Properties
	const auto save_location_index = stringData->GetSaveLocation(room_index);
	const auto map_location_index = stringData->GetMapLocation(room_index);
	const int save_location = save_location_index == 0xFF ? -1 : save_location_index;
	const int map_location = map_location_index == 0xFF ? -1 : map_location_index;
	add_property(properties, "FlagIsShopChurchInn", roomData->IsShop(room_index));
	add_property(properties, "WarpHasTree", roomData->IsTree(room_index));
	add_property(properties, "MiscSaveLocationStringIndex", save_location);
	add_property(properties, "MiscSaveLocationName", system_string(save_location));
	add_property(properties, "MiscMapLocationStringIndex", map_location);
	add_property(properties, "MiscMapLocationName", system_string(map_location));
	add_property(properties, "MiscMapPosition", map_location < 0 ? -1 : static_cast<int>(stringData->GetMapPosition(room_index)));
	add_property(properties, "MiscNoChestsFlag", roomData->GetNoChestFlagForRoom(room_index));
	add_property(properties, "MiscLifestockForSale", roomData->HasLifestockSaleFlag(room_index));
	add_property(properties, "MiscLifestockSaleFlag", roomData->HasLifestockSaleFlag(room_index) ? static_cast<int>(roomData->GetLifestockSaleFlag(room_index)) : -1);

	int next_layer_id = 3;
	int next_object_id = 1;
	auto add_object_group = [&](const char* name)
	{
		auto group = map_node.append_child("objectgroup");
		group.append_attribute("id") = next_layer_id++;
		group.append_attribute("name") = name;
		return group;
	};
	auto add_metadata_object = [&](pugi::xml_node& group, const char* name, const char* class_name)
	{
		auto object = group.append_child("object");
		object.append_attribute("id") = next_object_id++;
		object.append_attribute("name") = name;
		object.append_attribute("class") = class_name;
		return object;
	};

	// Warp objects
	auto warps_objectgroup = add_object_group("Warps");

	std::vector<WarpList::Warp> warps = roomData->GetWarpsForRoom(room_index);
	for (const auto& warp : warps) {
		const bool current_is_room1 = warp.room1 == room_index;
		auto warp_object = warps_objectgroup.append_child("object");
		warp_object.append_attribute("id") = next_object_id++;
		warp_object.append_attribute("visible") = 0;
		warp_object.append_attribute("name") = "Warp";
		warp_object.append_attribute("x") = current_is_room1 ? warp.x1 : warp.x2;
		warp_object.append_attribute("y") = current_is_room1 ? warp.y1 : warp.y2;
		warp_object.append_attribute("width") = warp.x_size;
		warp_object.append_attribute("height") = warp.y_size;
		// Warp properties
		auto warp_properties = warp_object.append_child("properties");
		add_property(warp_properties, "room1", warp.room1);
		add_property(warp_properties, "room2", warp.room2);
		add_property(warp_properties, "x2", warp.x2);
		add_property(warp_properties, "y2", warp.y2);
		const auto destination_room = current_is_room1 ? warp.room2 : warp.room1;
		add_property(warp_properties, "destinationRoom", destination_room);
		add_property(warp_properties, "destinationRoomLabel", room_name(destination_room));
		add_property(warp_properties, "destinationRoomName", room_display_name(destination_room));
		add_property(warp_properties, "destinationX", current_is_room1 ? warp.x2 : warp.x1);
		add_property(warp_properties, "destinationY", current_is_room1 ? warp.y2 : warp.y1);
		// Convert the warp type to a string
		std::string warp_type_str;
		switch (warp.type) {
			case WarpList::Warp::Type::NORMAL:   warp_type_str = "NORMAL";   break;
			case WarpList::Warp::Type::STAIR_SE: warp_type_str = "STAIR_SE"; break;
			case WarpList::Warp::Type::STAIR_SW: warp_type_str = "STAIR_SW"; break;
			default:                             warp_type_str = "UNKNOWN";  break;
		}
		add_property(warp_properties, "warpType", warp_type_str);
	}

	// Entity objects
	auto entities_objectgroup = add_object_group("Entities");

	std::vector<Entity> entities = spriteData->GetRoomEntities(room_index);
	const auto characters = stringData->GetRoomCharacters(room_index);
	const auto chests = roomData->GetChestsForRoom(room_index);
	std::size_t chest_index = 0;

	for (const auto& entity : entities) {
		auto entity_object = entities_objectgroup.append_child("object");
		entity_object.append_attribute("id") = next_object_id++;
		entity_object.append_attribute("visible") = 0;
		entity_object.append_attribute("name") = wstr_to_utf8(entity.GetTypeName()).c_str();
		entity_object.append_attribute("class") = wstr_to_utf8(entity.GetTypeName()).c_str();
		
		auto entity_properties = entity_object.append_child("properties");
		add_property(entity_properties, "Type", entity.GetType());
		add_property(entity_properties, "X", entity.GetXDbl());
		add_property(entity_properties, "Y", entity.GetYDbl());
		add_property(entity_properties, "Z", entity.GetZDbl());
		add_property(entity_properties, "Palette", entity.GetPalette());
		add_property(entity_properties, "Behaviour", entity.GetBehaviour());
		add_property(entity_properties, "BehaviourName", Labels::Get(Labels::C_BEHAVIOURS, entity.GetBehaviour()).value_or(L"(unknown)"));
		add_property(entity_properties, "Dialogue", entity.GetDialogue());
		const auto dialogue_index = static_cast<std::size_t>(entity.GetDialogue());
		if (dialogue_index < characters.size())
		{
			const auto character = characters[dialogue_index];
			add_property(entity_properties, "DialogueCharacter", character);
			add_property(entity_properties, "DialogueCharacterName", stringData->GetCharacterDisplayName(character));
		}
		add_property(entity_properties, "Hostile", entity.IsHostile());
		add_property(entity_properties, "NoRotate", entity.NoRotate());
		add_property(entity_properties, "NoPickup", entity.NoPickup());
		add_property(entity_properties, "HasDialogue", entity.HasDialogue());
		add_property(entity_properties, "Visible", entity.IsVisible());
		add_property(entity_properties, "Solid", entity.IsSolid());
		add_property(entity_properties, "Gravity", entity.HasGravity());
		add_property(entity_properties, "Friction", entity.HasFriction());
		add_property(entity_properties, "Speed", entity.GetSpeed());
		add_property(entity_properties, "Orientation", entity.GetOrientationName());
		add_property(entity_properties, "Reserved", entity.IsReservedSet());
		add_property(entity_properties, "TileCopy", entity.IsTileCopySet());
		add_property(entity_properties, "TileSource", entity.GetCopySource());
		if (entity.GetType() == 0x12 && chest_index < chests.size())
		{
			const auto contents = chests[chest_index++];
			add_property(entity_properties, "ChestContents", contents);
			add_property(entity_properties, "ChestContentsName", stringData->GetItemDisplayName(contents));
		}
	}

	// Tile swap objects
	auto tileswaps_objectgroup = add_object_group("TileSwaps");
	for (const auto& tileswap : roomData->GetTileSwaps(room_index))
	{
		auto object = add_metadata_object(tileswaps_objectgroup, "Tile Swap", "TileSwap");
		auto object_properties = object.append_child("properties");
		add_property(object_properties, "Trigger", tileswap.trigger);
		add_property(object_properties, "Active", tileswap.active);
		add_property(object_properties, "MapSourceX", tileswap.map.src_x);
		add_property(object_properties, "MapSourceY", tileswap.map.src_y);
		add_property(object_properties, "MapDestinationX", tileswap.map.dst_x);
		add_property(object_properties, "MapDestinationY", tileswap.map.dst_y);
		add_property(object_properties, "MapWidth", tileswap.map.width);
		add_property(object_properties, "MapHeight", tileswap.map.height);
		add_property(object_properties, "HeightmapSourceX", tileswap.heightmap.src_x);
		add_property(object_properties, "HeightmapSourceY", tileswap.heightmap.src_y);
		add_property(object_properties, "HeightmapDestinationX", tileswap.heightmap.dst_x);
		add_property(object_properties, "HeightmapDestinationY", tileswap.heightmap.dst_y);
		add_property(object_properties, "HeightmapWidth", tileswap.heightmap.width);
		add_property(object_properties, "HeightmapHeight", tileswap.heightmap.height);
		add_property(object_properties, "Mode", static_cast<int>(tileswap.mode));
		const char* mode_name = tileswap.mode == TileSwap::Mode::FLOOR ? "Floor"
			: tileswap.mode == TileSwap::Mode::WALL_NE ? "Wall NE" : "Wall NW";
		add_property(object_properties, "ModeName", mode_name);
	}

	// Door objects
	auto doors_objectgroup = add_object_group("Doors");
	for (const auto& door : roomData->GetDoors(room_index))
	{
		auto object = add_metadata_object(doors_objectgroup, "Door", "Door");
		object.append_attribute("x") = door.x;
		object.append_attribute("y") = door.y;
		auto object_properties = object.append_child("properties");
		add_property(object_properties, "X", door.x);
		add_property(object_properties, "Y", door.y);
		add_property(object_properties, "Size", static_cast<int>(door.size));
		add_property(object_properties, "SizeName", Door::SIZE_NAMES.at(door.size));
	}

	// Chest objects
	auto chests_objectgroup = add_object_group("Chests");
	for (const auto& chest : chests)
	{
		auto object = add_metadata_object(chests_objectgroup, "Chest", "Chest");
		auto object_properties = object.append_child("properties");
		add_property(object_properties, "Contents", chest);
		add_property(object_properties, "ContentsName", stringData->GetItemDisplayName(chest));
	}

	// Character objects
	auto characters_objectgroup = add_object_group("Characters");
	for (const auto& character : characters)
	{
		auto object = add_metadata_object(characters_objectgroup, "Character", "Character");
		auto object_properties = object.append_child("properties");
		add_property(object_properties, "Index", character);
		add_property(object_properties, "Name", stringData->GetCharacterDisplayName(character));
	}

	// Flag objects
	auto flags_objectgroup = add_object_group("Flags");
	for (const auto& flag : spriteData->GetEntityVisibilityFlagsForRoom(room_index))
	{
		auto object = add_metadata_object(flags_objectgroup, "Entity Visibility", "EntityVisibilityFlag");
		auto object_properties = object.append_child("properties");
		add_property(object_properties, "Flag", flag.flag);
		add_property(object_properties, "Entity", flag.entity);
		add_property(object_properties, "HideOnFlagSet", flag.set);
	}
	for (const auto& flag : spriteData->GetOneTimeEventFlagsForRoom(room_index))
	{
		auto object = add_metadata_object(flags_objectgroup, "One Time Event", "OneTimeEventFlag");
		auto object_properties = object.append_child("properties");
		add_property(object_properties, "OnFlag", flag.flag_on);
		add_property(object_properties, "OnFlagSet", flag.flag_on_set);
		add_property(object_properties, "OffFlag", flag.flag_off);
		add_property(object_properties, "OffFlagSet", flag.flag_off_set);
		add_property(object_properties, "Entity", flag.entity);
	}
	for (const auto& flag : spriteData->GetMultipleEntityHideFlagsForRoom(room_index))
	{
		auto object = add_metadata_object(flags_objectgroup, "Multiple Entity Hide", "MultipleEntityHideFlag");
		auto object_properties = object.append_child("properties");
		add_property(object_properties, "Flag", flag.flag);
		add_property(object_properties, "MinEntity", flag.entity);
	}
	for (const auto& flag : spriteData->GetLockedDoorFlagsForRoom(room_index))
	{
		auto object = add_metadata_object(flags_objectgroup, "Locked Door", "LockedDoorFlag");
		auto object_properties = object.append_child("properties");
		add_property(object_properties, "Flag", flag.flag);
		add_property(object_properties, "DoorEntity", flag.entity);
	}
	for (const auto& flag : spriteData->GetSacredTreeFlagsForRoom(room_index))
	{
		auto object = add_metadata_object(flags_objectgroup, "Sacred Tree", "SacredTreeFlag");
		auto object_properties = object.append_child("properties");
		add_property(object_properties, "Flag", flag.flag);
	}
	for (const auto& flag : spriteData->GetPermanentSwitchFlagsForRoom(room_index))
	{
		auto object = add_metadata_object(flags_objectgroup, "Permanent Switch", "PermanentSwitchFlag");
		auto object_properties = object.append_child("properties");
		add_property(object_properties, "Flag", flag.flag);
		add_property(object_properties, "SwitchEntity", flag.entity);
	}
	for (const auto& flag : roomData->GetTransitions(room_index))
	{
		auto object = add_metadata_object(flags_objectgroup, "Room Transition", "RoomTransitionFlag");
		auto object_properties = object.append_child("properties");
		const bool on_flag_set = flag.src_rm == room_index;
		const auto destination = on_flag_set ? flag.dst_rm : flag.src_rm;
		add_property(object_properties, "Flag", flag.flag);
		add_property(object_properties, "OnFlagSet", on_flag_set);
		add_property(object_properties, "DestinationRoom", destination);
		add_property(object_properties, "DestinationRoomLabel", room_name(destination));
		add_property(object_properties, "DestinationRoomName", room_display_name(destination));
	}
	for (const auto& flag : roomData->GetNormalTileSwaps(room_index))
	{
		auto object = add_metadata_object(flags_objectgroup, "Tile Swap", "TileSwapFlag");
		auto object_properties = object.append_child("properties");
		add_property(object_properties, "Flag", flag.flag);
		add_property(object_properties, "Always", flag.always);
		add_property(object_properties, "TileSwapIndex", static_cast<int>(flag.index + 1));
	}
	for (const auto& flag : roomData->GetLockedDoorTileSwaps(room_index))
	{
		auto object = add_metadata_object(flags_objectgroup, "Locked Door Tile Swap", "LockedDoorTileSwapFlag");
		auto object_properties = object.append_child("properties");
		add_property(object_properties, "Flag", flag.flag);
		add_property(object_properties, "Always", flag.always);
		add_property(object_properties, "TileSwapIndex", static_cast<int>(flag.index + 1));
	}
	if (roomData->HasTreeWarpFlag(room_index))
	{
		const auto flag = roomData->GetTreeWarp(room_index);
		const auto destination = flag.room1 == room_index ? flag.room2 : flag.room1;
		auto object = add_metadata_object(flags_objectgroup, "Tree Warp", "TreeWarpFlag");
		auto object_properties = object.append_child("properties");
		add_property(object_properties, "Flag", flag.flag);
		add_property(object_properties, "DestinationRoom", destination);
		add_property(object_properties, "DestinationRoomLabel", room_name(destination));
		add_property(object_properties, "DestinationRoomName", room_display_name(destination));
	}

	map_node.attribute("nextlayerid") = next_layer_id;
	map_node.attribute("nextobjectid") = next_object_id;

	return tmx.save_file(fname.c_str());
}

} // namespace Landstalker
