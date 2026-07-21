#include <future>
#include <mutex>
#include <chrono>
#include <landstalker/3d_maps/Tilemap3D.h>
#include <landstalker/3d_maps/Tilemap3DCompressor.h>

#include <stdexcept>
#include <unordered_map>
#include <set>
#include <map>
#include <functional>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>

#include <landstalker/misc/BitBarrel.h>
#include <landstalker/misc/BitBarrelWriter.h>
#include <landstalker/misc/Literals.h>
#include <landstalker/misc/Utils.h>

namespace Landstalker {

#include <landstalker/3d_maps/Tilemap3DCompressor.h>

uint16_t Tilemap3D::Decode(const uint8_t* src)
{
    return Tilemap3DCompressor::Decode(*this, src);
}

uint16_t Tilemap3D::Encode(uint8_t* dst, size_t size)
{
    return Tilemap3DCompressor::Encode(*this, dst, size);
}

namespace
{

// Splits one line of a CSV into hexadecimal values. Returns false on anything unparseable.
bool ReadCsvRow(const std::string& row, std::vector<uint16_t>& cells)
{
	std::istringstream rss(row);
	std::string cell;
	while (std::getline(rss, cell, ','))
	{
		try
		{
			cells.push_back(static_cast<uint16_t>(std::stoul(cell, nullptr, 16)));
		}
		catch (const std::exception&)
		{
			return false;
		}
	}
	return true;
}

bool ReadCsv(const std::string& csv, std::vector<std::vector<uint16_t>>& rows)
{
	std::istringstream iss(csv);
	std::string row;
	while (std::getline(iss, row))
	{
		// Ignore blank lines so a trailing newline does not read as an empty row.
		if (row.find_first_not_of(" \t\r\n") == std::string::npos)
		{
			continue;
		}
		rows.push_back({});
		if (!ReadCsvRow(row, rows.back()))
		{
			return false;
		}
	}
	return true;
}

}

bool Tilemap3D::FromCsv(const std::string& foreground_csv, const std::string& background_csv, const std::string& heightmap_csv)
{
	std::vector<std::vector<uint16_t>> fgvec, bgvec, hmvec;
	if (!ReadCsv(foreground_csv, fgvec) ||
	    !ReadCsv(background_csv, bgvec) ||
	    !ReadCsv(heightmap_csv, hmvec))
	{
		return false;
	}

	// The heightmap CSV leads with a "left,top" row, then one row per heightmap row.
	if (hmvec.size() < 2 || hmvec.front().size() != 2 ||
	    fgvec.empty() || fgvec.front().empty() ||
	    bgvec.empty() || bgvec.front().empty())
	{
		return false;
	}

	const std::size_t w = fgvec.front().size();
	const std::size_t h = fgvec.size();
	const std::size_t hw = hmvec[1].size();
	const std::size_t hh = hmvec.size() - 1;
	const std::size_t l = hmvec[0][0];
	const std::size_t t = hmvec[0][1];

	if (w == 0 || w > 64 || h == 0 || h > 64 ||
	    hw == 0 || hw > 64 || hh == 0 || hh > 64 ||
	    l > 63 || t > 63 || bgvec.size() != h)
	{
		return false;
	}
	for (std::size_t i = 0; i < h; ++i)
	{
		if (bgvec[i].size() != w || fgvec[i].size() != w)
		{
			return false;
		}
	}
	for (std::size_t i = 1; i <= hh; ++i)
	{
		if (hmvec[i].size() != hw)
		{
			return false;
		}
	}

	Resize(static_cast<uint8_t>(w), static_cast<uint8_t>(h));
	ResizeHeightmap(static_cast<uint8_t>(hw), static_cast<uint8_t>(hh));
	SetLeft(static_cast<uint8_t>(l));
	SetTop(static_cast<uint8_t>(t));

	uint16_t i = 0;
	for (std::size_t y = 0; y < h; ++y)
	{
		for (std::size_t x = 0; x < w; ++x)
		{
			SetBlock(bgvec[y][x], i, Tilemap3D::Layer::BG);
			SetBlock(fgvec[y][x], i++, Tilemap3D::Layer::FG);
		}
	}
	for (int y = 0; y < static_cast<int>(hh); ++y)
	{
		for (int x = 0; x < static_cast<int>(hw); ++x)
		{
			SetCellProps({ x, y }, (hmvec[y + 1][x] >> 12) & 0xF);
			SetHeight({ x, y }, (hmvec[y + 1][x] >> 8) & 0xF);
			SetCellType({ x, y }, hmvec[y + 1][x] & 0xFF);
		}
	}
	return true;
}

bool Tilemap3D::ToCsv(std::string& foreground_csv, std::string& background_csv, std::string& heightmap_csv) const
{
	if (GetWidth() == 0 || GetHeight() == 0 || GetHeightmapWidth() == 0 || GetHeightmapHeight() == 0)
	{
		return false;
	}

	std::ostringstream fg, bg, hm;
	for (uint16_t i = 0; i < GetWidth() * GetHeight(); ++i)
	{
		fg << StrPrintf("%04X", GetBlock(i, Tilemap3D::Layer::FG).value);
		bg << StrPrintf("%04X", GetBlock(i, Tilemap3D::Layer::BG).value);
		const bool end_of_row = (i + 1) % GetWidth() == 0;
		fg << (end_of_row ? "\n" : ",");
		bg << (end_of_row ? "\n" : ",");
	}

	hm << StrPrintf("%02X", GetLeft()) << "," << StrPrintf("%02X", GetTop()) << "\n";
	for (int y = 0; y < GetHeightmapHeight(); ++y)
	{
		for (int x = 0; x < GetHeightmapWidth(); ++x)
		{
			hm << StrPrintf("%X%X%02X", GetCellProps({ x, y }), GetHeight({ x, y }), GetCellType({ x, y }));
			hm << ((x + 1) == GetHeightmapWidth() ? "\n" : ",");
		}
	}

	foreground_csv = fg.str();
	background_csv = bg.str();
	heightmap_csv = hm.str();
	return true;
}

uint8_t Tilemap3D::GetLeft() const
{
    return left;
}

uint8_t Tilemap3D::GetTop() const
{
    return top;
}

uint8_t Tilemap3D::GetWidth() const
{
    return width;
}

uint8_t Tilemap3D::GetHeight() const
{
    return height;
}

uint16_t Tilemap3D::GetSize() const
{
    return width * height;
}

void Tilemap3D::Resize(uint8_t w, uint8_t h)
{
    const std::vector<uint16_t> old_bg = background;
    const std::vector<uint16_t> old_fg = foreground;

    background.resize(w * h);
    foreground.resize(w * h);
    
    auto obit = old_bg.cbegin();
    auto ofit = old_fg.cbegin();
    auto nbit = background.begin();
    auto nfit = foreground.begin();
    for (int i = 0; i < h; ++i)
    {
        for (int j = 0; j < w; j++)
        {
            if ((i < height) && (j < width))
            {
                *nbit++ = *obit++;
                *nfit++ = *ofit++;
            }
            else
            {
                *nbit++ = 0;
                *nfit++ = 0;
            }
        }
        if (w < width)
        {
            obit += width - w;
            ofit += width - w;
        }
    }

    width = w;
    height = h;
}

void Tilemap3D::ResizeHeightmap(uint8_t w, uint8_t h)
{
    const std::vector<uint16_t> old_hm = heightmap;

    heightmap.resize(w * h);

    auto ohit = old_hm.cbegin();
    auto nhit = heightmap.begin();
    for (int i = 0; i < h; ++i)
    {
        for (int j = 0; j < w; j++)
        {
            if ((i < hmheight) && (j < hmwidth))
            {
                *nhit++ = *ohit++;
            }
            else
            {
                *nhit++ = 0x4000;
            }
        }
        if (w < hmwidth)
        {
            ohit += width - w;
        }
    }

    hmwidth = w;
    hmheight = h;
}

void Tilemap3D::InsertHeightmapColumn(uint8_t before)
{
    if (before < hmheight && hmheight < 64)
    {
        const std::vector<uint16_t> orig = heightmap;
        hmheight += 1;
        heightmap.resize(hmwidth * hmheight);
        auto src = orig.data();
        auto dst = heightmap.data();
        int idx = 0;
        for (int y = 0; y < hmheight; ++y)
        {
            for (int x = 0; x < hmwidth; ++x)
            {
                if (y <= before)
                {
                    dst[idx] = src[idx];
                    ++idx;
                }
                else
                {
                    dst[idx] = src[idx - hmwidth];
                    ++idx;
                }
            }
        }
    }
}

void Tilemap3D::InsertHeightmapRow(uint8_t before)
{
    if (before < hmwidth && hmwidth < 64)
    {
        const std::vector<uint16_t> orig = heightmap;
        hmwidth += 1;
        heightmap.resize(hmwidth * hmheight);
        auto src = orig.data();
        auto dst = heightmap.data();
        for (int y = 0; y < hmheight; ++y)
        {
            for (int x = 0; x < hmwidth; ++x)
            {
                if (x <= before)
                {
                    dst[y * hmwidth + x] = src[y * (hmwidth - 1) + x];
                }
                else
                {
                    dst[y * hmwidth + x] = src[y * (hmwidth - 1) + x - 1];
                }
            }
        }
    }
}

void Tilemap3D::DeleteHeightmapColumn(uint8_t row)
{
    if (row < hmheight && hmheight > 1)
    {
        std::vector<uint16_t> orig = heightmap;
        hmheight -= 1;
        heightmap.resize(hmwidth * hmheight);
        auto src = orig.data();
        auto dst = heightmap.data();
        for (int y = 0; y < hmheight; ++y)
        {
            for (int x = 0; x < hmwidth; ++x)
            {
                if (y < row)
                {
                    dst[y * hmwidth + x] = src[y * hmwidth + x];
                }
                else
                {
                    dst[y * hmwidth + x] = src[(y + 1) * hmwidth + x];
                }
            }
        }
    }
}

void Tilemap3D::ClearTilemap()
{
    std::fill(foreground.begin(), foreground.end(), 0_u16);
    std::fill(background.begin(), background.end(), 0_u16);
}

void Tilemap3D::InsertTilemapRow(int row)
{
    if (row < width && width < 64)
    {
        const std::vector<uint16_t> orig_fg = foreground;
        const std::vector<uint16_t> orig_bg = background;
        width += 1;
        foreground.resize(width * height);
        background.resize(width * height);
        auto fg_src = orig_fg.data();
        auto fg_dst = foreground.data();
        auto bg_src = orig_bg.data();
        auto bg_dst = background.data();
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                if (x <= row)
                {
                    fg_dst[y * width + x] = fg_src[y * (width - 1) + x];
                    bg_dst[y * width + x] = bg_src[y * (width - 1) + x];
                }
                else
                {
                    fg_dst[y * width + x] = fg_src[y * (width - 1) + x - 1];
                    bg_dst[y * width + x] = bg_src[y * (width - 1) + x - 1];
                }
            }
        }
    }
}

