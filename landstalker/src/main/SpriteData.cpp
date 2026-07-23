#include <landstalker/main/SpriteData.h>

#include <set>
#include <numeric>
#include <queue>
#include <cmath>
#include <landstalker/main/AsmUtils.h>
#include <landstalker/main/RomLabels.h>
#include <landstalker/misc/Literals.h>
#include <yaml-cpp/yaml.h>

namespace Landstalker {

const std::unordered_map<SpriteData::EnemyStats::DropProbability, std::string> SpriteData::EnemyStats::DropProbabilityNames =
{
	{ DropProbability::ONE_IN_64, "ONE_IN_64" },
	{ DropProbability::ONE_IN_128, "ONE_IN_128" },
	{ DropProbability::ONE_IN_256, "ONE_IN_256" },
	{ DropProbability::ONE_IN_512, "ONE_IN_512" },
	{ DropProbability::ONE_IN_1024, "ONE_IN_1024" },
	{ DropProbability::ONE_IN_2048, "ONE_IN_2048" },
	{ DropProbability::NO_DROP, "NO_DROP" },
	{ DropProbability::GUARANTEED_DROP, "GUARANTEED_DROP" }
};

template <std::size_t N>
std::vector<std::array<uint8_t, N>> DeserialiseFixedWidth(const std::vector<uint8_t>& bytes)
{
	std::vector<std::array<uint8_t, N>> result;
	std::size_t i = 0, j = 0;
	result.push_back(std::array<uint8_t, N>());
	while (i < bytes.size())
	{
		if (j >= N)
		{
			if (bytes[i] == 0xFF)
			{
				break;
			}
			j = 0;
			result.push_back(std::array<uint8_t, N>());
		}
		result.back()[j++] = bytes[i++];
	}
	if (j != N)
	{
		result.pop_back();
	}
	return result;
}

template <std::size_t N>
std::vector<uint8_t> SerialiseFixedWidth(const std::vector<std::array<uint8_t, N>>& data, bool terminate_data = true)
{
	std::vector<uint8_t> result;
	result.reserve(data.size() * N);

	for (const auto& d : data)
	{
		for (const auto& e : d)
		{
			result.push_back(e);
		}
	}

	if (terminate_data)
	{
		result.push_back(0xFF);
		if ((result.size() & 1) == 1)
		{
			result.push_back(0xFF);
		}
	}
	return result;
}

template <std::size_t N>
std::map<uint8_t, std::array<uint8_t, N>> DeserialiseMap(const std::vector<uint8_t>& bytes)
{
	std::map<uint8_t, std::array<uint8_t, N>> result;
	std::size_t i = 0, j = 0;
	uint8_t key = bytes[i++];
	std::array<uint8_t, N> buf;
	while (i < bytes.size())
	{
		if (j >= N)
		{
			j = 0;
			result.insert({key, buf});
			key = bytes[i++];
			if (key == 0xFF)
			{
				break;
			}
		}
		buf[j++] = bytes[i++];
	}
	return result;
}

template <std::size_t N>
std::vector<uint8_t> SerialiseMap(const std::map<uint8_t, std::array<uint8_t, N>>& data, bool terminate_data = true)
{
	std::vector<uint8_t> result;
	result.reserve(data.size() * (N + sizeof(uint8_t)));

	for (const auto& d : data)
	{
		result.push_back(d.first);
		for (const auto& e : d.second)
		{
			result.push_back(e);
		}
	}

	if (terminate_data)
	{
		result.push_back(0xFF);
		if ((result.size() & 1) == 1)
		{
			result.push_back(0xFF);
		}
	}
	return result;
}

std::map<uint8_t, uint8_t> DeserialiseMap(const std::vector<uint8_t>& bytes, bool reverse_key_value = false)
{
	std::map<uint8_t, uint8_t> result;
	std::size_t i = 0;
	while (i < bytes.size())
	{
		uint8_t key = bytes[i++];
		uint8_t val = bytes[i++];
		if (reverse_key_value)
		{
			std::swap(key, val);
		}
		if (val == 0xFF || key == 0xFF)
		{
			break;
		}
		result.insert({ key, val });
	}
	return result;
}

std::vector<uint8_t> SerialiseMap(const std::map<uint8_t, uint8_t>& data, bool reverse_key_value = false, bool terminate_data = true)
{
	std::vector<uint8_t> result;
	result.reserve(data.size() * 2);

	for (const auto& d : data)
	{
		if (reverse_key_value)
		{
			result.push_back(d.second);
			result.push_back(d.first);
		}
		else
		{
			result.push_back(d.first);
			result.push_back(d.second);
		}
	}

	if (terminate_data)
	{
		result.push_back(0xFF);
		result.push_back(0xFF);
	}
	return result;
}

template <typename T>
std::vector<std::array<uint8_t, T::SIZE>> EncodeFlags(const std::vector<T>& data_in)
{
	std::vector<std::array<uint8_t, T::SIZE>> data_out;
	for (auto& d : data_in)
	{
		data_out.push_back(d.GetData());
	}
	return data_out;
}

template <typename T>
std::vector<T> DecodeFlags(const std::vector<std::array<uint8_t, T::SIZE>>& data_in)
{
	std::vector<T> data_out;
	for (auto& d : data_in)
	{
		data_out.emplace_back(d);
	}
	return data_out;
};

template <typename T>
std::vector<T> GetFlagsForRoom(uint16_t room, const std::vector<T>& flags)
{
	std::vector<T> data;
	for (const auto& f : flags)
	{
		if (f.room == room)
		{
			data.push_back(f);
		}
	}
	return data;
}

template <typename T>
void SetFlagsForRoom(uint16_t room, const std::vector<T>& src, std::vector<T>& dst)
{
	std::queue<typename std::vector<T>::iterator> iterators;
	std::size_t count = 0;
	// Delete excess
	for (auto it = dst.begin(); it != dst.end(); )
	{
		if (it->room == room)
		{
			if (count < src.size())
			{
				count++;
				it++;
			}
			else
			{
				it = dst.erase(it);
			}
		}
		else
		{
			++it;
		}
	}
	// Save list of iterators to existing entries
	for (auto it = dst.begin(); it != dst.end(); ++it)
	{
		if (it->room == room)
		{
			iterators.push(it);
		}
	}
	// For each new entry, either change next iterator or, if none left, push on a new entry.
	for (const auto& f : src)
	{
		if (iterators.empty())
		{
			dst.push_back(f);
		}
		else
		{
			auto& it = iterators.front();
			*it = f;
			iterators.pop();
		}
	}
}

SpriteData::SpriteData(const std::filesystem::path& asm_file)
	: DataManager("Sprite Data", asm_file)
{
	if (!LoadAsmFilenames())
	{
		throw std::runtime_error(std::string("Unable to load file data from \'") + asm_file.string() + '\'');
	}
	if (!AsmLoadSpriteFrames())
	{
		throw std::runtime_error(std::string("Unable to load sprite frame data from \'") + m_sprite_frames_data_file.string() + '\'');
	}
	if (!AsmLoadSpritePointers())
	{
		throw std::runtime_error(std::string("Unable to load sprite pointer data from \'") + m_sprite_anim_frames_file.string() + '\'');
	}
	if (!AsmLoadSpritePalettes())
	{
		throw std::runtime_error(std::string("Unable to load sprite palette data from \'") + m_palette_data_file.string() + '\'');
	}
	if (!AsmLoadSpriteData())
	{
		throw std::runtime_error(std::string("Unable to load sprite data from \'") + asm_file.string() + '\'');
	}
	InitCache();
}

SpriteData::SpriteData(const Rom& rom)
	: DataManager("Sprite Data", rom)
{
	SetDefaultFilenames();
	if (!RomLoadSpriteFrames(rom))
	{
		throw std::runtime_error("Unable to load sprite frame data from ROM");
	}
	if (!RomLoadSpritePalettes(rom))
	{
		throw std::runtime_error("Unable to load sprite palette data from ROM");
	}
	if (!RomLoadSpriteData(rom))
	{
		throw std::runtime_error("Unable to load sprite data from ROM");
	}

	InitCache();
}

SpriteData::~SpriteData()
{
}

bool SpriteData::Save(const std::filesystem::path& dir)
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
	if (!AsmSaveSpriteFrames(directory))
	{
		throw std::runtime_error(std::string("Unable to save sprite frame data to \'") + directory.string() + '\'');
	}
	if (!AsmSaveSpritePointers(directory))
	{
		throw std::runtime_error(std::string("Unable to save sprite frame data to \'") + m_sprite_frames_data_file.string() + '\'');
	}
	if (!AsmSaveSpritePalettes(directory))
	{
		throw std::runtime_error(std::string("Unable to save sprite palette data to \'") + m_palette_data_file.string() + '\'');
	}
	if (!AsmSaveSpriteData(directory))
	{
		throw std::runtime_error(std::string("Unable to save sprite data to \'") + directory.string() + '\'');
	}
	CommitAllChanges();
	return true;
}

bool SpriteData::Save()
{
	return Save(GetBasePath());
}

bool SpriteData::HasBeenModified() const
{
	auto pair_pred = [](const auto& e) {return e.second != nullptr && e.second->HasSavedDataChanged(); };
	if (std::any_of(m_frames.begin(), m_frames.end(), pair_pred))
	{
		return true;
	}
	if (std::any_of(m_palettes_by_name.begin(), m_palettes_by_name.end(), pair_pred))
	{
		return true;
	}
	if (m_sprite_volume_orig != m_sprite_volume)
	{
		return true;
	}
	if (m_animations_orig != m_animations)
	{
		return true;
	}
	if (m_animation_frames_orig != m_animation_frames)
	{
		return true;
	}
	if (m_frames_orig != m_frames)
	{
		return true;
	}
	if (m_lo_palettes_orig != m_lo_palettes)
	{
		return true;
	}
	if (m_hi_palettes_orig != m_hi_palettes)
	{
		return true;
	}
	if (m_projectile1_palettes_orig != m_projectile1_palettes)
	{
		return true;
	}
	if (m_projectile2_palettes_orig != m_projectile2_palettes)
	{
		return true;
	}
	if (m_lo_palette_lookup_orig != m_lo_palette_lookup)
	{
		return true;
	}
	if (m_hi_palette_lookup_orig != m_hi_palette_lookup)
	{
		return true;
	}
	if (m_sprite_visibility_flags_orig != m_sprite_visibility_flags)
	{
		return true;
	}
	if (m_one_time_event_flags_orig != m_one_time_event_flags)
	{
		return true;
	}
	if (m_room_clear_flags_orig != m_room_clear_flags)
	{
		return true;
	}
	if (m_locked_door_flags_orig != m_locked_door_flags)
	{
		return true;
	}
	if (m_permanent_switch_flags_orig != m_permanent_switch_flags)
	{
		return true;
	}
	if (m_sacred_tree_flags_orig != m_sacred_tree_flags)
	{
		return true;
	}
	if (m_sprite_dimensions_orig != m_sprite_dimensions)
	{
		return true;
	}
	if (m_enemy_stats_orig != m_enemy_stats)
	{
		return true;
	}
	if (m_sprite_to_entity_lookup_orig != m_sprite_to_entity_lookup)
	{
		return true;
	}
	if (m_room_entities_orig != m_room_entities ||
		m_room_entity_table_size_orig != m_room_entity_table_size)
	{
		return true;
	}
	if (m_item_properties_orig != m_item_properties)
	{
		return true;
	}
	if (m_sprite_behaviours_orig != m_sprite_behaviours)
	{
		return true;
	}
	if (m_sprite_animation_flags_orig != m_sprite_animation_flags)
	{
		return true;
	}
	return false;
}

void SpriteData::RefreshPendingWrites(const Rom& rom)
{
	DataManager::RefreshPendingWrites(rom);
	if (!RomPrepareInjectSpriteFrames(rom))
	{
		throw std::runtime_error(std::string("Unable to prepare sprite frame data for ROM injection"));
	}
	if (!RomPrepareInjectSpritePalettes(rom))
	{
		throw std::runtime_error(std::string("Unable to prepare sprite palette data for ROM injection"));
	}
	if (!RomPrepareInjectSpriteData(rom))
	{
		throw std::runtime_error(std::string("Unable to prepare sprite data for ROM injection"));
	}
}

std::wstring SpriteData::GetEntityDisplayName(uint8_t id)
{
	return Labels::Get(Labels::C_ENTITIES, id).value_or(L"Entity" + std::to_wstring(id));
}

std::wstring SpriteData::GetSpriteDisplayName(uint8_t id)
{
	return Labels::Get(Labels::C_SPRITES, id).value_or(StrWPrintf(RomLabels::Sprites::SPRITE_GFX, id));
}

std::wstring SpriteData::GetSpriteAnimationDisplayName(uint8_t id, const std::string& name) const
{
	const auto& anims = m_animations.at(id);
	int anim_id = static_cast<int>(std::distance(anims.cbegin(), std::find(anims.cbegin(), anims.cend(), name)));
	return Labels::Get(Labels::C_SPRITE_ANIMATIONS, (id << 8) | anim_id).value_or(std::wstring(name.cbegin(), name.cend()));
}

std::wstring SpriteData::GetSpriteFrameDisplayName(uint8_t id, const std::string& name) const
{
	const auto& frames = m_sprite_frames.at(id);
	int frame_id = static_cast<int>(std::distance(frames.cbegin(), std::find(frames.cbegin(), frames.cend(), name)));
	return Labels::Get(Labels::C_SPRITE_FRAMES, (id << 8) | frame_id).value_or(std::wstring(name.cbegin(), name.cend()));
}

std::wstring SpriteData::GetSpriteLowPaletteDisplayName(uint8_t id)
{
	return Labels::Get(Labels::C_LOW_PALETTES, id).value_or(StrWPrintf(RomLabels::Sprites::PALETTE_LO, id));
}

std::wstring SpriteData::GetSpriteHighPaletteDisplayName(uint8_t id)
{
	return Labels::Get(Labels::C_HIGH_PALETTES, id).value_or(StrWPrintf(RomLabels::Sprites::PALETTE_HI, id));
}

std::wstring SpriteData::GetBehaviourDisplayName(int behav_id)
{
	return Labels::Get(Labels::C_BEHAVIOURS, behav_id).value_or(StrWPrintf(L"Behaviour%d", behav_id));
}

SpriteData::SpriteMetadata SpriteData::GetSpriteMetadata(uint8_t id) const
{
	SpriteMetadata metadata;
	if(!IsSprite(id))
	{
		throw std::runtime_error("Sprite ID does not exist");
	}
	std::set<std::string> included_frames;
	std::vector<std::string> frame_names;
	for (const auto& anim_name : m_animations.at(id))
	{
		for (const auto& frame_name : m_animation_frames.at(anim_name))
		{
			if(included_frames.count(frame_name) > 0)
			{
				continue;
			}
			included_frames.insert(frame_name);
			frame_names.push_back(frame_name);
		}
	}
	if(!frame_names.empty())
	{
		Rect bounding_box;
		for(const auto& frame_name : frame_names)
		{
			const auto& frame = m_frames.at(frame_name);
			bounding_box = bounding_box.GetUnion(frame->GetData()->GetBoundingBox());
		}
		metadata.frame_height = bounding_box.GetHeight();
		metadata.frame_width = bounding_box.GetWidth();
		metadata.origin = Point(-bounding_box.GetLeft(), -bounding_box.GetTop());
		metadata.frame_count = static_cast<unsigned int>(frame_names.size());
		metadata.hitbox = GetSpriteHitbox(id);
		metadata.animation_flags = GetSpriteAnimationFlags(id);
		for(unsigned int i = 0; i < frame_names.size(); ++i)
		{
			if(m_frames.at(frame_names.at(i))->GetData()->GetCompressed())
			{
				metadata.compressed_frames.push_back(i);
			}
		}
		for(const auto& anim_name : m_animations.at(id))
		{
			metadata.animations[anim_name] = {};
			for(const auto& frame_name : m_animation_frames.at(anim_name))
			{
				int frame_index = static_cast<int>(std::distance(frame_names.cbegin(), std::find(frame_names.cbegin(), frame_names.cend(), frame_name)));
				metadata.animations[anim_name].push_back(frame_index);
			}
		}
		metadata.volume = m_sprite_volume.at(id);
	}

	return metadata;
}

