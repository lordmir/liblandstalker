#include <landstalker/3d_maps/RoomToTmx.h>
#include <landstalker/3d_maps/MapToTmx.h>
#include <landstalker/rooms/Room.h>
#include <landstalker/rooms/Entity.h>
#include <landstalker/rooms/WarpList.h>
#include <landstalker/rooms/RoomMetadata.h>
#include <landstalker/blockset/BlocksetCmp.h>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <pugixml.hpp>
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

bool RoomToTmx::ExportToTmx(const std::string& fname, int roomnum, std::shared_ptr<GameData> gameData, const std::string& blockset_filename)
{
	std::shared_ptr<RoomData> roomData = gameData->GetRoomData();
	pugi::xml_document tmx = MapToTmx::GenerateXmlDocument(fname, *(roomData->GetMapForRoom(roomnum)->GetData()), blockset_filename);

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

	RoomMetadata meta = RoomMetadata::FromGameData(roomnum, gameData);

	// Properties
	auto properties = tmx.child("map").append_child("properties");

	auto tileset_properties = tmx.child("map").child("tileset").append_child("properties");
	add_property(tileset_properties, "Palette", roomData->GetRoomPaletteDisplayName(meta.palette));
	for(unsigned int i = 0; i < static_cast<unsigned int>(roomData->GetPaletteForRoom(roomnum)->GetData()->GetSize()); i++)
	{
		std::string palette_name = "PaletteColour" + std::to_string(i);
		add_property(tileset_properties, palette_name, roomData->GetPaletteForRoom(roomnum)->GetData()->GetColour(i));
	}
	auto blocksets = roomData->GetBlocksetsForRoom(roomnum);
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
	add_property(tileset_properties, "TilesetName", roomData->GetTilesetDisplayName(meta.tileset));
	
	// Room Properties
	auto room = roomData->GetRoom(roomnum);
	add_property(properties, "RoomName", room->GetDisplayName());
	add_property(properties, "RoomLabel", meta.name);
	add_property(properties, "RoomNumber", meta.room_index);
	add_property(properties, "RoomTileset", meta.tileset);
	add_property(properties, "RoomPalette", meta.palette);
	add_property(properties, "RoomPrimaryBlockset", meta.pri_blockset);
	add_property(properties, "RoomSecondaryBlockset", meta.sec_blockset);
	add_property(properties, "RoomBGM", meta.bgm);
	add_property(properties, "RoomMap", meta.map);
	add_property(properties, "RoomUnknownParam1", meta.unknown_param1);
	add_property(properties, "RoomUnknownParam2", meta.unknown_param2);
	add_property(properties, "RoomZBegin", meta.z_begin);
	add_property(properties, "RoomZEnd", meta.z_end);
	// Warps Properties
	add_property(properties, "WarpFallDestination", meta.fall_destination);
	add_property(properties, "WarpClimbDestination", meta.climb_destination);
	// Flags Properties
	add_property(properties, "FlagVisit", meta.visit_flag);
	add_property(properties, "FlagHasLantern", meta.has_lantern_flag);
	add_property(properties, "FlagLantern", meta.lantern_flag);
	// Misc Properties
	add_property(properties, "FlagIsShopChurchInn", meta.is_shop);
	add_property(properties, "HasTree", meta.is_tree);
	add_property(properties, "HasTreeWarp", meta.has_tree_warp);
	add_property(properties, "TreeWarpFlag", meta.tree_warp_flag);
	add_property(properties, "TreeWarpRoom", meta.tree_warp_room);
	add_property(properties, "LifestockForSale", meta.has_lifestock_flag);
	add_property(properties, "LifestockSaleFlag", meta.lifestock_flag);
	add_property(properties, "SaveLocation", meta.save_location);
	add_property(properties, "MapLocation", meta.map_location);
	add_property(properties, "MapPosition", meta.map_position);

	// Warp objects
	auto warps_objectgroup = tmx.child("map").append_child("objectgroup");
	warps_objectgroup.append_attribute("id") = "3";
	warps_objectgroup.append_attribute("name") = "Warps";

	int warp_id = 1;
	for (const auto& warp : meta.warps) {
		auto warp_object = warps_objectgroup.append_child("object");
		warp_object.append_attribute("id") = warp_id;
		warp_object.append_attribute("visible") = 0;
		warp_object.append_attribute("name") = "Warp";
		warp_object.append_attribute("x") = warp.x1;
		warp_object.append_attribute("y") = warp.y1;
		warp_object.append_attribute("width") = warp.x_size;
		warp_object.append_attribute("height") = warp.y_size;
		// Warp properties
		auto warp_properties = warp_object.append_child("properties");
		add_property(warp_properties, "room1", warp.room1);
		add_property(warp_properties, "room2", warp.room2);
		add_property(warp_properties, "x2", warp.x2);
		add_property(warp_properties, "y2", warp.y2);
		// Convert the warp type to a string
		std::string warp_type_str;
		switch (warp.type) {
			case WarpList::Warp::Type::NORMAL:   warp_type_str = "NORMAL";   break;
			case WarpList::Warp::Type::STAIR_SE: warp_type_str = "STAIR_SE"; break;
			case WarpList::Warp::Type::STAIR_SW: warp_type_str = "STAIR_SW"; break;
			default:                             warp_type_str = "UNKNOWN";  break;
		}
		add_property(warp_properties, "warpType", warp_type_str);
		warp_id++;
	}

	// Entity objects
	auto entities_objectgroup = tmx.child("map").append_child("objectgroup");
	entities_objectgroup.append_attribute("id") = 4;
	entities_objectgroup.append_attribute("name") = "Entities";

	int entity_id = 1;
	for (const auto& entity : meta.entities) {
		auto entity_object = entities_objectgroup.append_child("object");
		entity_object.append_attribute("id") = entity_id;
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
		add_property(entity_properties, "Dialogue", entity.GetDialogue());
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
		entity_id++;
	}

	return tmx.save_file(fname.c_str());
}

