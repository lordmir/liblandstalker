#include <landstalker/3d_maps/MapToTmx.h>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace Landstalker {

static std::string GetData(const Tilemap3D& map, Tilemap3D::Layer layer)
{
	std::ostringstream ss;
	int i = 0;
	for (int y = 0; y < map.GetHeight(); ++y)
	{
		for (int x = 0; x < map.GetWidth(); ++x, ++i)
		{
			ss << std::setw(4) << std::setfill('0') << (map.GetBlock({ x, y }, layer) + 1);
			if (i < map.GetWidth() * map.GetHeight() - 1)
			{
				ss << ",";
			}
		}
		ss << std::endl;
	}
	return ss.str();
}

static std::vector<uint16_t> ReadData(int width, int height, const std::string& csv)
{
	std::istringstream ss(csv);
	std::vector<uint16_t> retval(width * height);
	int x = 0, y = 0;
    std::string row, cell;
	while(std::getline(ss, row) && y < height)
	{
		std::istringstream rss(row);
		while(std::getline(rss, cell, ',') && x < width)
		{
			retval[x + y * width] = (std::stoi(cell) - 1) & 0x03FF;
			x++;
		}
		x = 0;
		y++;
	}
	return retval;
}

bool MapToTmx::ImportFromTmx(const std::string& fname, Tilemap3D& map)
{
	pugi::xml_document tmx;
	tmx.load_file(fname.c_str());
	int width = tmx.child("map").attribute("width").as_int() - 1;
	int height = tmx.child("map").attribute("height").as_int();
	std::vector<uint16_t> fg, bg;
	for (pugi::xml_node_iterator it = tmx.child("layer").begin(); it != tmx.child("layer").end(); ++it)
	{
		auto data = it->child("data");
		if (data && data.attribute("encoding").as_string() == std::string("csv"))
		{
			if(it->attribute("id").as_int() == 1)
			{
				bg = ReadData(width, height, data.child_value());
			}
			else if (it->attribute("id").as_int() == 2)
			{
				fg = ReadData(width, height, data.child_value());
			}
		}
	}

	if (width > 0 && width < 64 &&
	    height > 0 && height < 64 &&
		fg.size() == static_cast<std::size_t>(width * height) &&
		bg.size() == static_cast<std::size_t>(width * height))
	{
		map.Resize(width, height);
		int i = 0;
		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x, ++i)
			{
				map.SetBlock({ fg[i], {x, y} }, Tilemap3D::Layer::FG);
				map.SetBlock({ bg[i], {x, y} }, Tilemap3D::Layer::BG);
			}
		}
		return true;
	}

	return false;
}

pugi::xml_document MapToTmx::GenerateXmlDocument(const std::string& fname, const Tilemap3D& map, const std::string& blockset_filename)
{
	pugi::xml_document tmx;
	std::filesystem::path fn(fname);
	std::filesystem::path bsfn(blockset_filename);
	auto map_node = tmx.append_child("map");
	map_node.append_attribute("version") = "1.10";
	map_node.append_attribute("tiledversion") = "1.10.1";
	map_node.append_attribute("class") = fn.stem().string().c_str();
	map_node.append_attribute("orientation") = "isometric";
	map_node.append_attribute("renderorder") = "left-down";
	map_node.append_attribute("width") = map.GetWidth() + 1;
	map_node.append_attribute("height") = map.GetHeight();
	map_node.append_attribute("tilewidth") = 32;
	map_node.append_attribute("tileheight") = 16;
	map_node.append_attribute("infinite") = 0;
	map_node.append_attribute("backgroundcolor") = "#181818";
	map_node.append_attribute("nextlayerid") = 3;
	map_node.append_attribute("nextobjectid") = 1;

	auto tileset_node = map_node.append_child("tileset");
	tileset_node.append_attribute("firstgid") = 1;
	tileset_node.append_attribute("name") = bsfn.stem().string().c_str();
	tileset_node.append_attribute("tilewidth") = 16;
	tileset_node.append_attribute("tileheight") = 16;
	tileset_node.append_attribute("tilecount") = 1024;
	tileset_node.append_attribute("columns") = 16;

	auto image_node = tileset_node.append_child("image");
	image_node.append_attribute("source") = std::filesystem::relative(fn.parent_path(), bsfn).string().c_str();
	image_node.append_attribute("width") = 256;
	image_node.append_attribute("height") = 1024;

	auto bg_layer = map_node.append_child("layer");
	bg_layer.append_attribute("id") = 1;
	bg_layer.append_attribute("name") = "Background";
	bg_layer.append_attribute("width") = map.GetWidth();
	bg_layer.append_attribute("height") = map.GetHeight();
	bg_layer.append_attribute("offsetx") = 16;
	bg_layer.append_attribute("offsety") = 0;
	auto bg_data = bg_layer.append_child("data");
	bg_data.append_attribute("encoding") = "csv";
	bg_data.set_value(GetData(map, Tilemap3D::Layer::BG).c_str());

	auto fg_layer = map_node.append_child("layer");
	fg_layer.append_attribute("id") = 2;
	fg_layer.append_attribute("name") = "Foreground";
	fg_layer.append_attribute("width") = map.GetWidth();
	fg_layer.append_attribute("height") = map.GetHeight();
	fg_layer.append_attribute("offsetx") = 0;
	fg_layer.append_attribute("offsety") = 0;
	auto fg_data = fg_layer.append_child("data");
	fg_data.append_attribute("encoding") = "csv";
	fg_data.set_value(GetData(map, Tilemap3D::Layer::FG).c_str());

	return tmx;
}

bool MapToTmx::ExportToTmx(const std::string& fname, const Tilemap3D& map, const std::string& blockset_filename)
{
	auto tmx = MapToTmx::GenerateXmlDocument(fname, map, blockset_filename);
	return tmx.save_file(fname.c_str());
}

} // namespace Landstalker