void Tilemap3D::InsertTilemapColumn(int col)
{
    if (col < height && height < 64)
    {
        const std::vector<uint16_t> orig_fg = foreground;
        const std::vector<uint16_t> orig_bg = background;
        height += 1;
        foreground.resize(width * height);
        background.resize(width * height);
        auto fg_src = orig_fg.data();
        auto fg_dst = foreground.data();
        auto bg_src = orig_bg.data();
        auto bg_dst = background.data();
        int idx = 0;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                if (y <= col)
                {
                    fg_dst[idx] = fg_src[idx];
                    bg_dst[idx] = bg_src[idx];
                    ++idx;
                }
                else
                {
                    fg_dst[idx] = fg_src[idx - width];
                    bg_dst[idx] = bg_src[idx - width];
                    ++idx;
                }
            }
        }
    }
}

void Tilemap3D::DeleteTilemapRow(int row)
{
    if (row < width && width > 1)
    {
        std::vector<uint16_t> orig_fg = foreground;
        std::vector<uint16_t> orig_bg = background;
        width -= 1;
        foreground.resize(width * height);
        const uint16_t* fg_src = orig_fg.data();
        uint16_t* fg_dst = foreground.data();
        const uint16_t* bg_src = orig_bg.data();
        uint16_t* bg_dst = background.data();
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                if (x < row)
                {
                    fg_dst[y * width + x] = fg_src[y * (width + 1) + x];
                    bg_dst[y * width + x] = bg_src[y * (width + 1) + x];
                }
                else
                {
                    fg_dst[y * width + x] = fg_src[y * (width + 1) + x + 1];
                    bg_dst[y * width + x] = bg_src[y * (width + 1) + x + 1];
                }
            }
        }
    }
}

