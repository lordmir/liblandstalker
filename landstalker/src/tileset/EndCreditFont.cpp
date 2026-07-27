#include <landstalker/tileset/EndCreditFont.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <landstalker/misc/Utils.h>
#include <landstalker/misc/LZ77.h>

namespace Landstalker
{

namespace
{
    constexpr std::size_t GLYPH_WIDTH = EndCreditFont::GLYPH_WIDTH;
    constexpr std::size_t GLYPH_HEIGHT = EndCreditFont::GLYPH_HEIGHT;
    constexpr std::size_t GLYPH_BPP = EndCreditFont::GLYPH_BPP;
    constexpr std::size_t GLYPH_PPB = 8 / GLYPH_BPP;
    constexpr uint8_t PIXEL_MASK = static_cast<uint8_t>((1 << GLYPH_BPP) - 1);

    // A column in the game format holds GLYPH_HEIGHT pixels, topmost pixel in the most significant bits.
    constexpr std::size_t COLUMN_SIZE_BYTES = GLYPH_HEIGHT / GLYPH_PPB;
    // A row in the unpacked tile holds GLYPH_WIDTH pixels, leftmost pixel in the most significant bits.
    constexpr std::size_t ROW_SIZE_BYTES = GLYPH_WIDTH / GLYPH_PPB;
    constexpr std::size_t GLYPH_SIZE_BYTES = GLYPH_HEIGHT * ROW_SIZE_BYTES;

    constexpr std::size_t BUFFER_SIZE = 65536;
    constexpr uint8_t MIN_GLYPH_WIDTH = EndCreditFont::MIN_GLYPH_WIDTH;

    // Both the packed columns and the unpacked rows are runs of pixels packed most significant bits
    // first, so the same pair of helpers serves for either.
    uint8_t GetPixel(const std::vector<uint8_t>& src, std::size_t offset, std::size_t index)
    {
        const std::size_t shift = (GLYPH_PPB - 1 - index % GLYPH_PPB) * GLYPH_BPP;
        return static_cast<uint8_t>((src[offset + index / GLYPH_PPB] >> shift) & PIXEL_MASK);
    }

