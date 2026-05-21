#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <landstalker/3d_maps/Tilemap3DCmp.h>

namespace fs = std::filesystem;
using namespace Landstalker;

std::vector<uint8_t> ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }
    return {};
}

size_t GetUncompressedSize(const Tilemap3D& tm) {
    return tm.GetSize() * 4 + tm.GetHeightmapSize() * 2 + 6;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <map1.cmp> [map2.cmp ...]\n";
        std::cerr << "Accepts multiple map files as arguments. Wildcards like *.cmp are supported.\n";
        return 1;
    }
    
    std::cout << std::left << std::setw(30) << "Map Name" 
              << std::right << std::setw(12) << "Size (WxH)"
              << std::setw(15) << "Orig Comp Size" 
              << std::setw(15) << "Uncomp Size" 
              << std::setw(15) << "Recomp Size"
              << std::setw(12) << "Size Diff" 
              << std::setw(12) << "Ratio (%)" 
              << std::setw(15) << "Encode Time"
              << std::setw(12) << "Identical?" << "\n";
    std::cout << std::string(138, '-') << "\n";

    std::vector<fs::path> paths;
    for (int i = 1; i < argc; ++i) {
        fs::path p(argv[i]);
        if (fs::is_regular_file(p)) {
            paths.push_back(p);
        } else {
            std::cerr << "Warning: " << argv[i] << " is not a regular file or does not exist.\n";
        }
    }
    std::sort(paths.begin(), paths.end());

    size_t total_orig = 0;
    size_t total_recomp = 0;

    for (const auto& path : paths) {
        std::string filename = path.filename().string();
        std::vector<uint8_t> compressed = ReadFile(path.string());
        
        if (compressed.empty()) {
            std::cout << std::left << std::setw(30) << filename 
                      << std::right << std::setw(12) << "-"
                      << std::setw(15) << "0" 
                      << std::setw(15) << "ERROR" 
                      << std::setw(15) << "-" 
                      << std::setw(12) << "-"
                      << std::setw(12) << "-"
                      << std::setw(15) << "-"
                      << std::setw(12) << "-" << "\n";
            continue;
        }

        try {
            // Decompress (First pass)
            Tilemap3D tm1;
            tm1.Decode(compressed.data());
            
            size_t uncompressed_size = GetUncompressedSize(tm1);
            std::string dims = std::to_string(tm1.GetWidth()) + "x" + std::to_string(tm1.GetHeight());

            // Recompress
            std::vector<uint8_t> recompressed(65536 * 4, 0); // Large buffer for recompression
            
            auto start_time = std::chrono::high_resolution_clock::now();
            uint16_t recompressed_size = tm1.Encode(recompressed.data(), recompressed.size());
            auto end_time = std::chrono::high_resolution_clock::now();
            
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

            // Decompress (Second pass)
            Tilemap3D tm2;
            tm2.Decode(recompressed.data());

            // Verify identity
            bool identical = (tm1 == tm2);
            
            // Calculate stats
            int size_diff = static_cast<int>(recompressed_size) - static_cast<int>(compressed.size());
            double ratio = (uncompressed_size > 0) ? (static_cast<double>(recompressed_size) / uncompressed_size * 100.0) : 0.0;
            std::string diff_str = (size_diff > 0) ? "+" + std::to_string(size_diff) : std::to_string(size_diff);
            std::string time_str = std::to_string(duration_ms) + " ms";

            total_orig += compressed.size();
            total_recomp += recompressed_size;

            std::cout << std::left << std::setw(30) << filename 
                      << std::right << std::setw(12) << dims
                      << std::setw(15) << compressed.size() 
                      << std::setw(15) << uncompressed_size 
                      << std::setw(15) << recompressed_size 
                      << std::setw(12) << diff_str
                      << std::setw(12) << std::fixed << std::setprecision(2) << ratio
                      << std::setw(15) << time_str
                      << std::setw(12) << (identical ? "Yes" : "No") << "\n";
        } catch (...) {
            std::cout << std::left << std::setw(30) << filename 
                      << std::right << std::setw(12) << "-"
                      << std::setw(15) << compressed.size() 
                      << std::setw(15) << "ERROR" 
                      << std::setw(15) << "-" 
                      << std::setw(12) << "-"
                      << std::setw(12) << "-"
                      << std::setw(15) << "-"
                      << std::setw(12) << "-" << "\n";
        }
    }

    std::cout << std::string(138, '-') << "\n";
    std::cout << "Total Original Size: " << total_orig << "\n";
    std::cout << "Total Recompressed Size: " << total_recomp << "\n";

    return 0;
}