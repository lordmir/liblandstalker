#include <landstalker/main/Z80AsmFile.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

class TemporaryAsmDir
{
public:
    TemporaryAsmDir()
        : dir(std::filesystem::temp_directory_path() /
              ("landstalker_z80asmfile_" + std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count())))
    {
        std::filesystem::create_directories(dir);
    }

    ~TemporaryAsmDir()
    {
        std::error_code error;
        std::filesystem::remove_all(dir, error);
    }

    std::filesystem::path Write(const std::string& relative_path, const std::string& contents) const
    {
        const std::filesystem::path full = dir / relative_path;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream stream(full);
        stream << contents;
        return full;
    }

    std::filesystem::path dir;
};

TEST(Z80AsmFileTest, ReadsPlainDbDwAndEqu)
{
    TemporaryAsmDir tmp;
    const auto path = tmp.Write("simple.asm",
        "SOME_CONST equ 5\n"
        "t_DATA:\tdb 01h, 00h, 00h, 00h,  52h, 09h,  00h, 80h\n"
        "\tdw 1234h\n");

    Landstalker::Z80AsmFile file(path);
    ASSERT_TRUE(file.Good());
    ASSERT_TRUE(file.ConstantExists("SOME_CONST"));
    EXPECT_EQ(file.GetConstant("SOME_CONST"), 5u);
    ASSERT_TRUE(file.Goto("t_DATA"));
    const std::vector<uint8_t> expected = { 0x01, 0x00, 0x00, 0x00, 0x52, 0x09, 0x00, 0x80, 0x34, 0x12 };
    EXPECT_EQ(file.ReadBytes(expected.size()), expected);
}

TEST(Z80AsmFileTest, ResolvesForwardLabelReferencesInDw)
{
    TemporaryAsmDir tmp;
    const auto path = tmp.Write("table.asm",
        "pt_TABLE:\tdw ENTRY_A, ENTRY_B\n"
        "ENTRY_A:\tdb 0AAh, 0BBh\n"
        "ENTRY_B:\tdb 0CCh, 0DDh, 0EEh\n");

    Landstalker::Z80AsmFile file(path);
    ASSERT_TRUE(file.Good());
    std::size_t entry_a_offset = 0, entry_b_offset = 0;
    ASSERT_TRUE(file.GetLabelOffset("ENTRY_A", entry_a_offset));
    ASSERT_TRUE(file.GetLabelOffset("ENTRY_B", entry_b_offset));

    ASSERT_TRUE(file.Goto("pt_TABLE"));
    const auto ptrs = file.ReadBytes(4);
    const uint16_t ptr_a = static_cast<uint16_t>(ptrs[0] | (ptrs[1] << 8));
    const uint16_t ptr_b = static_cast<uint16_t>(ptrs[2] | (ptrs[3] << 8));
    EXPECT_EQ(ptr_a, entry_a_offset);
    EXPECT_EQ(ptr_b, entry_b_offset);

    EXPECT_EQ(file.ReadBytesAt(entry_a_offset, 2), (std::vector<uint8_t>{ 0xAA, 0xBB }));
    EXPECT_EQ(file.ReadBytesAt(entry_b_offset, 3), (std::vector<uint8_t>{ 0xCC, 0xDD, 0xEE }));
}

TEST(Z80AsmFileTest, InlinesIncludedFilesRelativeToParentDirectory)
{
    TemporaryAsmDir tmp;
    tmp.Write("sub/child.asm", "CHILD_LABEL:\tdb 011h, 022h\n");
    const auto path = tmp.Write("parent.asm",
        "PARENT_LABEL:\tdb 0FFh\n"
        "\tinclude \"sub/child.asm\"\n"
        "AFTER_LABEL:\tdb 033h\n");

    Landstalker::Z80AsmFile file(path);
    ASSERT_TRUE(file.Good());
    const std::vector<uint8_t> expected = { 0xFF, 0x11, 0x22, 0x33 };
    EXPECT_EQ(file.ToBinary(), expected);

    std::size_t child_offset = 0, after_offset = 0;
    ASSERT_TRUE(file.GetLabelOffset("CHILD_LABEL", child_offset));
    ASSERT_TRUE(file.GetLabelOffset("AFTER_LABEL", after_offset));
    EXPECT_EQ(child_offset, 1u);
    EXPECT_EQ(after_offset, 3u);
}

TEST(Z80AsmFileTest, OrgPadsForwardWithinSameOriginButLeavesFirstOrgUnpadded)
{
    TemporaryAsmDir tmp;
    const auto path = tmp.Write("banked.asm",
        "\torg 8000h\n"
        "FIRST:\tdb 0AAh, 0BBh\n"
        "\torg 8010h\n"
        "SECOND:\tdb 0CCh\n");

    Landstalker::Z80AsmFile file(path);
    ASSERT_TRUE(file.Good());
    std::size_t first_offset = 0, second_offset = 0;
    ASSERT_TRUE(file.GetLabelOffset("FIRST", first_offset));
    ASSERT_TRUE(file.GetLabelOffset("SECOND", second_offset));
    // First org (8000h) is the origin - FIRST sits right at offset 0, no padding inserted for it.
    EXPECT_EQ(first_offset, 0u);
    // Second org (8010h) pads forward to offset (8010h - 8000h) = 10h.
    EXPECT_EQ(second_offset, 0x10u);
    EXPECT_EQ(file.ToBinary().size(), 0x11u);
}

TEST(Z80AsmFileTest, MatchesRealSoundBank4TableLayout)
{
    // Mirrors the structure of code/audio/soundbank4.asm closely enough to prove the org-padding
    // and include mechanisms combine correctly: a variable-length included block, followed by an
    // `org` jump to a fixed pointer-table offset, followed by more includes.
    TemporaryAsmDir tmp;
    tmp.Write("ym_instruments.asm", "YM_INSTMT_00:\tdb 1, 2, 3, 4, 5\n");
    tmp.Write("music/music00.asm", "\tdb 0, 0, 0, 200\n");
    tmp.Write("music/music_null.asm", "\tdb 0FFh, 0, 0\n");
    const auto path = tmp.Write("soundbank4.asm",
        "\tcpu z80\n"
        "\tphase\t0\n"
        "\torg 8000h\n"
        "\tinclude \"ym_instruments.asm\"\n"
        "\torg 8010h\n"
        "\tdw MUSIC_00, MUSIC_NULL\n"
        "MUSIC_00:\tinclude \"music/music00.asm\"\n"
        "MUSIC_NULL:\tinclude \"music/music_null.asm\"\n");

    Landstalker::Z80AsmFile file(path);
    ASSERT_TRUE(file.Good());
    // ym_instruments.asm contributes 5 bytes at offset 0; org 8010h pads forward to offset 0x10.
    const auto table = file.ReadBytesAt(0x10, 4);
    std::size_t music_00_offset = 0, music_null_offset = 0;
    ASSERT_TRUE(file.GetLabelOffset("MUSIC_00", music_00_offset));
    ASSERT_TRUE(file.GetLabelOffset("MUSIC_NULL", music_null_offset));
    const uint16_t ptr0 = static_cast<uint16_t>(table[0] | (table[1] << 8));
    const uint16_t ptr1 = static_cast<uint16_t>(table[2] | (table[3] << 8));
    EXPECT_EQ(ptr0, music_00_offset);
    EXPECT_EQ(ptr1, music_null_offset);
    EXPECT_EQ(file.ReadBytesAt(music_00_offset, 4), (std::vector<uint8_t>{ 0, 0, 0, 200 }));
}

} // namespace
