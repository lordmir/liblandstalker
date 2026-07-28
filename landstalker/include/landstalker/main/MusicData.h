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

    // Decodes a single channel's command stream starting at the beginning of `bytes`. The stream
    // ends at the first FFh command (end/chain/jump - always 3 bytes), at a jump-to-marker
    // (F8h A0h-BFh) when no play-once section has been opened (with one open, later passes can
    // skip past the jump, so decoding continues), at a jump-to-marker that lands exactly on one
    // of `stop_offsets` (the label/pointer targets after the stream's start - a backward jump
    // butting up against the next label is that stream's true end), or when `bytes` runs out.
    //
    // A label/pointer target is deliberately NOT an end by itself: many of the base game's SFX
    // channels have no terminator of their own and fall through into the next label's data
    // (usually a shared FFh stop) - the decoded stream absorbs that tail so it is always
    // self-contained, no matter how a save re-orders the blocks.
    static EventStream DecodeEventStream(const std::vector<uint8_t>& bytes,
        const std::vector<std::size_t>& stop_offsets = {});
    static std::vector<uint8_t> EncodeEventStream(const EventStream& events);

    // Playback tick rate in Hz for a raw MusicTrack::tempo byte. The driver writes (tempo + 3) to
    // the YM2612's Timer B register, whose NTSC period is 300.37 * (256 - value) microseconds -
    // see landstalker_tools_python/scripts/midi_export.py (calculate_frame_duration_sec). 0 if the
    // byte is out of the register's usable range (tempo > 252).
    static double GetTempoHz(uint8_t tempo);

    static constexpr std::size_t MUSIC_CHANNEL_COUNT = 10; // FM1-5, DAC(ch6), PSG1-3, PSG noise
    static constexpr std::size_t SFX_FULL_CHANNEL_COUNT = 10;
    static constexpr std::size_t SFX_OVERLAY_CHANNEL_COUNT = 3; // FM4, FM5, FM6

    // The FM (YM2612) instrument patch table at the start of bank 4 (ym_instruments.asm,
    // ROM 1F8000h-1F8910h): YM_INSTMT_00-4F, selected by an FM channel's FEh command. Each patch
    // is 29 bytes: seven groups of four per-operator register values in YM2612 register order
    // (operator slots S1, S3, S2, S4 - regs 30h/40h/50h/60h/70h/80h/90h), then one
    // feedback/algorithm byte (reg B0h). Kept as raw register bytes - the editor layer decodes
    // the packed bit fields for display.
    static constexpr std::size_t YM_INSTRUMENT_COUNT = 80;
    static constexpr std::size_t YM_INSTRUMENT_SIZE = 29;
    using YmInstrument = std::array<uint8_t, YM_INSTRUMENT_SIZE>;
    using YmInstrumentTable = std::array<YmInstrument, YM_INSTRUMENT_COUNT>;

    // The driver's instrument/frequency/pitch-effect parameter tables (instrument_params.asm,
    // resident inside the driver at ROM 1F70A3h-1F7300h). Kept as raw table values; the two
    // variable-length groups keep their control/marker bytes verbatim (a pitch effect ends with
    // its 80h loop / 81h hold byte; a PSG envelope's steps carry bit 7 sustain markers and may
    // have trailing padding), so a load/save cycle is byte-exact.
    static constexpr std::size_t YM_FREQUENCY_COUNT = 84;   // 7 octaves x 12 semitones from C
    static constexpr std::size_t PSG_FREQUENCY_COUNT = 64;  // ~5 octaves + 4 semitones from A
    static constexpr std::size_t YM_LEVEL_COUNT = 16;
    static constexpr std::size_t ALGO_COUNT = 8;
    static constexpr std::size_t PITCH_EFFECT_COUNT = 16;
    static constexpr std::size_t PSG_ENVELOPE_COUNT = 16;
    struct InstrumentParams
    {
        std::array<uint16_t, YM_FREQUENCY_COUNT> ym_frequencies = {};   // (block << 11) | F-number
        std::array<uint16_t, PSG_FREQUENCY_COUNT> psg_frequencies = {}; // 10-bit tone periods
        std::array<uint8_t, YM_LEVEL_COUNT> ym_levels = {};             // volume -> TL attenuation
        std::array<uint8_t, ALGO_COUNT> slots_per_algo = {};            // carrier bitmask per algorithm
        std::array<std::vector<uint8_t>, PITCH_EFFECT_COUNT> pitch_effects;
        std::array<std::vector<uint8_t>, PSG_ENVELOPE_COUNT> psg_envelopes;

        bool operator==(const InstrumentParams&) const = default;
    };

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

    // The bytes a track occupies in its bank when saved: the 24-byte header (autofade/tempo plus
    // ten channel pointers) plus each distinct channel stream's encoded bytes - channels with
    // identical content share one copy (e.g. the PSG noise channel frequently reuses PSG3's
    // data), matching how the save path actually lays tracks out.
    static std::size_t GetMusicTrackSize(const MusicTrack& track);

    // Bytes used / capacity for one of the two music banks (bank 0 = soundbank4, ids 00h-1Fh;
    // bank 1 = soundbank3, ids 20h-3Fh). Used counts the pointer table, each referenced pool
    // entry once (shared slots don't double-count), and - for bank 4 - the YM instrument table
    // that shares the bank. Capacity is the Z80 bank window size (8000h bytes).
    std::pair<std::size_t, std::size_t> GetMusicBankUsage(std::size_t bank) const;

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

    const YmInstrumentTable& GetYmInstruments() const;
    void SetYmInstruments(const YmInstrumentTable& instruments);

    const InstrumentParams& GetInstrumentParams() const;
    void SetInstrumentParams(const InstrumentParams& params);

    // Unlike the track/SFX streams (which need the external Z80 toolchain to rebuild), the
    // instrument table is fixed-size at a fixed bank offset, so it CAN be written straight into a
    // ROM - this queues that write (the first 910h bytes of the bank 4 section).
    virtual void RefreshPendingWrites(const Rom& rom);

protected:
    virtual void CommitAllChanges();

private:
    bool CreateDirectoryStructure(const std::filesystem::path& dir);
    void InitCache();

    bool AsmLoadMusic();
    bool AsmLoadSfx();
    bool AsmLoadInstrumentParams();
    bool RomLoadMusic(const Rom& rom);
    bool RomLoadSfx(const Rom& rom);
    bool RomLoadInstrumentParams(const Rom& rom);

    bool AsmSaveMusic(const std::filesystem::path& dir);
    bool AsmSaveSfx(const std::filesystem::path& dir);
    bool AsmSaveInstrumentParams(const std::filesystem::path& dir);

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

    YmInstrumentTable m_ym_instruments = {};
    YmInstrumentTable m_ym_instruments_orig = {};

    InstrumentParams m_instrument_params;
    InstrumentParams m_instrument_params_orig;
};

} // namespace Landstalker

#endif // _MUSIC_DATA_H_