std::string SpriteData::GetSpriteMetadataYaml(uint8_t id) const
{
	auto metadata = GetSpriteMetadata(id);
	YAML::Emitter out;
	out << YAML::BeginMap << YAML::Key << GetSpriteName(id) << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "sprite_id" << YAML::Value << static_cast<int>(id);
	out << YAML::Key << "frame_width" << YAML::Value << metadata.frame_width;
	out << YAML::Key << "frame_height" << YAML::Value << metadata.frame_height;
	out << YAML::Key << "frame_count" << YAML::Value << metadata.frame_count;
	out << YAML::Key << "origin" << YAML::Value << YAML::Flow << YAML::BeginSeq << metadata.origin.x << YAML::Value << metadata.origin.y << YAML::EndSeq;
	out << YAML::Key << "hitbox" << YAML::Value << YAML::Flow << YAML::BeginMap
	                 << YAML::Key << "base" << YAML::Value << (static_cast<double>(metadata.hitbox.base) / 8.0)
					 << YAML::Key << "height" << YAML::Value << (static_cast<double>(metadata.hitbox.height) / 16.0) << YAML::EndMap;
	out << YAML::Key << "volume" << YAML::Value << (static_cast<double>(metadata.volume) / 16.0);
	out << YAML::Key << "compressed_frames" << YAML::Value << YAML::Flow << YAML::BeginSeq;
	for (const auto& frame_index : metadata.compressed_frames)
	{
		out << frame_index;
	}
	out << YAML::EndSeq;
	out << YAML::Key << "animation_flags" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "idle_frame_count" << YAML::Value << (metadata.animation_flags.idle_animation_frames == AnimationFlags::IdleAnimationFrameCount::ONE_FRAME ? 1 : 2);
	out << YAML::Key << "dedicated_idle_frames" << YAML::Value << (metadata.animation_flags.idle_animation_source == AnimationFlags::IdleAnimationSource::DEDICATED);
	out << YAML::Key << "dedicated_jump_frames" << YAML::Value << (metadata.animation_flags.jump_animation_source == AnimationFlags::JumpAnimationSource::DEDICATED);
	out << YAML::Key << "walk_frame_count" << YAML::Value << (metadata.animation_flags.walk_animation_frame_count == AnimationFlags::WalkAnimationFrameCount::TWO_FRAMES ? 2 : 4);
	out << YAML::Key << "dedicated_damage_frames" << YAML::Value << (metadata.animation_flags.take_damage_animation_source == AnimationFlags::TakeDamageAnimationSource::DEDICATED);
	out << YAML::Key << "no_rotate" << YAML::Value << metadata.animation_flags.do_not_rotate;
	out << YAML::Key << "full_animations" << YAML::Value << metadata.animation_flags.has_full_animations;
	out << YAML::EndMap;
	out << YAML::Key << "animations" << YAML::Value << YAML::BeginMap;
	for (const auto& [anim_name, frame_indices] : metadata.animations)
	{
		out << YAML::Key << anim_name << YAML::Value << YAML::Flow << YAML::BeginSeq;
		for (const auto& frame_index : frame_indices)
		{
			out << frame_index;
		}
		out << YAML::EndSeq;
	}
	out << YAML::EndMap << YAML::EndMap;
	return std::string(out.c_str());
}

SpriteData::EntityMetadata SpriteData::GetEntityMetadata(uint8_t id, std::shared_ptr<StringData> sd) const
{
	EntityMetadata metadata;
	if(!IsEntity(id))
	{
		throw std::runtime_error("Entity ID does not exist");
	}
	auto palettes = GetEntityPaletteIdxs(id);
	if(palettes.first >= 0)
	{
		metadata.low_palette = GetLoPalette(static_cast<uint8_t>(palettes.first))->GetName();
	}
	if(palettes.second >= 0)
	{
		metadata.high_palette = GetHiPalette(static_cast<uint8_t>(palettes.second))->GetName();
	}
	auto sfx = sd->GetEntityTalkSound(id);
	if(sfx > 0)
	{
		metadata.talk_sfx = sfx;
	}
	if(IsEntityItem(id))
	{
		metadata.item_properties = GetItemProperties(id);
	}
	if(IsEntityEnemy(id))
	{
		metadata.enemy_stats = GetEnemyStats(id);
	}
	return metadata;
}

std::string SpriteData::GetEntityMetadataYaml(uint8_t id, std::shared_ptr<StringData> sd) const
{
	auto metadata = GetEntityMetadata(id, sd);
	YAML::Emitter out;
	out << YAML::BeginMap << YAML::Key << wstr_to_utf8(GetEntityDisplayName(id)) << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "entity_id" << YAML::Value << static_cast<int>(id) << YAML::Comment(wstr_to_utf8(Labels::Get(Labels::C_ENTITIES, id).value_or(L"")));
	if(metadata.low_palette.has_value())
	{
		out << YAML::Key << "low_palette" << YAML::Value << metadata.low_palette.value();
	}
	if(metadata.high_palette.has_value())
	{
		out << YAML::Key << "high_palette" << YAML::Value << metadata.high_palette.value();
	}
	if(metadata.talk_sfx.has_value())
	{
		out << YAML::Key << "talk_sfx" << YAML::Value << static_cast<int>(metadata.talk_sfx.value())
		    << YAML::Comment(wstr_to_utf8(Landstalker::Labels::Get(Landstalker::Labels::C_SOUNDS, metadata.talk_sfx.value()).value_or(L"")));
	}
	if(metadata.item_properties.has_value())
	{
		out << YAML::Key << "item_properties" << YAML::Value << YAML::BeginMap;
		const auto& item_props = metadata.item_properties.value();
		out << YAML::Key << "use_verb" << YAML::Value << static_cast<int>(item_props.verb) << YAML::Comment(wstr_to_utf8(sd->GetMainString(item_props.verb)));
		out << YAML::Key << "equipment_index" << YAML::Value << static_cast<int>(item_props.equipment_index);
		out << YAML::Key << "max_quantity" << YAML::Value << static_cast<int>(item_props.max_quantity);
		out << YAML::Key << "price" << YAML::Value << static_cast<int>(item_props.price);
		out << YAML::EndMap;
	}
	if(metadata.enemy_stats.has_value())
	{
		out << YAML::Key << "enemy_stats" << YAML::Value << YAML::BeginMap;
		const auto& enemy_stats = metadata.enemy_stats.value();
		out << YAML::Key << "health" << YAML::Value << static_cast<int>(enemy_stats.health);
		out << YAML::Key << "attack" << YAML::Value << static_cast<int>(enemy_stats.attack);
		out << YAML::Key << "defence" << YAML::Value << static_cast<int>(enemy_stats.defence);
		out << YAML::Key << "gold_drop" << YAML::Value << static_cast<int>(enemy_stats.gold_drop);
		out << YAML::Key << "item_drop" << YAML::Value << static_cast<int>(enemy_stats.item_drop) << YAML::Comment(wstr_to_utf8(sd->GetItemDisplayName(enemy_stats.item_drop)));
		out << YAML::Key << "item_drop_probability" << YAML::Value << static_cast<int>(enemy_stats.drop_probability) << YAML::Comment(EnemyStats::DropProbabilityNames.at(enemy_stats.drop_probability));
		out <<YAML::EndMap;
	}
	out << YAML::EndMap << YAML::EndMap;
	return std::string(out.c_str());
}

uint8_t SpriteData::GetSpriteFromEntity(uint8_t id) const
{
	assert(m_sprite_to_entity_lookup.find(id) != m_sprite_to_entity_lookup.cend());
	return m_sprite_to_entity_lookup.find(id)->second;
}

void SpriteData::SetEntitySprite(uint8_t entity, uint8_t sprite)
{
	assert(m_sprite_to_entity_lookup.find(entity) != m_sprite_to_entity_lookup.cend());
	m_sprite_to_entity_lookup[entity] = sprite;
}

bool SpriteData::EntityHasSprite(uint8_t id) const
{
	return (m_sprite_to_entity_lookup.find(id) != m_sprite_to_entity_lookup.cend());
}

std::vector<uint8_t> SpriteData::GetEntitiesFromSprite(uint8_t id) const
{
	std::vector<uint8_t> results;
	for (const auto& lookup : m_sprite_to_entity_lookup)
	{
		if (lookup.second == id)
		{
			results.push_back(lookup.first);
		}
	}
	return results;
}

std::size_t SpriteData::GetEntityCount() const
{
	return m_sprite_to_entity_lookup.size();
}

std::vector<uint8_t> SpriteData::GetEntityIds() const
{
	std::vector<uint8_t> result;
	result.reserve(m_sprite_to_entity_lookup.size());
	// m_sprite_to_entity_lookup is keyed by entity id, so it iterates in ascending id order.
	for (const auto& e : m_sprite_to_entity_lookup)
	{
		result.push_back(e.first);
	}
	return result;
}

std::optional<uint8_t> SpriteData::GetFreeEntityId() const
{
	for (int id = 0; id < FIRST_ITEM_ENTITY; ++id)
	{
		if (m_sprite_to_entity_lookup.find(static_cast<uint8_t>(id)) == m_sprite_to_entity_lookup.cend())
		{
			return static_cast<uint8_t>(id);
		}
	}
	return std::nullopt;
}

std::optional<uint8_t> SpriteData::AddEntity(uint8_t sprite_id, int lo_palette, int hi_palette)
{
	if (!IsSprite(sprite_id))
	{
		return std::nullopt;
	}
	const auto id = GetFreeEntityId();
	if (!id)
	{
		return std::nullopt;
	}
	m_sprite_to_entity_lookup[*id] = sprite_id;
	SetEntityPalette(*id, lo_palette, hi_palette);
	return id;
}

bool SpriteData::IsEntityUsedInRooms(uint8_t id) const
{
	return std::any_of(m_room_entities.cbegin(), m_room_entities.cend(),
		[id](const auto& room)
		{
			return std::any_of(room.second.cbegin(), room.second.cend(),
				[id](const Entity& e) { return e.GetType() == id; });
		});
}

std::vector<uint16_t> SpriteData::GetRoomsUsingEntity(uint8_t id) const
{
	std::vector<uint16_t> result;
	for (const auto& room : m_room_entities)
	{
		if (std::any_of(room.second.cbegin(), room.second.cend(),
			[id](const Entity& e) { return e.GetType() == id; }))
		{
			result.push_back(room.first);
		}
	}
	return result;
}

bool SpriteData::DeleteEntity(uint8_t id, const std::shared_ptr<StringData>& strings)
{
	if (IsEntityItem(id) || !IsEntity(id) || IsEntityUsedInRooms(id))
	{
		return false;
	}
	m_sprite_to_entity_lookup.erase(id);
	m_lo_palette_lookup.erase(id);
	m_hi_palette_lookup.erase(id);
	m_enemy_stats.erase(id);
	if (strings)
	{
		// Zero clears the entry rather than storing a real sound.
		strings->SetEntityTalkSound(id, 0);
	}
	Labels::Remap(Labels::C_ENTITIES, { { id, -1 } });
	return true;
}

bool SpriteData::SwapEntities(uint8_t a, uint8_t b, const std::shared_ptr<StringData>& strings)
{
	if (a == b || IsEntityItem(a) || IsEntityItem(b) || !IsEntity(a) || !IsEntity(b))
	{
		return false;
	}
	// Presence-aware swap for an id-keyed map: afterwards a holds what b held and vice versa,
	// "absent" included, so an entity with no palette/stats override stays without one.
	const auto swap_in = [a, b](auto& table)
	{
		const auto ia = table.find(a);
		const auto ib = table.find(b);
		const bool has_a = ia != table.end();
		const bool has_b = ib != table.end();
		typename std::decay_t<decltype(table)>::mapped_type va{}, vb{};
		if (has_a) va = ia->second;
		if (has_b) vb = ib->second;
		table.erase(a);
		table.erase(b);
		if (has_b) table[a] = vb;
		if (has_a) table[b] = va;
	};
	swap_in(m_sprite_to_entity_lookup);
	swap_in(m_lo_palette_lookup);
	swap_in(m_hi_palette_lookup);
	swap_in(m_enemy_stats);
	if (strings)
	{
		const uint8_t sa = strings->GetEntityTalkSound(a);
		const uint8_t sb = strings->GetEntityTalkSound(b);
		strings->SetEntityTalkSound(a, sb);
		strings->SetEntityTalkSound(b, sa);
	}
	// Remap reads all sources before writing, so the two labels cross over in one call.
	Labels::Remap(Labels::C_ENTITIES, { { a, b }, { b, a } });
	return true;
}

SpriteData::Hitbox SpriteData::GetSpriteHitbox(uint8_t id) const
{
	assert(m_sprite_dimensions.find(id) != m_sprite_dimensions.cend());
	const auto& result = m_sprite_dimensions.find(id)->second;
	return { result[0], result[1] };
}

SpriteData::Hitbox SpriteData::GetEntityHitbox(uint8_t id) const
{
	if (!EntityHasSprite(id))
	{
		return { 8_u8, 16_u8 };
	}
	uint8_t sprite_id = GetSpriteFromEntity(id);
	return GetSpriteHitbox(sprite_id);
}

void SpriteData::SetSpriteHitbox(uint8_t id, const Hitbox& hitbox)
{
	assert(m_sprite_dimensions.find(id) != m_sprite_dimensions.cend());
	auto& result = m_sprite_dimensions.find(id)->second;
	result[0] = hitbox.base;
	result[1] = hitbox.height;
}

bool SpriteData::SpriteFrameExists(const std::string& name) const
{
	return m_frames.count(name) > 0;
}

void SpriteData::DeleteSpriteFrame(const std::string& name)
{
	if (SpriteFrameExists(name))
	{
		std::shared_ptr<SpriteFrameEntry> entry = m_frames.at(name);
		m_frames.erase(name);
		m_sprite_frames.at(entry->GetSprite()).erase(name);
		for (auto& a : m_animation_frames)
		{
			auto it = a.second.begin();
			while (it != a.second.end())
			{
				if (*it == name)
				{
					it = a.second.erase(it);
				}
				else
				{
					++it;
				}
			}
		}
	}
}

void SpriteData::AddSpriteFrame(uint8_t sprite_id, const std::string& name)
{
	if (!SpriteFrameExists(name))
	{
		std::shared_ptr<SpriteFrameEntry> entry = SpriteFrameEntry::Create(this, name, std::filesystem::path(RomLabels::Sprites::SPRITE_FRAME_FILE).parent_path() / (name + ".frm"));
		entry->SetSprite(sprite_id);
		auto& subsprite = entry->GetData()->AddSubSpriteBefore(0);
		// The default subsprite sits with its top-left on the origin, so it hangs down and to
		// the right of it. Lift it up by its own height so its bottom-left rests on the origin
		// instead - the origin is a sprite's ground anchor, and a sprite should stand on it
		// rather than dangle below.
		subsprite.y = -static_cast<int>(subsprite.h * entry->GetData()->GetTileHeight());
		entry->GetData()->PrepareSubSprites();

		m_frames[name] = entry;
		m_sprite_frames.at(sprite_id).insert(name);
	}
}

bool SpriteData::SpriteAnimationExists(const std::string& name) const
{
	return m_animation_frames.count(name) > 0;
}

void SpriteData::DeleteSpriteAnimation(const std::string& name)
{
	if (SpriteAnimationExists(name))
	{
		for (const auto& a : m_animations)
		{
			if (std::count(a.second.cbegin(), a.second.cend(), name) == static_cast<ptrdiff_t>(a.second.size()))
			{
				return;
			}
		}
		for (auto& a : m_animations)
		{
			for (auto it = a.second.begin(); it != a.second.end();)
			{
				if (*it == name)
				{
					it = a.second.erase(it);
				}
				else
				{
					++it;
				}
			}
		}
		m_animation_frames.erase(name);
	}
}

void SpriteData::AddSpriteAnimation(uint8_t sprite_id, const std::string& name)
{
	m_animation_frames.insert({name, {*m_sprite_frames.at(sprite_id).cbegin()}});
	m_animations.at(sprite_id).push_back(name);
}

void SpriteData::MoveSpriteAnimation(uint8_t sprite_id, const std::string& name, int pos)
{
	auto it = std::find(m_animations.at(sprite_id).begin(), m_animations.at(sprite_id).end(), name);
	if(it != m_animations.at(sprite_id).end())
	{
		if (pos < static_cast<int>(m_animations.at(sprite_id).size()))
		{
			std::iter_swap(it, m_animations.at(sprite_id).begin() + pos);
		}
	}
}

void SpriteData::DeleteSpriteAnimationFrame(const std::string& animation_name, int frame_id)
{
	if (SpriteAnimationExists(animation_name) && frame_id >= 0 &&
		frame_id < static_cast<int>(m_animation_frames.at(animation_name).size()))
	{
		m_animation_frames.at(animation_name).erase(m_animation_frames.at(animation_name).begin() + frame_id);
	}
}

void SpriteData::InsertSpriteAnimationFrame(const std::string& animation_name, int frame_id, const std::string& name)
{
	if (SpriteAnimationExists(animation_name) && frame_id >= 0 &&
		frame_id <= static_cast<int>(m_animation_frames.at(animation_name).size()))
	{
		m_animation_frames.at(animation_name).insert(m_animation_frames.at(animation_name).begin() + frame_id, name);
	}
}

void SpriteData::ChangeSpriteAnimationFrame(const std::string& animation_name, int frame_id, const std::string& name)
{
	if (SpriteAnimationExists(animation_name) && frame_id >= 0 &&
		frame_id < static_cast<int>(m_animation_frames.at(animation_name).size()))
	{
		m_animation_frames.at(animation_name)[frame_id] = name;
	}
}

void SpriteData::MoveSpriteAnimationFrame(const std::string& animation_name, int old_pos, int new_pos)
{
	if (SpriteAnimationExists(animation_name) && old_pos >= 0 && new_pos >= 0 &&
		old_pos < static_cast<int>(m_animation_frames.at(animation_name).size()) &&
		new_pos < static_cast<int>(m_animation_frames.at(animation_name).size()))
	{
		std::iter_swap(m_animation_frames.at(animation_name).begin() + old_pos, m_animation_frames.at(animation_name).begin() + new_pos);
	}
}

bool SpriteData::IsValidSpriteName(const std::string& name)
{
	// The sprite name becomes an assembly label: at most 30 characters, starting with a
	// letter, then letters, digits and underscores. The same rule the map and tileset names
	// use, inlined rather than reaching into RoomData for one predicate.
	if (name.empty() || name.size() > 30 ||
		!((name.front() >= 'A' && name.front() <= 'Z') ||
		  (name.front() >= 'a' && name.front() <= 'z')))
	{
		return false;
	}
	return std::all_of(std::next(name.cbegin()), name.cend(), [](const char c)
	{
		return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || c == '_';
	});
}

