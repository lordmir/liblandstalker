#include <landstalker/main/AudioData.h>

#include <landstalker/main/RomLabels.h>
#include <landstalker/main/Z80AsmFile.h>
#include <landstalker/misc/Utils.h>

namespace
{
	constexpr std::size_t PCM_TABLE_COUNT_ROM = 24;
	constexpr std::size_t PCM_TABLE_COUNT_ASM_MAX = 256;
	constexpr std::size_t PCM_TABLE_ENTRY_SIZE = 8;
	constexpr uint16_t PCM_TABLE_START_BASE = 0x8000;

	Landstalker::AudioData::PcmSample DecodePcmSample(const std::vector<uint8_t>& b)
	{
		Landstalker::AudioData::PcmSample s;
		s.rate = b[0];
		s.reserved = b[1];
		s.bank = b[2];
		s.reserved2 = b[3];
		s.length = static_cast<uint16_t>(b[4] | (b[5] << 8));
		const uint16_t raw_start = static_cast<uint16_t>(b[6] | (b[7] << 8));
		s.start_offset = static_cast<uint16_t>(raw_start - PCM_TABLE_START_BASE);
		return s;
	}

	void EncodePcmSample(const Landstalker::AudioData::PcmSample& s, std::vector<uint8_t>& out)
	{
		const uint16_t raw_start = static_cast<uint16_t>(s.start_offset + PCM_TABLE_START_BASE);
		out.push_back(s.rate);
		out.push_back(s.reserved);
		out.push_back(s.bank);
		out.push_back(s.reserved2);
		out.push_back(static_cast<uint8_t>(s.length & 0xFF));
		out.push_back(static_cast<uint8_t>((s.length >> 8) & 0xFF));
		out.push_back(static_cast<uint8_t>(raw_start & 0xFF));
		out.push_back(static_cast<uint8_t>((raw_start >> 8) & 0xFF));
	}
}

