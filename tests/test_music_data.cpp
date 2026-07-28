#include <landstalker/main/MusicData.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <landstalker/main/RomLabels.h>

namespace {

using Landstalker::MusicData;

class TemporaryProjectDir
{
public:
    TemporaryProjectDir()
        : dir(std::filesystem::temp_directory_path() /
              ("landstalker_musicdata_" + std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count())))
    {
        std::filesystem::create_directories(dir);
    }

    ~TemporaryProjectDir()
    {
        std::error_code error;
        std::filesystem::remove_all(dir, error);
    }

    void Write(const std::string& relative_path, const std::string& contents) const
    {
        const std::filesystem::path full = dir / relative_path;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream stream(full);
        stream << contents;
    }

    std::filesystem::path dir;
};

// Builds a minimal but structurally faithful code/audio project: one real music track per bank
// (ids 00h and 20h), 31 slots in each bank's table reusing a shared silent MUSIC_NULL track, and
// one SFX entry - enough to exercise the pointer tables, org-padding, dw-label fixups and channel
// dedup without needing the full 64-track/58-SFX game data.
void WriteMinimalProject(const TemporaryProjectDir& proj)
{
    proj.Write("code/audio/ym_instruments.asm", "YM_INSTMT_00:\tdb 1, 2, 3\n");

    proj.Write("code/audio/music/music00.asm",
        "; Music Track 00\n"
        "\tdb 0, 0, 0, 186\n"
        "\tdw MUSIC_00_YM1\n"
        "\tdw MUSIC_00_YM2\n"
        "\tdw MUSIC_00_YM2\n"
        "\tdw MUSIC_00_YM2\n"
        "\tdw MUSIC_00_YM2\n"
        "\tdw MUSIC_00_YM2\n"
        "\tdw MUSIC_00_YM2\n"
        "\tdw MUSIC_00_YM2\n"
        "\tdw MUSIC_00_YM2\n"
        "\tdw MUSIC_00_YM2\n"
        "MUSIC_00_YM1:\n"
        "\tdb 0FCh, 1, 0F8h, 20h, 30h, 18h, 0F8h, 0A0h\n" // ends in an F8-A0 loop-back jump
        "MUSIC_00_YM2\n" // colon-less label, matching the disassembly's occasional style
        "\tdb 0FFh, 0, 0\n");
    proj.Write("code/audio/music/music_null.asm",
        "\tdb 0, 0, 0, 200\n"
        "\tdw MUSIC_NULL_CMD\n"
        "\tdw MUSIC_NULL_CMD\n"
        "\tdw MUSIC_NULL_CMD\n"
        "\tdw MUSIC_NULL_CMD\n"
        "\tdw MUSIC_NULL_CMD\n"
        "\tdw MUSIC_NULL_CMD\n"
        "\tdw MUSIC_NULL_CMD\n"
        "\tdw MUSIC_NULL_CMD\n"
        "\tdw MUSIC_NULL_CMD\n"
        "\tdw MUSIC_NULL_CMD\n"
        "MUSIC_NULL_CMD:\tdb 0FFh, 0, 0\n");
    proj.Write("code/audio/soundbank4.asm",
        "\tcpu z80\n"
        "\tphase\t0\n"
        "\torg 8000h\n"
        "\tinclude \"ym_instruments.asm\"\n"
        "\torg 8910h\n"
        "\tdw MUSIC_00, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "MUSIC_00:\tinclude \"music/music00.asm\"\n"
        "MUSIC_NULL:\tinclude \"music/music_null.asm\"\n"
        "\tend\n");

    proj.Write("code/audio/music/music20.asm",
        "; Music Track 20\n"
        "\tdb 0, 5, 0, 200\n"
        "\tdw MUSIC_20_YM1\n"
        "\tdw MUSIC_20_YM1\n"
        "\tdw MUSIC_20_YM1\n"
        "\tdw MUSIC_20_YM1\n"
        "\tdw MUSIC_20_YM1\n"
        "\tdw MUSIC_20_YM1\n"
        "\tdw MUSIC_20_YM1\n"
        "\tdw MUSIC_20_YM1\n"
        "\tdw MUSIC_20_YM1\n"
        "\tdw MUSIC_20_YM1\n"
        "MUSIC_20_YM1:\tdb 41h, 20h, 0FFh, 0, 0\n"); // a duration-bearing note, then stop
    proj.Write("code/audio/soundbank3.asm",
        "\tcpu z80\n"
        "\tphase\t0\n"
        "\torg 8000h\n"
        "\tdw MUSIC_20, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "\tdw MUSIC_NULL, MUSIC_NULL, MUSIC_NULL, MUSIC_NULL\n"
        "MUSIC_20:\tinclude \"music/music20.asm\"\n"
        "MUSIC_NULL:\tinclude \"music/music_null.asm\"\n"
        "\tend\n");

    proj.Write("code/audio/sfx/sfx01_header.asm",
        "; SFX 01\n"
        "\tdb 2\n"
        "\tdw SFX_01_NOOP\n"
        "\tdw SFX_01_YM5\n"
        "\tdw SFX_01_NOOP\n");
    proj.Write("code/audio/sfx/sfx01_data.asm",
        "SFX_01_YM5:\tdb 0FEh, 40h, 0FFh, 0, 0\n"
        "SFX_01_NOOP:\tdb 0FFh, 0, 0\n");
    proj.Write("code/audio/sfx/sfx_null_header.asm",
        "\tdb 2\n"
        "\tdw SFX_NULL_NOOP\n"
        "\tdw SFX_NULL_NOOP\n"
        "\tdw SFX_NULL_NOOP\n");
    proj.Write("code/audio/sfx/sfx_null_data.asm",
        "SFX_NULL_NOOP:\tdb 0FFh, 0, 0\n");
    {
        // pt_SFX is always exactly 58 entries in the real driver (ids 41h-7Ah) - slot 1 is a real
        // effect, the rest alias a shared do-nothing entry, mirroring the music table's null reuse.
        std::string sfx_asm = "pt_SFX:\tdw SFX_01";
        for (int i = 0; i < 57; ++i)
        {
            sfx_asm += ", SFX_NULL";
        }
        sfx_asm += "\nSFX_01:\tinclude \"sfx/sfx01_header.asm\"\n"
                   "\tinclude \"sfx/sfx01_data.asm\"\n"
                   "SFX_NULL:\tinclude \"sfx/sfx_null_header.asm\"\n"
                   "\tinclude \"sfx/sfx_null_data.asm\"\n";
        proj.Write("code/audio/sfx.asm", sfx_asm);
    }
}

