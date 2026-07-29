#include <landstalker/sprites/SpriteFrame.h>
#include <vector>
#include <iterator>
#include <algorithm>
#include <landstalker/main/Rom.h>
#include <landstalker/misc/LZ77.h>
#include <landstalker/misc/Utils.h>

namespace Landstalker {

SpriteFrame::SpriteFrame(const std::vector<uint8_t>& src)
	: m_compressed(false)
{
	SetBits(src);
}

SpriteFrame::SpriteFrame(const std::string& filename)
	: m_compressed(false)
{
	Open(filename);
}

SpriteFrame::SpriteFrame()
	: m_sprite_gfx(std::make_shared<Tileset>()),
	  m_compressed(false)
{
}

SpriteFrame::SpriteFrame(const SpriteFrame& rhs)
	: m_subsprites(rhs.m_subsprites),
	  m_sprite_gfx(std::make_shared<Tileset>(*rhs.m_sprite_gfx)),
	  m_compressed(rhs.m_compressed)
{
}

SpriteFrame& SpriteFrame::operator=(const SpriteFrame& rhs)
{
	this->m_compressed = rhs.m_compressed;
	this->m_sprite_gfx = std::make_shared<Tileset>(*rhs.m_sprite_gfx);
	this->m_subsprites = rhs.m_subsprites;
	return *this;
}

bool SpriteFrame::operator==(const SpriteFrame& rhs) const
{
	return ((this->m_subsprites == rhs.m_subsprites) &&
		    (*this->m_sprite_gfx == *rhs.m_sprite_gfx) &&
		    (this->m_compressed == rhs.m_compressed));
}

bool SpriteFrame::operator!=(const SpriteFrame& rhs) const
{
	return !(*this == rhs);
}

bool SpriteFrame::Open(const std::string& filename)
{
	bool retval = false;
	std::streampos filesize;
	std::ifstream ifs(filename, std::ios::binary);

	ifs.unsetf(std::ios::skipws);

	ifs.seekg(0, std::ios::end);
	filesize = ifs.tellg();
	ifs.seekg(0, std::ios::beg);

	std::vector<uint8_t> bytes;
	bytes.reserve(static_cast<std::size_t>(filesize));
	bytes.insert(bytes.begin(),
		std::istream_iterator<uint8_t>(ifs),
		std::istream_iterator<uint8_t>());

	if (bytes.size() > 0)
	{
		Clear();
		SetBits(bytes);
		retval = true;
	}
	return retval;
}

std::vector<uint8_t> SpriteFrame::GetBits()
{
	return GetBits(m_compressed);
}

std::vector<uint8_t> SpriteFrame::GetBits(bool compressed)
{
	std::vector<uint8_t> bits;
	std::size_t expected_tiles = 0;
	std::size_t actual_tiles = m_sprite_gfx->GetTileCount();

	if (m_subsprites.size() > 0)
	{
		for (const auto& s : m_subsprites)
		{
			uint8_t b1, b2;
			b1 = (((s.y < 0) ? (s.y + 0x100) : s.y) >> 1) & 0x7C;
			b1 |= (s.w - 1) & 0x03;
			b2 = (((s.x < 0) ? (s.x + 0x100) : s.x) >> 1) & 0x7C;
			b2 |= (s.h - 1) & 0x03;

			bits.push_back(b1);
			bits.push_back(b2);
			expected_tiles += s.h * s.w;
		}
		bits.back() |= 0x80;
	}

	int last_cmd = static_cast<int>(bits.size());
	if (compressed) // compression
	{
		uint16_t word_count = static_cast<uint16_t>(std::min<std::size_t>(actual_tiles, expected_tiles) * 16);
		bits.push_back(0x20 | word_count >> 8);
		bits.push_back(word_count & 0xFF);
		auto tiles = m_sprite_gfx->GetBits();
		std::vector<uint8_t> buffer(65536);
		buffer.resize(LZ77::Encode(tiles.data(), word_count*2, buffer.data()));
		bits.insert(bits.end(), buffer.begin(), buffer.end());
	}
	else
	{
		// At load time the game issues one VDP DMA per command - LoadSpriteTiles copies each raw
		// run from ROM and each zero-fill run from the Zeros table. The sprite-tile DMA queue only
		// holds 80 ops (g_DMAOpQueue), and the retail frames keep well within that by capping each
		// frame at ~8 commands. Splitting every short blank gap into its own zero-fill (as the old
		// THRESHOLD=4 pass did) fragmented busy frames into 30+ ops, which overflowed the queue in
		// crowded rooms and scribbled sprite data over the tileset/HUD VRAM. So compress within an
		// op budget: prefer to split blank runs out (smaller frame), but when that exceeds the
		// budget, fold the smallest gaps back into copy runs until the frame fits.
		const std::size_t OP_BUDGET = 8;
		uint16_t total_words = static_cast<uint16_t>(std::min<std::size_t>(actual_tiles, expected_tiles) * 16);
		auto tiles = m_sprite_gfx->GetBits();
		auto word_at = [&](uint32_t k) -> uint16_t { return (tiles[k * 2] << 8) | tiles[k * 2 + 1]; };

		// Split the frame into alternating copy (non-blank) and gap (blank) segments.
		struct Segment { bool gap; uint32_t start; uint32_t len; };
		std::vector<Segment> segs;
		for (uint32_t i = 0; i < total_words; )
		{
			bool blank = (word_at(i) == 0);
			uint32_t j = i;
			while (j < total_words && ((word_at(j) == 0) == blank)) { ++j; }
			segs.push_back({ blank, i, j - i });
			i = j;
		}

		// One op per segment. While over budget, fold the smallest gap into a copy run and merge
		// the copies it joins. Segments stay contiguous, so the merged copy reads straight from the
		// tile data - the folded gap's positions are already zero there.
		while (segs.size() > OP_BUDGET)
		{
			std::size_t smallest = segs.size();
			for (std::size_t k = 0; k < segs.size(); ++k)
			{
				if (segs[k].gap && (smallest == segs.size() || segs[k].len < segs[smallest].len))
				{
					smallest = k;
				}
			}
			if (smallest == segs.size())
			{
				break; // no gaps left to absorb
			}
			segs[smallest].gap = false;
			std::vector<Segment> merged;
			for (const auto& s : segs)
			{
				if (!merged.empty() && !merged.back().gap && !s.gap)
				{
					merged.back().len += s.len;
				}
				else
				{
					merged.push_back(s);
				}
			}
			segs = std::move(merged);
		}

		// Emit: copy = 0x00 | len (followed by the tile data), zero-fill = 0x80 | len.
		for (const auto& s : segs)
		{
			last_cmd = static_cast<int>(bits.size());
			if (s.gap)
			{
				bits.push_back(static_cast<uint8_t>(0x80 | (s.len >> 8)));
				bits.push_back(static_cast<uint8_t>(s.len & 0xFF));
			}
			else
			{
				bits.push_back(static_cast<uint8_t>(0x00 | (s.len >> 8)));
				bits.push_back(static_cast<uint8_t>(s.len & 0xFF));
				bits.insert(bits.end(), tiles.begin() + s.start * 2, tiles.begin() + (s.start + s.len) * 2);
			}
		}
	}
	// Fill in padding if required
	if (actual_tiles < expected_tiles)
	{
		last_cmd = static_cast<int>(bits.size());
		uint16_t word_count = static_cast<uint16_t>((expected_tiles - actual_tiles) * 16);
		bits.push_back(0x80 | (word_count >> 8));
		bits.push_back(word_count & 0xFF);
	}

	// Mark final command to stop decompression
	bits.at(last_cmd) |= 0x40;
	return bits;
}

std::size_t SpriteFrame::SetBits(const std::vector<uint8_t>& src)
{
	auto it = src.begin();
	m_subsprites.clear();
	std::size_t tile_idx = 0;
	do
	{
		int y = ((*it & 0x7C) << 1);
		if (y > 0x80)
		{
			y -= 0x100;
		}
		std::size_t w = (*it & 0x03) + 1;
		int x = ((*++it & 0x7C) << 1);
		if (x > 0x80)
		{
			x -= 0x100;
		}
		std::size_t h = (*it & 0x03) + 1;
		m_subsprites.emplace_back(x,y,static_cast<int>(w),static_cast<int>(h),static_cast<int>(tile_idx));
		tile_idx += w * h;
	} while ((*it++ & 0x80) == 0);

	std::vector<uint8_t> sprite_gfx(tile_idx * 32, 0);
	auto dest_it = sprite_gfx.begin();

	uint16_t command;
	uint8_t ctrl = 0;
	uint16_t count;
	do
	{
		command = (*it << 8) | *std::next(it);
		ctrl = command >> 12;
		count = command & 0xFFF;
		it += 2;

		if ((ctrl & 0x08) > 0)
		{
			dest_it += count * 2;
		}
		else if ((ctrl & 0x02) > 0)
		{
			std::size_t elen = 0;
			std::size_t dlen = LZ77::Decode(&(*it), src.end() - it, &(*dest_it), elen);
			dest_it += dlen;
			it += elen;
			m_compressed = true;
		}
		else
		{
			std::copy(it, it + count * 2, dest_it);
			dest_it += count * 2;
			it += count * 2;
		}
	} while ((ctrl & 0x04) == 0);

	m_sprite_gfx = std::make_shared<Tileset>(sprite_gfx);

	return std::distance(src.begin(), it);
}

bool SpriteFrame::Save(const std::string& filename)
{
	return Save(filename, m_compressed);
}

bool SpriteFrame::Save(const std::string& filename, bool compressed)
{
	bool retval = false;
	auto bits = GetBits(compressed);
	std::ofstream ofs(filename, std::ios::out | std::ios::binary);
	if (ofs)
	{
		ofs.write(reinterpret_cast<const char*>(bits.data()), bits.size());
		ofs.close();
		retval = true;
	}
	return retval;
}

void SpriteFrame::Clear()
{
	m_subsprites.clear();
	m_sprite_gfx->Clear();
}

int SpriteFrame::GetLeft() const
{
	if (m_subsprites.size() == 0)
	{
		return 0;
	}
	int left = 0xFFFFFF;
	for (const auto& s : m_subsprites)
	{
		if (s.x < left)
		{
			left = s.x;
		}
	}
	return left;
}

int SpriteFrame::GetRight() const
{
	if (m_subsprites.size() == 0)
	{
		return 0;
	}
	int right = -0xFFFFFF;
	for (const auto& s : m_subsprites)
	{
		int r = s.x + static_cast<int>(s.w * m_sprite_gfx->GetTileWidth());
		if (r > right)
		{
			right = r;
		}
	}
	return right;
}

int SpriteFrame::GetWidth() const
{
	return GetRight() - GetLeft();
}

int SpriteFrame::GetTop() const
{
	if (m_subsprites.size() == 0)
	{
		return 0;
	}
	int top = 0xFFFFFF;
	for (const auto& s : m_subsprites)
	{
		if (s.y < top)
		{
			top = s.y;
		}
	}
	return top;
}

int SpriteFrame::GetBottom() const
{
	if (m_subsprites.size() == 0)
	{
		return 0;
	}
	int bottom = -0xFFFFFF;
	for (const auto& s : m_subsprites)
	{
		int b = s.y + static_cast<int>(s.h * m_sprite_gfx->GetTileHeight());
		if (b > bottom)
		{
			bottom = b;
		}
	}
	return bottom;
}

int SpriteFrame::GetHeight() const
{
	return GetBottom() - GetTop();
}

Rect SpriteFrame::GetBoundingBox() const
{
    return  {GetLeft(), GetTop(), GetWidth(), GetHeight()};
}

Point SpriteFrame::GetOrigin() const
{
	return Point(-GetLeft(), -GetTop());
}

std::vector<uint8_t> SpriteFrame::GetTile(const Tile& tile) const
{
	return m_sprite_gfx->GetTile(tile);
}

std::pair<int, int> SpriteFrame::GetTilePosition(const Tile& tile) const
{
	int x = 0; int y = 0;

	auto it = m_subsprites.cbegin();
	for (; it != m_subsprites.cend(); ++it)
	{
		if ((tile.GetIndex() >= it->tile_idx) &&
		    (tile.GetIndex()) < (it->tile_idx + (it->w * it->h)))
		{
			int p = static_cast<int>(tile.GetIndex() - it->tile_idx);
			x = it->x + 8 * (p / static_cast<int>(it->h));
			y = it->y + 8 * (p % static_cast<int>(it->h));
			break;
		}
	}

	return std::pair<int, int>(x, y);
}

std::vector<uint8_t>& SpriteFrame::GetTilePixels(int tile_index)
{
	return m_sprite_gfx->GetTilePixels(tile_index);
}

std::shared_ptr<const Tileset> SpriteFrame::GetTileset() const
{
	return m_sprite_gfx;
}

std::shared_ptr<Tileset> SpriteFrame::GetTileset()
{
	return m_sprite_gfx;
}

std::size_t SpriteFrame::GetTileCount() const
{
	return m_sprite_gfx->GetTileCount();
}

std::size_t SpriteFrame::GetExpectedTileCount() const
{
	std::size_t c = 0;
	for (const auto& s : m_subsprites)
	{
		c += s.h * s.w;
	}
	return c;
}

std::size_t SpriteFrame::GetSubSpriteCount() const
{
	return m_subsprites.size();
}

std::size_t SpriteFrame::GetTileWidth() const
{
	return m_sprite_gfx->GetTileWidth();
}

std::size_t SpriteFrame::GetTileHeight() const
{
	return m_sprite_gfx->GetTileHeight();
}

std::size_t SpriteFrame::GetTileBitDepth() const
{
	return m_sprite_gfx->GetTileBitDepth();
}

bool SpriteFrame::GetCompressed() const
{
	return m_compressed;
}

void SpriteFrame::SetCompressed(bool compressed)
{
	m_compressed = compressed;
}

SpriteFrame::SubSprite& SpriteFrame::GetSubSprite(std::size_t idx)
{
	return m_subsprites[idx];
}

SpriteFrame::SubSprite SpriteFrame::GetSubSprite(std::size_t idx) const
{
	return m_subsprites[idx];
}

std::vector<SpriteFrame::SubSprite> SpriteFrame::GetSubSprites() const
{
	return m_subsprites;
}

SpriteFrame::SubSprite& SpriteFrame::AddSubSpriteBefore(std::size_t idx)
{
	SubSprite ss;
	if (m_subsprites.size() >= MAX_SUBSPRITES)
	{
		return m_subsprites.front();
	}
	if (idx >= m_subsprites.size())
	{
		idx = 0;
	}
	if (!m_subsprites.empty())
	{
		ss = m_subsprites.front();
		ss.w = 1;
		ss.h = 1;
		while (ss.x < 248 && ss.y < 248)
		{
			if (std::none_of(m_subsprites.cbegin(), m_subsprites.cend(), [&](const auto& cs) {
				return cs.Collides(ss);
				}))
			{
				break;
			}
			ss.x += SubSprite::TILE_WIDTH;
			if (std::none_of(m_subsprites.cbegin(), m_subsprites.cend(), [&](const auto& cs) {
				return cs.Collides(ss);
				}))
			{
				break;
			}
			ss.y += SubSprite::TILE_HEIGHT;
		}
	}

	return *m_subsprites.insert(m_subsprites.begin() + idx, 1, ss);
}

void SpriteFrame::DeleteSubSprite(std::size_t idx)
{
	m_subsprites.erase(m_subsprites.begin() + idx);
}

void SpriteFrame::SwapSubSprite(std::size_t src1, std::size_t src2)
{
	std::swap(m_subsprites[src1], m_subsprites[src2]);	
}

void SpriteFrame::SetSubSprites(const std::vector<SubSprite>& subs)
{
	m_subsprites = subs;
	PrepareSubSprites();
}

void SpriteFrame::PrepareSubSprites()
{
	int c = 0;
	for (auto& s : m_subsprites)
	{
		s.tile_idx = c;
		c += static_cast<int>(s.w * s.h);
	}
	m_sprite_gfx->Resize(c);
}

bool SpriteFrame::SubSprite::Collides(const SubSprite& rhs) const
{
	if (this == &rhs)
	{
		return false;
	}
	return this->x < static_cast<int>(rhs.x + rhs.w * TILE_WIDTH) &&
		static_cast<int>(this->x + this->w * TILE_WIDTH) > rhs.x &&
		this->y < static_cast<int>(rhs.y + rhs.h * TILE_HEIGHT) &&
		static_cast<int>(this->y + this->h * TILE_HEIGHT) > rhs.y;
}

} // namespace Landstalker
