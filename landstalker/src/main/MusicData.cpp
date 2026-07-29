#include <landstalker/main/MusicData.h>

#include <algorithm>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

#include <landstalker/main/RomLabels.h>
#include <landstalker/misc/Labels.h>
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

	// The label/pointer targets after `start`, as offsets relative to it - DecodeEventStream's
	// stop_offsets. Only marks where a stream terminated by a backward jump ends; a stream with no
	// terminator of its own decodes straight through them (see DecodeEventStream).
	std::vector<std::size_t> RelativeStops(const std::vector<std::size_t>& sorted_boundaries, std::size_t start)
	{
		std::vector<std::size_t> out;
		for (auto it = std::upper_bound(sorted_boundaries.begin(), sorted_boundaries.end(), start);
			it != sorted_boundaries.end() && *it - start <= MAX_EVENT_STREAM_SCAN; ++it)
		{
			out.push_back(*it - start);
		}
		return out;
	}

	std::vector<std::size_t> LabelStops(const Landstalker::Z80AsmFile& f, std::size_t start)
	{
		std::vector<std::size_t> out;
		for (std::size_t next = f.NextLabelOffsetAfter(start);
			next - start <= MAX_EVENT_STREAM_SCAN; next = f.NextLabelOffsetAfter(next))
		{
			out.push_back(next - start);
			if (next >= f.ToBinary().size())
			{
				break;
			}
		}
		return out;
	}

	std::size_t MusicChannelOffset(const std::vector<uint8_t>& header, std::size_t ch, bool subtract_bank_base)
	{
		const std::size_t p = 4 + ch * 2;
		const uint16_t ptr = static_cast<uint16_t>(header[p] | (header[p + 1] << 8));
		return subtract_bank_base
			? static_cast<std::size_t>(static_cast<uint16_t>(ptr - BANK_PTR_BASE))
			: static_cast<std::size_t>(ptr);
	}

	MusicData::MusicTrack DecodeMusicTrackFromBank(const std::vector<uint8_t>& bank, std::size_t offset,
		bool subtract_bank_base, const std::vector<std::size_t>& boundaries)
	{
		MusicData::MusicTrack track;
		const std::size_t header_size = 4 + MusicData::MUSIC_CHANNEL_COUNT * 2;
		if (offset + header_size > bank.size())
		{
			return track;
		}
		const std::vector<uint8_t> header(bank.begin() + offset, bank.begin() + offset + header_size);
		track.autofade_frames = static_cast<uint16_t>(header[1] | (header[2] << 8));
		track.tempo = header[3];
		for (std::size_t ch = 0; ch < MusicData::MUSIC_CHANNEL_COUNT; ++ch)
		{
			const std::size_t ch_offset = MusicChannelOffset(header, ch, subtract_bank_base);
			track.channels[ch] = MusicData::DecodeEventStream(SliceFrom(bank, ch_offset),
				RelativeStops(boundaries, ch_offset));
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
			const std::size_t ch_offset = MusicChannelOffset(header, ch, false);
			track.channels[ch] = MusicData::DecodeEventStream(f.ReadBytesAt(ch_offset, MAX_EVENT_STREAM_SCAN),
				LabelStops(f, ch_offset));
		}
		return track;
	}

	std::vector<std::size_t> SfxChannelOffsets(const std::vector<uint8_t>& driver, std::size_t offset)
	{
		std::vector<std::size_t> out;
		if (offset >= driver.size())
		{
			return out;
		}
		const std::size_t n = (driver[offset] == 1) ? MusicData::SFX_FULL_CHANNEL_COUNT : MusicData::SFX_OVERLAY_CHANNEL_COUNT;
		for (std::size_t ch = 0; ch < n && offset + 1 + ch * 2 + 1 < driver.size(); ++ch)
		{
			const std::size_t p = offset + 1 + ch * 2;
			// The driver is phase-0 (org 0), so a raw pointer here is already the byte offset.
			out.push_back(static_cast<std::size_t>(driver[p] | (driver[p + 1] << 8)));
		}
		return out;
	}

	MusicData::SfxEntry DecodeSfxEntryFromDriver(const std::vector<uint8_t>& driver, std::size_t offset,
		const std::vector<std::size_t>& boundaries)
	{
		MusicData::SfxEntry entry;
		if (offset >= driver.size())
		{
			return entry;
		}
		entry.type = driver[offset];
		const auto ch_offsets = SfxChannelOffsets(driver, offset);
		const std::size_t n = (entry.type == 1) ? MusicData::SFX_FULL_CHANNEL_COUNT : MusicData::SFX_OVERLAY_CHANNEL_COUNT;
		entry.channels.resize(n);
		for (std::size_t ch = 0; ch < ch_offsets.size(); ++ch)
		{
			entry.channels[ch] = MusicData::DecodeEventStream(SliceFrom(driver, ch_offsets[ch]),
				RelativeStops(boundaries, ch_offsets[ch]));
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
			entry.channels[ch] = MusicData::DecodeEventStream(f.ReadBytesAt(ch_offset, MAX_EVENT_STREAM_SCAN),
				LabelStops(f, ch_offset));
		}
		return entry;
	}

	MusicData::YmInstrumentTable YmInstrumentsFromBytes(const std::vector<uint8_t>& bytes)
	{
		MusicData::YmInstrumentTable table = {};
		for (std::size_t i = 0; i < MusicData::YM_INSTRUMENT_COUNT; ++i)
		{
			for (std::size_t b = 0; b < MusicData::YM_INSTRUMENT_SIZE; ++b)
			{
				const std::size_t pos = i * MusicData::YM_INSTRUMENT_SIZE + b;
				table[i][b] = (pos < bytes.size()) ? bytes[pos] : 0;
			}
		}
		return table;
	}

	std::vector<uint8_t> YmInstrumentsToBytes(const MusicData::YmInstrumentTable& table)
	{
		std::vector<uint8_t> out;
		out.reserve(MusicData::YM_INSTRUMENT_COUNT * MusicData::YM_INSTRUMENT_SIZE);
		for (const auto& instrument : table)
		{
			out.insert(out.end(), instrument.begin(), instrument.end());
		}
		return out;
	}

	// Writes the instrument table to code/audio/ym_instruments.asm, one YM_INSTMT_NN label per
	// patch - the labels are what landstalker_tools_python's instrument extractor keys off, so
	// they're preserved even though the driver itself only ever indexes the table by id * 29.
	bool WriteYmInstrumentsFile(const std::filesystem::path& full_path, const MusicData::YmInstrumentTable& table)
	{
		Landstalker::Z80AsmFile f;
		f.WriteComment("FM (YM2612) instrument patches, YM_INSTMT_00 .. YM_INSTMT_4F.");
		f.WriteComment("29 bytes each: seven groups of four per-operator register values, in");
		f.WriteComment("YM2612 register order (regs 30h/40h/50h/60h/70h/80h/90h, operator slots");
		f.WriteComment("S1, S3, S2, S4), then one feedback/algorithm byte (reg B0h).");
		for (std::size_t i = 0; i < MusicData::YM_INSTRUMENT_COUNT; ++i)
		{
			f.WriteLabel("YM_INSTMT_" + HexId(i));
			f.WriteBytes(std::vector<uint8_t>(table[i].begin(), table[i].end()), 16);
		}
		return f.WriteFile(full_path);
	}

	uint16_t ReadWordLE(const std::vector<uint8_t>& bytes, std::size_t pos)
	{
		const uint8_t lo = (pos < bytes.size()) ? bytes[pos] : 0;
		const uint8_t hi = (pos + 1 < bytes.size()) ? bytes[pos + 1] : 0;
		return static_cast<uint16_t>(lo | (hi << 8));
	}

	// Relative positions of the fixed tables within the instrument-params region (both the ASM
	// file's data stream and the ROM section share this layout; the two pointer tables and their
	// variable-length data follow).
	constexpr std::size_t YM_FREQUENCIES_OFFSET = 0;
	constexpr std::size_t PSG_FREQUENCIES_OFFSET = YM_FREQUENCIES_OFFSET + MusicData::YM_FREQUENCY_COUNT * 2;
	constexpr std::size_t YM_LEVELS_OFFSET = PSG_FREQUENCIES_OFFSET + MusicData::PSG_FREQUENCY_COUNT * 2;
	constexpr std::size_t SLOTS_PER_ALGO_OFFSET = YM_LEVELS_OFFSET + MusicData::YM_LEVEL_COUNT;
	constexpr std::size_t PITCH_EFFECT_TABLE_OFFSET = SLOTS_PER_ALGO_OFFSET + MusicData::ALGO_COUNT;

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
		if (IsSilentTrack(t))
		{
			return "(Silent)";
		}
		// Labels' bgm names already carry their "[NN] " id prefix, so they're used verbatim.
		const auto label = Landstalker::Labels::Get(Landstalker::Labels::C_BGMS, static_cast<int>(first_slot_id));
		return label ? Landstalker::wstr_to_utf8(*label) : ("Track " + HexId(first_slot_id) + "h");
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
		if (IsNoopSfx(e))
		{
			return "(No effect)";
		}
		// As with the bgm names, sound labels carry their own "<NN> " id prefix.
		const auto label = Landstalker::Labels::Get(Landstalker::Labels::C_SOUNDS, static_cast<int>(first_slot_id));
		return label ? Landstalker::wstr_to_utf8(*label) : ("SFX " + HexId(first_slot_id) + "h");
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

