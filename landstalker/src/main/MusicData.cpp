#include <landstalker/main/MusicData.h>

#include <iomanip>
#include <map>
#include <sstream>

#include <landstalker/main/RomLabels.h>
#include <landstalker/main/Z80AsmFile.h>
#include <landstalker/misc/Utils.h>

namespace
{
	using Landstalker::MusicData;

	// Generous upper bound on how many bytes a single channel's command stream can span - decoding
	// always stops earlier, at the first FFh command, so this only needs to be larger than any real
	// stream (the longest in the base game is a few hundred bytes).
	constexpr std::size_t MAX_EVENT_STREAM_SCAN = 4096;
	constexpr uint16_t BANK_PTR_BASE = 0x8000; // Z80 bank window base (soundbank3/4) - not used for SFX

	const std::array<std::string, MusicData::MUSIC_CHANNEL_COUNT> MUSIC_CHANNEL_NAMES = {
		"YM1", "YM2", "YM3", "YM4", "YM5", "YM6", "PSG1", "PSG2", "PSG3", "PSGN"
	};

	std::string HexId(std::size_t id)
	{
		std::ostringstream oss;
		oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << id;
		return oss.str();
	}

	std::vector<uint8_t> SliceFrom(const std::vector<uint8_t>& data, std::size_t offset)
	{
		const std::size_t start = std::min(offset, data.size());
		const std::size_t end = std::min(start + MAX_EVENT_STREAM_SCAN, data.size());
		return std::vector<uint8_t>(data.begin() + start, data.begin() + end);
	}

	MusicData::MusicTrack DecodeMusicTrackFromBank(const std::vector<uint8_t>& bank, std::size_t offset, bool subtract_bank_base)
	{
		MusicData::MusicTrack track;
		const auto header = SliceFrom(bank, offset);
		if (header.size() < 4 + MusicData::MUSIC_CHANNEL_COUNT * 2)
		{
			return track;
		}
		track.autofade_frames = static_cast<uint16_t>(header[1] | (header[2] << 8));
		track.tempo = header[3];
		for (std::size_t ch = 0; ch < MusicData::MUSIC_CHANNEL_COUNT; ++ch)
		{
			const std::size_t p = 4 + ch * 2;
			uint16_t ptr = static_cast<uint16_t>(header[p] | (header[p + 1] << 8));
			const std::size_t ch_offset = subtract_bank_base
				? static_cast<std::size_t>(static_cast<uint16_t>(ptr - BANK_PTR_BASE))
				: static_cast<std::size_t>(ptr);
			track.channels[ch] = MusicData::DecodeEventStream(SliceFrom(bank, ch_offset));
		}
		return track;
	}

	MusicData::MusicTrack DecodeMusicTrackAsm(const Landstalker::Z80AsmFile& f, std::size_t offset)
	{
		MusicData::MusicTrack track;
		const auto header = f.ReadBytesAt(offset, 4 + MusicData::MUSIC_CHANNEL_COUNT * 2);
		if (header.size() < 4 + MusicData::MUSIC_CHANNEL_COUNT * 2)
		{
			return track;
		}
		track.autofade_frames = static_cast<uint16_t>(header[1] | (header[2] << 8));
		track.tempo = header[3];
		for (std::size_t ch = 0; ch < MusicData::MUSIC_CHANNEL_COUNT; ++ch)
		{
			const std::size_t p = 4 + ch * 2;
			const std::size_t ch_offset = static_cast<std::size_t>(header[p] | (header[p + 1] << 8));
			track.channels[ch] = MusicData::DecodeEventStream(f.ReadBytesAt(ch_offset, MAX_EVENT_STREAM_SCAN));
		}
		return track;
	}

