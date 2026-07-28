#ifndef _AUDIO_DATA_H_
#define _AUDIO_DATA_H_

#include <landstalker/main/DataManager.h>

namespace Landstalker {

// Holds the raw PCM sample banks used by the Cube/Iwadare Z80 sound driver for digital sample
// playback, plus the sample directory that indexes into them. The sound driver itself and its
// music/SFX banks (cube.bin, soundbank3.bin, soundbank4.bin) are built from source under
// code/audio and are not handled here.
class AudioData : public DataManager
{
public:
    // One entry in the PCM sample directory (t_SAMPLE_LOAD_DATA / samples.asm). See
    // code/audio/samples.asm for the on-disk layout.
    struct PcmSample
    {
        uint8_t rate = 0;          // raw playback-rate byte; larger = slower/lower pitch
        uint8_t bank = 0;          // which PCM bank (0 or 1) the sample data lives in
        uint16_t length = 0;       // sample length in bytes
        uint16_t start_offset = 0; // offset into the bank (raw on-disk value minus 8000h)
        uint8_t reserved = 0;      // unused byte at +1; preserved for round-trip fidelity
        uint8_t reserved2 = 0;     // unused byte at +3; preserved for round-trip fidelity

        bool operator==(const PcmSample&) const = default;
    };

    AudioData(const std::filesystem::path& asm_file);
    AudioData(const Rom& rom);

    virtual ~AudioData() {}

    virtual bool Save(const std::filesystem::path& dir);
    virtual bool Save();

    virtual bool HasBeenModified() const;
    virtual void RefreshPendingWrites(const Rom& rom);

    const ByteVector& GetPcmBank0() const;
    void SetPcmBank0(const ByteVector& data);
    const ByteVector& GetPcmBank1() const;
    void SetPcmBank1(const ByteVector& data);

    // The sample directory. Its maximum length depends on how the data was loaded: 24 entries
    // for a ROM load (the table is a fixed-size region within the driver binary - anything
    // longer would overwrite adjacent driver code/data) or 256 for an ASM load (the table is a
    // plain, independently-sized `db` list).
    const std::vector<PcmSample>& GetPcmSampleTable() const;
    void SetPcmSampleTable(const std::vector<PcmSample>& table);
    std::size_t GetMaxPcmSampleCount() const;

    // Sample playback rate in Hz for a raw rate byte, per the driver's DAC timing formula.
    static uint32_t GetPcmSampleRateHz(uint8_t rate);

protected:
    virtual void CommitAllChanges();
private:
    bool LoadAsmFilenames();
    void SetDefaultFilenames();
    bool CreateDirectoryStructure(const std::filesystem::path& dir);
    void InitCache();

    bool AsmLoadAudio();
    bool RomLoadAudio(const Rom& rom);
    bool AsmLoadPcmTable();
    bool RomLoadPcmTable(const Rom& rom);

    bool AsmSaveAudio(const std::filesystem::path& dir);
    bool AsmSavePcmTable(const std::filesystem::path& dir);

    bool RomPrepareInjectAudio(const Rom& rom);

    bool m_is_asm;

    std::filesystem::path m_pcm_bank0_filename;
    std::filesystem::path m_pcm_bank1_filename;
    std::filesystem::path m_pcm_table_filename;

    ByteVector m_pcm_bank0;
    ByteVector m_pcm_bank0_orig;
    ByteVector m_pcm_bank1;
    ByteVector m_pcm_bank1_orig;

    std::vector<PcmSample> m_pcm_table;
    std::vector<PcmSample> m_pcm_table_orig;
};

} // namespace Landstalker

#endif // _AUDIO_DATA_H_