MusicData::EventStream MusicData::DecodeEventStream(const std::vector<uint8_t>& bytes,
	const std::vector<std::size_t>& stop_offsets)
{
	EventStream events;
	std::size_t i = 0;
	bool seen_play_once = false;
	const auto is_stop_offset = [&](std::size_t pos)
	{
		return std::binary_search(stop_offsets.begin(), stop_offsets.end(), pos);
	};
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
			if (b == 0xF8)
			{
				const uint8_t sub = operand & 0xE0;
				if (sub == 0x40 || sub == 0x60)
				{
					// A play-once section opener (docs/sound_driver_format.md section 5.4). Its
					// presence means bytes after a jump-to-marker can still be reached: on repeat
					// passes the driver skips forward from here to the section's closing marker,
					// which may lie beyond the jump (this is how a channel plays one continuation
					// on the first pass and a different one on later passes).
					seen_play_once = true;
				}
				else if (sub == 0xA0 && (!seen_play_once || is_stop_offset(i)))
				{
					// Jump to a marker: an unconditional backward jump - the idiomatic way a track
					// loops forever. With no play-once section open, nothing after it is reachable,
					// so it closes the stream the same way FFh does. With one open, decoding keeps
					// going (a play-once skip can land past the jump) until a jump butts up against
					// the next label/pointer target - that stream's true end.
					break;
				}
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

std::size_t MusicData::GetMusicTrackSize(const MusicTrack& track)
{
	std::size_t size = 4 + MUSIC_CHANNEL_COUNT * 2;
	std::set<std::vector<uint8_t>> seen;
	for (const auto& channel : track.channels)
	{
		auto bytes = EncodeEventStream(channel);
		if (seen.insert(bytes).second)
		{
			size += bytes.size();
		}
	}
	return size;
}

std::pair<std::size_t, std::size_t> MusicData::GetMusicBankUsage(std::size_t bank) const
{
	constexpr std::size_t BANK_CAPACITY = 0x8000;
	std::size_t used = RomLabels::Audio::MUSIC_TABLE_ENTRY_COUNT * 2;
	if (bank == 0)
	{
		used += YM_INSTRUMENT_COUNT * YM_INSTRUMENT_SIZE; // the instrument table shares bank 4
	}
	std::set<std::size_t> pool_indices;
	const std::size_t first = bank * RomLabels::Audio::MUSIC_TABLE_ENTRY_COUNT;
	for (std::size_t i = first; i < first + RomLabels::Audio::MUSIC_TABLE_ENTRY_COUNT && i < m_music_slot_map.size(); ++i)
	{
		pool_indices.insert(m_music_slot_map[i]);
	}
	for (const std::size_t index : pool_indices)
	{
		if (index < m_music_pool.size())
		{
			used += GetMusicTrackSize(m_music_pool[index].track);
		}
	}
	return { used, BANK_CAPACITY };
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
	if (!AsmLoadInstrumentParams())
	{
		throw std::runtime_error(std::string("Unable to load instrument parameters from \'") + asm_file.string() + '\'');
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
	if (!RomLoadInstrumentParams(rom))
	{
		throw std::runtime_error(std::string("Unable to load instrument parameters from ROM"));
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
	if (!AsmSaveInstrumentParams(directory))
	{
		throw std::runtime_error(std::string("Unable to save instrument parameters to \'") + directory.string() + '\'');
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
		|| m_sfx_pool != m_sfx_pool_orig || m_sfx_slot_map != m_sfx_slot_map_orig
		|| m_ym_instruments != m_ym_instruments_orig
		|| m_instrument_params != m_instrument_params_orig;
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

const MusicData::YmInstrumentTable& MusicData::GetYmInstruments() const
{
	return m_ym_instruments;
}

void MusicData::SetYmInstruments(const YmInstrumentTable& instruments)
{
	m_ym_instruments = instruments;
}

const MusicData::InstrumentParams& MusicData::GetInstrumentParams() const
{
	return m_instrument_params;
}

void MusicData::SetInstrumentParams(const InstrumentParams& params)
{
	m_instrument_params = params;
}

void MusicData::RefreshPendingWrites(const Rom& rom)
{
	DataManager::RefreshPendingWrites(rom);
	// The blob is shorter than the section, so this only overwrites the instrument region at the
	// start of the bank - the pointer table and track data after it are untouched.
	m_pending_writes.push_back({ RomLabels::Audio::MUSIC_BANK_4_SECTION,
		std::make_shared<ByteVector>(YmInstrumentsToBytes(m_ym_instruments)) });

	// The instrument-params tables re-laid as one contiguous blob, zero-padded to the section
	// size. If edits have grown the variable-length tables past the section, the blob is left
	// oversized so WillFitInRom() reports the problem instead of truncating.
	const auto instr_sec = rom.get_section(RomLabels::Audio::INSTRUMENT_PARAMS_SECTION);
	const auto driver_sec = rom.get_section(RomLabels::Audio::SOUND_DRIVER_SECTION);
	const std::size_t ptr_base = instr_sec.begin - driver_sec.begin;
	ByteVector blob(PITCH_EFFECT_TABLE_OFFSET, 0);
	for (std::size_t i = 0; i < YM_FREQUENCY_COUNT; ++i)
	{
		blob[YM_FREQUENCIES_OFFSET + i * 2] = static_cast<uint8_t>(m_instrument_params.ym_frequencies[i] & 0xFF);
		blob[YM_FREQUENCIES_OFFSET + i * 2 + 1] = static_cast<uint8_t>(m_instrument_params.ym_frequencies[i] >> 8);
	}
	for (std::size_t i = 0; i < PSG_FREQUENCY_COUNT; ++i)
	{
		blob[PSG_FREQUENCIES_OFFSET + i * 2] = static_cast<uint8_t>(m_instrument_params.psg_frequencies[i] & 0xFF);
		blob[PSG_FREQUENCIES_OFFSET + i * 2 + 1] = static_cast<uint8_t>(m_instrument_params.psg_frequencies[i] >> 8);
	}
	std::copy(m_instrument_params.ym_levels.begin(), m_instrument_params.ym_levels.end(), blob.begin() + YM_LEVELS_OFFSET);
	std::copy(m_instrument_params.slots_per_algo.begin(), m_instrument_params.slots_per_algo.end(), blob.begin() + SLOTS_PER_ALGO_OFFSET);

	const auto append_pointer_table_and_data = [&](const auto& lists)
	{
		const std::size_t table_pos = blob.size();
		blob.resize(blob.size() + lists.size() * 2);
		std::size_t data_pos = blob.size();
		for (std::size_t i = 0; i < lists.size(); ++i)
		{
			const uint16_t target = static_cast<uint16_t>(ptr_base + data_pos);
			blob[table_pos + i * 2] = static_cast<uint8_t>(target & 0xFF);
			blob[table_pos + i * 2 + 1] = static_cast<uint8_t>(target >> 8);
			blob.insert(blob.end(), lists[i].begin(), lists[i].end());
			data_pos = blob.size();
		}
	};
	append_pointer_table_and_data(m_instrument_params.pitch_effects);
	append_pointer_table_and_data(m_instrument_params.psg_envelopes);
	if (blob.size() < instr_sec.size())
	{
		blob.resize(instr_sec.size(), 0);
	}
	m_pending_writes.push_back({ RomLabels::Audio::INSTRUMENT_PARAMS_SECTION,
		std::make_shared<ByteVector>(std::move(blob)) });
}

void MusicData::CommitAllChanges()
{
	m_music_pool_orig = m_music_pool;
	m_music_slot_map_orig = m_music_slot_map;
	m_sfx_pool_orig = m_sfx_pool;
	m_sfx_slot_map_orig = m_sfx_slot_map;
	m_ym_instruments_orig = m_ym_instruments;
	m_instrument_params_orig = m_instrument_params;
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
	m_ym_instruments_orig = m_ym_instruments;
	m_instrument_params_orig = m_instrument_params;
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
	// The instrument table occupies the first 910h bytes of the bank (org 8000h puts the data
	// stream's offset 0 at the bank window base) - the music pointer table follows it.
	m_ym_instruments = YmInstrumentsFromBytes(bank4.ReadBytesAt(0, YM_INSTRUMENT_COUNT * YM_INSTRUMENT_SIZE));

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

bool MusicData::AsmLoadInstrumentParams()
{
	Z80AsmFile f(GetBasePath() / RomLabels::Audio::INSTRUMENT_PARAMS_FILE);
	if (!f.Good())
	{
		return false;
	}
	const auto read_slice_at_label = [&](const std::string& label, std::vector<uint8_t>& out) -> bool
	{
		std::size_t offset = 0;
		if (!f.GetLabelOffset(label, offset))
		{
			return false;
		}
		out = f.ReadBytesAt(offset, f.NextLabelOffsetAfter(offset) - offset);
		return true;
	};

	std::vector<uint8_t> bytes;
	if (!read_slice_at_label("t_YM_FREQUENCIES", bytes) || bytes.size() < YM_FREQUENCY_COUNT * 2)
	{
		return false;
	}
	for (std::size_t i = 0; i < YM_FREQUENCY_COUNT; ++i)
	{
		m_instrument_params.ym_frequencies[i] = ReadWordLE(bytes, i * 2);
	}
	if (!read_slice_at_label("t_PSG_FREQUENCIES", bytes) || bytes.size() < PSG_FREQUENCY_COUNT * 2)
	{
		return false;
	}
	for (std::size_t i = 0; i < PSG_FREQUENCY_COUNT; ++i)
	{
		m_instrument_params.psg_frequencies[i] = ReadWordLE(bytes, i * 2);
	}
	if (!read_slice_at_label("t_YM_LEVELS", bytes) || bytes.size() < YM_LEVEL_COUNT)
	{
		return false;
	}
	std::copy_n(bytes.begin(), YM_LEVEL_COUNT, m_instrument_params.ym_levels.begin());
	if (!read_slice_at_label("t_SLOTS_PER_ALGO", bytes) || bytes.size() < ALGO_COUNT)
	{
		return false;
	}
	std::copy_n(bytes.begin(), ALGO_COUNT, m_instrument_params.slots_per_algo.begin());
	for (std::size_t i = 0; i < PITCH_EFFECT_COUNT; ++i)
	{
		if (!read_slice_at_label("t_PITCH_EFFECT_" + std::to_string(i), m_instrument_params.pitch_effects[i]))
		{
			return false;
		}
	}
	for (std::size_t i = 0; i < PSG_ENVELOPE_COUNT; ++i)
	{
		if (!read_slice_at_label("t_PSG_INSTRUMENT_" + std::to_string(i), m_instrument_params.psg_envelopes[i]))
		{
			return false;
		}
	}
	return true;
}

bool MusicData::RomLoadInstrumentParams(const Rom& rom)
{
	const auto instr_sec = rom.get_section(RomLabels::Audio::INSTRUMENT_PARAMS_SECTION);
	const auto driver_sec = rom.get_section(RomLabels::Audio::SOUND_DRIVER_SECTION);
	const auto bytes = rom.read_array<uint8_t>(instr_sec.begin, instr_sec.size());
	// Driver pointers are Z80 addresses with the driver at org 0, so a pointer value minus the
	// section's own offset into the driver is a position within `bytes`.
	const std::size_t ptr_base = instr_sec.begin - driver_sec.begin;

	for (std::size_t i = 0; i < YM_FREQUENCY_COUNT; ++i)
	{
		m_instrument_params.ym_frequencies[i] = ReadWordLE(bytes, YM_FREQUENCIES_OFFSET + i * 2);
	}
	for (std::size_t i = 0; i < PSG_FREQUENCY_COUNT; ++i)
	{
		m_instrument_params.psg_frequencies[i] = ReadWordLE(bytes, PSG_FREQUENCIES_OFFSET + i * 2);
	}
	for (std::size_t i = 0; i < YM_LEVEL_COUNT; ++i)
	{
		m_instrument_params.ym_levels[i] = (YM_LEVELS_OFFSET + i < bytes.size()) ? bytes[YM_LEVELS_OFFSET + i] : 0;
	}
	for (std::size_t i = 0; i < ALGO_COUNT; ++i)
	{
		m_instrument_params.slots_per_algo[i] = (SLOTS_PER_ALGO_OFFSET + i < bytes.size()) ? bytes[SLOTS_PER_ALGO_OFFSET + i] : 0;
	}

	std::vector<std::size_t> pitch_targets(PITCH_EFFECT_COUNT);
	for (std::size_t i = 0; i < PITCH_EFFECT_COUNT; ++i)
	{
		pitch_targets[i] = ReadWordLE(bytes, PITCH_EFFECT_TABLE_OFFSET + i * 2) - ptr_base;
	}
	// The PSG envelope pointer table follows the last pitch-effect waveform, which (like every
	// waveform) ends at its 80h/81h control byte.
	std::size_t psg_table_pos = *std::max_element(pitch_targets.begin(), pitch_targets.end());
	while (psg_table_pos < bytes.size() && bytes[psg_table_pos] != 0x80 && bytes[psg_table_pos] != 0x81)
	{
		++psg_table_pos;
	}
	++psg_table_pos;
	std::vector<std::size_t> psg_targets(PSG_ENVELOPE_COUNT);
	for (std::size_t i = 0; i < PSG_ENVELOPE_COUNT; ++i)
	{
		psg_targets[i] = ReadWordLE(bytes, psg_table_pos + i * 2) - ptr_base;
	}

	// Each list's extent runs to the next pointer target (or the section's end), the same
	// boundary-delimiting used for channel streams - keeping any trailing padding intact.
	std::vector<std::size_t> boundaries = pitch_targets;
	boundaries.insert(boundaries.end(), psg_targets.begin(), psg_targets.end());
	boundaries.push_back(psg_table_pos);
	boundaries.push_back(bytes.size());
	std::sort(boundaries.begin(), boundaries.end());
	boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
	const auto slice = [&](std::size_t start) -> std::vector<uint8_t>
	{
		const auto it = std::upper_bound(boundaries.begin(), boundaries.end(), start);
		const std::size_t end = std::min<std::size_t>((it != boundaries.end()) ? *it : bytes.size(), bytes.size());
		if (start >= end)
		{
			return {};
		}
		return std::vector<uint8_t>(bytes.begin() + start, bytes.begin() + end);
	};
	for (std::size_t i = 0; i < PITCH_EFFECT_COUNT; ++i)
	{
		m_instrument_params.pitch_effects[i] = slice(pitch_targets[i]);
	}
	for (std::size_t i = 0; i < PSG_ENVELOPE_COUNT; ++i)
	{
		m_instrument_params.psg_envelopes[i] = slice(psg_targets[i]);
	}
	return true;
}

bool MusicData::AsmSaveInstrumentParams(const std::filesystem::path& dir)
{
	Z80AsmFile f;
	f.WriteComment("Instrument, frequency and pitch-effect tables for the Cube sound driver -");
	f.WriteComment("regenerated by the editor; see docs/sound_driver_format.md and the driver");
	f.WriteComment("source (sounddrv.asm) for how each table is consumed.");
	f.WriteLabel("t_YM_FREQUENCIES");
	f.WriteWords(std::vector<uint16_t>(m_instrument_params.ym_frequencies.begin(), m_instrument_params.ym_frequencies.end()), 12);
	f.WriteLabel("t_PSG_FREQUENCIES");
	f.WriteWords(std::vector<uint16_t>(m_instrument_params.psg_frequencies.begin(), m_instrument_params.psg_frequencies.end()), 12);
	f.WriteLabel("t_YM_LEVELS");
	f.WriteBytes(std::vector<uint8_t>(m_instrument_params.ym_levels.begin(), m_instrument_params.ym_levels.end()), 16);
	f.WriteLabel("t_SLOTS_PER_ALGO");
	f.WriteBytes(std::vector<uint8_t>(m_instrument_params.slots_per_algo.begin(), m_instrument_params.slots_per_algo.end()), 8);

	std::vector<std::string> pitch_labels, psg_labels;
	for (std::size_t i = 0; i < PITCH_EFFECT_COUNT; ++i)
	{
		pitch_labels.push_back("t_PITCH_EFFECT_" + std::to_string(i));
	}
	for (std::size_t i = 0; i < PSG_ENVELOPE_COUNT; ++i)
	{
		psg_labels.push_back("t_PSG_INSTRUMENT_" + std::to_string(i));
	}
	f.WriteLabel("pt_PITCH_EFFECTS");
	f.WriteWordRefs(pitch_labels);
	for (std::size_t i = 0; i < PITCH_EFFECT_COUNT; ++i)
	{
		f.WriteLabel(pitch_labels[i]);
		f.WriteBytes(m_instrument_params.pitch_effects[i], 16);
	}
	f.WriteLabel("pt_PSG_INSTRUMENTS");
	f.WriteWordRefs(psg_labels);
	for (std::size_t i = 0; i < PSG_ENVELOPE_COUNT; ++i)
	{
		f.WriteLabel(psg_labels[i]);
		f.WriteBytes(m_instrument_params.psg_envelopes[i], 16);
	}
	return f.WriteFile(dir / RomLabels::Audio::INSTRUMENT_PARAMS_FILE);
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
		std::vector<std::size_t> track_offsets;
		for (std::size_t i = 0; i + 1 < table_bytes.size(); i += 2)
		{
			const uint16_t ptr = static_cast<uint16_t>(table_bytes[i] | (table_bytes[i + 1] << 8));
			track_offsets.push_back(static_cast<std::size_t>(static_cast<uint16_t>(ptr - BANK_PTR_BASE)));
		}
		// A ROM bank has no labels to delimit channel streams by, so every pointer target in the
		// bank (track headers and each track's channel pointers) serves as a boundary instead.
		const std::size_t header_size = 4 + MUSIC_CHANNEL_COUNT * 2;
		std::vector<std::size_t> boundaries = track_offsets;
		for (const std::size_t offset : track_offsets)
		{
			if (offset + header_size > bank_bytes.size())
			{
				continue;
			}
			const std::vector<uint8_t> header(bank_bytes.begin() + offset, bank_bytes.begin() + offset + header_size);
			for (std::size_t ch = 0; ch < MUSIC_CHANNEL_COUNT; ++ch)
			{
				boundaries.push_back(MusicChannelOffset(header, ch, true));
			}
		}
		std::sort(boundaries.begin(), boundaries.end());
		boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
		for (const std::size_t offset : track_offsets)
		{
			slots.push_back(DecodeMusicTrackFromBank(bank_bytes, offset, true, boundaries));
		}
	};
	load_bank(RomLabels::Audio::MUSIC_BANK_4_TABLE_SECTION, RomLabels::Audio::MUSIC_BANK_4_SECTION);
	load_bank(RomLabels::Audio::MUSIC_BANK_3_TABLE_SECTION, RomLabels::Audio::MUSIC_BANK_3_SECTION);

	{
		const auto bank4_sec = rom.get_section(RomLabels::Audio::MUSIC_BANK_4_SECTION);
		const auto instrument_bytes = rom.read_array<uint8_t>(bank4_sec.begin,
			static_cast<uint32_t>(std::min<std::size_t>(bank4_sec.size(), YM_INSTRUMENT_COUNT * YM_INSTRUMENT_SIZE)));
		m_ym_instruments = YmInstrumentsFromBytes(instrument_bytes);
	}

	BuildMusicPoolFromSlots(slots);
	return true;
}

bool MusicData::RomLoadSfx(const Rom& rom)
{
	auto table_sec = rom.get_section(RomLabels::Audio::SFX_TABLE_SECTION);
	auto table_bytes = rom.read_array<uint8_t>(table_sec.begin, table_sec.size());
	auto driver_sec = rom.get_section(RomLabels::Audio::SOUND_DRIVER_SECTION);
	auto driver_bytes = rom.read_array<uint8_t>(driver_sec.begin, driver_sec.size());

	std::vector<std::size_t> entry_offsets;
	for (std::size_t i = 0; i + 1 < table_bytes.size(); i += 2)
	{
		entry_offsets.push_back(static_cast<std::size_t>(table_bytes[i] | (table_bytes[i + 1] << 8)));
	}
	// As with the music banks, delimit channel streams by every pointer target: each entry's own
	// offset plus every entry's channel pointers.
	std::vector<std::size_t> boundaries = entry_offsets;
	for (const std::size_t offset : entry_offsets)
	{
		const auto ch_offsets = SfxChannelOffsets(driver_bytes, offset);
		boundaries.insert(boundaries.end(), ch_offsets.begin(), ch_offsets.end());
	}
	std::sort(boundaries.begin(), boundaries.end());
	boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

	std::vector<SfxEntry> slots;
	slots.reserve(entry_offsets.size());
	for (const std::size_t offset : entry_offsets)
	{
		slots.push_back(DecodeSfxEntryFromDriver(driver_bytes, offset, boundaries));
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

	if (!WriteYmInstrumentsFile(dir / "code/audio/ym_instruments.asm", m_ym_instruments))
	{
		return false;
	}

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
