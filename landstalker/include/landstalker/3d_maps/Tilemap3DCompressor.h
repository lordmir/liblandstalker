#ifndef TILEMAP3DCOMPRESSOR_H
#define TILEMAP3DCOMPRESSOR_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>

namespace Landstalker {

class Tilemap3D;
class BitBarrel;
class BitBarrelWriter;

/**
 * @brief Handles the custom compression scheme used by Landstalker 3D isometric maps.
 * 
 * The Landstalker map compression consists of two distinct parts:
 * 1. Layers (Foreground/Background): Uses a highly customized LZ77 algorithm. It relies on
 *    a 14-entry offset dictionary (6 fixed, 8 dynamic) to encode repetitive spatial patterns, 
 *    and a 2-entry tile dictionary (TD0, TD1) to efficiently encode incrementing sequences of tiles.
 * 2. Heightmap: Uses a standard Run-Length Encoding (RLE) scheme.
 */
class Tilemap3DCompressor
{
public:
    /**
     * @brief Fully decodes a compressed map bitstream into the provided Tilemap3D object.
     * @return The total number of bytes consumed from the source buffer.
     */
    static uint16_t Decode(Tilemap3D& map, const uint8_t* src);

    /**
     * @brief Fully encodes a Tilemap3D object into the destination buffer.
     * Uses a multi-pass heuristic search to find the optimal compression ratio.
     * @return The total number of bytes written to the destination buffer.
     */
    static uint16_t Encode(const Tilemap3D& map, uint8_t* dst, size_t size);

private:
    /** 
     * @brief Represents an LZ77 back-reference or a run of raw tiles.
     * Tracks spatial runs, back-reference dictionary indices, and 2D vertical sequences.
     */
    struct LZ77Entry
    {
        LZ77Entry(int run_length_in, int back_offset_idx_in, int index_in)
            : run_length(run_length_in), back_offset_idx(back_offset_idx_in), index(index_in)
        {}
        int run_length;
        int back_offset_idx;
        int index;
        std::vector<std::pair<bool, int>> vertical_info;
    };

    struct TileEntry
    {
        TileEntry(uint8_t code_in, uint16_t data_in, uint8_t data_length_in)
            : code(code_in), data(data_in), data_length(data_length_in)
        {}
        uint8_t code;
        uint16_t data;
        uint8_t data_length;
    };

        // --- DECODE PIPELINE ---

    /// Orchestrates the decompression of the foreground and background layers.
    static uint16_t DecodeLayers(Tilemap3D& map, const uint8_t* src);
    /// Parses the map dimensions and extracts the tile and offset dictionaries from the bitstream.
    static void ReadMapHeader(Tilemap3D& map, BitBarrel& bb, std::array<uint16_t, 2>& tileDictionary, std::array<uint16_t, 14>& offsetDictionary);
    /// Decompresses the LZ77 structural layout of the map (where tiles are copied from).
    static void DecodeOffsets(const Tilemap3D& map, BitBarrel& bb, const std::array<uint16_t, 14>& offsetDictionary, std::vector<uint16_t>& buffer);
    /// Populates the literal tile values into the map using the tile dictionary and bitstream data.
    static void DecodeTiles(Tilemap3D& map, BitBarrel& bb, const std::array<uint16_t, 2>& tileDictionary, std::vector<uint16_t>& buffer);
    
    /// Decodes the RLE-compressed heightmap.
    static uint16_t DecodeHeightmap(Tilemap3D& map, const uint8_t* src);

        // --- ENCODE PIPELINE ---

    /// Evaluates multiple frequency vs. length weighting ratios to find the best LZ77 compression size.
    static uint16_t EncodeLayersMultiPass(const Tilemap3D& map, uint8_t* dst, size_t size);
    /// Performs a single LZ77 compression pass using specific heuristic weights.
    static uint16_t EncodeLayersSinglePass(const Tilemap3D& map, uint8_t* dst, size_t size, double freq_weight = 1.0, double len_weight = 0.0);
    
    /// Scans the map to find the 8 most frequent spatial offsets to populate the dynamic offset dictionary.
    static void CalculateOffsetDictionary(const std::vector<uint16_t>& tiles, double freq_weight, double len_weight, std::array<uint16_t, 14>& offsets);
    /// Parses the map layers to generate the optimal LZ77 sequence and vertical runs based on the offset dictionary.
    static void EncodeOffsets(const Tilemap3D& map, const std::vector<uint16_t>& tiles, const std::array<uint16_t, 14>& offsets, std::vector<LZ77Entry>& lz77, std::vector<bool>& compressed);
    static void OptimizeVerticalRun(const Tilemap3D& map, std::vector<LZ77Entry>& lz77, LZ77Entry& entry, size_t tiles_size);
    /// Performs an exhaustive search to find the optimal TD0 and TD1 base tiles to minimize literal bit-cost.
    static void CalculateTileDictionary(const std::vector<uint16_t>& tiles, const std::vector<bool>& compressed, std::array<uint16_t, 2>& tile_dict);
    /// Evaluates the uncompressed tiles against the tile dictionary and records the necessary bitwise operations.
    static void EncodeTiles(const std::vector<uint16_t>& tiles, const std::vector<bool>& compressed, const std::array<uint16_t, 2>& tile_dict, std::vector<TileEntry>& tile_entries);
    
    /// Orchestrates the BitBarrelWriter to serialize the header, LZ77 stream, and literal tile entries.
    static uint16_t WriteLayerData(const Tilemap3D& map, const std::array<uint16_t, 14>& offsets, const std::array<uint16_t, 2>& tile_dict, const std::vector<LZ77Entry>& lz77, const std::vector<TileEntry>& tile_entries, size_t tiles_size, uint8_t* dst, size_t size);
    static void WriteLayerHeader(const Tilemap3D& map, const std::array<uint16_t, 14>& offsets, const std::array<uint16_t, 2>& tile_dict, BitBarrelWriter& cmap);
    static void WriteLayerOffsets(const std::vector<LZ77Entry>& lz77, size_t tiles_size, BitBarrelWriter& cmap);
    static void WriteLayerTiles(const std::vector<TileEntry>& tile_entries, BitBarrelWriter& cmap);
    
    /// Encodes the heightmap using Run-Length Encoding (RLE).
    static uint16_t EncodeHeightmap(const Tilemap3D& map, uint8_t* dst, size_t size);
};

} // namespace Landstalker

#endif // TILEMAP3DCOMPRESSOR_H