void Tilemap3D::DeleteTilemapColumn(int col)
{
    if (col < height && height > 1)
    {
        std::vector<uint16_t> orig_fg = foreground;
        std::vector<uint16_t> orig_bg = background;
        height -= 1;
        foreground.resize(width * height);
        background.resize(width * height);
        auto fg_src = orig_fg.data();
        auto fg_dst = foreground.data();
        auto bg_src = orig_bg.data();
        auto bg_dst = background.data();
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                if (y < col)
                {
                    fg_dst[y * width + x] = fg_src[y * width + x];
                    bg_dst[y * width + x] = bg_src[y * width + x];
                }
                else
                {
                    fg_dst[y * width + x] = fg_src[(y + 1) * width + x];
                    bg_dst[y * width + x] = bg_src[(y + 1) * width + x];
                }
            }
        }
    }
}

void Tilemap3D::DeleteHeightmapRow(uint8_t col)
{
    if (col < hmwidth && hmwidth > 1)
    {
        std::vector<uint16_t> orig = heightmap;
        hmwidth -= 1;
        heightmap.resize(hmwidth * hmheight);
        const uint16_t* src = orig.data();
        uint16_t* dst = heightmap.data();
        for (int y = 0; y < hmheight; ++y)
        {
            for (int x = 0; x < hmwidth; ++x)
            {
                if (x < col)
                {
                    dst[y * hmwidth + x] = src[y * (hmwidth + 1) + x];
                }
                else
                {
                    dst[y * hmwidth + x] = src[y * (hmwidth + 1) + x + 1];
                }
            }
        }
    }
}

