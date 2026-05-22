#include <landstalker/3d_maps/Tilemap3DCompressor.h>
#include <landstalker/3d_maps/Tilemap3D.h>

#include <future>
#include <mutex>
#include <chrono>
#include <stdexcept>
#include <unordered_map>
#include <set>
#include <map>
#include <functional>
#include <algorithm>
#include <iostream>

#include <landstalker/misc/BitBarrel.h>
#include <landstalker/misc/BitBarrelWriter.h>
#include <landstalker/misc/Literals.h>

namespace {
static uint16_t getCodedNumber(Landstalker::BitBarrel& bb)
{
    uint16_t exp = 0, num = 0;
    
    while(!bb.getNextBit())
    {
        exp++;
    }
    
    if(exp)
    {
        num = 1 << exp;
        num += static_cast<uint16_t>(bb.readBits(exp));
    }
    
    return num;
}

static uint16_t ilog2(uint16_t num)
{
    uint16_t ret = 0;
    while(num)
    {
        num >>= 1;
        ret++;
    }
    return ret;
}

static void makeCodedNumber(uint16_t value, Landstalker::BitBarrelWriter& bb)
{
    uint16_t exp = ilog2(value) - 1;
    uint16_t i = exp;
    uint16_t num = value - (1 << exp);

    while (i--)
    {
        bb.WriteBits(0, 1);
    }
    bb.WriteBits(1, 1);

    if (exp)
    {
        bb.WriteBits(num, exp);
    }
}

static int findMatchFrequency(const std::vector<uint16_t>& input, size_t offset, std::unordered_map<int, int>& fc, std::unordered_map<int, int>& lc, std::unordered_map<int, int>& vc, const std::vector<uint16_t>& fixed_offsets, int map_width)
{
    size_t lookback_size = std::min<size_t>(offset, 4095);
    size_t lookahead_size = input.size() - offset;
    
    int best_fixed = 0;
    for (uint16_t b : fixed_offsets) {
        if (b == 0 || b > lookback_size) continue;
        int match_run = 0;
        for (size_t m = 0; m < lookahead_size; ++m) {
            if (input[offset - b + m] != input[offset + m]) break;
            match_run++;
        }
        if (match_run > best_fixed) best_fixed = match_run;
    }

    int best_dyn = 0;
    size_t best_b = 0;
    int best_dyn_vert = 0;

    for (size_t b = 1; b <= lookback_size; ++b)
    {
        if (std::find(fixed_offsets.begin(), fixed_offsets.end(), b) != fixed_offsets.end()) continue;

        int match_run = 0;
        for (size_t m = 0; m < lookahead_size; ++m)
        {
            if (input[offset - b + m] != input[offset + m]) break;
            match_run++;
        }
        
        if (match_run > 0)
        {
            int vertical_run = 0;
            size_t next_offset = offset;
            bool right = false;
            while (true)
            {
                next_offset += map_width + (right ? 1 : 0);
                if (next_offset + match_run <= input.size() && next_offset >= b)
                {
                    bool matches = true;
                    for (int m = 0; m < match_run; ++m) {
                        if (input[next_offset - b + m] != input[next_offset + m]) {
                            matches = false;
                            break;
                        }
                    }
                    if (matches) {
                        vertical_run++;
                        right = !right;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }

            if (match_run > best_dyn || (match_run == best_dyn && vertical_run > best_dyn_vert))
            {
                best_dyn = match_run;
                best_b = b;
                best_dyn_vert = vertical_run;
            }
        }
    }
    
    int best_overall = std::max(best_fixed, best_dyn);
    if (best_dyn > best_fixed && best_dyn >= 2)
    {
        fc[best_b]++;
        lc[best_b] += best_dyn;
        vc[best_b] += best_dyn * best_dyn_vert;
    }
    
    return best_overall;
}

static std::pair<int, int> findMatch(const std::vector<uint16_t>& input, size_t offset, const std::vector<uint16_t>& back_offsets)
{
    size_t lookback_size = std::min<size_t>(offset, 4095);
    size_t lookahead_size = input.size() - offset;
    std::pair<int, int> ret = { 0,0 };
    for (size_t i = 0; i < back_offsets.size(); ++i)
    {
        size_t b = back_offsets[i];
        if ((b == 0) || (b > lookback_size)) continue;
        int match_run = 0;
        for (size_t m = 0; m < lookahead_size; ++m)
        {
            if (input[offset - b + m] != input[offset + m])
            {
                break;
            }
            match_run++;
        }
        if (match_run > ret.second)
        {
            ret.second = match_run;
            ret.first = i;
        }
    }
    if (ret.second == 0)
    {
        ret.first = 0;
        ret.second = 1;
    }

    return ret;
}
}

namespace Landstalker {

uint16_t Tilemap3DCompressor::Decode(Tilemap3D& map, const uint8_t* src)
{
    uint16_t layers_size = DecodeLayers(map, src);
    uint16_t hm_size = DecodeHeightmap(map, src + layers_size);
    return layers_size + hm_size;
}

uint16_t Tilemap3DCompressor::Encode(const Tilemap3D& map, uint8_t* dst, size_t size)
{
    uint16_t layers_size = EncodeLayersMultiPass(map, dst, size);
    uint16_t hm_size = EncodeHeightmap(map, dst + layers_size, size - layers_size);
    return layers_size + hm_size;
}

uint16_t Tilemap3DCompressor::DecodeLayers(Tilemap3D& map, const uint8_t* src)
{
    BitBarrel bb(src);
    map.foreground.clear();
    map.background.clear();
    map.heightmap.clear();
    
    std::array<uint16_t, 2> tileDictionary = {0, 0};
    std::array<uint16_t, 14> offsetDictionary = {0};
    
    ReadMapHeader(map, bb, tileDictionary, offsetDictionary);

    const uint16_t t = map.GetSize() * 2;
    std::vector<uint16_t> buffer(t, 0);

    DecodeOffsets(map, bb, offsetDictionary, buffer);
    DecodeTiles(map, bb, tileDictionary, buffer);
    
    bb.advanceNextByte();
    return static_cast<uint16_t>(bb.getBytePosition());
}

void Tilemap3DCompressor::ReadMapHeader(Tilemap3D& map, BitBarrel& bb, std::array<uint16_t, 2>& tileDictionary, std::array<uint16_t, 14>& offsetDictionary)
{
    map.left   = static_cast<uint8_t>(bb.readBits(8));
    map.top    = static_cast<uint8_t>(bb.readBits(8));
    map.width  = static_cast<uint8_t>(bb.readBits(8) + 1);
    map.height = static_cast<uint8_t>((bb.readBits(8) + 1) / 2);

    offsetDictionary[0] = 0xFFFF;
    offsetDictionary[1] = 1;
    offsetDictionary[2] = 2;
    offsetDictionary[3] = static_cast<uint16_t>(map.GetWidth());
    offsetDictionary[4] = static_cast<uint16_t>(map.GetWidth() * 2u);
    offsetDictionary[5] = static_cast<uint16_t>(map.GetWidth() + 1);
    
    tileDictionary[1] = static_cast<uint16_t>(bb.readBits(10));
    tileDictionary[0] = static_cast<uint16_t>(bb.readBits(10));
    
    for(size_t i = 6; i < 14; ++i)
    {
        offsetDictionary[i] = static_cast<uint16_t>(bb.readBits(12));
    }
}

void Tilemap3DCompressor::DecodeOffsets(const Tilemap3D& map, BitBarrel& bb, const std::array<uint16_t, 14>& offsetDictionary, std::vector<uint16_t>& buffer)
{
    const uint16_t t = map.GetSize() * 2;
    int16_t dst_addr = -1;
    
    while(true)
    {
        uint16_t start = getCodedNumber(bb);
        
        if(!start) start++;
        dst_addr += start;
        
        if(dst_addr >= t)
        {
            break;
        }
        
        uint8_t command = static_cast<uint8_t>(bb.readBits(3));
        if(command > 5)
        {
            command = static_cast<uint8_t>(6 + (((command & 1) << 2) | bb.readBits(2)));
        }
        buffer[dst_addr] = offsetDictionary[command];
        
        if(bb.getNextBit())
        {
            uint16_t row_addr = dst_addr;
            bool width_offset = bb.getNextBit();
            do
            {
                do
                {
                    row_addr += map.GetWidth() + (width_offset ? 1 : 0);
                    buffer[row_addr] = offsetDictionary[command];
                } while(bb.getNextBit());
                width_offset = !width_offset;
            } while(bb.getNextBit());
        }
    }
}

void Tilemap3DCompressor::DecodeTiles(Tilemap3D& map, BitBarrel& bb, const std::array<uint16_t, 2>& tileDictionary, std::vector<uint16_t>& buffer)
{
    const uint16_t t = map.GetSize() * 2;
    uint16_t tiles[2] = {tileDictionary[0], tileDictionary[1]};
    int16_t dst_addr = 0;
    do
    {
        uint16_t operand = buffer[dst_addr];
        uint16_t offset;
        if(operand != 0xFFFF)
        {
            offset = dst_addr - operand;
            do
            {
                buffer[dst_addr++] = buffer[offset++];
            } while ((dst_addr < t) && (buffer[dst_addr] == 0));
        }
        else
        {
            do
            {
                operand = static_cast<uint8_t>(bb.readBits(2));
                uint16_t value = 0;
                switch(operand)
                {
                    case 0:
                        if(tiles[0])
                        {
                            value = static_cast<uint16_t>(bb.readBits(ilog2(tiles[0])));
                        }
                        buffer[dst_addr++] = value;
                        break;
                    case 1:
                        if(tiles[1] != tileDictionary[1])
                        {
                            value = static_cast<uint16_t>(bb.readBits(ilog2(tiles[1] - tileDictionary[1])));
                        }
                        value += tileDictionary[1];
                        buffer[dst_addr++] = value;
                        break;
                    case 2:
                        value = tiles[0]++;
                        buffer[dst_addr++] = value;
                        break;
                    case 3:
                        value = tiles[1]++;
                        buffer[dst_addr++] = value;
                        break;
                }
            } while ((dst_addr < t) && (buffer[dst_addr] == 0));
        }
    } while(dst_addr < t);
    
    map.background.resize(map.GetSize());
    map.foreground.resize(map.GetSize());
    std::copy(buffer.begin() + t/2, buffer.end(), map.background.begin());
    std::copy(buffer.begin(), buffer.begin() + t / 2, map.foreground.begin());
}

uint16_t Tilemap3DCompressor::DecodeHeightmap(Tilemap3D& map, const uint8_t* src)
{
    BitBarrel bb(src);
    map.hmwidth = static_cast<uint8_t>(bb.readBits(8));
    map.hmheight = static_cast<uint8_t>(bb.readBits(8));
    
    uint16_t hm_pattern = 0;
    uint16_t hm_rle_count = 0;
    
    map.heightmap.assign(map.GetHeightmapSize(), 0);
    size_t dst_addr = 0;
    for(size_t y = 0; y < map.GetHeightmapHeight(); y++)
    {
        for(size_t x = 0; x < map.GetHeightmapWidth(); x++)
        {
            if(!hm_rle_count--)
            {
                uint8_t read_count = 0;
                hm_rle_count = 0;
                hm_pattern = static_cast<uint16_t>(bb.readBits(16));
                do
                {
                    read_count = static_cast<uint8_t>(bb.readBits(8));
                    hm_rle_count += read_count;
                } while(read_count == 0xFF);
            }
            map.heightmap[dst_addr++] = hm_pattern;
        }
    }
    bb.advanceNextByte();
    return static_cast<uint16_t>(bb.getBytePosition());
}

uint16_t Tilemap3DCompressor::EncodeLayersMultiPass(const Tilemap3D& map, uint8_t* dst, size_t size)
{
    uint16_t best_recompressed_size = 0xFFFF;
    std::vector<uint8_t> best_recompressed_data;
    std::mutex mtx;
    std::vector<std::future<void>> futures;

    auto evaluate_ratio = [&](double freq_weight, double len_weight, double vert_weight) {
        std::vector<uint8_t> recompressed(size, 0);
        
        uint16_t recompressed_size = 0xFFFF;
        try {
            recompressed_size = EncodeLayersSinglePass(map, recompressed.data(), size, freq_weight, len_weight, vert_weight);
        } catch (...) {
            return;
        }

        std::lock_guard<std::mutex> lock(mtx);
        if (recompressed_size < best_recompressed_size) {
            best_recompressed_size = recompressed_size;
            best_recompressed_data.assign(recompressed.begin(), recompressed.begin() + recompressed_size);
        }
    };

    std::vector<double> base_ratios = {1.0, 0.95, 0.90, 0.85, 0.50};
    std::vector<double> vert_ratios = {0.0, 0.25, 0.5, 1.0, 2.0};
    
    for (double r : base_ratios) {
        for (double v : vert_ratios) {
            futures.push_back(std::async(std::launch::async, evaluate_ratio, r, 1.0 - r, v));
        }
    }
    
    for (auto& f : futures) {
        f.get();
    }

    if (best_recompressed_data.size() <= size && best_recompressed_size != 0xFFFF) {
        std::copy(best_recompressed_data.begin(), best_recompressed_data.end(), dst);
        return best_recompressed_size;
    } else {
        throw std::runtime_error("Output buffer not large enough to hold result.");
    }
}

uint16_t Tilemap3DCompressor::EncodeLayersSinglePass(const Tilemap3D& map, uint8_t* dst, size_t size, double freq_weight, double len_weight, double vert_weight)
{
    std::array<uint16_t, 14> offsets = {0};
    offsets[0] = 0;
    offsets[1] = 1;
    offsets[2] = 2;
    offsets[3] = static_cast<uint16_t>(map.GetWidth());
    offsets[4] = static_cast<uint16_t>(map.GetWidth() * 2);
    offsets[5] = static_cast<uint16_t>(map.GetWidth() + 1);

    std::vector<uint16_t> tiles(map.GetSize() * 2);
    std::copy(map.foreground.begin(), map.foreground.end(), tiles.begin());
    std::copy(map.background.begin(), map.background.end(), tiles.begin() + map.GetSize());
    
    std::vector<bool> compressed(tiles.size(), false);
    std::vector<LZ77Entry> lz77;
    std::array<uint16_t, 2> tile_dict = { 0 };
    std::vector<TileEntry> tile_entries;

    CalculateOffsetDictionary(map, tiles, freq_weight, len_weight, vert_weight, offsets);
    EncodeOffsets(map, tiles, offsets, lz77, compressed);
    CalculateTileDictionary(tiles, compressed, tile_dict);
    EncodeTiles(tiles, compressed, tile_dict, tile_entries);

    return WriteLayerData(map, offsets, tile_dict, lz77, tile_entries, tiles.size(), dst, size);
}

void Tilemap3DCompressor::CalculateOffsetDictionary(const Tilemap3D& map, const std::vector<uint16_t>& tiles, double freq_weight, double len_weight, double vert_weight, std::array<uint16_t, 14>& offsets)
{
    std::unordered_map<int, int> offset_freq_count;
    std::unordered_map<int, int> offset_length_count;
    std::unordered_map<int, int> offset_vertical_count;
    size_t idx = 1;
    
    std::vector<uint16_t> fixed_offsets(offsets.begin(), offsets.begin() + 6);
    
    do
    {
        int run = findMatchFrequency(tiles, idx, offset_freq_count, offset_length_count, offset_vertical_count, fixed_offsets, map.GetWidth());
        if (run == 0)
        {
            idx++;
        }
        else
        {
            idx += run;
        }
    } while (idx < tiles.size());

    std::unordered_map<int, double> offset_score;
    double max_freq = 1.0;
    double max_len = 1.0;
    double max_vert = 1.0;
    
    for (auto const& [key, val] : offset_freq_count) {
        if (val > max_freq) max_freq = val;
        if (offset_length_count[key] > max_len) max_len = offset_length_count[key];
        if (offset_vertical_count[key] > max_vert) max_vert = offset_vertical_count[key];
    }
    
    for (auto const& [key, val] : offset_freq_count) {
        double norm_freq = (double)val / max_freq;
        double norm_len = (double)offset_length_count[key] / max_len;
        double norm_vert = (double)offset_vertical_count[key] / max_vert;
        offset_score[key] = (norm_freq * freq_weight) + (norm_len * len_weight) + (norm_vert * vert_weight);
    }

    typedef std::function<bool(const std::pair<int, int>&, const std::pair<int, int>&)> Comparator;
    Comparator comparator = [&](const std::pair<int, int>& p1, const std::pair<int, int>& p2)
    {
        if (offset_score[p1.first] != offset_score[p2.first])
        {
            return offset_score[p1.first] > offset_score[p2.first];
        }
        else
        {
            return p1.first > p2.first;
        }
    };

    std::multiset<std::pair<int, int>, Comparator> frequency_counts(offset_freq_count.begin(), offset_freq_count.end(), comparator);
    size_t offset_idx = 6;
    for (auto it = frequency_counts.cbegin(); it != frequency_counts.cend(); ++it)
    {
        if (std::find(offsets.begin(), offsets.begin() + offset_idx, it->first) == offsets.begin() + offset_idx)
        {
            offsets[offset_idx++] = static_cast<uint16_t>(it->first);
            if(offset_idx >= 14) break;
        }
    }
}

void Tilemap3DCompressor::OptimizeVerticalRun(const Tilemap3D& map, std::vector<LZ77Entry>& lz77, LZ77Entry& entry, size_t tiles_size)
{
    size_t count = 0;
    bool right = false;
    bool begin = true;
    if (entry.back_offset_idx != -1)
    {
        size_t next = entry.index;
        size_t prev = next;
        
        auto it = lz77.begin() + (&entry - lz77.data());
        
        while (next < tiles_size)
        {
            next += map.GetWidth() + (right ? 1 : 0);
            auto nit = std::find_if(it, lz77.end(), [&](const LZ77Entry& comp)
                {
                    return (comp.index == static_cast<int>(next)) && (comp.back_offset_idx == entry.back_offset_idx);
                });
            if (nit != lz77.end())
            {
                count++;
                nit->back_offset_idx = -1;
                prev = next;
            }
            else
            {
                if (count > 0)
                {
                    entry.vertical_info.emplace_back(right, static_cast<int>(count));
                    count = 0;
                }
                else
                {
                    if (begin == false)
                    {
                        break;
                    }
                }
                begin = false;
                right = !right;
                next = prev;
            }
        }
    }
}

void Tilemap3DCompressor::EncodeOffsets(const Tilemap3D& map, const std::vector<uint16_t>& tiles, const std::array<uint16_t, 14>& offsets, std::vector<LZ77Entry>& lz77, std::vector<bool>& compressed)
{
    lz77.emplace_back(1, 0, 0);
    size_t idx = 1;
    
    std::vector<uint16_t> offset_vec(offsets.begin(), offsets.end());
    do
    {
        auto result = findMatch(tiles, idx, offset_vec);
        if ((result.first != 0) || (lz77.back().back_offset_idx != 0))
        {
            lz77.emplace_back(result.second, result.first, static_cast<int>(idx));
        }
        else
        {
            lz77.back().run_length++;
        }
        if (lz77.back().back_offset_idx == 0)
        {
            compressed[idx] = false;
            idx++;
        }
        else
        {
            std::fill(compressed.begin() + idx, compressed.begin() + idx + lz77.back().run_length, true);
            idx += lz77.back().run_length;
        }
    } while (idx < tiles.size());

    // STEP 5: Vertical Run Optimization
    // To save space on large blocks of terrain (like floors or walls), the LZ77 parser 
    // attempts to combine horizontal matches that repeat directly below one another.
    // It scans downwards (and down-right) to build vertical_info runs.
    for (auto& entry : lz77)
    {
        OptimizeVerticalRun(map, lz77, entry, tiles.size());
    }

    auto it_remove = std::remove_if(lz77.begin(), lz77.end(), [](const LZ77Entry& comp)
        {
            return comp.back_offset_idx == -1;
        });
    lz77.erase(it_remove, lz77.end());
}

void Tilemap3DCompressor::CalculateTileDictionary(const std::vector<uint16_t>& tiles, const std::vector<bool>& compressed, std::array<uint16_t, 2>& tile_dict)
{
    // STEP 6: Determine the optimal Tile Dictionary (TD0, TD1)
    // The tile dictionary stores two base tiles. The encoder can output tiles incredibly cheaply
    // if they are sequential increments of these bases.
    // This performs an exhaustive O(N^2) search across all unique uncompressed tiles to find 
    // the two bases that result in the absolute minimum bit-overhead for the remaining literals.
    std::map<uint16_t, int> incrementing_tile_counts;
    std::map<uint16_t, int> ranged_tile_counts;
    for (size_t i = 0; i < tiles.size(); ++i)
    {
        if (compressed[i] == false)
        {
            for (auto& itc : incrementing_tile_counts)
            {
                if (tiles[i] == itc.first + itc.second)
                {
                    itc.second++;
                }
                if ((tiles[i] >= itc.first) && (tiles[i] < itc.first + itc.second))
                {
                    ranged_tile_counts[tiles[i]]++;
                }
            }
            if (incrementing_tile_counts.find(tiles[i]) == incrementing_tile_counts.end())
            {
                incrementing_tile_counts[tiles[i]] = 1;
            }
        }
    }
    uint16_t max_tile = incrementing_tile_counts.empty() ? 0 : incrementing_tile_counts.rbegin()->first;
    uint16_t min_dict_entry = max_tile == 0 ? 0 : 1 << (ilog2(max_tile) - 1);
    
    std::vector<uint16_t> unique_tiles;
    for (const auto& irt : incrementing_tile_counts) unique_tiles.push_back(irt.first);
    if(unique_tiles.empty()) unique_tiles.push_back(0);
    
    int best_bits = 99999999;
    uint16_t best_td0 = unique_tiles.front();
    uint16_t best_td1 = min_dict_entry;

    for (uint16_t td0 : unique_tiles) {
        for (uint16_t td1 : unique_tiles) {
            int bits = 0;
            uint16_t ti[2] = {0, 0};
            for (size_t i = 0; i < tiles.size(); ++i) {
                if (!compressed[i]) {
                    bits += 2;
                    if (tiles[i] == td0 + ti[0]) {
                        ti[0]++;
                    } else if (tiles[i] == td1 + ti[1]) {
                        ti[1]++;
                    } else if (tiles[i] >= td0 && tiles[i] < td0 + ti[0]) {
                        bits += ilog2(ti[0]);
                    } else {
                        if (tiles[i] >= (1U << ilog2(td1 + ti[1]))) {
                            bits += 99999999;
                            break;
                        }
                        bits += ilog2(td1 + ti[1]);
                    }
                }
            }
            if (bits < best_bits) {
                best_bits = bits;
                best_td0 = td0;
                best_td1 = td1;
            }
        }
    }
    
    tile_dict[0] = best_td0;
    tile_dict[1] = best_td1;
}

void Tilemap3DCompressor::EncodeTiles(const std::vector<uint16_t>& tiles, const std::vector<bool>& compressed, const std::array<uint16_t, 2>& tile_dict, std::vector<TileEntry>& tile_entries)
{
    // STEP 7: Encode the literal tiles
    // Uncompressed tiles are categorized and encoded using a 2-bit operand:
    // 00: Unrelated tile (requires full bit depth relative to TD1)
    // 01: Tile is within a specific range of TD0
    // 10: Tile is exactly TD1 + increment (costs 2 bits)
    // 11: Tile is exactly TD0 + increment (costs 2 bits)
    uint16_t tile_increment[2] = { 0 };
    for (size_t i = 0; i < tiles.size(); ++i)
    {
        if (compressed[i] == false)
        {
            if (tiles[i] == tile_dict[0] + tile_increment[0])
            {
                tile_increment[0]++;
                tile_entries.emplace_back(3, 0, 0);
            }
            else if (tiles[i] == tile_dict[1] + tile_increment[1])
            {
                tile_increment[1]++;
                tile_entries.emplace_back(2, 0, 0);
            }
            else if ((tiles[i] >= tile_dict[0]) && (tiles[i] < (tile_dict[0] + tile_increment[0])))
            {
                tile_entries.emplace_back(1, static_cast<uint16_t>(tiles[i] - tile_dict[0]), static_cast<uint8_t>(ilog2(tile_increment[0])));
            }
            else
            {
                tile_entries.emplace_back(0, tiles[i], static_cast<uint8_t>(ilog2(tile_dict[1] + tile_increment[1])));
            }
        }
    }
}

uint16_t Tilemap3DCompressor::WriteLayerData(const Tilemap3D& map, const std::array<uint16_t, 14>& offsets, const std::array<uint16_t, 2>& tile_dict, const std::vector<LZ77Entry>& lz77, const std::vector<TileEntry>& tile_entries, size_t tiles_size, uint8_t* dst, size_t size)
{
    BitBarrelWriter cmap;
    WriteLayerHeader(map, offsets, tile_dict, cmap);
    WriteLayerOffsets(lz77, tiles_size, cmap);
    WriteLayerTiles(tile_entries, cmap);
    
    cmap.AdvanceNextByte();
    uint16_t current_pos = cmap.GetByteCount();
    if (current_pos > size) throw std::runtime_error("Output buffer not large enough to hold result.");
    std::copy(cmap.Begin(), cmap.End(), dst);
    return current_pos - 1;
}

void Tilemap3DCompressor::WriteLayerHeader(const Tilemap3D& map, const std::array<uint16_t, 14>& offsets, const std::array<uint16_t, 2>& tile_dict, BitBarrelWriter& cmap)
{
    cmap.Write<uint8_t>(map.GetLeft());
    cmap.Write<uint8_t>(map.GetTop());
    cmap.Write<uint8_t>(map.GetWidth() - 1);
    cmap.Write<uint8_t>(map.GetHeight() * 2 - 1);
    
    cmap.WriteBits(tile_dict[0], 10);
    cmap.WriteBits(tile_dict[1], 10);
    
    for (size_t i = 0; i < 8; ++i)
    {
        cmap.WriteBits(offsets[6 + i], 12);
    }
}

void Tilemap3DCompressor::WriteLayerOffsets(const std::vector<LZ77Entry>& lz77, size_t tiles_size, BitBarrelWriter& cmap)
{
    int last_idx = -1;
    for (const auto& entry : lz77)
    {
        makeCodedNumber(static_cast<uint16_t>(entry.index - last_idx), cmap);
        last_idx = entry.index;
        if (entry.back_offset_idx < 6)
        {
            cmap.WriteBits(entry.back_offset_idx, 3);
        }
        else
        {
            cmap.WriteBits(3, 2);
            cmap.WriteBits(entry.back_offset_idx - 6, 3);
        }
        if (!entry.vertical_info.empty())
        {
            cmap.WriteBits(1, 1);

            bool begin = true;
            for (const auto& v : entry.vertical_info)
            {
                if (begin)
                {
                    cmap.WriteBits(v.first, 1);
                    begin = false;
                }
                else
                {
                    cmap.WriteBits(1, 1);
                }
                for (int i = 1; i < v.second; ++i)
                {
                    cmap.WriteBits(1, 1);
                }
                cmap.WriteBits(0, 1);
            }
            cmap.WriteBits(0, 1);
        }
        else
        {
            cmap.WriteBits(0, 1);
        }
    }
    if (last_idx < static_cast<int>(tiles_size))
    {
        makeCodedNumber(static_cast<uint16_t>(tiles_size - last_idx + 1), cmap);
    }
    else
    {
        makeCodedNumber(1, cmap);
    }
}

void Tilemap3DCompressor::WriteLayerTiles(const std::vector<TileEntry>& tile_entries, BitBarrelWriter& cmap)
{
    for (const auto& entry : tile_entries)
    {
        cmap.WriteBits(entry.code, 2);
        switch (entry.code)
        {
        case 0:
        case 1:
            cmap.WriteBits(entry.data, entry.data_length);
            break;
        default:
            break;
        }
    }
}

uint16_t Tilemap3DCompressor::EncodeHeightmap(const Tilemap3D& map, uint8_t* dst, size_t size)
{
    BitBarrelWriter cmap;
    cmap.Write<uint8_t>(map.GetHeightmapWidth());
    cmap.Write<uint8_t>(map.GetHeightmapHeight());
    
    // Heightmap Compression: Run-Length Encoding (RLE)
    // Pairs adjacent identical heights into (run_length, pattern) entries.
    // The original hardware limits maximum run length outputs to 0xFF (255) per byte, 
    // so longer runs are split automatically.
    std::vector<std::pair<int, uint16_t>> hm_buffer;
    for (size_t i = 0; i < map.GetHeightmapSize(); ++i)
    {
        if (hm_buffer.empty() || hm_buffer.back().second != map.heightmap[i])
        {
            hm_buffer.emplace_back(0, map.heightmap[i]);
        }
        else
        {
            hm_buffer.back().first++;
        }
    }

    for (const auto& entry : hm_buffer)
    {
        cmap.Write<uint16_t>(entry.second);
        int len = entry.first;
        while (len >= 0xFF)
        {
            cmap.Write<uint8_t>(0xFF);
            len -= 0xFF;
        }
        cmap.Write<uint8_t>(static_cast<uint8_t>(len));
    }
    
    if (cmap.GetByteCount() <= size)
    {
        std::copy(cmap.Begin(), cmap.End(), dst);
    }
    else
    {
        throw std::runtime_error("Output buffer not large enough to hold result.");
    }
    return static_cast<uint16_t>(cmap.GetByteCount());
}

} // namespace Landstalker