	MusicData::SfxEntry DecodeSfxEntryFromDriver(const std::vector<uint8_t>& driver, std::size_t offset)
	{
		MusicData::SfxEntry entry;
		if (offset >= driver.size())
		{
			return entry;
		}
		entry.type = driver[offset];
		const std::size_t n = (entry.type == 1) ? MusicData::SFX_FULL_CHANNEL_COUNT : MusicData::SFX_OVERLAY_CHANNEL_COUNT;
		const auto ptrs = SliceFrom(driver, offset + 1);
		entry.channels.resize(n);
		for (std::size_t ch = 0; ch < n && ch * 2 + 1 < ptrs.size(); ++ch)
		{
			const uint16_t ptr = static_cast<uint16_t>(ptrs[ch * 2] | (ptrs[ch * 2 + 1] << 8));
			// The driver is phase-0 (org 0), so a raw pointer here is already the byte offset.
			entry.channels[ch] = MusicData::DecodeEventStream(SliceFrom(driver, ptr));
		}
		return entry;
	}

	MusicData::SfxEntry DecodeSfxEntryAsm(const Landstalker::Z80AsmFile& f, std::size_t offset)
	{
		MusicData::SfxEntry entry;
		const auto type_byte = f.ReadBytesAt(offset, 1);
		if (type_byte.empty())
		{
			return entry;
		}
		entry.type = type_byte[0];
		const std::size_t n = (entry.type == 1) ? MusicData::SFX_FULL_CHANNEL_COUNT : MusicData::SFX_OVERLAY_CHANNEL_COUNT;
		const auto ptrs = f.ReadBytesAt(offset + 1, n * 2);
		entry.channels.resize(n);
		for (std::size_t ch = 0; ch < n && ch * 2 + 1 < ptrs.size(); ++ch)
		{
			const std::size_t ch_offset = static_cast<std::size_t>(ptrs[ch * 2] | (ptrs[ch * 2 + 1] << 8));
			entry.channels[ch] = MusicData::DecodeEventStream(f.ReadBytesAt(ch_offset, MAX_EVENT_STREAM_SCAN));
		}
		return entry;
	}

	const std::vector<uint8_t> STOP_EVENT = { 0xFF, 0x00, 0x00 };

	bool IsSilentTrack(const MusicData::MusicTrack& t)
	{
		for (const auto& ch : t.channels)
		{
			if (MusicData::EncodeEventStream(ch) != STOP_EVENT)
			{
				return false;
			}
		}
		return true;
	}

	std::string DefaultMusicTrackName(std::size_t first_slot_id, const MusicData::MusicTrack& t)
	{
		return IsSilentTrack(t) ? "(Silent)" : ("Track " + HexId(first_slot_id) + "h");
	}

	bool IsNoopSfx(const MusicData::SfxEntry& e)
	{
		for (const auto& ch : e.channels)
		{
			if (MusicData::EncodeEventStream(ch) != STOP_EVENT)
			{
				return false;
			}
		}
		return true;
	}

	std::string DefaultSfxName(std::size_t first_slot_id, const MusicData::SfxEntry& e)
	{
		return IsNoopSfx(e) ? "(No effect)" : ("SFX " + HexId(first_slot_id) + "h");
	}

	// Writes one track's header + channel data to music/music{ID}.asm. Channels whose encoded
	// content is identical (e.g. the PSG noise channel often just reuses the PSG3 data) share a
	// single label and are only written once, mirroring the disassembly's own convention.
	bool WriteMusicTrackFile(const std::filesystem::path& full_path, const std::string& id_hex, const MusicData::MusicTrack& track)
	{
		Landstalker::Z80AsmFile f;
		f.WriteComment("Music Track " + id_hex);
		const std::vector<uint8_t> header = {
			0,
			static_cast<uint8_t>(track.autofade_frames & 0xFF),
			static_cast<uint8_t>((track.autofade_frames >> 8) & 0xFF),
			track.tempo
		};
		f.WriteBytes(header, header.size());

		std::vector<std::string> channel_labels(MusicData::MUSIC_CHANNEL_COUNT);
		std::map<std::vector<uint8_t>, std::string> seen;
		std::vector<std::pair<std::string, std::vector<uint8_t>>> blocks_to_write;
		for (std::size_t ch = 0; ch < MusicData::MUSIC_CHANNEL_COUNT; ++ch)
		{
			auto bytes = MusicData::EncodeEventStream(track.channels[ch]);
			const auto it = seen.find(bytes);
			if (it != seen.end())
			{
				channel_labels[ch] = it->second;
				continue;
			}
			const std::string label = "MUSIC_" + id_hex + "_" + MUSIC_CHANNEL_NAMES[ch];
			channel_labels[ch] = label;
			seen[bytes] = label;
			blocks_to_write.push_back({ label, std::move(bytes) });
		}

		f.WriteWordRefs(channel_labels);
		for (const auto& block : blocks_to_write)
		{
			f.WriteLabel(block.first);
			f.WriteBytes(block.second, 16);
		}
		return f.WriteFile(full_path);
	}

