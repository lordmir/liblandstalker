#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <landstalker/3d_maps/Tilemap3D.h>
#include <landstalker/misc/BitBarrel.h>

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

static uint16_t getCodedNumber(BitBarrel& bb)
{
    uint16_t exp = 0, num = 0;
    while(!bb.getNextBit()) exp++;
    if(exp) {
        num = 1 << exp;
        num += static_cast<uint16_t>(bb.readBits(exp));
    }
    return num;
}

void DumpTokens(const std::vector<uint8_t>& data) {
    BitBarrel bb(data.data());
    
    uint8_t left = static_cast<uint8_t>(bb.readBits(8));
    uint8_t top = static_cast<uint8_t>(bb.readBits(8));
    uint16_t width = static_cast<uint16_t>(bb.readBits(8)) + 1;
    uint16_t height = static_cast<uint16_t>((bb.readBits(8) + 1) / 2);
    
    uint16_t td1 = static_cast<uint16_t>(bb.readBits(10));
    uint16_t td0 = static_cast<uint16_t>(bb.readBits(10));
    
    std::vector<uint16_t> offsets = {
        0xFFFF, 1, 2, 
        static_cast<uint16_t>(width), 
        static_cast<uint16_t>(width * 2), 
        static_cast<uint16_t>(width + 1)
    };
    for (int i=0; i<8; ++i) offsets.push_back(static_cast<uint16_t>(bb.readBits(12)));
    
    uint16_t t = width * height * 2;
    int16_t dst_addr = -1;
    
    std::cout << "Index | Token Type | Length | Details\n";
    std::cout << "--------------------------------------------------\n";
    
    while(true) {
        uint16_t start = getCodedNumber(bb);
        if(!start) start++;
        
        if (start > 1) {
            std::cout << std::setw(4) << dst_addr + 1 << "  | LITERALS   | " << std::setw(6) << start - 1 << " | \n";
        }
        
        dst_addr += start;
        if(dst_addr >= t) break;
        
        uint8_t command = static_cast<uint8_t>(bb.readBits(3));
        if(command > 5) command = static_cast<uint8_t>(6 + (((command & 1) << 2) | bb.readBits(2)));
        
        std::string verts = "";
        uint16_t total_len = 1;
        
        if(bb.getNextBit()) {
            bool width_offset = bb.getNextBit();
            do {
                int count = 0;
                do {
                    count++;
                } while(bb.getNextBit());
                
                if (!verts.empty()) verts += ", ";
                verts += (width_offset ? "DR:" : "D:") + std::to_string(count);
                total_len += count;
                width_offset = !width_offset;
            } while(bb.getNextBit());
        }
        
        std::cout << std::setw(4) << dst_addr << "  | MATCH      | " << std::setw(6) << total_len << " | Offset: " << offsets[command];
        if (!verts.empty()) std::cout << " | Verts: [" << verts << "]";
        std::cout << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <map_file.cmp>\n";
        return 1;
    }
    std::vector<uint8_t> compressed = ReadFile(argv[1]);
    if (compressed.empty()) return 1;
    
    std::cout << "=== " << argv[1] << " Original Tokens ===\n";
    DumpTokens(compressed);
    return 0;
}