bool SpriteData::IsSpriteNameInUse(const std::string& name) const
{
	// Sprites, animations and frames all share one label namespace.
	return m_ids.count(name) != 0 || SpriteAnimationExists(name) || SpriteFrameExists(name);
}

std::optional<uint8_t> SpriteData::AddSprite(const std::string& name)
{
	if (!IsValidSpriteName(name) || IsSpriteNameInUse(name) || m_animations.size() >= MAX_SPRITES)
	{
		return std::nullopt;
	}
	// Ids are dense by contract - the game indexes the animation offset table directly - so
	// the next free id is one past the last, and a new sprite is appended there.
	const auto id = static_cast<uint8_t>(m_animations.size());

	// Seed every id-keyed table the sprite needs to exist. AddSpriteFrame and
	// AddSpriteAnimation assume these are already present, and GetSpriteHitbox asserts on the
	// dimensions entry, so a sprite that skipped it would trip the metadata export.
	m_names[id] = name;
	m_ids[name] = id;
	m_animations[id] = {};
	m_sprite_frames[id] = {};
	m_sprite_dimensions[id] = { 0, 0 };
	m_sprite_volume[id] = 0;

	// One empty frame with a single 1x1 subsprite - AddSpriteFrame's default subsprite is
	// exactly that. The names are derived from the sprite name, which is unique, rather than
	// the id, which a later delete or move could hand to a different sprite.
	std::string frame_name = name + "Frame00";
	for (unsigned int suffix = 1; SpriteFrameExists(frame_name); ++suffix)
	{
		frame_name = StrPrintf("%sFrame00_%u", name.c_str(), suffix);
	}
	AddSpriteFrame(id, frame_name);

	std::string anim_name = name + "Anim00";
	for (unsigned int suffix = 1; SpriteAnimationExists(anim_name); ++suffix)
	{
		anim_name = StrPrintf("%sAnim00_%u", name.c_str(), suffix);
	}
	AddSpriteAnimation(id, anim_name);

	// Reserve enough VRAM for what the frame actually holds - a single tile - rather than
	// leaving the volume at zero, which would allocate nothing.
	const auto frame = m_frames.find(frame_name);
	const auto tiles = frame != m_frames.cend() && frame->second->GetData()
		? frame->second->GetData()->GetTileCount() : 1;
	m_sprite_volume[id] = static_cast<uint16_t>(std::max<std::size_t>(1, tiles));
	return id;
}

bool SpriteData::RenameSprite(uint8_t id, const std::string& new_name)
{
	if (!IsSprite(id) || !IsValidSpriteName(new_name))
	{
		return false;
	}
	const auto old_name = m_names.at(id);
	if (old_name == new_name)
	{
		return true;
	}
	if (IsSpriteNameInUse(new_name))
	{
		return false;
	}
	// Only the sprite's own label moves; its frames and animations keep their names, which
	// are independent labels the pointer tables reference directly.
	m_ids.erase(old_name);
	m_names[id] = new_name;
	m_ids[new_name] = id;
	return true;
}

bool SpriteData::IsSpriteUsedByEntities(uint8_t id) const
{
	return std::any_of(m_sprite_to_entity_lookup.cbegin(), m_sprite_to_entity_lookup.cend(),
		[&](const auto& lookup) { return lookup.second == id; });
}

void SpriteData::RemapSprites(const std::map<uint8_t, int>& mapping, bool remap_entity_references)
{
	const auto remapped = [&](uint8_t id)
	{
		const auto entry = mapping.find(id);
		return entry == mapping.cend() ? static_cast<int>(id) : entry->second;
	};

	// The animation and frame counts are needed to renumber the composite label ids, and
	// they have to be read before the tables are rebuilt - including for an id being deleted,
	// whose entries are dropped below.
	std::map<uint8_t, std::size_t> anim_counts;
	std::map<uint8_t, std::size_t> frame_counts;
	for (const auto& entry : mapping)
	{
		anim_counts[entry.first] = m_animations.count(entry.first) ? m_animations.at(entry.first).size() : 0;
		frame_counts[entry.first] = m_sprite_frames.count(entry.first) ? m_sprite_frames.at(entry.first).size() : 0;
	}

	// Drop the animations and frames of every sprite being deleted before the id-keyed
	// tables are rebuilt, so nothing is left pointing at graphics that have gone.
	for (const auto& entry : mapping)
	{
		if (entry.second >= 0)
		{
			continue;
		}
		if (m_animations.count(entry.first))
		{
			for (const auto& anim : m_animations.at(entry.first))
			{
				m_animation_frames.erase(anim);
			}
		}
		if (m_sprite_frames.count(entry.first))
		{
			for (const auto& frame : m_sprite_frames.at(entry.first))
			{
				m_frames.erase(frame);
			}
		}
	}

	// Every id-keyed table is rebuilt wholesale rather than edited in place: the mapping is a
	// permutation, so an in-place pass would collide with keys it has not visited yet.
	const auto remap_map = [&](auto& table)
	{
		std::remove_reference_t<decltype(table)> rebuilt;
		for (auto& item : table)
		{
			const auto updated = remapped(item.first);
			if (updated >= 0)
			{
				rebuilt.emplace(static_cast<uint8_t>(updated), std::move(item.second));
			}
		}
		table = std::move(rebuilt);
	};
	remap_map(m_names);
	remap_map(m_animations);
	remap_map(m_animations_orig);
	remap_map(m_sprite_frames);
	remap_map(m_sprite_volume);
	remap_map(m_sprite_volume_orig);
	remap_map(m_sprite_dimensions);
	remap_map(m_sprite_dimensions_orig);
	remap_map(m_sprite_animation_flags);
	remap_map(m_sprite_animation_flags_orig);

	// m_ids is just the inverse of m_names, so rebuild it rather than remap it.
	m_ids.clear();
	for (const auto& name : m_names)
	{
		m_ids.emplace(name.second, name.first);
	}

	// Each frame entry carries the id of the sprite that owns it; refresh from the rebuilt
	// ownership rather than tracking every frame through the permutation.
	for (const auto& sprite : m_sprite_frames)
	{
		for (const auto& frame_name : sprite.second)
		{
			const auto frame = m_frames.find(frame_name);
			if (frame != m_frames.cend())
			{
				frame->second->SetSprite(sprite.first);
			}
		}
	}

	// Entities reference sprites by id, so the values of the entity -> sprite lookup follow
	// the move. A sprite being deleted has no entities (DeleteSprite refuses otherwise), so
	// a -1 here would only arise from misuse; drop the entry rather than store a bad id.
	// A content swap skips this: leaving the references put is what makes the two sprites
	// change places in the entities that draw them.
	if (remap_entity_references)
	{
		for (auto it = m_sprite_to_entity_lookup.begin(); it != m_sprite_to_entity_lookup.end();)
		{
			const auto updated = remapped(it->second);
			if (updated < 0)
			{
				it = m_sprite_to_entity_lookup.erase(it);
			}
			else
			{
				it->second = static_cast<uint8_t>(updated);
				++it;
			}
		}
	}

	// The display-name categories key off the same ids, animations and frames through
	// composite ids that embed the sprite in their high byte.
	std::map<int, int> sprite_labels;
	std::map<int, int> anim_labels;
	std::map<int, int> frame_labels;
	for (const auto& entry : mapping)
	{
		sprite_labels.emplace(entry.first, entry.second);
		for (std::size_t anim = 0; anim < anim_counts[entry.first]; ++anim)
		{
			anim_labels.emplace((entry.first << 8) | static_cast<int>(anim),
				entry.second < 0 ? -1 : ((entry.second << 8) | static_cast<int>(anim)));
		}
		for (std::size_t frame = 0; frame < frame_counts[entry.first]; ++frame)
		{
			frame_labels.emplace((entry.first << 8) | static_cast<int>(frame),
				entry.second < 0 ? -1 : ((entry.second << 8) | static_cast<int>(frame)));
		}
	}
	Labels::Remap(Labels::C_SPRITES, sprite_labels);
	Labels::Remap(Labels::C_SPRITE_ANIMATIONS, anim_labels);
	Labels::Remap(Labels::C_SPRITE_FRAMES, frame_labels);
}

bool SpriteData::SwapSprites(uint8_t a, uint8_t b)
{
	if (!IsSprite(a) || !IsSprite(b))
	{
		return false;
	}
	if (a == b)
	{
		return true;
	}
	// A two-way mapping swaps the two sprites' content; with entity references left alone the
	// entities keep the ids they point at, so the two sprites change places in the entities that
	// draw them.
	std::map<uint8_t, int> mapping;
	mapping.emplace(a, static_cast<int>(b));
	mapping.emplace(b, static_cast<int>(a));
	RemapSprites(mapping, false);
	return true;
}

bool SpriteData::DeleteSprite(uint8_t id)
{
	if (!IsSprite(id) || m_animations.size() <= 1 || IsSpriteUsedByEntities(id))
	{
		return false;
	}
	// Pull every higher id down so the numbering stays dense: the game reads the animation
	// offset table by id, so a hole would be loaded as a real sprite. RemapSprites drops the
	// deleted sprite's own frames and animations.
	const auto count = static_cast<int>(m_animations.size());
	std::map<uint8_t, int> mapping;
	mapping.emplace(id, -1);
	for (int slot = id + 1; slot < count; ++slot)
	{
		mapping.emplace(static_cast<uint8_t>(slot), slot - 1);
	}
	RemapSprites(mapping);
	return true;
}

std::optional<uint8_t> SpriteData::ImportSprite(const std::string& new_name, const std::string& yaml_data,
	const std::filesystem::path& frame_dir)
{
	if (!IsValidSpriteName(new_name) || IsSpriteNameInUse(new_name))
	{
		return std::nullopt;
	}
	try
	{
		const auto root = YAML::Load(yaml_data);
		if (!root.IsMap() || root.size() != 1)
		{
			return std::nullopt;
		}
		// The single top-level key is the sprite's original name, which is also the stem the
		// matching export uses for its frame files.
		const auto entry = *root.begin();
		const auto stem = entry.first.as<std::string>();
		const auto body = entry.second;
		const auto frame_count = body["frame_count"].as<unsigned int>(0u);
		if (frame_count == 0)
		{
			return std::nullopt;
		}

		// Read every frame binary up front, so a missing file aborts before anything is
		// added rather than leaving a half-built sprite behind.
		std::vector<ByteVector> frame_bytes;
		for (unsigned int i = 0; i < frame_count; ++i)
		{
			const auto path = frame_dir / StrPrintf("%s_frm%02u.frm", stem.c_str(), i);
			if (!std::filesystem::exists(path))
			{
				return std::nullopt;
			}
			frame_bytes.push_back(ReadBytes(path));
		}

		const auto new_sprite = static_cast<uint8_t>(m_animations.size());
		if (m_animations.size() >= MAX_SPRITES)
		{
			return std::nullopt;
		}
		m_names[new_sprite] = new_name;
		m_ids[new_name] = new_sprite;
		m_animations[new_sprite] = {};
		m_sprite_frames[new_sprite] = {};

		// Frames first, in the order the export listed them, so the animation frame indices
		// below line up. Named after the new sprite to keep the label namespace clean.
		std::vector<std::string> frame_names;
		for (unsigned int i = 0; i < frame_count; ++i)
		{
			std::string frame_name = StrPrintf("%sFrame%02u", new_name.c_str(), i);
			for (unsigned int suffix = 1; SpriteFrameExists(frame_name); ++suffix)
			{
				frame_name = StrPrintf("%sFrame%02u_%u", new_name.c_str(), i, suffix);
			}
			auto frame = SpriteFrameEntry::Create(this, frame_bytes[i], frame_name,
				std::filesystem::path(RomLabels::Sprites::SPRITE_FRAME_FILE).parent_path() / (frame_name + ".frm"));
			frame->SetSprite(new_sprite);
			m_frames[frame_name] = frame;
			m_sprite_frames[new_sprite].insert(frame_name);
			frame_names.push_back(frame_name);
		}

		// Then the animations, each a list of frame indices into the frames just created.
		unsigned int anim_index = 0;
		for (const auto& anim : body["animations"])
		{
			std::string anim_name = StrPrintf("%sAnim%02u", new_name.c_str(), anim_index++);
			for (unsigned int suffix = 1; SpriteAnimationExists(anim_name); ++suffix)
			{
				anim_name = StrPrintf("%sAnim%02u_%u", new_name.c_str(), anim_index - 1, suffix);
			}
			std::vector<std::string> frames;
			for (const auto& idx : anim.second)
			{
				const auto frame_index = idx.as<unsigned int>();
				if (frame_index < frame_names.size())
				{
					frames.push_back(frame_names[frame_index]);
				}
			}
			if (frames.empty())
			{
				frames.push_back(frame_names.front());
			}
			m_animation_frames[anim_name] = frames;
			m_animations[new_sprite].push_back(anim_name);
		}
		if (m_animations[new_sprite].empty())
		{
			// A sprite has to have at least one animation to be drawable.
			const auto anim_name = new_name + "Anim00";
			m_animation_frames[anim_name] = { frame_names.front() };
			m_animations[new_sprite].push_back(anim_name);
		}

		// Metadata: volume, hitbox and animation flags. Defaults keep a sprite valid when the
		// YAML omits them.
		m_sprite_volume[new_sprite] = static_cast<uint16_t>(std::lround(body["volume"].as<double>(1.0) * 16.0));
		const auto hitbox = body["hitbox"];
		m_sprite_dimensions[new_sprite] = {
			static_cast<uint8_t>(std::lround(hitbox["base"].as<double>(0.0) * 8.0)),
			static_cast<uint8_t>(std::lround(hitbox["height"].as<double>(0.0) * 16.0)) };

		const auto flags = body["animation_flags"];
		if (flags && flags.IsMap())
		{
			AnimationFlags af;
			af.idle_animation_frames = flags["idle_frame_count"].as<int>(2) == 1
				? AnimationFlags::IdleAnimationFrameCount::ONE_FRAME : AnimationFlags::IdleAnimationFrameCount::TWO_FRAMES;
			af.idle_animation_source = flags["dedicated_idle_frames"].as<bool>(false)
				? AnimationFlags::IdleAnimationSource::DEDICATED : AnimationFlags::IdleAnimationSource::USE_WALK_FRAMES;
			af.jump_animation_source = flags["dedicated_jump_frames"].as<bool>(false)
				? AnimationFlags::JumpAnimationSource::DEDICATED : AnimationFlags::JumpAnimationSource::USE_IDLE_FRAMES;
			af.walk_animation_frame_count = flags["walk_frame_count"].as<int>(4) == 2
				? AnimationFlags::WalkAnimationFrameCount::TWO_FRAMES : AnimationFlags::WalkAnimationFrameCount::FOUR_FRAMES;
			af.take_damage_animation_source = flags["dedicated_damage_frames"].as<bool>(false)
				? AnimationFlags::TakeDamageAnimationSource::DEDICATED : AnimationFlags::TakeDamageAnimationSource::USE_IDLE_FRAMES;
			af.do_not_rotate = flags["no_rotate"].as<bool>(false);
			af.has_full_animations = flags["full_animations"].as<bool>(false);
			if (!af.IsDefault())
			{
				m_sprite_animation_flags[new_sprite] = af;
			}
		}

		// Restore which frames were stored compressed.
		for (const auto& idx : body["compressed_frames"])
		{
			const auto frame_index = idx.as<unsigned int>();
			if (frame_index < frame_names.size())
			{
				m_frames[frame_names[frame_index]]->GetData()->SetCompressed(true);
			}
		}
		return new_sprite;
	}
	catch (const std::exception&)
	{
	}
	return std::nullopt;
}

bool SpriteData::IsEntity(uint8_t id) const
{
	return (m_sprite_to_entity_lookup.find(id) != m_sprite_to_entity_lookup.cend());
}

bool SpriteData::IsSprite(uint8_t id) const
{
	return (m_names.find(id) != m_names.cend());;
}

bool SpriteData::IsItem(uint8_t sprite_id) const
{
	auto entities = GetEntitiesFromSprite(sprite_id);
	return std::all_of(entities.cbegin(), entities.cend(), [this](const auto& e) { return IsEntityItem(e); });
}

bool SpriteData::IsEntityItem(uint8_t entity_id) const
{
	return entity_id >= 0xC0;
}

bool SpriteData::IsEntityEnemy(uint8_t entity_id) const
{
	return m_enemy_stats.count(entity_id) > 0;
}

bool SpriteData::HasFrontAndBack(uint8_t entity_id) const
{
	auto sprite_id = GetSpriteFromEntity(entity_id);
	return !IsItem(sprite_id) && GetSpriteAnimationCount(sprite_id) > 1;
}

bool SpriteData::CanRotate(uint8_t id) const
{
	if (m_sprite_animation_flags.count(id) > 0)
	{
		return !m_sprite_animation_flags.at(id).do_not_rotate;
	}
	else
	{
		return true;
	}
}

std::string SpriteData::GetSpriteName(uint8_t id) const
{
	assert(m_names.find(id) != m_names.cend());
	return m_names.find(id)->second;
}

uint8_t SpriteData::GetSpriteId(const std::string& name) const
{
	assert(m_ids.find(name) != m_ids.cend());
	return m_ids.find(name)->second;
}

uint32_t SpriteData::GetSpriteAnimationCount(uint8_t id) const
{
	assert(m_animations.find(id) != m_animations.cend());
	return static_cast<uint32_t>(m_animations.find(id)->second.size());
}