    void SetPixel(std::vector<uint8_t>& dst, std::size_t offset, std::size_t index, uint8_t value)
    {
        const std::size_t shift = (GLYPH_PPB - 1 - index % GLYPH_PPB) * GLYPH_BPP;
        dst[offset + index / GLYPH_PPB] |= static_cast<uint8_t>((value & PIXEL_MASK) << shift);
    }
}

EndCreditFont::EndCreditFont()
    : Tileset(GLYPH_WIDTH, GLYPH_HEIGHT, GLYPH_BPP)
{
}

EndCreditFont::EndCreditFont(const std::vector<uint8_t>& src, bool compressed)
    : Tileset(GLYPH_WIDTH, GLYPH_HEIGHT, GLYPH_BPP)
{
    SetBits(src, compressed);
}

EndCreditFont::EndCreditFont(const Tileset& tileset)
    : Tileset(tileset)
{
}

EndCreditFont::~EndCreditFont()
{
}

bool EndCreditFont::operator==(const EndCreditFont& rhs) const
{
    return (m_widths == rhs.m_widths) && Tileset::operator==(rhs);
}

bool EndCreditFont::operator!=(const EndCreditFont& rhs) const
{
    return !(*this == rhs);
}

uint32_t EndCreditFont::SetBits(const std::vector<uint8_t>& src, bool compressed)
{
    uint32_t consumed = static_cast<uint32_t>(src.size());
    std::vector<uint8_t> packed;

    if (compressed)
    {
        packed.resize(BUFFER_SIZE);
        std::size_t elen = 0;
        const std::size_t dlen = LZ77::Decode(src.data(), src.size(), packed.data(), elen);
        packed.resize(dlen);
        consumed = static_cast<uint32_t>(elen);
    }
    else
    {
        packed = src;
    }

    Tileset::SetBits(Unpack(packed));
    return consumed;
}

bool EndCreditFont::Open(const std::string& filename, bool compressed)
{
    try
    {
        SetBits(ReadBytes(filename), compressed);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::vector<uint8_t> EndCreditFont::GetBits(bool compressed)
{
    auto packed = Pack(Tileset::GetBits());
    if (compressed == false)
    {
        return packed;
    }

    std::vector<uint8_t> buffer(BUFFER_SIZE);
    buffer.resize(LZ77::Encode(packed.data(), packed.size(), buffer.data()));
    return buffer;
}

bool EndCreditFont::Save(const std::string& filename, bool compressed)
{
    try
    {
        WriteBytes(GetBits(compressed), filename);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

uint8_t EndCreditFont::ResolveGlyphWidth(const Tileset& tileset, std::size_t glyph_index, uint8_t recorded_width)
{
    if (recorded_width != 0)
    {
        // A recorded width is authoritative. The tile editor clips its canvas to it, so pixels
        // beyond it cannot be reached, and Pack() does not read them.
        return std::max(recorded_width, MIN_GLYPH_WIDTH);
    }

    // No width on record - a tile appended in the editor. Fall back to what its pixels occupy.
    uint8_t drawn = 0;
    if (glyph_index < tileset.GetTileCount())
    {
        const std::size_t width = std::min(tileset.GetTileWidth(), GLYPH_WIDTH);
        const auto pixels = tileset.GetTile(Tile(static_cast<uint16_t>(glyph_index)));
        for (std::size_t row = 0; row < tileset.GetTileHeight(); ++row)
        {
            for (std::size_t col = width; col > drawn; --col)
            {
                if (pixels[row * tileset.GetTileWidth() + col - 1] != 0)
                {
                    drawn = static_cast<uint8_t>(col);
                    break;
                }
            }
        }
    }

    return std::max(drawn, MIN_GLYPH_WIDTH);
}

uint8_t EndCreditFont::GetGlyphWidth(std::size_t glyph_index) const
{
    return ResolveGlyphWidth(*this, glyph_index, (glyph_index < m_widths.size()) ? m_widths[glyph_index] : 0);
}

void EndCreditFont::SetGlyphWidth(std::size_t glyph_index, uint8_t width)
{
    if (glyph_index >= m_widths.size())
    {
        // Zero marks a glyph with no recorded width, which falls back to measuring its pixels.
        m_widths.resize(glyph_index + 1, 0);
    }
    m_widths[glyph_index] = std::clamp(width, MIN_GLYPH_WIDTH, static_cast<uint8_t>(GLYPH_WIDTH));
}

const std::vector<uint8_t>& EndCreditFont::GetGlyphWidths() const
{
    return m_widths;
}

void EndCreditFont::SetGlyphWidths(const std::vector<uint8_t>& widths)
{
    m_widths = widths;
}

std::vector<uint8_t> EndCreditFont::Unpack(const std::vector<uint8_t>& packed)
{
    std::vector<uint8_t> bits;
    // The smallest possible glyph is a width byte plus a single column.
    bits.reserve(packed.size() / (1 + COLUMN_SIZE_BYTES) * GLYPH_SIZE_BYTES);
    m_widths.clear();

    std::size_t i = 0;
    while (i < packed.size())
    {
        const std::size_t width = packed[i++];
        if (width == 0)
        {
            // Terminator - the game stops walking the glyph list at a zero width.
            break;
        }
        if (width > GLYPH_WIDTH)
        {
            throw std::runtime_error("Glyph " + std::to_string(m_widths.size()) +
                " width larger than expected: " + std::to_string(width));
        }
        if (i + width * COLUMN_SIZE_BYTES > packed.size())
        {
            throw std::runtime_error("Unexpected end of data in glyph " + std::to_string(m_widths.size()));
        }

        std::vector<uint8_t> glyph(GLYPH_SIZE_BYTES, 0);
        for (std::size_t col = 0; col < width; ++col)
        {
            for (std::size_t row = 0; row < GLYPH_HEIGHT; ++row)
            {
                SetPixel(glyph, row * ROW_SIZE_BYTES, col, GetPixel(packed, i + col * COLUMN_SIZE_BYTES, row));
            }
        }
        i += width * COLUMN_SIZE_BYTES;

        m_widths.push_back(static_cast<uint8_t>(width));
        bits.insert(bits.end(), glyph.cbegin(), glyph.cend());
    }

    return bits;
}

std::vector<uint8_t> EndCreditFont::Pack(const std::vector<uint8_t>& bits) const
{
    const std::size_t glyph_count = bits.size() / GLYPH_SIZE_BYTES;
    std::vector<uint8_t> packed;
    packed.reserve(bits.size() + glyph_count);

    for (std::size_t glyph_index = 0; glyph_index < glyph_count; ++glyph_index)
    {
        const std::size_t offset = glyph_index * GLYPH_SIZE_BYTES;
        const std::size_t width = GetGlyphWidth(glyph_index);

        packed.push_back(static_cast<uint8_t>(width));
        std::vector<uint8_t> columns(width * COLUMN_SIZE_BYTES, 0);
        for (std::size_t col = 0; col < width; ++col)
        {
            for (std::size_t row = 0; row < GLYPH_HEIGHT; ++row)
            {
                SetPixel(columns, col * COLUMN_SIZE_BYTES, row, GetPixel(bits, offset + row * ROW_SIZE_BYTES, col));
            }
        }
        packed.insert(packed.end(), columns.cbegin(), columns.cend());
    }

    return packed;
}

} // namespace Landstalker
