#include <landstalker/3d_maps/RoomToTmx.h>
#include <landstalker/3d_maps/MapToTmx.h>
#include <landstalker/rooms/Room.h>
#include <landstalker/rooms/Entity.h>
#include <landstalker/rooms/WarpList.h>
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

	// Properties
	auto room = roomData->GetRoom(roomnum);
	auto properties = tmx.child("map").append_child("properties");

	auto tileset_properties = tmx.child("map").child("tileset").append_child("properties");
	add_property(tileset_properties, "Palette", roomData->GetRoomPaletteDisplayName(room->room_palette));
	for(unsigned int i = 0; i < roomData->GetPaletteForRoom(roomnum)->GetData()->GetSize(); i++)
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
	add_property(tileset_properties, "TilesetName", roomData->GetTilesetDisplayName(room->tileset));
	
	// Room Properties
	add_property(properties, "RoomName", room->GetDisplayName());
	add_property(properties, "RoomLabel", room->name);
	add_property(properties, "RoomNumber", roomnum);
	add_property(properties, "RoomTileset", room->tileset);
	add_property(properties, "RoomPalette", room->room_palette);
	add_property(properties, "RoomPrimaryBlockset", room->pri_blockset);
	add_property(properties, "RoomSecondaryBlockset", room->sec_blockset);
	add_property(properties, "RoomBGM", room->bgm);
	add_property(properties, "RoomMap", room->map);
	add_property(properties, "RoomUnknownParam1", room->unknown_param1);
	add_property(properties, "RoomUnknownParam2", room->unknown_param2);
	add_property(properties, "RoomZBegin", room->room_z_begin);
	add_property(properties, "RoomZEnd", room->room_z_end);
	// Warps Properties
	add_property(properties, "WarpFallDestination", roomData->GetFallDestination(roomnum));
	add_property(properties, "WarpClimbDestination", roomData->GetClimbDestination(roomnum));
	// Flags Properties
	add_property(properties, "FlagHasLantern", roomData->HasLanternFlag(roomnum));
	add_property(properties, "FlagLantern", roomData->GetLanternFlag(roomnum));
	// Misc Properties
	add_property(properties, "FlagIsShopChurchInn", roomData->IsShop(roomnum));
	add_property(properties, "MiscLifestockForSale", roomData->HasLifestockSaleFlag(roomnum));
	add_property(properties, "MiscLifestockSaleFlag", roomData->GetLifestockSaleFlag(roomnum));

	// Warp objects
	auto warps_objectgroup = tmx.child("map").append_child("objectgroup");
	warps_objectgroup.append_attribute("id") = "3";
	warps_objectgroup.append_attribute("name") = "Warps";

	int warp_id = 1;
    std::vector<WarpList::Warp> warps = roomData->GetWarpsForRoom(roomnum);
	for (const auto& warp : warps) {
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
	std::vector<Entity> entities = gameData->GetSpriteData()->GetRoomEntities(roomnum);

	for (const auto& entity : entities) {
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

} // namespace Landstalker