	// Writes one SFX entry's header + channel data to sfx/sfx{ID}_header.asm and _data.asm. A
	// channel whose content is just the 3-byte stop event ([FFh,0,0]) is named "_NOOP" and, like
	// any other identical content within the entry, only written once.
	bool WriteSfxEntryFiles(const std::filesystem::path& header_path, const std::filesystem::path& data_path,
		const std::string& id_hex, const MusicData::SfxEntry& entry)
	{
		Landstalker::Z80AsmFile header_file;
		header_file.WriteComment("SFX " + id_hex);
		header_file.WriteBytes({ entry.type }, 1);

		Landstalker::Z80AsmFile data_file;
		std::vector<std::string> channel_labels(entry.channels.size());
		std::map<std::vector<uint8_t>, std::string> seen;
		std::size_t next_channel_index = 1;
		for (std::size_t ch = 0; ch < entry.channels.size(); ++ch)
		{
			auto bytes = MusicData::EncodeEventStream(entry.channels[ch]);
			const auto it = seen.find(bytes);
			if (it != seen.end())
			{
				channel_labels[ch] = it->second;
				continue;
			}
			const std::string label = "SFX_" + id_hex + (bytes == STOP_EVENT ? "_NOOP" : ("_CH" + std::to_string(next_channel_index++)));
			channel_labels[ch] = label;
			seen[bytes] = label;
			data_file.WriteLabel(label);
			data_file.WriteBytes(bytes, 16);
		}
		header_file.WriteWordRefs(channel_labels);

		return header_file.WriteFile(header_path) && data_file.WriteFile(data_path);
	}
}