void Tilemap3D::SetLeft(uint8_t pleft)
{
    this->left = pleft;
}

void Tilemap3D::SetTop(uint8_t ptop)
{
    this->top = ptop;
}

uint8_t Tilemap3D::GetHeightmapWidth() const
{
    return hmwidth;
}

uint8_t Tilemap3D::GetHeightmapHeight() const
{
    return hmheight;
}

uint16_t Tilemap3D::GetHeightmapSize() const
{
    return hmwidth * hmheight;
}

uint8_t Tilemap3D::GetTileWidth() const
{
    return tile_width;
}

uint8_t Tilemap3D::GetTileHeight() const
{
    return tile_height;
}

void Tilemap3D::SetTileDims(uint8_t tw, uint8_t th)
{
    tile_width = tw;
    tile_height = th;
}

std::size_t Tilemap3D::GetCartesianWidth() const
{
    return (GetWidth() + GetHeight() + GetLeft()) * 2;
}

std::size_t Tilemap3D::GetCartesianHeight() const
{
    return (GetWidth() + GetHeight() + GetTop() + 1);
}

std::size_t Tilemap3D::GetPixelWidth() const
{
    return GetCartesianWidth() * tile_width;
}

std::size_t Tilemap3D::GetPixelHeight() const
{
    return GetCartesianHeight() * tile_height;
}

bool Tilemap3D::IsIsoPointValid(const IsoPoint2D& iso) const
{
    return ((iso.x >= 0 && iso.x < GetWidth()) &&
        (iso.y >= 0 && iso.y < GetHeight()));
}

bool Tilemap3D::IsPointValid(const Point2D& p, Layer layer) const
{
    const auto iso = ToIsometric(p, layer);
    return IsIsoPointValid(iso);
}

bool Tilemap3D::IsPixelPointValid(const PixelPoint2D& pix, Layer layer) const
{
    const auto iso = PixelToIsometric(pix, layer);
    return IsIsoPointValid(iso);
}

bool Tilemap3D::IsHMPointValid(const HMPoint2D& p) const
{
    return ((p.x >= 0 && p.x < hmwidth) &&
            (p.y >= 0 && p.y < hmheight));
}

Point2D Tilemap3D::IsoToCartesian(const IsoPoint2D& iso, Layer /*layer*/) const
{
    if (IsIsoPointValid(iso) == false) return { -1, -1 };
    return Point2D{-1, -1};
}

Point2D Tilemap3D::PixelToCartesian(const PixelPoint2D& pix, Layer layer) const
{
    if (IsPixelPointValid(pix, layer) == false) return { -1, -1 };
    return Point2D{ -1, -1 };
}