TEST(MusicDataTest, DecodesEventStreamsAndStopsAtTerminators)
{
    // Note without duration, note with duration, an F8h-family command, then FFh stop.
    const std::vector<uint8_t> bytes = { 0x10, 0xA0, 0x05, 0xFE, 0x08, 0xFF, 0x00, 0x00, 0xAA, 0xBB };
    const auto events = MusicData::DecodeEventStream(bytes);
    ASSERT_EQ(events.size(), 4u);
    EXPECT_FALSE(events[0].is_command);
    EXPECT_EQ(events[0].value, 0x10);
    EXPECT_FALSE(events[0].has_duration);
    EXPECT_FALSE(events[1].is_command);
    EXPECT_EQ(events[1].value, 0x20);
    EXPECT_TRUE(events[1].has_duration);
    EXPECT_EQ(events[1].duration, 0x05);
    EXPECT_TRUE(events[2].is_command);
    EXPECT_EQ(events[2].value, 0xFE);
    EXPECT_EQ(events[2].operand, (std::vector<uint8_t>{0x08}));
    EXPECT_TRUE(events[3].is_command);
    EXPECT_EQ(events[3].value, 0xFF);
    EXPECT_EQ(events[3].operand, (std::vector<uint8_t>{0x00, 0x00}));

    // FFh always terminates - trailing bytes (AA BB) must not be decoded.
    const auto reencoded = MusicData::EncodeEventStream(events);
    const std::vector<uint8_t> expected(bytes.begin(), bytes.begin() + 8);
    EXPECT_EQ(reencoded, expected);
}