namespace Landstalker {

MusicData::EventStream MusicData::DecodeEventStream(const std::vector<uint8_t>& bytes)
{
	EventStream events;
	std::size_t i = 0;
	while (i < bytes.size())
	{
		const uint8_t b = bytes[i];
		if (b >= 0xF8)
		{
			SoundEvent ev;
			ev.is_command = true;
			ev.value = b;
			const uint8_t operand = (i + 1 < bytes.size()) ? bytes[i + 1] : 0;
			ev.operand.push_back(operand);
			if (b == 0xFF)
			{
				ev.operand.push_back((i + 2 < bytes.size()) ? bytes[i + 2] : 0);
				events.push_back(ev);
				break; // FFh (end/chain/jump) always terminates the stream
			}
			events.push_back(ev);
			i += 2;
			// F8h's "jump to a marker" sub-command (control byte A0h-BFh) is an unconditional
			// backward jump - the idiomatic way a track loops forever (see
			// docs/sound_driver_format.md section 5.4). Nothing physically after it is ever
			// reached, and (unlike a finite/counted loop) there is no fall-through case, so this
			// closes the stream the same way FFh does.
			if (b == 0xF8 && (operand & 0xE0) == 0xA0)
			{
				break;
			}
		}
		else
		{
			SoundEvent ev;
			ev.is_command = false;
			ev.has_duration = (b & 0x80) != 0;
			ev.value = b & 0x7F;
			i += 1;
			if (ev.has_duration)
			{
				ev.duration = (i < bytes.size()) ? bytes[i] : 0;
				i += 1;
			}
			events.push_back(ev);
		}
	}
	return events;
}

double MusicData::GetTempoHz(uint8_t tempo)
{
	constexpr double YM2612_TIMER_B_PERIOD_CONSTANT_US = 300.37; // NTSC clock timing constant
	const double timer_b = static_cast<double>(tempo) + 3.0;
	const double frame_duration_us = YM2612_TIMER_B_PERIOD_CONSTANT_US * (256.0 - timer_b);
	return (frame_duration_us > 0.0) ? (1000000.0 / frame_duration_us) : 0.0;
}

std::vector<uint8_t> MusicData::EncodeEventStream(const EventStream& events)
{
	std::vector<uint8_t> out;
	for (const auto& ev : events)
	{
		if (ev.is_command)
		{
			out.push_back(ev.value);
			out.insert(out.end(), ev.operand.begin(), ev.operand.end());
		}
		else
		{
			out.push_back(static_cast<uint8_t>((ev.value & 0x7F) | (ev.has_duration ? 0x80 : 0)));
			if (ev.has_duration)
			{
				out.push_back(ev.duration);
			}
		}
	}
	return out;
}

MusicData::MusicData(const std::filesystem::path& asm_file)
	: DataManager("Music Data", asm_file)
{
	if (!AsmLoadMusic())
	{
		throw std::runtime_error(std::string("Unable to load music data from \'") + asm_file.string() + '\'');
	}
	if (!AsmLoadSfx())
	{
		throw std::runtime_error(std::string("Unable to load SFX data from \'") + asm_file.string() + '\'');
	}
	InitCache();
}

MusicData::MusicData(const Rom& rom)
	: DataManager("Music Data", rom)
{
	if (!RomLoadMusic(rom))
	{
		throw std::runtime_error(std::string("Unable to load music data from ROM"));
	}
	if (!RomLoadSfx(rom))
	{
		throw std::runtime_error(std::string("Unable to load SFX data from ROM"));
	}
	InitCache();
}

bool MusicData::Save(const std::filesystem::path& dir)
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
	if (!AsmSaveMusic(directory))
	{
		throw std::runtime_error(std::string("Unable to save music data to \'") + directory.string() + '\'');
	}
	if (!AsmSaveSfx(directory))
	{
		throw std::runtime_error(std::string("Unable to save SFX data to \'") + directory.string() + '\'');
	}
	CommitAllChanges();
	return true;
}

bool MusicData::Save()
{
	return Save(GetBasePath());
}

bool MusicData::HasBeenModified() const
{
	return m_music_pool != m_music_pool_orig || m_music_slot_map != m_music_slot_map_orig
		|| m_sfx_pool != m_sfx_pool_orig || m_sfx_slot_map != m_sfx_slot_map_orig;
}

const std::vector<MusicData::MusicTrackEntry>& MusicData::GetMusicTrackPool() const
{
	return m_music_pool;
}

void MusicData::SetMusicTrackPool(const std::vector<MusicTrackEntry>& pool)
{
	m_music_pool = pool;
}

const std::vector<std::size_t>& MusicData::GetMusicSlotMap() const
{
	return m_music_slot_map;
}

void MusicData::SetMusicSlotMap(const std::vector<std::size_t>& map)
{
	m_music_slot_map = map;
}

const std::vector<MusicData::SfxPoolEntry>& MusicData::GetSfxPool() const
{
	return m_sfx_pool;
}

void MusicData::SetSfxPool(const std::vector<SfxPoolEntry>& pool)
{
	m_sfx_pool = pool;
}

const std::vector<std::size_t>& MusicData::GetSfxSlotMap() const
{
	return m_sfx_slot_map;
}

void MusicData::SetSfxSlotMap(const std::vector<std::size_t>& map)
{
	m_sfx_slot_map = map;
}

void MusicData::CommitAllChanges()
{
	m_music_pool_orig = m_music_pool;
	m_music_slot_map_orig = m_music_slot_map;
	m_sfx_pool_orig = m_sfx_pool;
	m_sfx_slot_map_orig = m_sfx_slot_map;
	m_pending_writes.clear();
}