namespace Landstalker {

AudioData::AudioData(const std::filesystem::path& asm_file)
	: DataManager("Audio Data", asm_file),
	  m_is_asm(true)
{
	SetDefaultFilenames();
	if (!LoadAsmFilenames())
	{
		throw std::runtime_error(std::string("Unable to load file data from \'") + asm_file.string() + '\'');
	}
	if (!AsmLoadAudio())
	{
		throw std::runtime_error(std::string("Unable to load audio data from \'") + asm_file.string() + '\'');
	}
	if (!AsmLoadPcmTable())
	{
		throw std::runtime_error(std::string("Unable to load PCM sample table from \'") + m_pcm_table_filename.string() + '\'');
	}
	InitCache();
}

AudioData::AudioData(const Rom& rom)
	: DataManager("Audio Data", rom),
	  m_is_asm(false)
{
	SetDefaultFilenames();
	if (!RomLoadAudio(rom))
	{
		throw std::runtime_error(std::string("Unable to load audio data from ROM"));
	}
	if (!RomLoadPcmTable(rom))
	{
		throw std::runtime_error(std::string("Unable to load PCM sample table from ROM"));
	}
	InitCache();
}

bool AudioData::Save(const std::filesystem::path& dir)
{
	std::filesystem::path directory = dir;
	if (std::filesystem::exists(directory) && std::filesystem::is_regular_file(directory))
	{
		directory = directory.parent_path();
	}
	if (!CreateDirectoryStructure(directory))
	{
		throw std::runtime_error(std::string("Unable to create directory structure at \'") + directory.string() + '\'');
	}
	if (!AsmSaveAudio(directory))
	{
		throw std::runtime_error(std::string("Unable to save audio data to \'") + directory.string() + '\'');
	}
	if (!AsmSavePcmTable(directory))
	{
		throw std::runtime_error(std::string("Unable to save PCM sample table to \'") + directory.string() + '\'');
	}
	CommitAllChanges();
	return true;
}

bool AudioData::Save()
{
	return Save(GetBasePath());
}

bool AudioData::HasBeenModified() const
{
	if (m_pcm_bank0 != m_pcm_bank0_orig)
	{
		return true;
	}
	if (m_pcm_bank1 != m_pcm_bank1_orig)
	{
		return true;
	}
	if (m_pcm_table != m_pcm_table_orig)
	{
		return true;
	}
	return false;
}

void AudioData::RefreshPendingWrites(const Rom& rom)
{
	DataManager::RefreshPendingWrites(rom);
	if (!RomPrepareInjectAudio(rom))
	{
		throw std::runtime_error(std::string("Unable to prepare audio data for ROM injection"));
	}
}

const ByteVector& AudioData::GetPcmBank0() const
{
	return m_pcm_bank0;
}

void AudioData::SetPcmBank0(const ByteVector& data)
{
	m_pcm_bank0 = data;
}

const ByteVector& AudioData::GetPcmBank1() const
{
	return m_pcm_bank1;
}

void AudioData::SetPcmBank1(const ByteVector& data)
{
	m_pcm_bank1 = data;
}

const std::vector<AudioData::PcmSample>& AudioData::GetPcmSampleTable() const
{
	return m_pcm_table;
}

void AudioData::SetPcmSampleTable(const std::vector<PcmSample>& table)
{
	m_pcm_table = table;
	if (m_pcm_table.size() > GetMaxPcmSampleCount())
	{
		m_pcm_table.resize(GetMaxPcmSampleCount());
	}
}

std::size_t AudioData::GetMaxPcmSampleCount() const
{
	return m_is_asm ? PCM_TABLE_COUNT_ASM_MAX : PCM_TABLE_COUNT_ROM;
}

uint32_t AudioData::GetPcmSampleRateHz(uint8_t rate)
{
	constexpr double Z80_CLOCK_HZ = 3579545.0;
	return static_cast<uint32_t>(Z80_CLOCK_HZ / (209 + 13 * rate) + 0.5);
}

void AudioData::CommitAllChanges()
{
	m_pcm_bank0_orig = m_pcm_bank0;
	m_pcm_bank1_orig = m_pcm_bank1;
	m_pcm_table_orig = m_pcm_table;
	m_pending_writes.clear();
}

bool AudioData::LoadAsmFilenames()
{
	try
	{
		bool retval = true;
		AsmFile f(GetAsmFilename().string());
		retval = retval && GetFilenameFromAsm(f, RomLabels::Audio::PCM_BANK_0_SECTION, m_pcm_bank0_filename);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Audio::PCM_BANK_1_SECTION, m_pcm_bank1_filename);
		return retval;
	}
	catch (...)
	{
	}
	return false;
}

void AudioData::SetDefaultFilenames()
{
	if (m_pcm_bank0_filename.empty()) m_pcm_bank0_filename = RomLabels::Audio::PCM_BANK_0_FILE;
	if (m_pcm_bank1_filename.empty()) m_pcm_bank1_filename = RomLabels::Audio::PCM_BANK_1_FILE;
	// Not discoverable via a label in the top-level ASM file (samples.asm is only included
	// transitively, through the Z80 driver's own main.asm) - always the same fixed path.
	if (m_pcm_table_filename.empty()) m_pcm_table_filename = RomLabels::Audio::PCM_TABLE_FILE;
}

bool AudioData::CreateDirectoryStructure(const std::filesystem::path& dir)
{
	bool retval = true;
	retval = retval && CreateDirectoryTree(dir / m_pcm_bank0_filename);
	retval = retval && CreateDirectoryTree(dir / m_pcm_bank1_filename);
	retval = retval && CreateDirectoryTree(dir / m_pcm_table_filename);
	return retval;
}

void AudioData::InitCache()
{
	m_pcm_bank0_orig = m_pcm_bank0;
	m_pcm_bank1_orig = m_pcm_bank1;
	m_pcm_table_orig = m_pcm_table;
}