std::vector<std::string> SpriteData::GetSpriteAnimations(uint8_t id) const
{
	assert(m_animations.find(id) != m_animations.cend());
	return m_animations.find(id)->second;
}

std::vector<std::string> SpriteData::GetSpriteAnimations(const std::string& name) const
{
	auto id = GetSpriteId(name);
	assert(m_animations.find(id) != m_animations.cend());
	return m_animations.find(id)->second;
}

uint32_t SpriteData::GetSpriteFrameCount(uint8_t id) const
{
	assert(m_sprite_frames.find(id) != m_sprite_frames.cend());
	return static_cast<uint32_t>(m_sprite_frames.find(id)->second.size());
}

std::vector<std::string> SpriteData::GetSpriteFrames(uint8_t id) const
{
	assert(m_sprite_frames.find(id) != m_sprite_frames.cend());
	const auto& result = m_sprite_frames.find(id)->second;
	return std::vector<std::string> (result.cbegin(), result.cend());
}

std::vector<std::string> SpriteData::GetSpriteFrames(const std::string& name) const
{
	return GetSpriteFrames(GetSpriteId(name));
}

int SpriteData::GetSpriteFrameId(uint8_t sprite_id, const std::string& name) const
{
	auto frames = GetSpriteFrames(sprite_id);
	for (int i = 0; i < static_cast<int>(frames.size()); ++i)
	{
		if (frames[i] == name)
		{
			return i;
		}
	}
	return -1;
}

int SpriteData::GetDefaultEntityAnimationId(uint8_t id) const
{
	if (IsEntityItem(id))
	{
		// Item
		return (id >> 3) & 7;
	}
	else
	{
		uint8_t spr_id = GetSpriteFromEntity(id);
		return GetSpriteAnimationCount(spr_id) > 1 ? 1 : 0;
	}
}

int SpriteData::GetDefaultEntityFrameId(uint8_t id) const
{
	if (IsEntityItem(id))
	{
		// Item
		return id & 7;
	}
	else
	{
		return 0;
	}
}

int SpriteData::GetDefaultAbsFrameId(uint8_t ent_id) const
{
	if (IsEntityItem(ent_id))
	{
		// Item
		return ent_id & 0x3F;
	}
	else
	{
		uint8_t spr_id = GetSpriteFromEntity(ent_id);
		const auto& anim_ids = m_animations.at(spr_id);
		const std::string& frame_name = (anim_ids.size() > 1UL) ? m_animation_frames.at(anim_ids[1])[0] : m_animation_frames.at(anim_ids[0])[0];
		const auto& frames = m_sprite_frames.at(spr_id);
		return static_cast<int>(std::distance(frames.cbegin(), frames.find(frame_name)));
	}
}

std::shared_ptr<SpriteFrameEntry> SpriteData::GetDefaultEntityFrame(uint8_t id) const
{
	uint8_t spr_id = GetSpriteFromEntity(id);
	if (id >= 0xC0)
	{
		// Item
		return GetSpriteFrame(spr_id, (id >> 3) & 7, id & 0x07);
	}
	else
	{
		if (GetSpriteAnimationCount(spr_id) > 1)
		{
			return GetSpriteFrame(spr_id, 1, 0);
		}
		else
		{
			return GetSpriteFrame(spr_id, 0, 0);
		}
	}
	
}

std::shared_ptr<SpriteFrameEntry> SpriteData::GetSpriteFrame(const std::string& name) const
{
	assert(m_frames.find(name) != m_frames.cend());
	return m_frames.find(name)->second;
}

std::shared_ptr<SpriteFrameEntry> SpriteData::GetSpriteFrame(uint8_t id, uint8_t frame) const
{
	assert(m_sprite_frames.find(id) != m_sprite_frames.cend());
	assert(m_sprite_frames.find(id)->second.size() > frame);
	auto it = m_sprite_frames.find(id)->second.cbegin();
	std::advance(it, frame);
	assert(m_frames.find(*it) != m_frames.cend());
	return m_frames.find(*it)->second;
}

std::shared_ptr<SpriteFrameEntry> SpriteData::GetSpriteFrame(uint8_t id, uint8_t anim, uint8_t frame) const
{
	if (m_animations.find(id) == m_animations.cend())
	{
		return nullptr;
	}
	if (anim >= m_animations.find(id)->second.size())
	{
		return nullptr;
	}
	const auto& name = m_animations.find(id)->second[anim];
	return GetSpriteFrame(name, frame);
}

std::shared_ptr<SpriteFrameEntry> SpriteData::GetSpriteFrame(const std::string& anim_name, uint8_t frame) const
{
	if (m_animation_frames.find(anim_name) == m_animation_frames.cend())
	{
		return nullptr;
	}
	if (frame >= m_animation_frames.find(anim_name)->second.size())
	{
		return nullptr;
	}
	const auto& name = m_animation_frames.find(anim_name)->second[frame];
	if (m_frames.find(name) == m_frames.cend())
	{
		return nullptr;
	}
	return m_frames.find(name)->second;
}

uint32_t SpriteData::GetSpriteAnimationFrameCount(uint8_t id, uint8_t anim_id) const
{
	if (IsItem(id))
	{
		return 1;
	}
	else
	{
		return static_cast<uint32_t>(GetSpriteAnimationFrames(id, anim_id).size());
	}
}

uint32_t SpriteData::GetSpriteAnimationFrameCount(const std::string& name) const
{
	assert(m_animation_frames.find(name) != m_animation_frames.cend());
	return static_cast<uint32_t>(m_animation_frames.find(name)->second.size());
}

std::vector<std::string> SpriteData::GetSpriteAnimationFrames(uint8_t id, uint8_t anim_id) const
{
	assert(m_animations.find(id) != m_animations.cend());
	assert(m_animations.find(id)->second.size() > anim_id);
	const auto& name = m_animations.find(id)->second[anim_id];
	return GetSpriteAnimationFrames(name);
}

std::vector<std::string> SpriteData::GetSpriteAnimationFrames(const std::string& anim) const
{
	assert(m_animation_frames.find(anim) != m_animation_frames.cend());
	return m_animation_frames.find(anim)->second;
}

std::vector<std::string> SpriteData::GetSpriteAnimationFrames(const std::string& name, uint8_t anim_id) const
{
	auto id = GetSpriteId(name);
	return GetSpriteAnimationFrames(id, anim_id);
}

SpriteData::AnimationFlags SpriteData::GetSpriteAnimationFlags(uint8_t id) const
{
	if (m_sprite_animation_flags.count(id) > 0)
	{
		return m_sprite_animation_flags.at(id);
	}
	return AnimationFlags();
}

void SpriteData::SetSpriteAnimationFlags(uint8_t id, const AnimationFlags& flags)
{
	if (flags.IsDefault())
	{
		if (m_sprite_animation_flags.count(id) > 0)
		{
			m_sprite_animation_flags.erase(id);
		}
	}
	else
	{
		m_sprite_animation_flags[id] = flags;
	}
}

uint16_t SpriteData::GetSpriteVolume(uint8_t id) const
{
	if (m_sprite_volume.count(id) > 0)
	{
		return m_sprite_volume.at(id);
	}
	else
	{
		return 0;
	}
}

void SpriteData::SetSpriteVolume(uint8_t id, uint16_t val)
{
	m_sprite_volume[id] = val;
}

std::vector<Entity> SpriteData::GetRoomEntities(uint16_t room) const
{
	auto it = m_room_entities.find(room);
	if (it == m_room_entities.cend())
	{
		return std::vector<Entity>();
	}
	else
	{
		return it->second;
	}
}

void SpriteData::SetRoomEntities(uint16_t room, const std::vector<Entity>& entities)
{
	// Writing back an unchanged list has to be a no-op. Rooms with no entities have no
	// entry at all, so storing an empty vector for one would insert a new element and
	// leave the project looking modified without anything having been edited.
	const auto existing = m_room_entities.find(room);
	if (existing == m_room_entities.cend() ? entities.empty() : existing->second == entities)
	{
		return;
	}
	m_room_entities[room] = entities;
}

std::size_t SpriteData::GetRoomEntityTableSize() const
{
	return m_room_entity_table_size;
}

void SpriteData::SetRoomEntityTableSize(std::size_t rooms)
{
	m_room_entity_table_size = rooms;
}

void SpriteData::RemapRooms(const RoomIndexMap& mapping)
{
	if (!IsValidRoomRenumbering(mapping))
	{
		return;
	}
	RemapRoomKeys(mapping, m_room_entities);
	RemapRoomRecords(mapping, m_sprite_visibility_flags, { &EntityFlag::room });
	RemapRoomRecords(mapping, m_one_time_event_flags, { &OneTimeEventFlag::room });
	RemapRoomRecords(mapping, m_room_clear_flags, { &RoomClearFlag::room });
	RemapRoomRecords(mapping, m_locked_door_flags, { &RoomClearFlag::room });
	RemapRoomRecords(mapping, m_permanent_switch_flags, { &RoomClearFlag::room });
	RemapRoomRecords(mapping, m_sacred_tree_flags, { &SacredTreeFlag::room });
	// The entity offset table is sized by room count, so it shrinks with the room list.
	const auto deleted = CountDeletedRooms(mapping);
	if (deleted > 0 && m_room_entity_table_size >= deleted)
	{
		m_room_entity_table_size -= deleted;
	}
}

std::vector<EntityFlag> SpriteData::GetEntityVisibilityFlagsForRoom(uint16_t room)
{
	return GetFlagsForRoom(room, m_sprite_visibility_flags);
}

void SpriteData::SetEntityVisibilityFlagsForRoom(uint16_t room, const std::vector<EntityFlag>& data)
{
	SetFlagsForRoom(room, data, m_sprite_visibility_flags);
}

std::vector<OneTimeEventFlag> SpriteData::GetOneTimeEventFlagsForRoom(uint16_t room)
{
	return GetFlagsForRoom(room, m_one_time_event_flags);
}

void SpriteData::SetOneTimeEventFlagsForRoom(uint16_t room, const std::vector<OneTimeEventFlag>& data)
{
	SetFlagsForRoom(room, data, m_one_time_event_flags);
}

std::vector<RoomClearFlag> SpriteData::GetMultipleEntityHideFlagsForRoom(uint16_t room)
{
	return GetFlagsForRoom(room, m_room_clear_flags);
}

void SpriteData::SetMultipleEntityHideFlagsForRoom(uint16_t room, const std::vector<RoomClearFlag>& data)
{
	SetFlagsForRoom(room, data, m_room_clear_flags);
}

std::vector<RoomClearFlag> SpriteData::GetLockedDoorFlagsForRoom(uint16_t room)
{
	return GetFlagsForRoom(room, m_locked_door_flags);
}

void SpriteData::SetLockedDoorFlagsForRoom(uint16_t room, const std::vector<RoomClearFlag>& data)
{
	SetFlagsForRoom(room, data, m_locked_door_flags);
}

std::vector<RoomClearFlag> SpriteData::GetPermanentSwitchFlagsForRoom(uint16_t room)
{
	return GetFlagsForRoom(room, m_permanent_switch_flags);
}

void SpriteData::SetPermanentSwitchFlagsForRoom(uint16_t room, const std::vector<RoomClearFlag>& data)
{
	SetFlagsForRoom(room, data, m_permanent_switch_flags);
}

std::vector<SacredTreeFlag> SpriteData::GetSacredTreeFlagsForRoom(uint16_t room)
{
	return GetFlagsForRoom(room, m_sacred_tree_flags);
}

void SpriteData::SetSacredTreeFlagsForRoom(uint16_t room, const std::vector<SacredTreeFlag>& data)
{
	SetFlagsForRoom(room, data, m_sacred_tree_flags);
}

const std::map<std::string, std::shared_ptr<PaletteEntry>>& SpriteData::GetAllPalettes() const
{
	return m_palettes_by_name;
}

std::shared_ptr<PaletteEntry> SpriteData::GetPalette(const std::string& name) const
{
	assert(m_palettes_by_name.find(name) != m_palettes_by_name.end());
	return m_palettes_by_name.find(name)->second;
}

std::shared_ptr<Palette> SpriteData::GetSpritePalette(int lo, int hi) const
{
	std::vector<std::shared_ptr<Palette>> pals;

	if (lo >= 0)
	{
		pals.push_back(m_lo_palettes.at(lo)->GetData());
	}
	if (hi >= 0)
	{
		pals.push_back(m_hi_palettes.at(hi)->GetData());
	}

	return std::make_shared<Palette>(pals);
}

std::shared_ptr<Palette> SpriteData::GetEntityPalette(uint8_t idx) const
{
	int lo = -1;
	int hi = -1;

	if (m_lo_palette_lookup.find(idx) != m_lo_palette_lookup.cend())
	{
		lo = m_lo_palette_lookup.find(idx)->second->GetIndex();
	}
	if (m_hi_palette_lookup.find(idx) != m_hi_palette_lookup.cend())
	{
		hi = m_hi_palette_lookup.find(idx)->second->GetIndex();
	}

	return GetSpritePalette(lo, hi);
}

void SpriteData::SetEntityPalette(uint8_t entity, int lo, int hi)
{
	if (lo == -1)
	{
		m_lo_palette_lookup.erase(entity);
	}
	else if (lo < static_cast<int>(m_lo_palettes.size()))
	{
		m_lo_palette_lookup[entity] = m_lo_palettes[lo];
	}
	if (hi == -1)
	{
		m_hi_palette_lookup.erase(entity);
	}
	else if (hi < static_cast<int>(m_hi_palettes.size()))
	{
		m_hi_palette_lookup[entity] = m_hi_palettes[hi];
	}
}

std::pair<int, int> SpriteData::GetEntityPaletteIdxs(uint8_t idx) const
{
	int lo = -1;
	int hi = -1;

	if (m_lo_palette_lookup.find(idx) != m_lo_palette_lookup.cend())
	{
		lo = m_lo_palette_lookup.find(idx)->second->GetIndex();
	}
	if (m_hi_palette_lookup.find(idx) != m_hi_palette_lookup.cend())
	{
		hi = m_hi_palette_lookup.find(idx)->second->GetIndex();
	}

	return { lo, hi };
}

uint8_t SpriteData::GetLoPaletteCount() const
{
	return static_cast<uint8_t>(m_lo_palettes.size());
}

std::shared_ptr<PaletteEntry> SpriteData::GetLoPalette(uint8_t idx) const
{
	assert(idx < m_lo_palettes.size());
	return m_lo_palettes[idx];
}

uint8_t SpriteData::GetHiPaletteCount() const
{
	return static_cast<uint8_t>(m_hi_palettes.size());
}

std::shared_ptr<PaletteEntry> SpriteData::GetHiPalette(uint8_t idx) const
{
	assert(idx < m_hi_palettes.size());
	return m_hi_palettes[idx];
}

bool SpriteData::IsLoPaletteUsed(uint8_t index) const
{
	return std::any_of(m_lo_palette_lookup.cbegin(), m_lo_palette_lookup.cend(),
		[index](const auto& e) { return e.second && e.second->GetIndex() == index; });
}

bool SpriteData::IsHiPaletteUsed(uint8_t index) const
{
	return std::any_of(m_hi_palette_lookup.cbegin(), m_hi_palette_lookup.cend(),
		[index](const auto& e) { return e.second && e.second->GetIndex() == index; });
}

std::vector<uint8_t> SpriteData::GetEntitiesUsingLoPalette(uint8_t index) const
{
	std::vector<uint8_t> result;
	for (const auto& e : m_lo_palette_lookup)
	{
		if (e.second && e.second->GetIndex() == index)
		{
			result.push_back(e.first);
		}
	}
	return result;
}

std::vector<uint8_t> SpriteData::GetEntitiesUsingHiPalette(uint8_t index) const
{
	std::vector<uint8_t> result;
	for (const auto& e : m_hi_palette_lookup)
	{
		if (e.second && e.second->GetIndex() == index)
		{
			result.push_back(e.first);
		}
	}
	return result;
}

std::optional<uint8_t> SpriteData::AddSpritePalette(std::vector<std::shared_ptr<PaletteEntry>>& pals,
	Palette::Type type, const std::string& name_format)
{
	if (pals.size() >= MAX_SPRITE_PALETTES)
	{
		return std::nullopt;
	}
	std::string name = StrPrintf(name_format, pals.size() + 1);
	for (unsigned int suffix = 1; m_palettes_by_name.count(name) != 0; ++suffix)
	{
		name = StrPrintf(name_format, pals.size() + 1) + "_" + std::to_string(suffix);
	}
	// Put the new file beside an existing sprite palette so it follows the project's layout.
	std::filesystem::path fpath;
	if (!pals.empty())
	{
		const std::filesystem::path sibling(pals.front()->GetFilename());
		fpath = (sibling.parent_path() / (name + sibling.extension().string()));
		fpath = std::filesystem::path(fpath.generic_string());
	}
	else
	{
		fpath = "assets_packed/sprites/palettes/" + name + ".bin";
	}
	const auto bytes = Palette(name, type).GetBytes();
	const auto entry = PaletteEntry::Create(this, bytes, name, fpath, type);
	const auto index = static_cast<uint8_t>(pals.size());
	entry->SetIndex(index);
	pals.push_back(entry);
	m_palettes_by_name.insert({ name, entry });
	return index;
}