bool MusicData::CreateDirectoryStructure(const std::filesystem::path& dir)
{
	bool retval = true;
	retval = retval && CreateDirectoryTree(dir / RomLabels::Audio::MUSIC_BANK_3_FILE);
	retval = retval && CreateDirectoryTree(dir / RomLabels::Audio::MUSIC_BANK_4_FILE);
	retval = retval && CreateDirectoryTree(dir / RomLabels::Audio::SFX_FILE);
	retval = retval && CreateDirectoryTree(dir / "code/audio/music/x.asm");
	retval = retval && CreateDirectoryTree(dir / "code/audio/sfx/x.asm");
	return retval;
}

void MusicData::InitCache()
{
	m_music_pool_orig = m_music_pool;
	m_music_slot_map_orig = m_music_slot_map;
	m_sfx_pool_orig = m_sfx_pool;
	m_sfx_slot_map_orig = m_sfx_slot_map;
}

void MusicData::BuildMusicPoolFromSlots(const std::vector<MusicTrack>& slots)
{
	m_music_pool.clear();
	m_music_slot_map.assign(slots.size(), 0);
	for (std::size_t i = 0; i < slots.size(); ++i)
	{
		std::size_t pool_index = m_music_pool.size();
		for (std::size_t p = 0; p < m_music_pool.size(); ++p)
		{
			if (m_music_pool[p].track == slots[i])
			{
				pool_index = p;
				break;
			}
		}
		if (pool_index == m_music_pool.size())
		{
			m_music_pool.push_back({ DefaultMusicTrackName(i, slots[i]), slots[i] });
		}
		m_music_slot_map[i] = pool_index;
	}
}

void MusicData::BuildSfxPoolFromSlots(const std::vector<SfxEntry>& slots)
{
	m_sfx_pool.clear();
	m_sfx_slot_map.assign(slots.size(), 0);
	for (std::size_t i = 0; i < slots.size(); ++i)
	{
		std::size_t pool_index = m_sfx_pool.size();
		for (std::size_t p = 0; p < m_sfx_pool.size(); ++p)
		{
			if (m_sfx_pool[p].entry == slots[i])
			{
				pool_index = p;
				break;
			}
		}
		if (pool_index == m_sfx_pool.size())
		{
			// SFX ids start at 41h.
			m_sfx_pool.push_back({ DefaultSfxName(i + 0x41, slots[i]), slots[i] });
		}
		m_sfx_slot_map[i] = pool_index;
	}
}

bool MusicData::AsmLoadMusic()
{
	std::vector<MusicTrack> slots;
	slots.reserve(RomLabels::Audio::MUSIC_TABLE_ENTRY_COUNT * 2);

	Z80AsmFile bank4(GetBasePath() / RomLabels::Audio::MUSIC_BANK_4_FILE);
	if (!bank4.Good())
	{
		return false;
	}
	const auto table4 = bank4.ReadBytesAt(RomLabels::Audio::MUSIC_BANK_4_TABLE_ASM_OFFSET, RomLabels::Audio::MUSIC_TABLE_ENTRY_COUNT * 2);
	for (std::size_t i = 0; i + 1 < table4.size(); i += 2)
	{
		const std::size_t offset = static_cast<std::size_t>(table4[i] | (table4[i + 1] << 8));
		slots.push_back(DecodeMusicTrackAsm(bank4, offset));
	}

	Z80AsmFile bank3(GetBasePath() / RomLabels::Audio::MUSIC_BANK_3_FILE);
	if (!bank3.Good())
	{
		return false;
	}
	const auto table3 = bank3.ReadBytesAt(0, RomLabels::Audio::MUSIC_TABLE_ENTRY_COUNT * 2);
	for (std::size_t i = 0; i + 1 < table3.size(); i += 2)
	{
		const std::size_t offset = static_cast<std::size_t>(table3[i] | (table3[i + 1] << 8));
		slots.push_back(DecodeMusicTrackAsm(bank3, offset));
	}
	BuildMusicPoolFromSlots(slots);
	return true;
}

