#ifndef _MUSIC_DATA_H_
#define _MUSIC_DATA_H_

#include <array>
#include <landstalker/main/DataManager.h>

namespace Landstalker {

// Holds the music tracks and sound effects played by the Cube/Iwadare Z80 sound driver, decoded
// from the two swappable music banks (soundbank3.asm/soundbank4.asm, ids 20h-3Fh/00h-1Fh) and the
// SFX table within the resident driver (sfx.asm, ids 41h-7Ah via pt_SFX). See
// docs/sound_driver_format.md in landstalker_disasm for the on-disk format this mirrors.
//
// Unlike AudioData's PCM banks/table, music and SFX binaries (soundbank3.bin, soundbank4.bin,
// cube.bin) are built from source by an external Z80 toolchain, not injected directly - so, as
// with AudioData's PCM table, Save() always regenerates the ASM source (regardless of whether the
// project was opened from ASM or ROM); there is no ROM binary injection path for this data.
class MusicData : public DataManager
{
public:
    // One event in a channel's command stream (docs/sound_driver_format.md section 5). Decoded as
    // an unopinionated token stream: notes/rests carry pitch and duration, and F8h-FFh command
    // bytes carry their raw opcode and operand without interpreting them, since a command's actual
    // meaning differs per channel type (FM/DAC/PSG tone/PSG noise). This keeps the codec identical
    // for every channel and keeps decode/encode exactly byte-for-byte round-trippable.
    struct SoundEvent
    {
        bool is_command = false;      // false: note/rest byte; true: an F8h-FFh command byte
        uint8_t value = 0;            // note/rest: the 7-bit pitch (00h-6Fh, 70h for rest); command: the opcode byte
        bool has_duration = false;    // note/rest only - whether a duration byte follows this one
        uint8_t duration = 0;         // note/rest only, valid iff has_duration
        std::vector<uint8_t> operand; // command only: F8h-FEh -> 1 byte; FFh -> 2 bytes (lo, hi)

        bool operator==(const SoundEvent&) const = default;
    };
    using EventStream = std::vector<SoundEvent>;

    // Decodes a single channel's command stream starting at the beginning of `bytes`, stopping
    // after the first FFh command (end/chain/jump - always 3 bytes) or when `bytes` is exhausted.
    static EventStream DecodeEventStream(const std::vector<uint8_t>& bytes);
    static std::vector<uint8_t> EncodeEventStream(const EventStream& events);

    // Playback tick rate in Hz for a raw MusicTrack::tempo byte. The driver writes (tempo + 3) to
    // the YM2612's Timer B register, whose NTSC period is 300.37 * (256 - value) microseconds -
    // see landstalker_tools_python/scripts/midi_export.py (calculate_frame_duration_sec). 0 if the
    // byte is out of the register's usable range (tempo > 252).
    static double GetTempoHz(uint8_t tempo);

    static constexpr std::size_t MUSIC_CHANNEL_COUNT = 10; // FM1-5, DAC(ch6), PSG1-3, PSG noise
    static constexpr std::size_t SFX_FULL_CHANNEL_COUNT = 10;
    static constexpr std::size_t SFX_OVERLAY_CHANNEL_COUNT = 3; // FM4, FM5, FM6

    struct MusicTrack
    {
        uint16_t autofade_frames = 0; // 0 = never auto-fades
        uint8_t tempo = 0;            // raw byte; driver writes (tempo + 3) to YM2612 Timer B
        std::array<EventStream, MUSIC_CHANNEL_COUNT> channels;

        bool operator==(const MusicTrack&) const = default;
    };

    struct SfxEntry
    {
        uint8_t type = 0; // 1 = full effect (10 channels, takes over every channel); else overlay (3 channels)
        std::vector<EventStream> channels;

        bool operator==(const SfxEntry&) const = default;
    };

    // A named entry in the track/SFX pool - the actual musical content, independent of which
    // slot(s) play it. The name only exists in this editor (there's nowhere to round-trip it
    // through the ASM source), so it resets to a generated default on every load.
    struct MusicTrackEntry
    {
        std::string name;
        MusicTrack track;

        bool operator==(const MusicTrackEntry&) const = default;
    };
    struct SfxPoolEntry
    {
        std::string name;
        SfxEntry entry;

        bool operator==(const SfxPoolEntry&) const = default;
    };

    static constexpr std::size_t MUSIC_SLOT_COUNT = 64; // ids 00h-3Fh: 00h-1Fh in bank 4, 20h-3Fh in bank 3
    static constexpr std::size_t SFX_SLOT_COUNT = 58;    // ids 41h-7Ah, in pt_SFX order

    MusicData(const std::filesystem::path& asm_file);
    MusicData(const Rom& rom);

    virtual ~MusicData() {}

    virtual bool Save(const std::filesystem::path& dir);
    virtual bool Save();

    virtual bool HasBeenModified() const;

    // The pool of distinct music tracks. Multiple slots may point at the same pool entry (e.g.
    // several ids sharing one silent track) - see GetMusicSlotMap(). A bank only ever contains the
    // pool entries actually referenced by one of its own slots; if the same entry is referenced by
    // slots in both banks, it's written into both (they're physically separate memory windows and
    // can't share a single copy).
    const std::vector<MusicTrackEntry>& GetMusicTrackPool() const;
    void SetMusicTrackPool(const std::vector<MusicTrackEntry>& pool);
    // MUSIC_SLOT_COUNT entries (ids 00h-3Fh), each an index into GetMusicTrackPool().
    const std::vector<std::size_t>& GetMusicSlotMap() const;
    void SetMusicSlotMap(const std::vector<std::size_t>& map);

    // The pool of distinct SFX entries - see GetMusicTrackPool() above, same idea.
    const std::vector<SfxPoolEntry>& GetSfxPool() const;
    void SetSfxPool(const std::vector<SfxPoolEntry>& pool);
    // SFX_SLOT_COUNT entries (ids 41h-7Ah), each an index into GetSfxPool().
    const std::vector<std::size_t>& GetSfxSlotMap() const;
    void SetSfxSlotMap(const std::vector<std::size_t>& map);

protected:
    virtual void CommitAllChanges();

private:
    bool CreateDirectoryStructure(const std::filesystem::path& dir);
    void InitCache();

    bool AsmLoadMusic();
    bool AsmLoadSfx();
    bool RomLoadMusic(const Rom& rom);
    bool RomLoadSfx(const Rom& rom);

    bool AsmSaveMusic(const std::filesystem::path& dir);
    bool AsmSaveSfx(const std::filesystem::path& dir);

    void BuildMusicPoolFromSlots(const std::vector<MusicTrack>& slots);
    void BuildSfxPoolFromSlots(const std::vector<SfxEntry>& slots);

    std::vector<MusicTrackEntry> m_music_pool;
    std::vector<MusicTrackEntry> m_music_pool_orig;
    std::vector<std::size_t> m_music_slot_map;
    std::vector<std::size_t> m_music_slot_map_orig;

    std::vector<SfxPoolEntry> m_sfx_pool;
    std::vector<SfxPoolEntry> m_sfx_pool_orig;
    std::vector<std::size_t> m_sfx_slot_map;
    std::vector<std::size_t> m_sfx_slot_map_orig;
};

} // namespace Landstalker

#endif // _MUSIC_DATA_H_