bool SpriteData::DeleteSpritePalette(std::vector<std::shared_ptr<PaletteEntry>>& pals,
	const std::wstring& label_category, uint8_t index, bool used)
{
	if (index >= pals.size() || pals.size() <= 1 || used)
	{
		return false;
	}
	m_palettes_by_name.erase(pals[index]->GetName());
	pals.erase(pals.begin() + index);
	// Re-index the entries above the hole; the entity lookups hold pointers to these entries and
	// read their index through GetIndex(), so they follow the shift without being touched here.
	std::map<int, int> label_map;
	label_map.emplace(index, -1);
	for (std::size_t slot = index; slot < pals.size(); ++slot)
	{
		pals[slot]->SetIndex(static_cast<int>(slot));
		label_map.emplace(static_cast<int>(slot) + 1, static_cast<int>(slot));
	}
	Labels::Remap(label_category, label_map);
	return true;
}

bool SpriteData::SwapSpritePalettes(std::vector<std::shared_ptr<PaletteEntry>>& pals,
	const std::wstring& label_category, uint8_t a, uint8_t b)
{
	if (a >= pals.size() || b >= pals.size())
	{
		return false;
	}
	if (a == b)
	{
		return true;
	}
	// Swap the colours in place so both entries keep their index - every entity lookup still
	// resolves to the same slot - then swap the display labels so the moved palette's name
	// follows it.
	std::swap(*pals[a]->GetData(), *pals[b]->GetData());
	Labels::Remap(label_category, { { a, b }, { b, a } });
	return true;
}

std::optional<uint8_t> SpriteData::AddLoPalette()
{
	return AddSpritePalette(m_lo_palettes, Palette::Type::SPRITE_LOW, RomLabels::Sprites::PALETTE_LO);
}

std::optional<uint8_t> SpriteData::AddHiPalette()
{
	return AddSpritePalette(m_hi_palettes, Palette::Type::SPRITE_HIGH, RomLabels::Sprites::PALETTE_HI);
}

bool SpriteData::DeleteLoPalette(uint8_t index)
{
	return DeleteSpritePalette(m_lo_palettes, Labels::C_LOW_PALETTES, index, IsLoPaletteUsed(index));
}

bool SpriteData::DeleteHiPalette(uint8_t index)
{
	return DeleteSpritePalette(m_hi_palettes, Labels::C_HIGH_PALETTES, index, IsHiPaletteUsed(index));
}

bool SpriteData::SwapLoPalettes(uint8_t a, uint8_t b)
{
	return SwapSpritePalettes(m_lo_palettes, Labels::C_LOW_PALETTES, a, b);
}

bool SpriteData::SwapHiPalettes(uint8_t a, uint8_t b)
{
	return SwapSpritePalettes(m_hi_palettes, Labels::C_HIGH_PALETTES, a, b);
}

uint8_t SpriteData::GetProjectile1PaletteCount() const
{
	return static_cast<uint8_t>(m_projectile1_palettes.size());
}

std::shared_ptr<PaletteEntry> SpriteData::GetProjectile1Palette(uint8_t idx) const
{
	assert(idx < m_projectile1_palettes.size());
	return m_projectile1_palettes[idx];
}

uint8_t SpriteData::GetProjectile2PaletteCount() const
{
	return static_cast<uint8_t>(m_projectile2_palettes.size());
}

std::shared_ptr<PaletteEntry> SpriteData::GetProjectile2Palette(uint8_t idx) const
{
	assert(idx < m_projectile2_palettes.size());
	return m_projectile2_palettes[idx];
}

SpriteData::ItemProperties SpriteData::GetItemProperties(uint8_t entity_index) const
{
	if (IsEntityItem(entity_index))
	{
		return ItemProperties(m_item_properties[entity_index & 0x3F]);
	}
	return ItemProperties();
}

void SpriteData::SetItemProperties(uint8_t entity_index, const SpriteData::ItemProperties& props)
{
	if (IsEntityItem(entity_index))
	{
		m_item_properties[entity_index & 0x3F] = props.Pack();
	}
}

SpriteData::EnemyStats SpriteData::GetEnemyStats(uint8_t entity_index) const
{
	if (IsEntityEnemy(entity_index))
	{
		return EnemyStats(m_enemy_stats.at(entity_index));
	}
	return EnemyStats();
}

void SpriteData::SetEnemyStats(uint8_t entity_index, const EnemyStats& stats)
{
	m_enemy_stats[entity_index] = stats.Pack();
}

void SpriteData::ClearEnemyStats(uint8_t entity_index)
{
	m_enemy_stats.erase(entity_index);
}

std::map<int, std::string> SpriteData::GetScriptNames() const
{
	std::map<int, std::string> names;
	for (const auto& s : m_sprite_behaviours)
	{
		names.insert({ s.first, s.second.first });
	}
	return names;
}

std::pair<std::string, std::vector<Behaviours::Command>> SpriteData::GetScript(int id) const
{
	auto it = m_sprite_behaviours.find(id);
	if (it != m_sprite_behaviours.cend())
	{
		return it->second;
	}
	return {"", {}};
}

void SpriteData::SetScript(int id, const std::string& name, const std::vector<Behaviours::Command>& cmds)
{
	m_sprite_behaviours[id] = std::make_pair(name, cmds);
}

void SpriteData::SetScript(int id, const std::vector<Behaviours::Command>& cmds)
{
	m_sprite_behaviours[id] = std::make_pair(m_sprite_behaviours.at(id).first, cmds);
}

void SpriteData::CommitAllChanges()
{
	auto pair_commit = [](const auto& e) {return e.second->Commit(); };
	std::for_each(m_frames.begin(), m_frames.end(), pair_commit);
	std::for_each(m_palettes_by_name.begin(), m_palettes_by_name.end(), pair_commit);
	m_frames_orig = m_frames;
	m_animations_orig = m_animations;
	m_animation_frames_orig = m_animation_frames;
	m_sprite_volume_orig = m_sprite_volume;
	m_lo_palettes_orig = m_lo_palettes;
	m_hi_palettes_orig = m_hi_palettes;
	m_projectile1_palettes_orig = m_projectile1_palettes;
	m_projectile2_palettes_orig = m_projectile2_palettes;
	m_hi_palette_lookup_orig = m_hi_palette_lookup;
	m_lo_palette_lookup_orig = m_lo_palette_lookup;
	m_sprite_visibility_flags_orig = m_sprite_visibility_flags;
	m_one_time_event_flags_orig = m_one_time_event_flags;
	m_room_clear_flags_orig = m_room_clear_flags;
	m_locked_door_flags_orig = m_locked_door_flags;
	m_permanent_switch_flags_orig = m_permanent_switch_flags;
	m_sacred_tree_flags_orig = m_sacred_tree_flags;
	m_sprite_dimensions_orig = m_sprite_dimensions;
	m_enemy_stats_orig = m_enemy_stats;
	m_sprite_to_entity_lookup_orig = m_sprite_to_entity_lookup;
	m_room_entities_orig = m_room_entities;
	m_room_entity_table_size_orig = m_room_entity_table_size;
	m_item_properties_orig = m_item_properties;
	m_sprite_behaviours_orig = m_sprite_behaviours;
	m_sprite_animation_flags_orig = m_sprite_animation_flags;
	m_pending_writes.clear();
}

bool SpriteData::LoadAsmFilenames()
{
	try
	{
		bool retval = true;
		AsmFile f(GetAsmFilename().string());
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::SPRITE_ANIM_FLAGS_LOOKUP, m_sprite_anim_flags_lookup_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::SPRITE_VISIBILITY_FLAGS, m_sprite_visibility_flags_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::ONE_TIME_EVENT_FLAGS, m_one_time_event_flags_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::ROOM_CLEAR_FLAGS, m_room_clear_flags_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::LOCKED_DOOR_SPRITE_FLAGS, m_locked_door_sprite_flags_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::PERMANENT_SWITCH_FLAGS, m_permanent_switch_flags_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::SACRED_TREE_FLAGS, m_sacred_tree_flags_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::SPRITE_GFX_IDX_LOOKUP, m_sprite_gfx_idx_lookup_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::SPRITE_DIMENSIONS_LOOKUP, m_sprite_dimensions_lookup_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::ROOM_SPRITE_TABLE_OFFSETS, m_room_sprite_table_offsets_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::ENEMY_STATS, m_enemy_stats_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::ROOM_SPRITE_TABLE, m_room_sprite_table_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::ITEM_PROPERTIES, m_item_properties_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::SPRITE_BEHAVIOUR_OFFSETS, m_sprite_behaviour_offset_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::SPRITE_BEHAVIOUR_TABLE, m_sprite_behaviour_table_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::SPRITE_FRAMES_DATA, m_sprite_frames_data_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::SPRITE_LUT, m_sprite_lut_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::SPRITE_ANIM_PTR_DATA, m_sprite_anims_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::SPRITE_FRAME_PTR_DATA, m_sprite_anim_frames_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::PALETTE_DATA, m_palette_data_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::PALETTE_PROJECTILE_1, m_proj1_pal_file);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Sprites::PALETTE_PROJECTILE_2, m_proj2_pal_file);
		return retval;
	}
	catch (std::exception&)
	{
		throw;
	}
}

void SpriteData::SetDefaultFilenames()
{
	if (m_sprite_anim_flags_lookup_file.empty())     m_sprite_anim_flags_lookup_file  = RomLabels::Sprites::SPRITE_ANIM_FLAGS_LOOKUP_FILE;
	if (m_sprite_visibility_flags_file.empty())      m_sprite_visibility_flags_file   = RomLabels::Sprites::SPRITE_VISIBILITY_FLAGS_FILE;
	if (m_one_time_event_flags_file.empty())         m_one_time_event_flags_file      = RomLabels::Sprites::ONE_TIME_EVENT_FLAGS_FILE;
	if (m_room_clear_flags_file.empty())             m_room_clear_flags_file          = RomLabels::Sprites::ROOM_CLEAR_FLAGS_FILE;
	if (m_locked_door_sprite_flags_file.empty())     m_locked_door_sprite_flags_file  = RomLabels::Sprites::LOCKED_DOOR_SPRITE_FLAGS_FILE;
	if (m_permanent_switch_flags_file.empty())       m_permanent_switch_flags_file    = RomLabels::Sprites::PERMANENT_SWITCH_FLAGS_FILE;
	if (m_sacred_tree_flags_file.empty())            m_sacred_tree_flags_file         = RomLabels::Sprites::SACRED_TREE_FLAGS_FILE;
	if (m_sprite_gfx_idx_lookup_file.empty())        m_sprite_gfx_idx_lookup_file     = RomLabels::Sprites::SPRITE_GFX_IDX_LOOKUP_FILE;
	if (m_sprite_dimensions_lookup_file.empty())     m_sprite_dimensions_lookup_file  = RomLabels::Sprites::SPRITE_DIMENSIONS_LOOKUP_FILE;
	if (m_room_sprite_table_offsets_file.empty())    m_room_sprite_table_offsets_file = RomLabels::Sprites::ROOM_SPRITE_TABLE_OFFSETS_FILE;
	if (m_enemy_stats_file.empty())                  m_enemy_stats_file               = RomLabels::Sprites::ENEMY_STATS_FILE;
	if (m_room_sprite_table_file.empty())            m_room_sprite_table_file         = RomLabels::Sprites::ROOM_SPRITE_TABLE_FILE;
	if (m_item_properties_file.empty())              m_item_properties_file           = RomLabels::Sprites::ITEM_PROPERTIES_FILE;
	if (m_sprite_behaviour_offset_file.empty())      m_sprite_behaviour_offset_file   = RomLabels::Sprites::SPRITE_BEHAVIOUR_OFFSET_FILE;
	if (m_sprite_behaviour_table_file.empty())       m_sprite_behaviour_table_file    = RomLabels::Sprites::SPRITE_BEHAVIOUR_TABLE_FILE;
	if (m_palette_data_file.empty())                 m_palette_data_file              = RomLabels::Sprites::PALETTE_DATA_FILE;
	if (m_palette_lut_file.empty())                  m_palette_lut_file               = RomLabels::Sprites::PALETTE_LUT_FILE;
	if (m_sprite_lut_file.empty())                   m_sprite_lut_file                = RomLabels::Sprites::SPRITE_LUT_FILE;
	if (m_sprite_anims_file.empty())                 m_sprite_anims_file              = RomLabels::Sprites::SPRITE_ANIMS_FILE;
	if (m_sprite_anim_frames_file.empty())           m_sprite_anim_frames_file        = RomLabels::Sprites::SPRITE_ANIM_FRAMES_FILE;
	if (m_sprite_frames_data_file.empty())           m_sprite_frames_data_file        = RomLabels::Sprites::SPRITE_FRAME_DATA_FILE;
	if (m_proj1_pal_file.empty())                    m_proj1_pal_file                 = RomLabels::Sprites::PALETTE_PROJECTILE_1_FILE;
	if (m_proj2_pal_file.empty())                    m_proj2_pal_file                 = RomLabels::Sprites::PALETTE_PROJECTILE_2_FILE;
}

bool SpriteData::CreateDirectoryStructure(const std::filesystem::path& dir)
{
	bool retval = true;
	retval = retval && CreateDirectoryTree(dir / m_sprite_anim_flags_lookup_file);
	retval = retval && CreateDirectoryTree(dir / m_sprite_visibility_flags_file);
	retval = retval && CreateDirectoryTree(dir / m_one_time_event_flags_file);
	retval = retval && CreateDirectoryTree(dir / m_room_clear_flags_file);
	retval = retval && CreateDirectoryTree(dir / m_locked_door_sprite_flags_file);
	retval = retval && CreateDirectoryTree(dir / m_permanent_switch_flags_file);
	retval = retval && CreateDirectoryTree(dir / m_sacred_tree_flags_file);
	retval = retval && CreateDirectoryTree(dir / m_sprite_gfx_idx_lookup_file);
	retval = retval && CreateDirectoryTree(dir / m_sprite_dimensions_lookup_file);
	retval = retval && CreateDirectoryTree(dir / m_room_sprite_table_offsets_file);
	retval = retval && CreateDirectoryTree(dir / m_enemy_stats_file);
	retval = retval && CreateDirectoryTree(dir / m_room_sprite_table_file);
	retval = retval && CreateDirectoryTree(dir / m_item_properties_file);
	retval = retval && CreateDirectoryTree(dir / m_sprite_behaviour_offset_file);
	retval = retval && CreateDirectoryTree(dir / m_sprite_behaviour_table_file);
	retval = retval && CreateDirectoryTree(dir / m_palette_data_file);
	retval = retval && CreateDirectoryTree(dir / m_palette_lut_file);
	retval = retval && CreateDirectoryTree(dir / m_sprite_lut_file);
	retval = retval && CreateDirectoryTree(dir / m_sprite_anims_file);
	retval = retval && CreateDirectoryTree(dir / m_sprite_anim_frames_file);
	retval = retval && CreateDirectoryTree(dir / m_sprite_frames_data_file);
	retval = retval && CreateDirectoryTree(dir / m_proj1_pal_file);
	retval = retval && CreateDirectoryTree(dir / m_proj2_pal_file);
	for (const auto& m : m_frames)
	{
		retval = retval && CreateDirectoryTree(dir / m.second->GetFilename());
	}
	for (const auto& m : m_palettes_by_name)
	{
		retval = retval && CreateDirectoryTree(dir / m.second->GetFilename());
	}
	return retval;
}

void SpriteData::InitCache()
{
	m_animations_orig = m_animations;
	m_animation_frames_orig = m_animation_frames;
	m_frames_orig = m_frames;
	m_sprite_volume_orig = m_sprite_volume;
	m_lo_palettes_orig = m_lo_palettes;
	m_hi_palettes_orig = m_hi_palettes;
	m_projectile1_palettes_orig = m_projectile1_palettes;
	m_projectile2_palettes_orig = m_projectile2_palettes;
	m_hi_palette_lookup_orig = m_hi_palette_lookup;
	m_lo_palette_lookup_orig = m_lo_palette_lookup;
	m_sprite_visibility_flags_orig = m_sprite_visibility_flags;
	m_one_time_event_flags_orig = m_one_time_event_flags;
	m_room_clear_flags_orig = m_room_clear_flags;
	m_locked_door_flags_orig = m_locked_door_flags;
	m_permanent_switch_flags_orig = m_permanent_switch_flags;
	m_sacred_tree_flags_orig = m_sacred_tree_flags;
	m_sprite_dimensions_orig = m_sprite_dimensions;
	m_enemy_stats_orig = m_enemy_stats;
	m_sprite_to_entity_lookup_orig = m_sprite_to_entity_lookup;
	m_room_entities_orig = m_room_entities;
	m_room_entity_table_size_orig = m_room_entity_table_size;
	m_item_properties_orig = m_item_properties;
	m_sprite_behaviours_orig = m_sprite_behaviours;
	m_sprite_animation_flags_orig = m_sprite_animation_flags;
}

ByteVector SpriteData::SerialisePaletteLUT() const
{
	ByteVector result;
	for (uint8_t i = 0; i < 0xFF; ++i)
	{
		if (m_lo_palette_lookup.find(i) != m_lo_palette_lookup.cend())
		{
			result.push_back(i);
			result.push_back(static_cast<uint8_t>(m_lo_palette_lookup.at(i)->GetIndex()));
		}
		if (m_hi_palette_lookup.find(i) != m_hi_palette_lookup.cend())
		{
			result.push_back(i);
			result.push_back(static_cast<uint8_t>(0x80 | m_hi_palette_lookup.at(i)->GetIndex()));
		}
	}
	result.push_back(0xFF);
	result.push_back(0xFF);
	return result;
}