IsoPoint2D Tilemap3D::ToIsometric(const Point2D& p, Layer /*layer*/) const
{
    int xgrid = (p.x - GetLeft()) / 2;
    int ygrid = (2 * (p.y - GetTop())) / 2;
    int x = (ygrid + xgrid - GetHeight() + 1) / 2;
    int y = (ygrid - xgrid + GetHeight() - 1) / 2;

    auto retval = IsoPoint2D{ x, y };

    return IsIsoPointValid(retval) ? retval : IsoPoint2D{0, 0};
}

IsoPoint2D Tilemap3D::PixelToIsometric(const PixelPoint2D& /*pix*/, Layer /*layer*/) const
{
    // TODO
    return IsoPoint2D{ -1, -1 };
}

PixelPoint2D Tilemap3D::IsoToPixel(const IsoPoint2D& iso, Layer layer, bool offset) const
{
    if (IsIsoPointValid(iso) == false) return { -1, -1 };
    int layer_offset = (layer == Layer::BG) ? 2 : 0;
    int left_offset = offset ? GetLeft() : 0;
    int top_offset = offset ? GetTop() : 0;
    return {
        ((iso.x - iso.y + (GetHeight() - 1)) * 2 + left_offset + layer_offset) * tile_width,
        (iso.x + iso.y + top_offset) * tile_height
    };
}

PixelPoint2D Tilemap3D::ToPixel(const Point2D& iso, Layer /*layer*/) const
{
    if (IsIsoPointValid(iso) == false) return { -1, -1 };
    return PixelPoint2D{ -1, -1 };
}

PixelPoint2D Tilemap3D::EntityPositionToPixel(uint16_t x, uint16_t y, uint16_t z) const
{
    const int SCALE_FACTOR = 0x100;
    const int LEFT = GetLeft() * SCALE_FACTOR;
    const int TOP = GetTop() * SCALE_FACTOR;
    const int HEIGHT = GetHeight() * SCALE_FACTOR;
    int xx = x - LEFT;
    int yy = y - TOP;
    int ix = (xx - yy + (HEIGHT - SCALE_FACTOR)) * 2 + LEFT;
    int iy = (xx + yy - z * 2) + TOP;
    return Point2D{ ix * tile_width / SCALE_FACTOR, iy * tile_height / SCALE_FACTOR };
}

PixelPoint2D Tilemap3D::Iso3DToPixel(const Point3D& iso) const
{
    //if (IsHMPointValid({ iso.x, iso.y }) == false) return { -1, -1 };
    int xx = iso.x - GetLeft();
    int yy = iso.y - GetTop();
    int ix = (xx - yy + (GetHeight() - 1)) * 2 + GetLeft();
    int iy = (xx + yy - iso.z * 2) + GetTop();
    return Point2D{ ix * tile_width, iy * tile_height };
}

Point3D Tilemap3D::PixelToIso3D(const PixelPoint2D& p) const
{
    if (IsPixelPointValid(p) == false) return { -1, -1, -1 };
    return Point3D{-1, -1, -1};
}

PixelPoint2D Tilemap3D::HMPointToPixel(const HMPoint2D& p) const
{
    return Iso3DToPixel({p.x + 12, p.y + 12, 0});
}

HMPoint2D Tilemap3D::PixelToHMPoint(const PixelPoint2D& p) const
{
    return {
        p.y / (2 * tile_height) + p.x / (4 * tile_width) - (GetLeft() + 2 * (GetTop() + GetHeight() - 1)) / 4 - 12,
        p.y / (2 * tile_height) - p.x / (4 * tile_width) + (GetLeft() - 2 * (GetTop() - GetHeight() + 1)) / 4 - 12
    };
}

bool Tilemap3D::IsBlockValid(uint16_t block) const
{
    return (block < GetSize());
}

bool Tilemap3D::IsBlockValid(const IsoPoint2D& iso) const
{
    return IsIsoPointValid(iso);
}

BlockLoc Tilemap3D::GetBlock(uint16_t block, Layer layer) const
{
    BlockLoc ret{0xFFFF,{-1,-1}};
    if (IsBlockValid(block))
    {
        ret.position.x = block % GetWidth();
        ret.position.y = block / GetWidth();
        if (layer == Layer::FG)
        {
            ret.value = foreground[block];
        }
        else
        {
            ret.value = background[block];
        }
    }
    return ret;
}