TEST(MusicDataTest, TempoHzMatchesPythonReferenceFormula)
{
    // landstalker_tools_python/scripts/midi_export.py: timer_b = tempo + 3,
    // frame_us = 300.37 * (256 - timer_b), hz = 1e6 / frame_us. Cross-checked against MUSIC_01
    // ("Torchlight")'s real tempo byte (186) via a standalone Python run: ~49.69 Hz.
    EXPECT_NEAR(MusicData::GetTempoHz(186), 49.69, 0.01);
    EXPECT_NEAR(MusicData::GetTempoHz(0), 13.159, 0.01);
    // Out of the Timer B register's usable range - reports 0 rather than dividing by <= 0.
    EXPECT_EQ(MusicData::GetTempoHz(253), 0.0);
}

TEST(MusicDataTest, F8LoopToMarkerAlsoTerminatesTheStream)
{
    // F8h "jump to marker" (control byte in A0h-BFh) is an unconditional backward jump - the
    // idiomatic way a track loops forever - so it must close the stream just like FFh, even
    // though further (unreachable) bytes follow.
    const std::vector<uint8_t> bytes = { 0xF8, 0xA1, 0x11, 0x22, 0x33 };
    const auto events = MusicData::DecodeEventStream(bytes);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_TRUE(events[0].is_command);
    EXPECT_EQ(events[0].value, 0xF8);
    EXPECT_EQ(events[0].operand, (std::vector<uint8_t>{0xA1}));
}

TEST(MusicDataTest, PlayOnceSectionKeepsDecodingPastJumpToMarker)
{
    // With a play-once section open (F8h 40h-7Fh), bytes after a jump-to-marker ARE reachable:
    // repeat passes skip forward from the play-once command to its closing marker, which can lie
    // beyond the jump. The stream must therefore keep decoding past the jump (the caller bounds
    // the slice at the next label/pointer target) - the base game's title theme (track 00h, FM1)
    // relies on this to play a different continuation on later passes.
    const std::vector<uint8_t> bytes = {
        0xF8, 0x40,       // play-once section A opens
        0x84, 0x04,       // a note (with duration)
        0xF8, 0xA0,       // jump to marker B - NOT the end of the stream this time
        0xF8, 0x60,       // the play-once section's closing marker, on the far side of the jump
        0x85, 0x04,       // the repeat-pass continuation
        0xFF, 0x00, 0x00  // hard end
    };
    const auto events = MusicData::DecodeEventStream(bytes);
    ASSERT_EQ(events.size(), 6u);
    EXPECT_EQ(events[2].operand, (std::vector<uint8_t>{0xA0}));
    EXPECT_EQ(events[3].operand, (std::vector<uint8_t>{0x60}));
    EXPECT_EQ(events[4].value, 0x05);
    EXPECT_EQ(events[5].value, 0xFF);
    // Byte-for-byte round trip, including everything after the jump.
    EXPECT_EQ(MusicData::EncodeEventStream(events), bytes);
}

const MusicData::MusicTrack& TrackAtSlot(const MusicData& md, std::size_t slot)
{
    return md.GetMusicTrackPool()[md.GetMusicSlotMap()[slot]].track;
}

const MusicData::SfxEntry& SfxAtSlot(const MusicData& md, std::size_t slot)
{
    return md.GetSfxPool()[md.GetSfxSlotMap()[slot]].entry;
}