void SpriteData::DeserialisePaletteLUT(const ByteVector& bytes)
{
	for (std::size_t i = 0; i < bytes.size(); i += 2)
	{
		uint8_t sprite = bytes[i];
		uint8_t pal = bytes[i + 1];
		if (sprite == 0xFF || pal == 0xFF)
		{
			break;
		}
		if ((pal & 0x80) == 0)
		{
			assert(pal < m_lo_palettes.size());
			m_lo_palette_lookup.insert({ sprite, m_lo_palettes[pal]});
		}
		else
		{
			pal &= 0x7F;
			assert(pal < m_hi_palettes.size());
			m_hi_palette_lookup.insert({ sprite, m_hi_palettes[pal] });
		}
	}
}

ByteVector SpriteData::SerialisePalArray(const std::vector<std::shared_ptr<PaletteEntry>>& pals) const
{
	ByteVector bytes;
	for (const auto& p : pals)
	{
		auto b = p->GetBytes();
		bytes.insert(bytes.end(), b->cbegin(), b->cend());
	}
	return bytes;
}

std::vector<std::shared_ptr<PaletteEntry>> SpriteData::DeserialisePalArray(const ByteVector& bytes, const std::string& name,
	const std::filesystem::path& path, Palette::Type type, bool unique_path)
{
	std::vector<std::shared_ptr<PaletteEntry>> result;
	const uint32_t size = Palette::GetSizeBytes(type);
	assert(bytes.size() % size == 0);
	auto it = bytes.cbegin();
	int idx = 0;
	bool format_name = (name.find('%') != std::string::npos);
	while (it != bytes.cend())
	{
		auto fname = format_name ? StrPrintf(name, idx + 1) : name + StrPrintf(":%d", idx);
		auto fpath = unique_path ? StrPrintf(path.string(), idx + 1) : path.string();
		auto b = ByteVector(it, it + size);
		if (b[0] > 0x0E)
		{
			// Invalid colour, probably signals end-of-data
			break;
		}
		auto e = PaletteEntry::Create(this, b, fname, fpath, type);
		e->SetIndex(idx);
		result.push_back(e);
		m_palettes_by_name.insert({ fname, e });
		it += size;
		idx++;
	}
	auto old_name = result.back()->GetName();
	if ((result.size() == 1) && (old_name.find(':') != std::string::npos))
	{
		auto pal = m_palettes_by_name.extract(old_name);
		auto new_name = old_name.substr(0, old_name.find(':'));
		pal.key() = new_name;
		pal.mapped()->SetName(new_name);
		m_palettes_by_name.insert(std::move(pal));
		
	}
	return result;
}

void SpriteData::DeserialiseRoomEntityTable(const ByteVector& offsets, const ByteVector& bytes)
{
	m_room_entity_table_size = std::max(m_room_entity_table_size, offsets.size() / 2);
	for (uint16_t i = 0; (i * 2) < static_cast<uint16_t>(offsets.size()); ++i)
	{
		uint16_t offset = (offsets[i * 2] << 8) | offsets[i * 2 + 1];
		if (offset == 0)
		{
			continue;
		}
		offset--;
		m_room_entities.insert({ i, std::vector<Entity>() });
		while (bytes[offset] != 0xFF || bytes[offset + 1] != 0xFF)
		{
			std::array<uint8_t, 8> data{};
			std::copy_n(bytes.cbegin() + offset, 8, data.begin());
			m_room_entities[i].emplace_back(data);
			offset += 8;
		}
	}
}

std::pair<ByteVector, ByteVector> SpriteData::SerialiseRoomEntityTable() const
{
	ByteVector bytes, offsets;
	// The table must span every room, not just up to the last one that has entities,
	// otherwise a trailing entity-less room makes the game index past the end of it.
	const std::size_t last_populated = m_room_entities.empty() ? std::size_t{ 0 } :
		static_cast<std::size_t>(m_room_entities.rbegin()->first) + 1;
	const std::size_t table_size = std::max(m_room_entity_table_size, last_populated);
	offsets.reserve(table_size * sizeof(uint16_t));
	for (std::size_t idx = 0; idx < table_size; ++idx)
	{
		const uint16_t i = static_cast<uint16_t>(idx);
		auto res = m_room_entities.find(i);
		if (res == m_room_entities.cend())
		{
			offsets.push_back(0);
			offsets.push_back(0);
			continue;
		}
		uint16_t offset = static_cast<uint16_t>(bytes.size() + 1);
		offsets.push_back((offset >> 8) & 0xFF);
		offsets.push_back(offset & 0xFF);
		for (const auto& ent : res->second)
		{
			auto data = ent.GetData();
			bytes.insert(bytes.end(), std::begin(data), std::end(data));
		}
		bytes.push_back(0xFF);
		bytes.push_back(0xFF);
	}
	return { bytes, offsets };
}

bool SpriteData::AsmLoadSpriteFrames()
{
	try
	{
		AsmFile file(GetBasePath() / m_sprite_frames_data_file);
		AsmFile::Label lbl;
		AsmFile::IncludeFile inc;
		while (file.IsGood())
		{
			file >> lbl >> inc;
			auto bytes = ReadBytes(GetBasePath() / inc.path);
			auto e = SpriteFrameEntry::Create(this, bytes, lbl, inc.path);
			m_frames.insert({ e->GetName(), e });
		}
		return true;
	}
	catch (const std::exception&)
	{
	}
	return false;
}

bool SpriteData::AsmLoadSpritePointers()
{
	std::vector<std::pair<uint16_t, uint16_t>> lut;
	try
	{
		auto lut_bytes = ReadBytes(GetBasePath() / m_sprite_lut_file);
		assert(lut_bytes.size() % 4 == 0);
		for (std::size_t i = 0; i < lut_bytes.size(); i += 4)
		{
			lut.push_back({ static_cast<uint16_t>((lut_bytes[i] << 8) | lut_bytes[i + 1]),
				            static_cast<uint16_t>((lut_bytes[i + 2] << 8) | lut_bytes[i + 3]) });
		}

		AsmFile anim_file(GetBasePath() / m_sprite_anims_file);
		std::string ptrname;
		for (uint8_t spr = 0; spr < static_cast<uint8_t>(lut.size()); ++spr)
		{
			std::string sprname = StrPrintf(RomLabels::Sprites::SPRITE_GFX, spr);
			if (anim_file.IsLabel())
			{
				AsmFile::Label lbl;
				anim_file >> lbl;
				if (lbl.label != RomLabels::Sprites::SPRITE_SECTION)
				{
					sprname = lbl.label;
				}
			}
			m_names.insert({ spr, sprname });
			m_ids.insert({ sprname, spr });
			m_sprite_volume[spr] = lut[spr].second;
			m_animations.insert({ spr, std::vector<std::string>() });
			int anim_end = 0xFFFF;
			if (spr < (lut.size() - 1))
			{
				anim_end = lut[spr + 1].first - lut[spr].first;
			}
			for (int anim = 0; anim < anim_end; ++anim)
			{
				if (!anim_file.IsGood())
				{
					break;
				}
				anim_file >> ptrname;
				m_animations[spr].push_back(ptrname);
			}
		}

		AsmFile frame_file(GetBasePath() / m_sprite_anim_frames_file);
		for (const auto& spr : m_animations)
		{
			for (const auto& animation : spr.second)
			{
				m_animation_frames.insert({ animation, std::vector<std::string>() });
				frame_file.Goto(animation);
				do
				{
					frame_file >> ptrname;
					assert(m_frames.find(ptrname) != m_frames.cend());
					m_animation_frames[animation].push_back(ptrname);
					m_frames[ptrname]->SetSprite(spr.first);
					if (m_sprite_frames.find(spr.first) == m_sprite_frames.cend())
					{
						m_sprite_frames.insert({spr.first, std::set<std::string>()});
					}
					m_sprite_frames[spr.first].insert(ptrname);
				} while (frame_file.IsGood() && !frame_file.IsLabel());
			}
		}
		return true;
	}
	catch (const std::exception&)
	{
	}
	return false;
}

bool SpriteData::AsmLoadSpritePalettes()
{
	std::vector<std::pair<uint16_t, uint16_t>> lut;
	try
	{
		AsmFile file(GetBasePath() / m_palette_data_file);
		AsmFile::IncludeFile inc;
		file.Goto(RomLabels::Sprites::PALETTE_LUT);
		file >> inc;
		m_palette_lut_file = inc.path;
		int idx = 0;
		file.Goto(RomLabels::Sprites::PALETTE_LO_DATA);
		do
		{
			file >> inc;
			std::string name = StrPrintf(RomLabels::Sprites::PALETTE_LO, idx + 1);
			auto e = PaletteEntry::Create(this, ReadBytes(GetBasePath() / inc.path), name, inc.path, Palette::Type::SPRITE_LOW);
			e->SetIndex(idx++);
			m_lo_palettes.push_back(e);
			m_palettes_by_name.insert({ name, e });
		} while (file.IsGood() && !file.IsLabel(RomLabels::Sprites::PALETTE_HI_DATA));
		idx = 0;
		file.Goto(RomLabels::Sprites::PALETTE_HI_DATA);
		do
		{
			file >> inc;
			std::string name = StrPrintf(RomLabels::Sprites::PALETTE_HI, idx + 1);
			auto e = PaletteEntry::Create(this, ReadBytes(GetBasePath() / inc.path), name, inc.path, Palette::Type::SPRITE_HIGH);
			e->SetIndex(idx++);
			m_hi_palettes.push_back(e);
			m_palettes_by_name.insert({ name, e });
		} while (file.IsGood() && !file.IsLabel(RomLabels::Sprites::PALETTE_LO_DATA));
		DeserialisePaletteLUT(ReadBytes(GetBasePath() / m_palette_lut_file));
		auto proj1_bytes = ReadBytes(GetBasePath() / m_proj1_pal_file);
		auto proj2_bytes = ReadBytes(GetBasePath() / m_proj2_pal_file);
		m_projectile1_palettes = DeserialisePalArray(proj1_bytes, RomLabels::Sprites::PALETTE_PROJECTILE_1, m_proj1_pal_file, Palette::Type::PROJECTILE);
		m_projectile2_palettes = DeserialisePalArray(proj2_bytes, RomLabels::Sprites::PALETTE_PROJECTILE_2, m_proj2_pal_file, Palette::Type::PROJECTILE2);
		return true;
	}
	catch (const std::exception&)
	{
	}
	return false;
}

bool SpriteData::AsmLoadSpriteData()
{

	m_sprite_visibility_flags  = DecodeFlags<EntityFlag>(DeserialiseFixedWidth<4>(ReadBytes(GetBasePath() / m_sprite_visibility_flags_file)));
	m_one_time_event_flags     = DecodeFlags<OneTimeEventFlag>(DeserialiseFixedWidth<6>(ReadBytes(GetBasePath() / m_one_time_event_flags_file)));
	m_room_clear_flags         = DecodeFlags<RoomClearFlag>(DeserialiseFixedWidth<4>(ReadBytes(GetBasePath() / m_room_clear_flags_file)));
	m_locked_door_flags        = DecodeFlags<RoomClearFlag>(DeserialiseFixedWidth<4>(ReadBytes(GetBasePath() / m_locked_door_sprite_flags_file)));
	m_permanent_switch_flags   = DecodeFlags<RoomClearFlag>(DeserialiseFixedWidth<4>(ReadBytes(GetBasePath() / m_permanent_switch_flags_file)));
	m_sacred_tree_flags        = DecodeFlags<SacredTreeFlag>(DeserialiseFixedWidth<4>(ReadBytes(GetBasePath() / m_sacred_tree_flags_file)));
	m_item_properties          = DeserialiseFixedWidth<4>(ReadBytes(GetBasePath() / m_item_properties_file));
	m_enemy_stats              = DeserialiseMap<5>(ReadBytes(GetBasePath() / m_enemy_stats_file));
	m_sprite_dimensions        = DeserialiseMap<2>(ReadBytes(GetBasePath() / m_sprite_dimensions_lookup_file));
	m_sprite_to_entity_lookup  = DeserialiseMap(ReadBytes(GetBasePath() / m_sprite_gfx_idx_lookup_file), true);
	auto anim_flags            = DeserialiseMap(ReadBytes(GetBasePath() / m_sprite_anim_flags_lookup_file));
	auto sprite_behaviour_offsets = ReadBytes(GetBasePath() / m_sprite_behaviour_offset_file);
	auto sprite_behaviours        = ReadBytes(GetBasePath() / m_sprite_behaviour_table_file);
	m_sprite_behaviours = Behaviours::Unpack(sprite_behaviour_offsets, sprite_behaviours);
	DeserialiseRoomEntityTable(ReadBytes(GetBasePath() / m_room_sprite_table_offsets_file),
		ReadBytes(GetBasePath() / m_room_sprite_table_file));

	m_sprite_animation_flags.clear();
	std::transform(anim_flags.cbegin(), anim_flags.cend(), std::inserter(m_sprite_animation_flags, m_sprite_animation_flags.end()), [](const auto& elem)
		{
			return std::pair<uint8_t, AnimationFlags>(elem.first, AnimationFlags(elem.second));
		});
	return true;
}

bool SpriteData::RomLoadSpriteFrames(const Rom& rom)
{
	const uint32_t sprites_begin = rom.get_address(RomLabels::Sprites::POINTER);
	const uint32_t sprites_end = rom.get_section(RomLabels::Sprites::SPRITE_SECTION).end;
	const uint32_t anim_ptrs_begin = rom.read<uint32_t>(sprites_begin);

	// Read offset table
	const uint32_t lookup_table_begin = sprites_begin + sizeof(uint32_t);
	const uint32_t lookup_table_size = (anim_ptrs_begin - lookup_table_begin) / sizeof(uint16_t);
	auto offset_table = rom.read_array<uint16_t>(lookup_table_begin, lookup_table_size);

	// Read anim pointers
	uint32_t min_address = 0xFFFFFF;
	std::vector<uint32_t> anim_idxs;
	uint32_t addr = anim_ptrs_begin;
	uint32_t ptr;
	while (addr < min_address)
	{
		ptr = rom.inc_read<uint32_t>(addr);
		min_address = std::min(min_address, ptr);
		anim_idxs.push_back(ptr);
	}
	const uint32_t frame_ptr_begin = min_address;
	std::for_each(anim_idxs.begin(), anim_idxs.end(), [frame_ptr_begin](uint32_t& val) {
		val = (val - frame_ptr_begin) / sizeof(uint32_t);
	});

	// Read frame pointers
	addr = min_address;
	min_address = 0xFFFFFF;
	std::vector<uint32_t> anim_frame_ptrs;
	while(addr < min_address)
	{
		ptr = rom.inc_read<uint32_t>(addr);
		min_address = std::min(min_address, ptr);
		anim_frame_ptrs.push_back(ptr);
	}

	// Parse anim and frame pointer list, segregate by sprite/animation as appropriate
	assert((offset_table.size() & 1) == 0);
	std::map<uint32_t, std::string> frames;
	std::map<uint32_t, std::string> frame_filenames;
	std::map<uint32_t, uint8_t> frame_sprite;
	std::size_t total_anim_count = 0;
	for (uint8_t i = 0; i < static_cast<uint8_t>(offset_table.size() / 2); ++i)
	{
		int sprite_frame_count = 0;
		std::string sprname = StrPrintf(RomLabels::Sprites::SPRITE_GFX, i);
		m_sprite_volume.insert({ i, offset_table[i * 2 + 1] });
		m_names.insert({ i, sprname });
		m_ids.insert({ sprname, i });
		uint16_t anim_count;
		if (static_cast<std::size_t>(i * 2 + 2) < offset_table.size())
		{
			anim_count = offset_table[i * 2 + 2] - offset_table[i * 2];
		}
		else
		{
			anim_count = static_cast<uint16_t>(anim_idxs.size()) - offset_table[i * 2];
		}
		m_animations.insert({ i, std::vector<std::string>() });
		m_animations[i].reserve(anim_count);
		for (int j = 0; j < anim_count; ++j)
		{
			std::string anim_name = StrPrintf(RomLabels::Sprites::SPRITE_ANIM, i, j);
			m_animations[i].push_back(anim_name);
			uint16_t frame_count;
			if ((total_anim_count + 1) < anim_idxs.size())
			{
				frame_count = static_cast<uint16_t>(anim_idxs[total_anim_count + 1] - anim_idxs[total_anim_count]);
			}
			else
			{
				frame_count = static_cast<uint16_t>(anim_frame_ptrs.size() - anim_idxs[total_anim_count]);
			}
			m_animation_frames.insert({ anim_name, std::vector<std::string>() });
			m_animation_frames[anim_name].reserve(frame_count);
			for (int k = 0; k < frame_count; ++k)
			{
				std::string frame_name;
				uint32_t frame_ptr = anim_frame_ptrs[anim_idxs[total_anim_count] + k];
				auto res = frames.find(frame_ptr);
				if (res != frames.end())
				{
					frame_name = res->second;
				}
				else
				{
					frame_name = StrPrintf(RomLabels::Sprites::SPRITE_FRAME, i, sprite_frame_count);
					frames.insert({ frame_ptr, frame_name });
					frame_filenames.insert({ frame_ptr, StrPrintf(RomLabels::Sprites::SPRITE_FRAME_FILE, i, sprite_frame_count++) });
					frame_sprite.insert({ frame_ptr, i });
				}
				m_animation_frames[anim_name].push_back(frame_name);
			}
			total_anim_count++;
		}
	}

	// Get begin, end addresses for sprite frame data
	auto it = frames.cbegin();
	std::vector<std::pair<uint32_t, uint32_t>> frame_addrs;
	for (int i = 0; i < static_cast<int>(frames.size()) - 1; ++i)
	{
		frame_addrs.push_back({ it->first, (++it)->first});
	}
	frame_addrs.push_back({ it->first, sprites_end });

	// Read each frame and store
	for (const auto& faddr : frame_addrs)
	{
		auto bytes = rom.read_array<uint8_t>(faddr.first, faddr.second - faddr.first);
		auto e = SpriteFrameEntry::Create(this, bytes, frames[faddr.first], frame_filenames[faddr.first]);
		e->SetStartAddress(faddr.first);
		e->SetSprite(frame_sprite[faddr.first]);
		m_frames.insert({ e->GetName(), e });
		if (m_sprite_frames.find(e->GetSprite()) == m_sprite_frames.cend())
		{
			m_sprite_frames.insert({ e->GetSprite(), std::set<std::string>() });
		}
		m_sprite_frames[e->GetSprite()].insert(e->GetName());
	}

	return true;
}

