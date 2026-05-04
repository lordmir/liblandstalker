#include <gtest/gtest.h>
#include <landstalker/3d_maps/Tilemap3DCmp.h>
#include <vector>
#include <iostream>
#include <iomanip>

using namespace Landstalker;

class Tilemap3DTest : public ::testing::Test {
protected:
    void PrintCompressionRatio(const std::string& test_name, size_t original_size, size_t compressed_size) {
        double ratio = (original_size == 0) ? 0.0 : (100.0 * compressed_size) / original_size;
        std::cout << "[ RATIO    ] " << test_name << ": "
                  << compressed_size << " / " << original_size << " bytes ("
                  << std::fixed << std::setprecision(2) << ratio << "%)" << std::endl;
    }
    
    size_t GetUncompressedSize(const Tilemap3D& tm) const {
        return tm.GetSize() * 4 + tm.GetHeightmapSize() * 2 + 6;
    }
};

TEST_F(Tilemap3DTest, AllZeros) {
    Tilemap3D tm;
    tm.Resize(32, 32);
    tm.ResizeHeightmap(16, 16);
    
    size_t uncompressed_size = GetUncompressedSize(tm);
    std::vector<uint8_t> buffer(65536, 0);
    
    uint16_t compressed_size = tm.Encode(buffer.data(), buffer.size());
    PrintCompressionRatio("AllZeros", uncompressed_size, compressed_size);
    
    Tilemap3D tm2;
    tm2.Decode(buffer.data());
    
    EXPECT_EQ(tm, tm2);
}

TEST_F(Tilemap3DTest, RepeatedStandardPattern) {
    Tilemap3D tm;
    tm.Resize(32, 32);
    tm.ResizeHeightmap(16, 16);
    
    for(int y = 0; y < 32; ++y) {
        for(int x = 0; x < 32; ++x) {
            uint16_t val = (x % 4) + (y % 4) * 4;
            tm.SetBlock(val, x + y * 32, Tilemap3D::Layer::BG);
            tm.SetBlock(val + 16, x + y * 32, Tilemap3D::Layer::FG);
        }
    }
    for(int y = 0; y < 16; ++y) {
        for(int x = 0; x < 16; ++x) {
            tm.SetHeightmapCell({x, y}, (x % 2 == 0) ? 0x0400 : 0x0800);
        }
    }
    
    size_t uncompressed_size = GetUncompressedSize(tm);
    std::vector<uint8_t> buffer(65536, 0);
    
    uint16_t compressed_size = tm.Encode(buffer.data(), buffer.size());
    PrintCompressionRatio("RepeatedStandardPattern", uncompressed_size, compressed_size);
    
    Tilemap3D tm2;
    tm2.Decode(buffer.data());
    
    EXPECT_EQ(tm, tm2);
}

TEST_F(Tilemap3DTest, IncrementingBlocks) {
    Tilemap3D tm;
    tm.Resize(16, 16);
    tm.ResizeHeightmap(8, 8);
    
    uint16_t counter = 0;
    for(int i = 0; i < 256; ++i) {
        tm.SetBlock(counter++, i, Tilemap3D::Layer::BG);
        tm.SetBlock(counter++, i, Tilemap3D::Layer::FG);
    }
    
    size_t uncompressed_size = GetUncompressedSize(tm);
    std::vector<uint8_t> buffer(65536, 0);
    
    uint16_t compressed_size = tm.Encode(buffer.data(), buffer.size());
    PrintCompressionRatio("IncrementingBlocks", uncompressed_size, compressed_size);
    
    Tilemap3D tm2;
    tm2.Decode(buffer.data());
    
    EXPECT_EQ(tm, tm2);
}