bool AudioData::AsmLoadAudio()
{
	m_pcm_bank0 = ReadBytes(GetBasePath() / m_pcm_bank0_filename);
	m_pcm_bank1 = ReadBytes(GetBasePath() / m_pcm_bank1_filename);
	return true;
}

bool AudioData::RomLoadAudio(const Rom& rom)
{
	auto pcm0_sec = rom.get_section(RomLabels::Audio::PCM_BANK_0_SECTION);
	m_pcm_bank0 = rom.read_array<uint8_t>(pcm0_sec.begin, pcm0_sec.size());
	auto pcm1_sec = rom.get_section(RomLabels::Audio::PCM_BANK_1_SECTION);
	m_pcm_bank1 = rom.read_array<uint8_t>(pcm1_sec.begin, pcm1_sec.size());
	return true;
}

bool AudioData::AsmLoadPcmTable()
{
	Z80AsmFile f(GetBasePath() / m_pcm_table_filename);
	if (!f.Good() || !f.Goto(RomLabels::Audio::PCM_TABLE_LABEL))
	{
		return false;
	}
	m_pcm_table.clear();
	for (std::size_t i = 0; i < PCM_TABLE_COUNT_ASM_MAX; ++i)
	{
		auto entry = f.ReadBytes(PCM_TABLE_ENTRY_SIZE);
		if (entry.size() < PCM_TABLE_ENTRY_SIZE)
		{
			break;
		}
		m_pcm_table.push_back(DecodePcmSample(entry));
	}
	return true;
}

bool AudioData::RomLoadPcmTable(const Rom& rom)
{
	auto sec = rom.get_section(RomLabels::Audio::PCM_TABLE_SECTION);
	auto bytes = rom.read_array<uint8_t>(sec.begin, sec.size());
	const std::size_t count = std::min(bytes.size() / PCM_TABLE_ENTRY_SIZE, PCM_TABLE_COUNT_ROM);
	m_pcm_table.clear();
	m_pcm_table.reserve(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		std::vector<uint8_t> entry(bytes.begin() + i * PCM_TABLE_ENTRY_SIZE,
			bytes.begin() + (i + 1) * PCM_TABLE_ENTRY_SIZE);
		m_pcm_table.push_back(DecodePcmSample(entry));
	}
	return true;
}

bool AudioData::AsmSaveAudio(const std::filesystem::path& dir)
{
	WriteBytes(m_pcm_bank0, dir / m_pcm_bank0_filename);
	WriteBytes(m_pcm_bank1, dir / m_pcm_bank1_filename);
	return true;
}

bool AudioData::AsmSavePcmTable(const std::filesystem::path& dir)
{
	std::vector<uint8_t> bytes;
	bytes.reserve(m_pcm_table.size() * PCM_TABLE_ENTRY_SIZE);
	for (const auto& sample : m_pcm_table)
	{
		EncodePcmSample(sample, bytes);
	}
	Z80AsmFile file;
	file.WriteComment("PCM Sample Directory");
	file.WriteLabel(RomLabels::Audio::PCM_TABLE_LABEL);
	file.WriteBytes(bytes, PCM_TABLE_ENTRY_SIZE);
	return file.WriteFile(dir / m_pcm_table_filename);
}

bool AudioData::RomPrepareInjectAudio(const Rom& /*rom*/)
{
	m_pending_writes.push_back({ RomLabels::Audio::PCM_BANK_0_SECTION, std::make_shared<ByteVector>(m_pcm_bank0) });
	m_pending_writes.push_back({ RomLabels::Audio::PCM_BANK_1_SECTION, std::make_shared<ByteVector>(m_pcm_bank1) });

	auto table_bytes = std::make_shared<ByteVector>();
	table_bytes->reserve(m_pcm_table.size() * PCM_TABLE_ENTRY_SIZE);
	for (const auto& sample : m_pcm_table)
	{
		EncodePcmSample(sample, *table_bytes);
	}
	m_pending_writes.push_back({ RomLabels::Audio::PCM_TABLE_SECTION, table_bytes });
	return true;
}

} // namespace Landstalker
