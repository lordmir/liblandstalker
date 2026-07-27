#ifndef _END_CREDIT_FONT_H_
#define _END_CREDIT_FONT_H_

#include <landstalker/tileset/Tileset.h>

namespace Landstalker
{

// The end credit font is not stored as a regular tileset. Each glyph is a variable width run of
// 16 pixel tall, 2bpp *columns*, prefixed by a single byte holding the glyph width in pixels:
//
//     [width] [column 0 (4 bytes)] [column 1 (4 bytes)] ... [column width-1 (4 bytes)]
//
// This class unpacks that layout into a conventional row-major tileset of fixed size
// GLYPH_WIDTH x GLYPH_HEIGHT tiles so that glyphs can be edited with the regular tileset tools,
// remembering the width of each glyph so that the original format can be reconstructed.
class EndCreditFont : public Tileset
{
public:
    // Maximum glyph width in pixels. The width is stored as a byte, but the tile is fixed size, so
    // this is what limits it in practice. The retail fonts top out at 16 pixels wide.
    static constexpr std::size_t GLYPH_WIDTH = 32;
    static constexpr std::size_t GLYPH_HEIGHT = 16;
    static constexpr std::size_t GLYPH_BPP = 2;
    // A zero width byte terminates the glyph list in the game's rendering code.
    static constexpr uint8_t MIN_GLYPH_WIDTH = 1;

    EndCreditFont();
    EndCreditFont(const std::vector<uint8_t>& src, bool compressed = true);
    // Adopts the pixels of an existing tileset. The glyph widths are not recoverable from those
    // alone, so they default to the width actually occupied by each glyph - see GetGlyphWidth().
    explicit EndCreditFont(const Tileset& tileset);
    virtual ~EndCreditFont();

    bool operator==(const EndCreditFont& rhs) const;
    bool operator!=(const EndCreditFont& rhs) const;

    // Returns the number of bytes consumed from src.
    uint32_t SetBits(const std::vector<uint8_t>& src, bool compressed = true);
    bool Open(const std::string& filename, bool compressed = true);
    std::vector<uint8_t> GetBits(bool compressed = true);
    bool Save(const std::string& filename, bool compressed = true);

    // The advance width of a glyph, in pixels. This is the greater of the width recorded in the
    // font data and the width actually occupied by the glyph's pixels, so that widening a glyph in
    // an editor does not clip it. Trailing blank columns present in the original data are kept.
    uint8_t GetGlyphWidth(std::size_t glyph_index) const;
    void SetGlyphWidth(std::size_t glyph_index, uint8_t width);

    // As above, for callers holding the pixels and the recorded widths separately.
    static uint8_t ResolveGlyphWidth(const Tileset& tileset, std::size_t glyph_index, uint8_t recorded_width);

    // The recorded widths, for callers that need to carry them alongside the pixels.
    const std::vector<uint8_t>& GetGlyphWidths() const;
    void SetGlyphWidths(const std::vector<uint8_t>& widths);

private:
    // Column-major game format <-> row-major tileset bits.
    std::vector<uint8_t> Unpack(const std::vector<uint8_t>& packed);
    std::vector<uint8_t> Pack(const std::vector<uint8_t>& bits) const;

    // Width in pixels of each glyph, as recorded in the font data.
    std::vector<uint8_t> m_widths;
};

} // namespace Landstalker

#endif // _END_CREDIT_FONT_H_