bool SpriteData::RomLoadSpritePalettes(const Rom& rom)
{
	uint32_t lut_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::PALETTE_LUT);
	uint32_t lo_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::PALETTE_LO_DATA);
	uint32_t hi_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::PALETTE_HI_DATA);
	uint32_t hi_end    = rom.get_section(RomLabels::Sprites::PALETTE_DATA).end;
	uint32_t proj1_begin = Disasm::ReadOffset8(rom, RomLabels::Sprites::PALETTE_PROJECTILE_1_MOVEW1);
	uint32_t proj1_end = rom.get_section(RomLabels::Sprites::PALETTE_PROJECTILE_1).end;
	uint32_t proj2_begin = Disasm::ReadOffset8(rom, RomLabels::Sprites::PALETTE_PROJECTILE_2_MOVEW1);
	uint32_t proj2_end = rom.get_section(RomLabels::Sprites::PALETTE_PROJECTILE_2).end;

	uint32_t lut_size = (lo_begin - lut_begin);
	uint32_t lo_size = (hi_begin - lo_begin);
	uint32_t hi_size = (hi_end - hi_begin);
	uint32_t proj1_size = (proj1_end - proj1_begin);
	uint32_t proj2_size = (proj2_end - proj2_begin);
	lut_size -= (lut_size % 2);
	lo_size -= (lo_size % Palette::GetSizeBytes(Palette::Type::SPRITE_LOW));
	hi_size -= (hi_size % Palette::GetSizeBytes(Palette::Type::SPRITE_HIGH));
	proj1_size -= (proj1_size % Palette::GetSizeBytes(Palette::Type::PROJECTILE));
	proj2_size -= (proj2_size % Palette::GetSizeBytes(Palette::Type::PROJECTILE2));

	auto lut_bytes = rom.read_array<uint8_t>(lut_begin, lut_size);
	auto hi_bytes = rom.read_array<uint8_t>(hi_begin, hi_size);
	auto lo_bytes = rom.read_array<uint8_t>(lo_begin, lo_size);
	auto proj1_bytes = rom.read_array<uint8_t>(proj1_begin, proj1_size);
	auto proj2_bytes = rom.read_array<uint8_t>(proj2_begin, proj2_size);

	m_hi_palettes = DeserialisePalArray(hi_bytes, RomLabels::Sprites::PALETTE_HI, RomLabels::Sprites::PALETTE_HI_FILE,
		Palette::Type::SPRITE_HIGH, true);
	m_lo_palettes = DeserialisePalArray(lo_bytes, RomLabels::Sprites::PALETTE_LO, RomLabels::Sprites::PALETTE_LO_FILE,
		Palette::Type::SPRITE_LOW, true);
	m_projectile1_palettes = DeserialisePalArray(proj1_bytes, RomLabels::Sprites::PALETTE_PROJECTILE_1,
		RomLabels::Sprites::PALETTE_PROJECTILE_1_FILE, Palette::Type::PROJECTILE, false);
	m_projectile2_palettes = DeserialisePalArray(proj2_bytes, RomLabels::Sprites::PALETTE_PROJECTILE_2,
		RomLabels::Sprites::PALETTE_PROJECTILE_2_FILE, Palette::Type::PROJECTILE2, false);

	DeserialisePaletteLUT(lut_bytes);

	return true;
}

bool SpriteData::RomLoadSpriteData(const Rom& rom)
{
	const uint32_t items_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::ITEM_PROPERTIES);
	const uint32_t items_size = rom.get_section(RomLabels::Sprites::ITEM_PROPERTIES_SECTION).end - items_begin;
	const uint32_t anim_flags_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::SPRITE_ANIM_FLAGS_LOOKUP);
	const uint32_t anim_flags_size = rom.get_section(RomLabels::Sprites::SPRITE_ANIM_FLAGS_LOOKUP_SECTION).end - anim_flags_begin;

	const uint32_t behav_offsets_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::SPRITE_BEHAVIOUR_OFFSETS);
	const uint32_t behav_table_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::SPRITE_BEHAVIOUR_TABLE);
	const uint32_t behav_table_end = rom.get_section(RomLabels::Sprites::SPRITE_BEHAVIOUR_SECTION).end;
	const uint32_t behav_offsets_size = behav_table_begin - behav_offsets_begin;
	const uint32_t behav_table_size = behav_table_end - behav_table_begin;


	const uint32_t visib_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::SPRITE_VISIBILITY_FLAGS);
	const uint32_t onetime_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::ONE_TIME_EVENT_FLAGS);
	const uint32_t room_clear_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::ROOM_CLEAR_FLAGS);
	const uint32_t locked_doors_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::LOCKED_DOOR_SPRITE_FLAGS);
	const uint32_t permanent_switch_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::PERMANENT_SWITCH_FLAGS);
	const uint32_t trees_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::SACRED_TREE_FLAGS);
	const uint32_t sprite_ent_lut_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::SPRITE_GFX_IDX_LOOKUP);
	const uint32_t sprite_dims_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::SPRITE_DIMENSIONS_LOOKUP);
	const uint32_t offsets_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::ROOM_SPRITE_TABLE_OFFSETS);
	const uint32_t enemy_data_begin = Disasm::ReadOffset16(rom, RomLabels::Sprites::ENEMY_STATS);
	const uint32_t entity_table_begin = rom.read<uint32_t>(RomLabels::Sprites::ROOM_SPRITE_TABLE);
	const uint32_t entity_table_end = rom.get_section(RomLabels::Sprites::SPRITE_DATA_SECTION).end;

	const uint32_t visib_size = onetime_begin - visib_begin;
	const uint32_t onetime_size = room_clear_begin - onetime_begin;
	const uint32_t room_clear_size = locked_doors_begin - room_clear_begin;
	const uint32_t locked_doors_size = permanent_switch_begin - locked_doors_begin;
	const uint32_t permanent_switch_size = trees_begin - permanent_switch_begin;
	const uint32_t trees_size = sprite_ent_lut_begin - trees_begin;
	const uint32_t sprite_ent_lut_size = sprite_dims_begin - sprite_ent_lut_begin;
	const uint32_t sprite_dims_size = offsets_begin - sprite_dims_begin;
	const uint32_t offsets_size = enemy_data_begin - offsets_begin;
	const uint32_t enemy_data_size = entity_table_begin - enemy_data_begin;
	const uint32_t entity_table_size = entity_table_end - entity_table_begin;

	m_sprite_visibility_flags = DecodeFlags<EntityFlag>(DeserialiseFixedWidth<4>(rom.read_array<uint8_t>(visib_begin, visib_size)));
	m_one_time_event_flags = DecodeFlags<OneTimeEventFlag>(DeserialiseFixedWidth<6>(rom.read_array<uint8_t>(onetime_begin, onetime_size)));
	m_room_clear_flags = DecodeFlags<RoomClearFlag>(DeserialiseFixedWidth<4>(rom.read_array<uint8_t>(room_clear_begin, room_clear_size)));
	m_locked_door_flags = DecodeFlags<RoomClearFlag>(DeserialiseFixedWidth<4>(rom.read_array<uint8_t>(locked_doors_begin, locked_doors_size)));
	m_permanent_switch_flags = DecodeFlags<RoomClearFlag>(DeserialiseFixedWidth<4>(rom.read_array<uint8_t>(permanent_switch_begin, permanent_switch_size)));
	m_sacred_tree_flags = DecodeFlags<SacredTreeFlag>(DeserialiseFixedWidth<4>(rom.read_array<uint8_t>(trees_begin, trees_size)));
	m_item_properties = DeserialiseFixedWidth<4>(rom.read_array<uint8_t>(items_begin, items_size));
	m_enemy_stats = DeserialiseMap<5>(rom.read_array<uint8_t>(enemy_data_begin, enemy_data_size));
	m_sprite_dimensions = DeserialiseMap<2>(rom.read_array<uint8_t>(sprite_dims_begin, sprite_dims_size));
	m_sprite_to_entity_lookup = DeserialiseMap(rom.read_array<uint8_t>(sprite_ent_lut_begin, sprite_ent_lut_size), true);
	auto anim_flags = DeserialiseMap(rom.read_array<uint8_t>(anim_flags_begin, anim_flags_size));
	std::transform(anim_flags.cbegin(), anim_flags.cend(), std::inserter(m_sprite_animation_flags, m_sprite_animation_flags.end()), [](const auto& elem)
		{
			return std::pair<uint8_t, AnimationFlags>(elem.first, AnimationFlags(elem.second));
		});
	auto sprite_behaviour_offsets = rom.read_array<uint8_t>(behav_offsets_begin, behav_offsets_size);
	auto sprite_behaviours = rom.read_array<uint8_t>(behav_table_begin, behav_table_size);
	m_sprite_behaviours = Behaviours::Unpack(sprite_behaviour_offsets, sprite_behaviours);
	DeserialiseRoomEntityTable(rom.read_array<uint8_t>(offsets_begin, offsets_size),
		rom.read_array<uint8_t>(entity_table_begin, entity_table_size));

	return true;
}

bool SpriteData::AsmSaveSpriteFrames(const std::filesystem::path& dir)
{
	return std::all_of(m_frames.begin(), m_frames.end(), [&](auto& f) { return f.second->Save(dir); });
}

bool SpriteData::AsmSaveSpritePointers(const std::filesystem::path& dir)
{
	try
	{
		AsmFile frm_file, anim_file, spr_file;

		frm_file.WriteFileHeader(m_sprite_frames_data_file, "Sprite Frame Data");
		for (const auto& frame : m_frames)
		{
			frm_file << AsmFile::Label(frame.first) << AsmFile::IncludeFile(frame.second->GetFilename(), AsmFile::FileType::BINARY);
			frm_file << AsmFile::Align(2);
		}
		frm_file.WriteFile(dir / m_sprite_frames_data_file);
		anim_file.WriteFileHeader(m_sprite_anim_frames_file, "Sprite Frame Pointers");
		for (const auto& anim : m_animation_frames)
		{
			anim_file << AsmFile::Label(anim.first);
			for (const auto& frame : anim.second)
			{
				anim_file << frame;
			}
		}
		anim_file.WriteFile(dir / m_sprite_anim_frames_file);
		spr_file.WriteFileHeader(m_sprite_anims_file, "Sprite Animation Pointers");
		spr_file << AsmFile::Label(RomLabels::Sprites::SPRITE_SECTION);
		for (const auto& spr : m_animations)
		{
			spr_file << AsmFile::Label(m_names[spr.first]);
			for (const auto& anim : spr.second)
			{
				spr_file << anim;
			}
		}
		spr_file.WriteFile(dir / m_sprite_anims_file);
		ByteVector lut;
		lut.reserve(m_animations.size() * 2 * sizeof(uint16_t));
		uint16_t anim_count = 0;
		for (const auto& spr : m_animations)
		{
			lut.push_back((anim_count >> 8) & 0xFF);
			lut.push_back(anim_count & 0xFF);
			lut.push_back((m_sprite_volume[spr.first] >> 8) & 0xFF);
			lut.push_back(m_sprite_volume[spr.first] & 0xFF);
			anim_count += static_cast<uint16_t>(spr.second.size());
		}
		WriteBytes(lut, dir / m_sprite_lut_file);
		return true;
	}
	catch (const std::exception&)
	{
	}
	return false;
}

bool SpriteData::AsmSaveSpritePalettes(const std::filesystem::path& dir)
{
	try
	{
		auto proj1_bytes = SerialisePalArray(m_projectile1_palettes);
		WriteBytes(proj1_bytes, dir / m_proj1_pal_file);
		auto proj2_bytes = SerialisePalArray(m_projectile2_palettes);
		WriteBytes(proj2_bytes, dir / m_proj2_pal_file);
		auto lookup_bytes = SerialisePaletteLUT();
		WriteBytes(lookup_bytes, dir / m_palette_lut_file);

		AsmFile file;
		file.WriteFileHeader(m_palette_data_file, "Sprite Palette Data");
		file << AsmFile::Label(RomLabels::Sprites::PALETTE_LUT) << AsmFile::IncludeFile(m_palette_lut_file, AsmFile::FileType::BINARY);
		file << AsmFile::Label(RomLabels::Sprites::PALETTE_LO_DATA);
		for (const auto& pal : m_lo_palettes)
		{
			file << AsmFile::IncludeFile(pal->GetFilename(), AsmFile::FileType::BINARY);
			pal->Save(dir);
		}
		file << AsmFile::Label(RomLabels::Sprites::PALETTE_HI_DATA);
		for (const auto& pal : m_hi_palettes)
		{
			file << AsmFile::IncludeFile(pal->GetFilename(), AsmFile::FileType::BINARY);
			pal->Save(dir);
		}
		file.WriteFile(dir / m_palette_data_file);

		return true;
	}
	catch (const std::exception&)
	{
	}
	return false;
}

bool SpriteData::AsmSaveSpriteData(const std::filesystem::path& dir)
{
	std::vector<std::array<uint8_t, 2>> anim_flags;
	std::transform(m_sprite_animation_flags.cbegin(), m_sprite_animation_flags.cend(), std::back_inserter<std::vector<std::array<uint8_t, 2>>>(anim_flags), [](const auto& elem)
		{
			return std::array<uint8_t, 2>({ elem.first, elem.second.Pack() });
		});
	auto behaviour_bytes = Behaviours::Pack(m_sprite_behaviours);
	WriteBytes(SerialiseFixedWidth<4>(EncodeFlags(m_sprite_visibility_flags)), dir / m_sprite_visibility_flags_file);
	WriteBytes(SerialiseFixedWidth<6>(EncodeFlags(m_one_time_event_flags)), dir / m_one_time_event_flags_file);
	WriteBytes(SerialiseFixedWidth<4>(EncodeFlags(m_room_clear_flags)), dir / m_room_clear_flags_file);
	WriteBytes(SerialiseFixedWidth<4>(EncodeFlags(m_locked_door_flags)), dir / m_locked_door_sprite_flags_file);
	WriteBytes(SerialiseFixedWidth<4>(EncodeFlags(m_permanent_switch_flags)), dir / m_permanent_switch_flags_file);
	WriteBytes(SerialiseFixedWidth<4>(EncodeFlags(m_sacred_tree_flags)), dir / m_sacred_tree_flags_file);
	WriteBytes(SerialiseFixedWidth<4>(m_item_properties, false), dir / m_item_properties_file);
	WriteBytes(SerialiseMap<5>(m_enemy_stats), dir / m_enemy_stats_file);
	WriteBytes(SerialiseMap<2>(m_sprite_dimensions), dir / m_sprite_dimensions_lookup_file);
	WriteBytes(SerialiseMap(m_sprite_to_entity_lookup, true), dir / m_sprite_gfx_idx_lookup_file);
	WriteBytes(SerialiseFixedWidth<2>(anim_flags), dir / m_sprite_anim_flags_lookup_file);
	WriteBytes(behaviour_bytes.first, dir / m_sprite_behaviour_offset_file);
	WriteBytes(behaviour_bytes.second, dir / m_sprite_behaviour_table_file);
	auto result = SerialiseRoomEntityTable();
	WriteBytes(result.first, dir / m_room_sprite_table_file);
	WriteBytes(result.second, dir / m_room_sprite_table_offsets_file);
	return true;
}

bool SpriteData::RomPrepareInjectSpriteFrames(const Rom& rom)
{
	uint32_t begin = rom.get_section(RomLabels::Sprites::SPRITE_SECTION).begin;
	uint32_t lut_size = static_cast<uint32_t>(m_animations.size() * 2 * sizeof(uint16_t));
	uint32_t anim_ptr_table_size = static_cast<uint32_t>(m_animation_frames.size() * sizeof(uint32_t));
	uint32_t frame_ptr_table_size = std::accumulate(m_animation_frames.cbegin(), m_animation_frames.cend(), 0,
		[](int sum, const auto& elem) {
			return sum + static_cast<int>(elem.second.size());
		}) * sizeof(uint32_t);

	// Reserve enough space for pointers, etc.
	const uint32_t anim_ptrs_begin = begin + sizeof(uint32_t) + lut_size;
	const uint32_t frame_ptrs_begin = anim_ptrs_begin + anim_ptr_table_size;
	const uint32_t frames_begin = frame_ptrs_begin + frame_ptr_table_size;
	ByteVectorPtr bytes(std::make_shared<ByteVector>(frames_begin - begin));
	std::unordered_map<std::string, uint32_t> frame_ptrs;

	for (const auto& f : m_frames)
	{
		auto b = f.second->GetBytes();
		frame_ptrs.insert({ f.first, static_cast<uint32_t>(begin + bytes->size()) });
		bytes->insert(bytes->end(), b->begin(), b->end());
		if (((bytes->size() + begin) & 1) == 1)
		{
			bytes->push_back(0xFF);
		}
	}

	auto it = Insert<uint32_t>(bytes->begin(), anim_ptrs_begin);
	uint16_t anim_count = 0;
	for (const auto& spr : m_animations)
	{
		it = Insert<uint16_t>(it, anim_count);
		it = Insert<uint16_t>(it, m_sprite_volume[spr.first]);
		anim_count += static_cast<uint16_t>(spr.second.size());
	}
	uint32_t frame_count = 0;
	for (const auto& anim : m_animation_frames)
	{
		it = Insert<uint32_t>(it, frame_count * sizeof(uint32_t) + frame_ptrs_begin);
		frame_count += static_cast<uint32_t>(anim.second.size());
	}
	for (const auto& anim : m_animation_frames)
	{
		for (const auto& frame : anim.second)
		{
			it = Insert<uint32_t>(it, frame_ptrs[frame]);
		}
	}

	m_pending_writes.push_back({ RomLabels::Sprites::SPRITE_SECTION, bytes });

	return true;
}

