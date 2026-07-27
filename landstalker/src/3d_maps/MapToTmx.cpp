#include <landstalker/3d_maps/MapToTmx.h>
#include <cctype>
#include <exception>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <string>
#include <vector>

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

static std::string GetHMData(const Tilemap3D& map)
{
    std::ostringstream ss;
    int total_cells = map.GetHeightmapWidth() * map.GetHeightmapHeight();

    int i = 0;
    for (int y = 0; y < map.GetHeightmapHeight(); ++y)
    {
        for (int x = 0; x < map.GetHeightmapWidth(); ++x, ++i)
        {
            ss << Landstalker::Hex(map.GetHeightmapCell({ x, y }));
            if (i < total_cells - 1)
            {
                ss << ",";
            }
        }
        ss << std::endl;
    }

    return ss.str();
}

// Reads the "heightmap" map property written by GetHMData: a comma separated list of hex
// cells, one text row per heightmap row. XML attribute value normalisation turns those row
// breaks into spaces, so treat any comma or whitespace as a separator and lean on the
// declared heightmap size to confirm the right number of cells arrived.
static bool ReadHMData(const std::string& data, std::size_t expected, std::vector<uint16_t>& cells)
{
	cells.clear();
	std::string token;
	auto flush = [&cells, &token]()
	{
		if (token.empty())
		{
			return true;
		}
		try
		{
			cells.push_back(static_cast<uint16_t>(std::stoul(token, nullptr, 16)));
		}
		catch (const std::exception&)
		{
			return false;
		}
		token.clear();
		return true;
	};

	for (const char c : data)
	{
		if (c == ',' || std::isspace(static_cast<unsigned char>(c)))
		{
			if (!flush())
			{
				return false;
			}
		}
		else
		{
			token += c;
		}
	}
	return flush() && cells.size() == expected;
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
	if (!tmx.load_file(fname.c_str()))
	{
		return false;
	}
	const auto map_node = tmx.child("map");
	int width = map_node.attribute("width").as_int() - 1;
	int height = map_node.attribute("height").as_int();
	std::vector<uint16_t> fg, bg;
	for (const auto layer : map_node.children("layer"))
	{
		auto data = layer.child("data");
		if (data && data.attribute("encoding").as_string() == std::string("csv"))
		{
			if(layer.attribute("id").as_int() == 1)
			{
				bg = ReadData(width, height, data.child_value());
			}
			else if (layer.attribute("id").as_int() == 2)
			{
				fg = ReadData(width, height, data.child_value());
			}
		}
	}

	// The heightmap rides along in the map properties rather than in a layer. Older files
	// were written without it, so its absence is not an error - but a property that is
	// present and unreadable is, rather than silently dropping the user's heightmap.
	int hmwidth = 0;
	int hmheight = 0;
	int hmleft = 0;
	int hmtop = 0;
	std::string hmdata;
	for (const auto property : map_node.child("properties").children("property"))
	{
		const std::string name = property.attribute("name").as_string();
		const auto value = property.attribute("value");
		if (name == "hmwidth")
		{
			hmwidth = value.as_int();
		}
		else if (name == "hmheight")
		{
			hmheight = value.as_int();
		}
		else if (name == "hmleft")
		{
			hmleft = value.as_int();
		}
		else if (name == "hmtop")
		{
			hmtop = value.as_int();
		}
		else if (name == "heightmap")
		{
			hmdata = value.as_string();
		}
	}

	std::vector<uint16_t> hm;
	const bool has_heightmap = !hmdata.empty();
	if (has_heightmap &&
	    !(hmwidth > 0 && hmwidth <= 64 && hmheight > 0 && hmheight <= 64 &&
	      hmleft >= 0 && hmleft <= 63 && hmtop >= 0 && hmtop <= 63 &&
	      ReadHMData(hmdata, static_cast<std::size_t>(hmwidth) * hmheight, hm)))
	{
		return false;
	}

	if (width > 0 && width < 64 &&
	    height > 0 && height < 64 &&
		fg.size() == static_cast<std::size_t>(width * height) &&
		bg.size() == static_cast<std::size_t>(width * height))
	{
		map.Resize(static_cast<uint8_t>(width), static_cast<uint8_t>(height));
		int i = 0;
		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x, ++i)
			{
				map.SetBlock({ fg[i], {x, y} }, Tilemap3D::Layer::FG);
				map.SetBlock({ bg[i], {x, y} }, Tilemap3D::Layer::BG);
			}
		}
		if (has_heightmap)
		{
			map.ResizeHeightmap(static_cast<uint8_t>(hmwidth), static_cast<uint8_t>(hmheight));
			map.SetLeft(static_cast<uint8_t>(hmleft));
			map.SetTop(static_cast<uint8_t>(hmtop));
			int j = 0;
			for (int y = 0; y < hmheight; ++y)
			{
				for (int x = 0; x < hmwidth; ++x, ++j)
				{
					map.SetHeightmapCell({ x, y }, hm[j]);
				}
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
	auto map_properties = map_node.append_child("properties");
	auto hmwidth_property = map_properties.append_child("property");
	hmwidth_property.append_attribute("name") = "hmwidth";
	hmwidth_property.append_attribute("value") = map.GetHeightmapWidth();
	auto hmheight_property = map_properties.append_child("property");
	hmheight_property.append_attribute("name") = "hmheight";
	hmheight_property.append_attribute("value") = map.GetHeightmapHeight();
	auto hmleft_property = map_properties.append_child("property");
	hmleft_property.append_attribute("name") = "hmleft";
	hmleft_property.append_attribute("value") = map.GetLeft();
	auto hmtop_property = map_properties.append_child("property");
	hmtop_property.append_attribute("name") = "hmtop";
	hmtop_property.append_attribute("value") = map.GetTop();
	auto heightmap_property = map_properties.append_child("property");
	heightmap_property.append_attribute("name") = "heightmap";
	heightmap_property.append_attribute("value") = GetHMData(map).c_str();

	auto tileset_node = map_node.append_child("tileset");
	tileset_node.append_attribute("firstgid") = 1;
	tileset_node.append_attribute("name") = bsfn.stem().string().c_str();
	tileset_node.append_attribute("tilewidth") = 16;
	tileset_node.append_attribute("tileheight") = 16;
	tileset_node.append_attribute("tilecount") = 1024;
	tileset_node.append_attribute("columns") = 16;

	auto image_node = tileset_node.append_child("image");
	image_node.append_attribute("source") = std::filesystem::proximate(bsfn, fn.parent_path()).string().c_str();
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
	bg_data.append_child(pugi::node_pcdata).set_value(GetData(map, Tilemap3D::Layer::BG).c_str());

	auto fg_layer = map_node.append_child("layer");
	fg_layer.append_attribute("id") = 2;
	fg_layer.append_attribute("name") = "Foreground";
	fg_layer.append_attribute("width") = map.GetWidth();
	fg_layer.append_attribute("height") = map.GetHeight();
	fg_layer.append_attribute("offsetx") = 0;
	fg_layer.append_attribute("offsety") = 0;
	auto fg_data = fg_layer.append_child("data");
	fg_data.append_attribute("encoding") = "csv";
	fg_data.append_child(pugi::node_pcdata).set_value(GetData(map, Tilemap3D::Layer::FG).c_str());

	return tmx;
}

bool MapToTmx::ExportToTmx(const std::string& fname, const Tilemap3D& map, const std::string& blockset_filename)
{
	auto tmx = MapToTmx::GenerateXmlDocument(fname, map, blockset_filename);
	return tmx.save_file(fname.c_str());
}

} // namespace Landstalker