uint16_t Tilemap3D::GetBlock(const IsoPoint2D& iso, Layer layer) const
{
    uint16_t block = static_cast<uint16_t>(iso.x + iso.y * GetWidth());
    if (IsBlockValid(block))
    {
        if (layer == Layer::FG)
        {
            return foreground[block];
        }
        else
        {
            return background[block];
        }
    }
    return 0xFFFF;
}

bool Tilemap3D::SetBlock(uint16_t block_value, uint16_t block_index, Layer layer)
{
    if (IsBlockValid(block_index))
    {
        if (layer == Layer::FG)
        {
            foreground[block_index] = block_value & 0x3FF;
        }
        else
        {
            background[block_index] = block_value & 0x3FF;
        }
        return true;
    }
    return false;
}

bool Tilemap3D::SetBlock(const BlockLoc& loc, Layer layer)
{
    if (IsBlockValid(loc.position))
    {
        int block = loc.position.x + loc.position.y * GetWidth();
        if (layer == Layer::FG)
        {
            foreground[block] = loc.value & 0x3FF;
        }
        else
        {
            background[block] = loc.value & 0x3FF;
        }
        return true;
    }
    return false;
}

uint8_t Tilemap3D::GetHeight(const HMPoint2D& p) const
{
    if (IsHMPointValid(p) == false) return 0xFF;
    return (heightmap[p.x + p.y * hmwidth] & 0x0F00) >> 8;
}

bool Tilemap3D::SetHeight(const HMPoint2D& p, uint8_t pheight)
{
    if (IsHMPointValid(p) && pheight < 0x10)
    {
        int cell = p.x + p.y * hmwidth;
        heightmap[cell] &= 0xF0FF;
        heightmap[cell] |= (pheight & 0x0F) << 8;
        return true;
    }
    return false;
}

uint8_t Tilemap3D::GetCellProps(const HMPoint2D& p) const
{
    if (IsHMPointValid(p) == false) return 0xFF;
    return (heightmap[p.x + p.y * hmwidth] & 0xF000) >> 12;
}

bool Tilemap3D::SetCellProps(const HMPoint2D& p, uint8_t props)
{
    if (IsHMPointValid(p) && props < 0x10)
    {
        int cell = p.x + p.y * hmwidth;
        heightmap[cell] &= 0x0FFF;
        heightmap[cell] |= (props & 0x0F) << 12;
        return true;
    }
    return false;
}

uint8_t Tilemap3D::GetCellType(const HMPoint2D& p) const
{
    if (IsHMPointValid(p) == false) return 0xFF;
    return heightmap[p.x + p.y * hmwidth] & 0x00FF;
}

bool Tilemap3D::SetCellType(const HMPoint2D& p, uint8_t type)
{
    if (IsHMPointValid(p) == true)
    {
        int cell = p.x + p.y * hmwidth;
        heightmap[cell] &= 0xFF00;
        heightmap[cell] |= type;
        return true;
    }
    return false;
}

uint16_t Tilemap3D::GetHeightmapCell(const HMPoint2D& iso) const
{
    if (IsHMPointValid(iso) == true)
    {
        int cell = iso.x + iso.y * hmwidth;
        return heightmap[cell];
    }
    return 0xFFFF;
}

bool Tilemap3D::SetHeightmapCell(const HMPoint2D& iso, uint16_t value)
{
    if (IsHMPointValid(iso) == true)
    {
        int cell = iso.x + iso.y * hmwidth;
        heightmap[cell] = value;
        return true;
    }
    return false;
}

bool Tilemap3D::operator==(const Tilemap3D& rhs) const
{
    return ((this->height == rhs.height) &&
        (this->width == rhs.width) &&
        (this->left == rhs.left) &&
        (this->top == rhs.top) &&
        (this->hmwidth == rhs.hmwidth) &&
        (this->hmheight == rhs.hmheight) &&
        (this->heightmap == rhs.heightmap) &&
        (this->foreground == rhs.foreground) &&
        (this->background == rhs.background));
}

bool Tilemap3D::operator!=(const Tilemap3D& rhs) const
{
    return !(*this == rhs);
}
} // namespace Landstalker
