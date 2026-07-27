#include <gtest/gtest.h>
#include <landstalker/3d_maps/Tilemap3D.h>
#include <landstalker/3d_maps/MapToTmx.h>
#include <filesystem>
#include <string>
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

// Builds a map with distinctive, non-uniform block and heightmap data so that a round trip
// through a text format has something meaningful to lose.
static Tilemap3D MakeInterchangeTestMap() {
    Tilemap3D tm;
    tm.Resize(7, 5);
    tm.ResizeHeightmap(4, 3);
    tm.SetLeft(9);
    tm.SetTop(11);
    uint16_t i = 0;
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 7; ++x, ++i) {
            tm.SetBlock(static_cast<uint16_t>((i * 3) & 0x3FF), i, Tilemap3D::Layer::BG);
            tm.SetBlock(static_cast<uint16_t>((i * 7 + 1) & 0x3FF), i, Tilemap3D::Layer::FG);
        }
    }
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            tm.SetCellProps({ x, y }, static_cast<uint8_t>((x + y) & 0xF));
            tm.SetHeight({ x, y }, static_cast<uint8_t>((x * 2 + y) & 0xF));
            tm.SetCellType({ x, y }, static_cast<uint8_t>((x * 16 + y) & 0xFF));
        }
    }
    return tm;
}

static std::string TempTmxPath(const char* name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

TEST_F(Tilemap3DTest, CsvRoundTrip) {
    const Tilemap3D tm = MakeInterchangeTestMap();

    std::string fg, bg, hm;
    ASSERT_TRUE(tm.ToCsv(fg, bg, hm));

    Tilemap3D tm2;
    ASSERT_TRUE(tm2.FromCsv(fg, bg, hm));
    EXPECT_EQ(tm, tm2);
}

TEST_F(Tilemap3DTest, CsvRejectsMismatchedLayers) {
    const Tilemap3D tm = MakeInterchangeTestMap();
    std::string fg, bg, hm;
    ASSERT_TRUE(tm.ToCsv(fg, bg, hm));

    Tilemap3D tm2 = MakeInterchangeTestMap();
    const std::string one_row_bg = bg.substr(0, bg.find('\n') + 1);
    EXPECT_FALSE(tm2.FromCsv(fg, one_row_bg, hm));
    EXPECT_EQ(tm, tm2);   // a rejected import must leave the map untouched

    EXPECT_FALSE(tm2.FromCsv(fg, bg, ""));
    EXPECT_FALSE(tm2.FromCsv("", bg, hm));
    EXPECT_FALSE(tm2.FromCsv(fg, "not,hex,data\n", hm));
    EXPECT_EQ(tm, tm2);
}

TEST_F(Tilemap3DTest, CsvToleratesTrailingBlankLines) {
    const Tilemap3D tm = MakeInterchangeTestMap();
    std::string fg, bg, hm;
    ASSERT_TRUE(tm.ToCsv(fg, bg, hm));

    Tilemap3D tm2;
    ASSERT_TRUE(tm2.FromCsv(fg + "\n\n", bg + "\n", hm + "\n"));
    EXPECT_EQ(tm, tm2);
}

TEST_F(Tilemap3DTest, TmxRoundTripPreservesHeightmap) {
    const Tilemap3D tm = MakeInterchangeTestMap();
    const auto path = TempTmxPath("landstalker_tmx_roundtrip.tmx");
    ASSERT_TRUE(MapToTmx::ExportToTmx(path, tm, "blockset.png"));

    Tilemap3D tm2;
    ASSERT_TRUE(MapToTmx::ImportFromTmx(path, tm2));
    EXPECT_EQ(tm, tm2);
    std::filesystem::remove(path);
}

TEST_F(Tilemap3DTest, TmxWithoutHeightmapPropertyLeavesHeightmapAlone) {
    const Tilemap3D tm = MakeInterchangeTestMap();
    // Mimic a file written before the heightmap was exported.
    auto doc = MapToTmx::GenerateXmlDocument("legacy.tmx", tm, "blockset.png");
    doc.child("map").remove_child("properties");
    const auto path = TempTmxPath("landstalker_tmx_legacy.tmx");
    ASSERT_TRUE(doc.save_file(path.c_str()));

    Tilemap3D tm2 = MakeInterchangeTestMap();
    tm2.SetLeft(2);
    tm2.SetTop(3);
    ASSERT_TRUE(MapToTmx::ImportFromTmx(path, tm2));
    EXPECT_EQ(2, tm2.GetLeft());
    EXPECT_EQ(3, tm2.GetTop());
    std::filesystem::remove(path);
}

TEST_F(Tilemap3DTest, TmxWithUnreadableHeightmapPropertyIsRejected) {
    const Tilemap3D tm = MakeInterchangeTestMap();
    auto doc = MapToTmx::GenerateXmlDocument("corrupt.tmx", tm, "blockset.png");
    for (auto property : doc.child("map").child("properties").children("property")) {
        if (std::string(property.attribute("name").as_string()) == "heightmap") {
            property.attribute("value") = "0x0001,0x0002";   // too few cells
        }
    }
    const auto path = TempTmxPath("landstalker_tmx_corrupt.tmx");
    ASSERT_TRUE(doc.save_file(path.c_str()));

    Tilemap3D tm2;
    const Tilemap3D before = tm2;
    EXPECT_FALSE(MapToTmx::ImportFromTmx(path, tm2));
    EXPECT_EQ(before, tm2);   // nothing was applied
    std::filesystem::remove(path);
}