static pugi::xml_node GetPropertyNode(pugi::xml_node parent, const std::string& name)
{
	return parent.child("properties").find_child_by_attribute("property", "name", name.c_str());
}

static int GetPropertyInt(pugi::xml_node parent, const std::string& name, int default_val = 0)
{
	auto prop = GetPropertyNode(parent, name);
	if (prop) return prop.attribute("value").as_int();
	return default_val;
}

static bool GetPropertyBool(pugi::xml_node parent, const std::string& name, bool default_val = false)
{
	auto prop = GetPropertyNode(parent, name);
	if (prop) return prop.attribute("value").as_bool();
	return default_val;
}

static std::string GetPropertyString(pugi::xml_node parent, const std::string& name, const std::string& default_val = "")
{
	auto prop = GetPropertyNode(parent, name);
	if (prop) return prop.attribute("value").as_string();
	return default_val;
}

static double GetPropertyDouble(pugi::xml_node parent, const std::string& name, double default_val = 0.0)
{
	auto prop = GetPropertyNode(parent, name);
	if (prop) return prop.attribute("value").as_double();
	return default_val;
}

bool RoomToTmx::ImportFromTmx(const std::string& fname, int roomnum, std::shared_ptr<GameData> gameData, bool import_blocksets)
{
	pugi::xml_document tmx;
	if (!tmx.load_file(fname.c_str())) return false;

	auto map_node = tmx.child("map");
	if (!map_node) return false;

    // First import tiles and heightmap
    auto map = gameData->GetRoomData()->GetMapForRoom(roomnum)->GetData();
    MapToTmx::ImportFromTmx(fname, *map);

	RoomMetadata meta = RoomMetadata::FromGameData(roomnum, gameData);

	meta.name = GetPropertyString(map_node, "RoomLabel", meta.name);
	meta.tileset = GetPropertyInt(map_node, "RoomTileset", meta.tileset);
	meta.palette = GetPropertyInt(map_node, "RoomPalette", meta.palette);
	meta.pri_blockset = GetPropertyInt(map_node, "RoomPrimaryBlockset", meta.pri_blockset);
	meta.sec_blockset = GetPropertyInt(map_node, "RoomSecondaryBlockset", meta.sec_blockset);
	meta.bgm = GetPropertyInt(map_node, "RoomBGM", meta.bgm);
	meta.map = GetPropertyString(map_node, "RoomMap", meta.map);
	meta.unknown_param1 = GetPropertyInt(map_node, "RoomUnknownParam1", meta.unknown_param1);
	meta.unknown_param2 = GetPropertyInt(map_node, "RoomUnknownParam2", meta.unknown_param2);
	meta.z_begin = GetPropertyInt(map_node, "RoomZBegin", meta.z_begin);
	meta.z_end = GetPropertyInt(map_node, "RoomZEnd", meta.z_end);
	meta.fall_destination = GetPropertyInt(map_node, "WarpFallDestination", meta.fall_destination);
	meta.climb_destination = GetPropertyInt(map_node, "WarpClimbDestination", meta.climb_destination);
	meta.visit_flag = GetPropertyInt(map_node, "FlagVisit", meta.visit_flag);
	meta.has_lantern_flag = GetPropertyBool(map_node, "FlagHasLantern", meta.has_lantern_flag);
	meta.lantern_flag = GetPropertyInt(map_node, "FlagLantern", meta.lantern_flag);
	meta.is_shop = GetPropertyBool(map_node, "FlagIsShopChurchInn", meta.is_shop);
	meta.is_tree = GetPropertyBool(map_node, "HasTree", meta.is_tree);
	meta.has_tree_warp = GetPropertyBool(map_node, "HasTreeWarp", meta.has_tree_warp);
	meta.tree_warp_flag = GetPropertyInt(map_node, "TreeWarpFlag", meta.tree_warp_flag);
	meta.tree_warp_room = GetPropertyInt(map_node, "TreeWarpRoom", meta.tree_warp_room);
	meta.has_lifestock_flag = GetPropertyBool(map_node, "LifestockForSale", meta.has_lifestock_flag);
	meta.lifestock_flag = GetPropertyInt(map_node, "LifestockSaleFlag", meta.lifestock_flag);
	meta.save_location = GetPropertyInt(map_node, "SaveLocation", meta.save_location);
	meta.map_location = GetPropertyInt(map_node, "MapLocation", meta.map_location);
	meta.map_position = GetPropertyInt(map_node, "MapPosition", meta.map_position);

	// Warps
	auto warps_node = map_node.find_child_by_attribute("objectgroup", "name", "Warps");
	if (warps_node)
	{
		meta.warps.clear();
		for (auto warp_obj : warps_node.children("object"))
		{
			WarpList::Warp warp;
			warp.x1 = warp_obj.attribute("x").as_int();
			warp.y1 = warp_obj.attribute("y").as_int();
			warp.x_size = warp_obj.attribute("width").as_int();
			warp.y_size = warp_obj.attribute("height").as_int();
			warp.room1 = GetPropertyInt(warp_obj, "room1", roomnum);
			warp.room2 = GetPropertyInt(warp_obj, "room2");
			warp.x2 = GetPropertyInt(warp_obj, "x2");
			warp.y2 = GetPropertyInt(warp_obj, "y2");
			std::string type_str = GetPropertyString(warp_obj, "warpType", "NORMAL");
			if (type_str == "STAIR_SE") warp.type = WarpList::Warp::Type::STAIR_SE;
			else if (type_str == "STAIR_SW") warp.type = WarpList::Warp::Type::STAIR_SW;
			else warp.type = WarpList::Warp::Type::NORMAL;
			meta.warps.push_back(warp);
		}
	}

	// Entities
	auto entities_node = map_node.find_child_by_attribute("objectgroup", "name", "Entities");
	if (entities_node)
	{
		meta.entities.clear();
		for (auto ent_obj : entities_node.children("object"))
		{
			Entity ent;
			ent.SetType(GetPropertyInt(ent_obj, "Type"));
			ent.SetXDbl(GetPropertyDouble(ent_obj, "X"));
			ent.SetYDbl(GetPropertyDouble(ent_obj, "Y"));
			ent.SetZDbl(GetPropertyDouble(ent_obj, "Z"));
			ent.SetPalette(GetPropertyInt(ent_obj, "Palette"));
			ent.SetBehaviour(GetPropertyInt(ent_obj, "Behaviour"));
			ent.SetDialogue(GetPropertyInt(ent_obj, "Dialogue"));
			ent.SetHostile(GetPropertyBool(ent_obj, "Hostile"));
			ent.SetNoRotate(GetPropertyBool(ent_obj, "NoRotate"));
			ent.SetNoPickup(GetPropertyBool(ent_obj, "NoPickup"));
			ent.SetHasDialogue(GetPropertyBool(ent_obj, "HasDialogue"));
			ent.SetVisible(GetPropertyBool(ent_obj, "Visible"));
			ent.SetSolid(GetPropertyBool(ent_obj, "Solid"));
			ent.SetGravity(GetPropertyBool(ent_obj, "Gravity"));
			ent.SetFriction(GetPropertyBool(ent_obj, "Friction"));
			ent.SetSpeed(GetPropertyInt(ent_obj, "Speed"));
			ent.SetReserved(GetPropertyBool(ent_obj, "Reserved"));
			ent.SetTileCopy(GetPropertyBool(ent_obj, "TileCopy"));
			ent.SetCopySource(GetPropertyInt(ent_obj, "TileSource"));
			
			std::string orient = GetPropertyString(ent_obj, "Orientation", "SE");
			if (orient == "NE") ent.SetOrientation(Orientation::NE);
			else if (orient == "SW") ent.SetOrientation(Orientation::SW);
			else if (orient == "NW") ent.SetOrientation(Orientation::NW);
			else ent.SetOrientation(Orientation::SE);

			meta.entities.push_back(ent);
		}
	}

	meta.ApplyToGameData(gameData);

    if (import_blocksets)
    {
        auto tileset_node = map_node.child("tileset");
        if (tileset_node)
        {
            std::string pri_bs_data = GetPropertyString(tileset_node, "PrimaryBlocksetData");
            std::string sec_bs_data = GetPropertyString(tileset_node, "SecondaryBlocksetData");

            auto blocksets = gameData->GetRoomData()->GetBlocksetsForRoom(roomnum);
            if (!pri_bs_data.empty() && blocksets.size() > 0)
            {
                *blocksets.front()->GetData() = BlocksetCmp::FromCsv(pri_bs_data);
            }
            if (!sec_bs_data.empty() && blocksets.size() > 1)
            {
                *std::next(blocksets.begin())->get()->GetData() = BlocksetCmp::FromCsv(sec_bs_data);
            }
        }
    }

	return true;
}

} // namespace Landstalker
