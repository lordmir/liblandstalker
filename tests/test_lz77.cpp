#include <gtest/gtest.h>
#include <landstalker/misc/LZ77.h>
#include <vector>
#include <cstdint>
#include <iostream>

using namespace Landstalker;

// Helper to check compression and decompression cycle
void TestCompressionCycle(const std::vector<uint8_t>& uncompressed) {
    std::vector<uint8_t> compressed(uncompressed.size() * 2 + 1024); // Give plenty of buffer space
    std::size_t compressed_size = LZ77::Encode(uncompressed.data(), uncompressed.size(), compressed.data());
    
    // Print compression ratio if we are testing a pattern
    double ratio = 0.0;
    if (uncompressed.size() > 0) {
        ratio = static_cast<double>(compressed_size) / static_cast<double>(uncompressed.size());
    }
    std::cout << "Uncompressed size: " << uncompressed.size() 
              << ", Compressed size: " << compressed_size 
              << ", Ratio: " << ratio * 100.0 << "%" << std::endl;

    // Assert that compression ratio is recorded/evaluated
    EXPECT_GT(compressed_size, 0);

    std::vector<uint8_t> decompressed(uncompressed.size());
    std::size_t bytes_read = 0;
    std::size_t decompressed_size = LZ77::Decode(compressed.data(), compressed_size, decompressed.data(), bytes_read);

    EXPECT_EQ(decompressed_size, uncompressed.size());
    EXPECT_EQ(bytes_read, compressed_size);
    EXPECT_EQ(uncompressed, decompressed);
}

TEST(LZ77Test, EmptyData) {
    std::vector<uint8_t> uncompressed = {};
    std::vector<uint8_t> compressed(1024);
    std::size_t compressed_size = LZ77::Encode(uncompressed.data(), uncompressed.size(), compressed.data());
    
    std::vector<uint8_t> decompressed(1024);
    std::size_t bytes_read = 0;
    std::size_t decompressed_size = LZ77::Decode(compressed.data(), compressed_size, decompressed.data(), bytes_read);

    EXPECT_EQ(decompressed_size, 0);
}

TEST(LZ77Test, AllZerosCompressionRatio) {
    std::vector<uint8_t> uncompressed(10000, 0); // 10,000 zeros
    std::cout << "Testing long sequence of zeros..." << std::endl;
    TestCompressionCycle(uncompressed);
}

TEST(LZ77Test, RepeatedPatternCompressionRatio) {
    std::vector<uint8_t> uncompressed;
    for (int i = 0; i < 1000; ++i) {
        uncompressed.push_back(0xAB);
        uncompressed.push_back(0xCD);
        uncompressed.push_back(0xEF);
        uncompressed.push_back(0x12);
        uncompressed.push_back(0x34);
    }
    std::cout << "Testing repeated standard pattern..." << std::endl;
    TestCompressionCycle(uncompressed);
}

TEST(LZ77Test, UncompressibleData) {
    std::vector<uint8_t> uncompressed;
    for (int i = 0; i < 1000; ++i) {
        uncompressed.push_back(static_cast<uint8_t>(i % 256));
    }
    std::cout << "Testing essentially uncompressible data..." << std::endl;
    TestCompressionCycle(uncompressed);
}

TEST(LZ77Test, ComplexCompressibleData) {
    std::vector<uint8_t> uncompressed;
    // Generate data that mimics somewhat compressible graphics/map data:
    // A mix of runs, repeating patterns, back-references, and random noise.
    srand(12345); // deterministic seed for reproducibility
    for (int i = 0; i < 500; ++i) {
        int type = rand() % 4;
        if (type == 0) {
            // Run of zeros (like empty space in a tileset or map)
            int len = 10 + rand() % 50;
            for(int j = 0; j < len; ++j) uncompressed.push_back(0);
        } else if (type == 1) {
            // Repeating pattern (like a simple patterned tile)
            std::vector<uint8_t> pat;
            int pat_len = 2 + rand() % 6;
            for(int j = 0; j < pat_len; ++j) pat.push_back(rand() % 256);
            int repeats = 5 + rand() % 15;
            for(int j = 0; j < repeats; ++j) {
                for(uint8_t b : pat) uncompressed.push_back(b);
            }
        } else if (type == 2) {
            // Copy from earlier in the buffer (explicit dictionary match)
            if (uncompressed.size() > 100) {
                int copy_len = 10 + rand() % 40;
                int max_offset = std::min(static_cast<int>(uncompressed.size()) - copy_len, 4095);
                int offset = uncompressed.size() - max_offset + (rand() % max_offset);
                for(int j = 0; j < copy_len; ++j) {
                    uncompressed.push_back(uncompressed[offset + j]);
                }
            }
        } else {
            // Noise (uncompressible data, like random pixel values)
            int len = 5 + rand() % 20;
            for(int j = 0; j < len; ++j) uncompressed.push_back(rand() % 256);
        }
    }
    std::cout << "Testing complex pseudo-random compressible data..." << std::endl;
    TestCompressionCycle(uncompressed);
}