bool SpriteData::RomPrepareInjectSpritePalettes(const Rom& rom)
{
	uint32_t proj1_begin = rom.get_section(RomLabels::Sprites::PALETTE_PROJECTILE_1).begin;
	auto proj1_bytes = std::make_shared<ByteVector>(SerialisePalArray(m_projectile1_palettes));

	uint32_t proj2_begin = rom.get_section(RomLabels::Sprites::PALETTE_PROJECTILE_2).begin;
	auto proj2_bytes = std::make_shared<ByteVector>(SerialisePalArray(m_projectile2_palettes));

	uint32_t pal_lut_begin = rom.get_section(RomLabels::Sprites::PALETTE_DATA).begin;
	auto bytes = std::make_shared<ByteVector>(SerialisePaletteLUT());
	uint32_t lo_pals_begin = pal_lut_begin + static_cast<uint32_t>(bytes->size());
	for (const auto& p : m_lo_palettes)
	{
		auto b = p->GetBytes();
		bytes->insert(bytes->end(), b->cbegin(), b->cend());
	}
	uint32_t hi_pals_begin = pal_lut_begin + static_cast<uint32_t>(bytes->size());
	for (const auto& p : m_hi_palettes)
	{
		auto b = p->GetBytes();
		bytes->insert(bytes->end(), b->cbegin(), b->cend());
	}

	m_pending_writes.push_back({ RomLabels::Sprites::PALETTE_DATA, bytes });
	m_pending_writes.push_back(Asm::WriteOffset8(rom, RomLabels::Sprites::PALETTE_LUT, pal_lut_begin));
	m_pending_writes.push_back(Asm::WriteOffset8(rom, RomLabels::Sprites::PALETTE_LO_DATA, lo_pals_begin));
	m_pending_writes.push_back(Asm::WriteOffset8(rom, RomLabels::Sprites::PALETTE_HI_DATA, hi_pals_begin));
	m_pending_writes.push_back({ RomLabels::Sprites::PALETTE_PROJECTILE_1, proj1_bytes });
	m_pending_writes.push_back(Asm::WriteOffset8(rom, RomLabels::Sprites::PALETTE_PROJECTILE_1_MOVEW1, proj1_begin));
	m_pending_writes.push_back(Asm::WriteOffset8(rom, RomLabels::Sprites::PALETTE_PROJECTILE_1_MOVEW2, proj1_begin + 2));
	m_pending_writes.push_back(Asm::WriteOffset8(rom, RomLabels::Sprites::PALETTE_PROJECTILE_1_MOVEW3, proj1_begin));
	m_pending_writes.push_back(Asm::WriteOffset8(rom, RomLabels::Sprites::PALETTE_PROJECTILE_1_MOVEW4, proj1_begin + 2));
	m_pending_writes.push_back({ RomLabels::Sprites::PALETTE_PROJECTILE_2, proj2_bytes });
	m_pending_writes.push_back(Asm::WriteOffset8(rom, RomLabels::Sprites::PALETTE_PROJECTILE_2_MOVEW1, proj2_begin));
	m_pending_writes.push_back(Asm::WriteOffset8(rom, RomLabels::Sprites::PALETTE_PROJECTILE_2_MOVEW2, proj2_begin + 2));
	m_pending_writes.push_back(Asm::WriteOffset8(rom, RomLabels::Sprites::PALETTE_PROJECTILE_2_MOVEW3, proj2_begin + 4));
	m_pending_writes.push_back(Asm::WriteOffset8(rom, RomLabels::Sprites::PALETTE_PROJECTILE_2_MOVEW4, proj2_begin + 6));

	return true;
}

bool SpriteData::RomPrepareInjectSpriteData(const Rom& rom)
{
	auto room_entities = SerialiseRoomEntityTable();
	auto behaviour_bytes = Behaviours::Pack(m_sprite_behaviours);

	uint32_t item_begin = rom.get_section(RomLabels::Sprites::ITEM_PROPERTIES_SECTION).begin;
	auto item_bytes = std::make_shared<ByteVector>(SerialiseFixedWidth<4>(m_item_properties, false));

	std::vector<std::array<uint8_t, 2>> anim_flags;
	std::transform(m_sprite_animation_flags.cbegin(), m_sprite_animation_flags.cend(), std::back_inserter<std::vector<std::array<uint8_t, 2>>>(anim_flags), [](const auto& elem)
		{
			return std::array<uint8_t, 2>({ elem.first, elem.second.Pack() });
		});
	uint32_t unk_begin = rom.get_section(RomLabels::Sprites::SPRITE_ANIM_FLAGS_LOOKUP_SECTION).begin;
	auto unk_bytes = std::make_shared<ByteVector>(SerialiseFixedWidth<2>(anim_flags));

	uint32_t behavoff_begin = rom.get_section(RomLabels::Sprites::SPRITE_BEHAVIOUR_SECTION).begin;
	auto behav_bytes = std::make_shared<ByteVector>(behaviour_bytes.first);
	uint32_t behavtab_begin = behavoff_begin + static_cast<uint32_t>(behav_bytes->size());
	behav_bytes->insert(behav_bytes->end(), behaviour_bytes.second.cbegin(), behaviour_bytes.second.cend());

	uint32_t visib_begin = rom.get_section(RomLabels::Sprites::SPRITE_DATA_SECTION).begin;
	auto data_bytes = std::make_shared<ByteVector>(SerialiseFixedWidth<4>(EncodeFlags(m_sprite_visibility_flags)));

	uint32_t onetime_begin = static_cast<uint32_t>(data_bytes->size()) + visib_begin;
	auto onetime_bytes = SerialiseFixedWidth<6>(EncodeFlags(m_one_time_event_flags));
	data_bytes->insert(data_bytes->end(), onetime_bytes.cbegin(), onetime_bytes.cend());

	uint32_t clear_begin = static_cast<uint32_t>(data_bytes->size()) + visib_begin;
	auto clear_bytes = SerialiseFixedWidth<4>(EncodeFlags(m_room_clear_flags));
	data_bytes->insert(data_bytes->end(), clear_bytes.cbegin(), clear_bytes.cend());

	uint32_t door_begin = static_cast<uint32_t>(data_bytes->size()) + visib_begin;
	auto door_bytes = SerialiseFixedWidth<4>(EncodeFlags(m_locked_door_flags));
	data_bytes->insert(data_bytes->end(), door_bytes.cbegin(), door_bytes.cend());

	uint32_t switch_begin = static_cast<uint32_t>(data_bytes->size()) + visib_begin;
	auto switch_bytes = SerialiseFixedWidth<4>(EncodeFlags(m_permanent_switch_flags));
	data_bytes->insert(data_bytes->end(), switch_bytes.cbegin(), switch_bytes.cend());

	uint32_t tree_begin = static_cast<uint32_t>(data_bytes->size()) + visib_begin;
	auto tree_bytes = SerialiseFixedWidth<4>(EncodeFlags(m_sacred_tree_flags));
	data_bytes->insert(data_bytes->end(), tree_bytes.cbegin(), tree_bytes.cend());

	uint32_t sprent_begin = static_cast<uint32_t>(data_bytes->size()) + visib_begin;
	auto sprent_bytes = SerialiseMap(m_sprite_to_entity_lookup, true);
	data_bytes->insert(data_bytes->end(), sprent_bytes.cbegin(), sprent_bytes.cend());

	uint32_t hitbox_begin = static_cast<uint32_t>(data_bytes->size()) + visib_begin;
	auto hitbox_bytes = SerialiseMap<2>(m_sprite_dimensions);
	data_bytes->insert(data_bytes->end(), hitbox_bytes.cbegin(), hitbox_bytes.cend());
	if ((data_bytes->size() & 1) == 1)
	{
		data_bytes->push_back(0xFF);
	}

	uint32_t offsets_begin = static_cast<uint32_t>(data_bytes->size()) + visib_begin;
	data_bytes->insert(data_bytes->end(), room_entities.second.cbegin(), room_entities.second.cend());

	uint32_t enemy_begin = static_cast<uint32_t>(data_bytes->size()) + visib_begin;
	auto enemy_bytes = SerialiseMap<5>(m_enemy_stats);
	data_bytes->insert(data_bytes->end(), enemy_bytes.cbegin(), enemy_bytes.cend());

	uint32_t table_begin = static_cast<uint32_t>(data_bytes->size()) + visib_begin;
	data_bytes->insert(data_bytes->end(), room_entities.first.cbegin(), room_entities.first.cend());

	m_pending_writes.push_back({ RomLabels::Sprites::ITEM_PROPERTIES_SECTION, item_bytes });
	m_pending_writes.push_back(Asm::WriteOffset8(rom, RomLabels::Sprites::ITEM_PROPERTIES, item_begin));
	m_pending_writes.push_back({ RomLabels::Sprites::SPRITE_ANIM_FLAGS_LOOKUP_SECTION, unk_bytes });
	m_pending_writes.push_back(Asm::WriteOffset16(rom, RomLabels::Sprites::SPRITE_ANIM_FLAGS_LOOKUP, unk_begin));
	m_pending_writes.push_back({ RomLabels::Sprites::SPRITE_BEHAVIOUR_SECTION, behav_bytes });
	m_pending_writes.push_back(Asm::WriteOffset16(rom, RomLabels::Sprites::SPRITE_BEHAVIOUR_OFFSETS, behavoff_begin));
	m_pending_writes.push_back(Asm::WriteOffset16(rom, RomLabels::Sprites::SPRITE_BEHAVIOUR_TABLE, behavtab_begin));
	m_pending_writes.push_back({ RomLabels::Sprites::SPRITE_DATA_SECTION, data_bytes });
	m_pending_writes.push_back(Asm::WriteOffset16(rom, RomLabels::Sprites::SPRITE_VISIBILITY_FLAGS, visib_begin));
	m_pending_writes.push_back(Asm::WriteOffset16(rom, RomLabels::Sprites::ONE_TIME_EVENT_FLAGS, onetime_begin));
	m_pending_writes.push_back(Asm::WriteOffset16(rom, RomLabels::Sprites::ROOM_CLEAR_FLAGS, clear_begin));
	m_pending_writes.push_back(Asm::WriteOffset16(rom, RomLabels::Sprites::LOCKED_DOOR_SPRITE_FLAGS, door_begin));
	m_pending_writes.push_back(Asm::WriteOffset16(rom, RomLabels::Sprites::PERMANENT_SWITCH_FLAGS, switch_begin));
	m_pending_writes.push_back(Asm::WriteOffset16(rom, RomLabels::Sprites::SACRED_TREE_FLAGS, tree_begin));
	m_pending_writes.push_back(Asm::WriteOffset16(rom, RomLabels::Sprites::SPRITE_GFX_IDX_LOOKUP, sprent_begin));
	m_pending_writes.push_back(Asm::WriteOffset16(rom, RomLabels::Sprites::SPRITE_DIMENSIONS_LOOKUP, hitbox_begin));
	m_pending_writes.push_back(Asm::WriteOffset16(rom, RomLabels::Sprites::ROOM_SPRITE_TABLE_OFFSETS, offsets_begin));
	m_pending_writes.push_back(Asm::WriteOffset16(rom, RomLabels::Sprites::ENEMY_STATS, enemy_begin));
	m_pending_writes.push_back(Asm::WriteAddress32(RomLabels::Sprites::ROOM_SPRITE_TABLE, table_begin));

	return true;
}

SpriteData::AnimationFlags::AnimationFlags()
  : idle_animation_frames(IdleAnimationFrameCount::ONE_FRAME),
	idle_animation_source(IdleAnimationSource::USE_WALK_FRAMES),
	jump_animation_source(JumpAnimationSource::USE_IDLE_FRAMES),
	walk_animation_frame_count(WalkAnimationFrameCount::FOUR_FRAMES),
	do_not_rotate(false),
	take_damage_animation_source(TakeDamageAnimationSource::USE_IDLE_FRAMES),
	has_full_animations(false)
{
}

SpriteData::AnimationFlags::AnimationFlags(uint8_t packed)
  : idle_animation_frames((packed & (1 << 0)) > 0 ? IdleAnimationFrameCount::ONE_FRAME : IdleAnimationFrameCount::TWO_FRAMES),
	idle_animation_source((packed & (1 << 1)) > 0 ? IdleAnimationSource::DEDICATED : IdleAnimationSource::USE_WALK_FRAMES),
	jump_animation_source((packed & (1 << 2)) > 0 ? JumpAnimationSource::DEDICATED : JumpAnimationSource::USE_IDLE_FRAMES),
	walk_animation_frame_count((packed & (1 << 3)) > 0 ? WalkAnimationFrameCount::TWO_FRAMES : WalkAnimationFrameCount::FOUR_FRAMES),
	do_not_rotate((packed & (1 << 4)) > 0),
	take_damage_animation_source((packed & (1 << 5)) > 0 ? TakeDamageAnimationSource::DEDICATED : TakeDamageAnimationSource::USE_IDLE_FRAMES),
	has_full_animations((packed & (1 << 6)) > 0)
{
}

uint8_t SpriteData::AnimationFlags::Pack() const
{
	return uint8_t(
		(idle_animation_frames == IdleAnimationFrameCount::ONE_FRAME ? (1 << 0) : 0) |
		(idle_animation_source == IdleAnimationSource::DEDICATED ? (1 << 1) : 0) |
		(jump_animation_source == JumpAnimationSource::DEDICATED ? (1 << 2) : 0) |
		(walk_animation_frame_count == WalkAnimationFrameCount::TWO_FRAMES ? (1 << 3) : 0) |
		(do_not_rotate ? (1 << 4) : 0) |
		(take_damage_animation_source == TakeDamageAnimationSource::DEDICATED ? (1 << 5) : 0) |
		(has_full_animations ? (1 << 6) : 0)
	);
}

bool SpriteData::AnimationFlags::IsDefault() const
{
	return (idle_animation_frames == IdleAnimationFrameCount::ONE_FRAME) &&
		   (idle_animation_source == IdleAnimationSource::USE_WALK_FRAMES) &&
		   (jump_animation_source == JumpAnimationSource::USE_IDLE_FRAMES) &&
		   (walk_animation_frame_count == WalkAnimationFrameCount::FOUR_FRAMES) &&
		   !do_not_rotate &&
		   (take_damage_animation_source == TakeDamageAnimationSource::USE_IDLE_FRAMES) &&
		   !has_full_animations;
}

bool SpriteData::AnimationFlags::operator==(const AnimationFlags& rhs) const
{
	return (this->idle_animation_frames == rhs.idle_animation_frames) &&
		   (this->idle_animation_source == rhs.idle_animation_source) &&
		   (this->jump_animation_source == rhs.jump_animation_source) &&
		   (this->walk_animation_frame_count == rhs.walk_animation_frame_count) &&
	       (this->do_not_rotate == rhs.do_not_rotate) &&
		   (this->take_damage_animation_source == rhs.take_damage_animation_source) &&
		   (this->has_full_animations == rhs.has_full_animations);
}

bool SpriteData::AnimationFlags::operator!=(const AnimationFlags& rhs) const
{
	return !(*this == rhs);
}

SpriteData::ItemProperties::ItemProperties(const std::array<uint8_t, 4>& elems)
{
	verb = (elems[0] >> 4) + 12;
	max_quantity = elems[0] & 0x0F;
	equipment_index = elems[1];
	price = (elems[2] << 8) | elems[3];
}

std::array<uint8_t, 4> SpriteData::ItemProperties::Pack() const
{
	return std::array<uint8_t, 4>
	{
		static_cast<uint8_t>(((verb - 12) << 4) | (max_quantity & 0x0F)),
		equipment_index,
		static_cast<uint8_t>(price >> 8),
		static_cast<uint8_t>(price & 0xFF)
	};
}

SpriteData::EnemyStats::EnemyStats(const std::array<uint8_t, 5>& elems)
{
	health = elems[0];
	defence = elems[1];
	gold_drop = elems[2];
	attack = elems[3] & 0x7F;
	item_drop = elems[4] & 0x3F;
	drop_probability = static_cast<SpriteData::EnemyStats::DropProbability>((elems[4] >> 6) | ((elems[3] & 0x80) >> 5));
}

std::array<uint8_t, 5> SpriteData::EnemyStats::Pack() const
{
	uint8_t prob = static_cast<uint8_t>(drop_probability);
	return std::array<uint8_t, 5>
	{
		health,
		defence,
		gold_drop,
		static_cast<uint8_t>((attack & 0x7F) | ((prob & 0x04) << 5)),
		static_cast<uint8_t>((item_drop & 0x3F) | (prob << 6))
	};
}

} // namespace Landstalker