bool MusicData::AsmLoadSfx()
{
	Z80AsmFile f(GetBasePath() / RomLabels::Audio::SFX_FILE);
	if (!f.Good() || !f.Goto(RomLabels::Audio::SFX_TABLE_LABEL))
	{
		return false;
	}
	const auto table = f.ReadBytes(RomLabels::Audio::SFX_TABLE_ENTRY_COUNT * 2);
	std::vector<SfxEntry> slots;
	slots.reserve(table.size() / 2);
	for (std::size_t i = 0; i + 1 < table.size(); i += 2)
	{
		const std::size_t offset = static_cast<std::size_t>(table[i] | (table[i + 1] << 8));
		slots.push_back(DecodeSfxEntryAsm(f, offset));
	}
	BuildSfxPoolFromSlots(slots);
	return true;
}

bool MusicData::RomLoadMusic(const Rom& rom)
{
	std::vector<MusicTrack> slots;
	slots.reserve(RomLabels::Audio::MUSIC_TABLE_ENTRY_COUNT * 2);

	auto load_bank = [&](const std::string& table_section, const std::string& bank_section)
	{
		auto table_sec = rom.get_section(table_section);
		auto table_bytes = rom.read_array<uint8_t>(table_sec.begin, table_sec.size());
		auto bank_sec = rom.get_section(bank_section);
		auto bank_bytes = rom.read_array<uint8_t>(bank_sec.begin, bank_sec.size());
		for (std::size_t i = 0; i + 1 < table_bytes.size(); i += 2)
		{
			const uint16_t ptr = static_cast<uint16_t>(table_bytes[i] | (table_bytes[i + 1] << 8));
			const std::size_t offset = static_cast<std::size_t>(static_cast<uint16_t>(ptr - BANK_PTR_BASE));
			slots.push_back(DecodeMusicTrackFromBank(bank_bytes, offset, true));
		}
	};
	load_bank(RomLabels::Audio::MUSIC_BANK_4_TABLE_SECTION, RomLabels::Audio::MUSIC_BANK_4_SECTION);
	load_bank(RomLabels::Audio::MUSIC_BANK_3_TABLE_SECTION, RomLabels::Audio::MUSIC_BANK_3_SECTION);
	BuildMusicPoolFromSlots(slots);
	return true;
}

bool MusicData::RomLoadSfx(const Rom& rom)
{
	auto table_sec = rom.get_section(RomLabels::Audio::SFX_TABLE_SECTION);
	auto table_bytes = rom.read_array<uint8_t>(table_sec.begin, table_sec.size());
	auto driver_sec = rom.get_section(RomLabels::Audio::SOUND_DRIVER_SECTION);
	auto driver_bytes = rom.read_array<uint8_t>(driver_sec.begin, driver_sec.size());

	std::vector<SfxEntry> slots;
	slots.reserve(table_bytes.size() / 2);
	for (std::size_t i = 0; i + 1 < table_bytes.size(); i += 2)
	{
		const uint16_t ptr = static_cast<uint16_t>(table_bytes[i] | (table_bytes[i + 1] << 8));
		slots.push_back(DecodeSfxEntryFromDriver(driver_bytes, ptr));
	}
	BuildSfxPoolFromSlots(slots);
	return true;
}

