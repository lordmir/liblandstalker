#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <landstalker/3d_maps/Tilemap3D.h>
#include <landstalker/misc/BitBarrel.h>

namespace fs = std::filesystem;
using namespace Landstalker;

std::vector<uint8_t> ReadFile(const fs::path& path) {
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

void WriteFile(const fs::path& path, const std::string& content) {
    std::ofstream file(path);
    if (file) {
        file << content;
    }
}

void ExtractDicts(const std::vector<uint8_t>& data, uint16_t& td0, uint16_t& td1, std::vector<uint16_t>& dyn_offsets) {
    BitBarrel bb(data.data());
    bb.readBits(8); // left
    bb.readBits(8); // top
    bb.readBits(8); // width
    bb.readBits(8); // height
    
    td1 = static_cast<uint16_t>(bb.readBits(10));
    td0 = static_cast<uint16_t>(bb.readBits(10));
    
    dyn_offsets.clear();
    for (int i = 0; i < 8; ++i) {
        dyn_offsets.push_back(static_cast<uint16_t>(bb.readBits(12)));
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " -o <outdir> <map1.cmp> [map2.cmp ...]\n";
        return 1;
    }

    std::string outdir = "";
    std::vector<fs::path> paths;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            outdir = argv[++i];
        } else {
            paths.push_back(arg);
        }
    }

    if (outdir.empty()) {
        std::cerr << "Output directory (-o) is required.\n";
        return 1;
    }

    fs::create_directories(outdir);
    
    std::ofstream dict_csv(fs::path(outdir) / "dictionaries.csv");
    dict_csv << "Map,TD0,TD1,Dyn1,Dyn2,Dyn3,Dyn4,Dyn5,Dyn6,Dyn7,Dyn8\n";

    for (const auto& path : paths) {
        std::vector<uint8_t> compressed = ReadFile(path);
        if (compressed.empty()) {
            std::cerr << "Failed to read " << path << "\n";
            continue;
        }

        Tilemap3D tm;
        try {
            tm.Decode(compressed.data());
        } catch (...) {
            std::cerr << "Failed to decode " << path << "\n";
            continue;
        }
        
        uint16_t td0, td1;
        std::vector<uint16_t> dyn_offsets;
        ExtractDicts(compressed, td0, td1, dyn_offsets);
        
        std::string stem = path.stem().string();
        dict_csv << stem << "," << td0 << "," << td1;
        for(uint16_t doff : dyn_offsets) {
            dict_csv << "," << doff;
        }
        dict_csv << "\n";

        std::string fg, bg, hm;
        if (tm.ToCsv(fg, bg, hm)) {
            WriteFile(fs::path(outdir) / (stem + "_fg.csv"), fg);
            WriteFile(fs::path(outdir) / (stem + "_bg.csv"), bg);
            WriteFile(fs::path(outdir) / (stem + "_hm.csv"), hm);
        } else {
            std::cerr << "Failed to generate CSV for " << path << "\n";
        }
    }
    std::cout << "Done exporting maps and dictionaries to " << outdir << "\n";
    return 0;
}