TEST(MusicDataTest, LoadsMusicAndSfxFromAsmProject)
{
    TemporaryProjectDir proj;
    WriteMinimalProject(proj);

    MusicData md(proj.dir / "dummy_top.asm");
    ASSERT_EQ(md.GetMusicSlotMap().size(), MusicData::MUSIC_SLOT_COUNT);

    const auto& t00 = TrackAtSlot(md, 0);
    EXPECT_EQ(t00.tempo, 186);
    EXPECT_EQ(t00.autofade_frames, 0);
    // Channels 1-9 all alias MUSIC_00_YM2 (a bare stop) - only channel 0 differs.
    for (std::size_t ch = 1; ch < 9; ++ch)
    {
        EXPECT_EQ(t00.channels[ch], t00.channels[1]) << "channel " << ch;
    }
    EXPECT_NE(t00.channels[0], t00.channels[1]);
    {
        const auto enc = MusicData::EncodeEventStream(t00.channels[0]);
        EXPECT_EQ(enc, (std::vector<uint8_t>{0xFC, 0x01, 0xF8, 0x20, 0x30, 0x18, 0xF8, 0xA0}));
    }

    // Every remaining bank-4 slot (1-31) should share the same pool entry as the null track.
    for (std::size_t i = 2; i < 32; ++i)
    {
        EXPECT_EQ(md.GetMusicSlotMap()[i], md.GetMusicSlotMap()[1]) << "slot " << i;
    }

    const auto& t20 = TrackAtSlot(md, 32);
    EXPECT_EQ(t20.tempo, 200);
    EXPECT_EQ(t20.autofade_frames, 5);
    for (std::size_t ch = 0; ch < 10; ++ch)
    {
        const auto enc = MusicData::EncodeEventStream(t20.channels[ch]);
        EXPECT_EQ(enc, (std::vector<uint8_t>{0x41, 0x20, 0xFF, 0x00, 0x00}));
    }
    for (std::size_t i = 33; i < 64; ++i)
    {
        EXPECT_EQ(md.GetMusicSlotMap()[i], md.GetMusicSlotMap()[33]) << "slot " << i;
    }
    // Bank 3's and bank 4's null tracks decode to identical content, so they collapse into the
    // same pool entry even though they come from separate banks - AsmSaveMusic below is what's
    // responsible for writing that one entry into both banks' own files.
    EXPECT_EQ(TrackAtSlot(md, 1), TrackAtSlot(md, 33));
    EXPECT_EQ(md.GetMusicSlotMap()[1], md.GetMusicSlotMap()[33]);

    ASSERT_EQ(md.GetSfxSlotMap().size(), MusicData::SFX_SLOT_COUNT);
    const auto& s0 = SfxAtSlot(md, 0);
    EXPECT_EQ(s0.type, 2);
    ASSERT_EQ(s0.channels.size(), 3u);
    EXPECT_EQ(s0.channels[0], s0.channels[2]); // both NOOP
    EXPECT_NE(s0.channels[1], s0.channels[0]);
    {
        const auto enc = MusicData::EncodeEventStream(s0.channels[1]);
        EXPECT_EQ(enc, (std::vector<uint8_t>{0xFE, 0x40, 0xFF, 0x00, 0x00}));
    }
    for (std::size_t i = 1; i < 58; ++i)
    {
        EXPECT_EQ(md.GetSfxSlotMap()[i], md.GetSfxSlotMap()[1]) << "sfx slot " << i;
    }
}

TEST(MusicDataTest, AsmSaveRoundTripsThroughReload)
{
    TemporaryProjectDir proj;
    WriteMinimalProject(proj);
    MusicData original(proj.dir / "dummy_top.asm");

    TemporaryProjectDir out;
    // Save() regenerates soundbank3/4.asm and sfx.asm - ym_instruments.asm is untouched source we
    // don't own, so it needs to already exist for a re-load to succeed.
    std::filesystem::copy_file(proj.dir / "code/audio/ym_instruments.asm", [&] {
        const auto p = out.dir / "code/audio/ym_instruments.asm";
        std::filesystem::create_directories(p.parent_path());
        return p;
    }());
    original.Save(out.dir);

    // Pool ordering isn't meaningful (it's an artifact of first-appearance order, which can shift
    // across a regenerate/re-parse cycle) - what matters is that every slot resolves to the same
    // content as before.
    MusicData reloaded(out.dir / "dummy_top.asm");
    ASSERT_EQ(reloaded.GetMusicSlotMap().size(), MusicData::MUSIC_SLOT_COUNT);
    for (std::size_t i = 0; i < MusicData::MUSIC_SLOT_COUNT; ++i)
    {
        EXPECT_EQ(TrackAtSlot(reloaded, i), TrackAtSlot(original, i)) << "music slot " << i;
    }
    ASSERT_EQ(reloaded.GetSfxSlotMap().size(), MusicData::SFX_SLOT_COUNT);
    for (std::size_t i = 0; i < MusicData::SFX_SLOT_COUNT; ++i)
    {
        EXPECT_EQ(SfxAtSlot(reloaded, i), SfxAtSlot(original, i)) << "sfx slot " << i;
    }
    EXPECT_FALSE(reloaded.HasBeenModified());
}

} // namespace