bool MusicData::AsmSaveMusic(const std::filesystem::path& dir)
{
	if (m_music_slot_map.size() != MUSIC_SLOT_COUNT)
	{
		return false;
	}

	// Each bank only gets the pool entries its own slots actually reference (per-bank dedup, keyed
	// by pool index) - the same pool entry referenced from both banks is written into both, since
	// they're physically separate memory windows and can't share a single copy.
	const auto write_bank = [&](std::size_t start_index, const std::filesystem::path& bank_file,
		bool has_ym_instrument_preamble) -> bool
	{
		const std::size_t count = RomLabels::Audio::MUSIC_TABLE_ENTRY_COUNT;
		std::vector<std::string> table_labels(count);
		std::map<std::size_t, std::string> pool_index_to_label;
		std::vector<std::pair<std::string, std::string>> includes; // (label, id_hex), first-use order
		for (std::size_t k = 0; k < count; ++k)
		{
			const std::size_t idx = start_index + k;
			const std::size_t pool_index = m_music_slot_map[idx];
			const auto it = pool_index_to_label.find(pool_index);
			if (it != pool_index_to_label.end())
			{
				table_labels[k] = it->second;
				continue;
			}
			if (pool_index >= m_music_pool.size())
			{
				return false;
			}
			const std::string id_hex = HexId(idx);
			const std::string label = "MUSIC_" + id_hex;
			const std::filesystem::path track_path = dir / "code/audio/music" / ("music" + id_hex + ".asm");
			if (!WriteMusicTrackFile(track_path, id_hex, m_music_pool[pool_index].track))
			{
				return false;
			}
			pool_index_to_label[pool_index] = label;
			includes.push_back({ label, id_hex });
			table_labels[k] = label;
		}

		Z80AsmFile f;
		f.WriteComment("Sound Bank (regenerated)");
		f.WriteRaw("\t\tcpu z80");
		f.WriteRaw("\t\tlisting\toff");
		f.WriteRaw("\t\tphase\t0");
		f.WriteRaw("\t\torg 8000h");
		f.WriteRaw("");
		if (has_ym_instrument_preamble)
		{
			f.WriteRaw("\t\tinclude \"ym_instruments.asm\"");
			f.WriteRaw("\t\torg 8910h");
			f.WriteRaw("");
		}
		f.WriteWordRefs(table_labels);
		f.WriteRaw("");
		for (const auto& include : includes)
		{
			f.WriteRaw(include.first + ":\tinclude \"music/music" + include.second + ".asm\"");
		}
		f.WriteRaw("\t\tend");
		return f.WriteFile(dir / bank_file);
	};

	return write_bank(0, RomLabels::Audio::MUSIC_BANK_4_FILE, true)
		&& write_bank(RomLabels::Audio::MUSIC_TABLE_ENTRY_COUNT, RomLabels::Audio::MUSIC_BANK_3_FILE, false);
}

bool MusicData::AsmSaveSfx(const std::filesystem::path& dir)
{
	if (m_sfx_slot_map.size() != SFX_SLOT_COUNT)
	{
		return false;
	}

	std::vector<std::string> table_labels(SFX_SLOT_COUNT);
	std::map<std::size_t, std::string> pool_index_to_label;
	std::vector<std::pair<std::string, std::string>> includes;
	for (std::size_t i = 0; i < SFX_SLOT_COUNT; ++i)
	{
		const std::size_t pool_index = m_sfx_slot_map[i];
		const auto it = pool_index_to_label.find(pool_index);
		if (it != pool_index_to_label.end())
		{
			table_labels[i] = it->second;
			continue;
		}
		if (pool_index >= m_sfx_pool.size())
		{
			return false;
		}
		const std::string id_hex = HexId(i + 1);
		const std::string label = "SFX_" + id_hex;
		const std::filesystem::path header_path = dir / "code/audio/sfx" / ("sfx" + id_hex + "_header.asm");
		const std::filesystem::path data_path = dir / "code/audio/sfx" / ("sfx" + id_hex + "_data.asm");
		if (!WriteSfxEntryFiles(header_path, data_path, id_hex, m_sfx_pool[pool_index].entry))
		{
			return false;
		}
		pool_index_to_label[pool_index] = label;
		includes.push_back({ label, id_hex });
		table_labels[i] = label;
	}

	Z80AsmFile f;
	f.WriteLabel(RomLabels::Audio::SFX_TABLE_LABEL);
	f.WriteWordRefs(table_labels);
	f.WriteRaw("");
	for (const auto& include : includes)
	{
		f.WriteRaw(include.first + ":\tinclude \"sfx/sfx" + include.second + "_header.asm\"");
		f.WriteRaw("\t\tinclude \"sfx/sfx" + include.second + "_data.asm\"");
	}
	return f.WriteFile(dir / RomLabels::Audio::SFX_FILE);
}

} // namespace Landstalker
