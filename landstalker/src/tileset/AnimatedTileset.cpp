#include <landstalker/tileset/AnimatedTileset.h>
#include <yaml-cpp/yaml.h>

namespace Landstalker {

AnimatedTileset::AnimatedTileset(uint16_t base, uint16_t length, uint8_t speed, uint8_t frames)
	: m_base(base),
	  m_length(length),
	  m_speed(speed),
	  m_frames(frames),
	  m_base_tileset(0)
{
}

AnimatedTileset::AnimatedTileset(const std::vector<uint8_t>& bytes, uint16_t base, uint16_t length, uint8_t speed, uint8_t frames)
	: Tileset(bytes),
	m_base(base),
	m_length(length),
	m_speed(speed),
	m_frames(frames),
	m_base_tileset(0)
{
}

AnimatedTileset::AnimatedTileset(const std::string& filename, uint16_t base, uint16_t length, uint8_t speed, uint8_t frames)
	: Tileset(filename),
	  m_base(base),
	  m_length(length),
	  m_speed(speed),
	  m_frames(frames),
	  m_base_tileset(0)
{
}

AnimatedTileset::AnimatedTileset()
	: Tileset(),
	  m_base(0),
	  m_length(0),
	  m_speed(0),
	  m_frames(0),
	  m_base_tileset(0)
{
}

bool AnimatedTileset::operator==(const AnimatedTileset& rhs) const
{
	return ((*static_cast<const Tileset*>(this) == *static_cast<const Tileset*>(&rhs)) &&
		(this->m_base == rhs.m_base) &&
		(this->m_frames == rhs.m_frames) &&
		(this->m_length == rhs.m_length) &&
		(this->m_speed == rhs.m_speed) &&
		(this->m_base_tileset == rhs.m_base_tileset));
}

bool AnimatedTileset::operator!=(const AnimatedTileset& rhs) const
{
	return !(*this == rhs);
}

std::vector<uint8_t> AnimatedTileset::GetTile(const Tile& tile, uint8_t frame) const
{
	auto t = tile.GetIndex() - GetStartTile().GetIndex();
	auto f_offset = frame * GetFrameSizeTiles();
	return Tileset::GetTile(static_cast<uint16_t>(t + f_offset));
}

std::vector<uint8_t>& AnimatedTileset::GetTilePixels(int tile_index, uint8_t frame)
{
	auto t = tile_index - GetStartTile().GetIndex();
	auto f_offset = frame * GetFrameSizeTiles();
	return Tileset::GetTilePixels(t + f_offset);
}

uint16_t AnimatedTileset::GetBaseBytes() const
{
	return m_base;
}

Tile AnimatedTileset::GetStartTile() const
{
	return Tile(m_base / static_cast<uint16_t>(Tileset::GetTileSizeBytes()));
}

uint16_t AnimatedTileset::GetFrameSizeBytes() const
{
	return m_length;
}

std::size_t AnimatedTileset::GetFrameSizeTiles() const
{
	return (2 * m_length) / Tileset::GetTileSizeBytes();
}

uint8_t AnimatedTileset::GetAnimationSpeed() const
{
	return m_speed;
}

uint8_t AnimatedTileset::GetAnimationFrames() const
{
	return m_frames;
}

uint8_t AnimatedTileset::GetBaseTileset() const
{
	return m_base_tileset;
}

std::string AnimatedTileset::ExtractMetadataYaml() const
{
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "AnimatedTileset" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "base_tile_index" << YAML::Value << (m_base / Tileset::GetTileSizeBytes());
	out << YAML::Key << "animation_tiles" << YAML::Value << ((m_length * 2) / Tileset::GetTileSizeBytes());
	out << YAML::Key << "animation_frame_count" << YAML::Value << static_cast<int>(m_frames);
	out << YAML::Key << "animation_speed_hz" << YAML::Value << static_cast<double>(60.0 / m_speed);
	out << YAML::Key << "base_tileset" << YAML::Value << static_cast<int>(m_base_tileset);
	out << YAML::EndMap;
	return std::string(out.c_str());
}

bool AnimatedTileset::InsertMetadataYaml(const std::string& yaml_data)
{
	YAML::Node doc = YAML::Load(yaml_data);
	if (!doc["AnimatedTileset"])
	{
		return false;
	}
	doc = doc["AnimatedTileset"];
	if (doc["base_tile_index"])
	{
		m_base = static_cast<uint16_t>(doc["base_tile_index"].as<uint16_t>() * Tileset::GetTileSizeBytes());
	}
	if (doc["animation_tiles"])
	{
		m_length = static_cast<uint16_t>(doc["animation_tiles"].as<uint16_t>() * Tileset::GetTileSizeBytes() / 2);
	}
	if (doc["animation_speed_hz"] && doc["animation_speed_hz"].as<double>() > 0.0)
	{
		double frameskip = 60.0 / doc["animation_speed_hz"].as<double>();
		m_speed = static_cast<uint8_t>(std::clamp(frameskip, 1.0, 255.0));
	}
	if (doc["animation_frame_count"])
	{
		m_frames = doc["animation_frame_count"].as<uint8_t>();
	}
	if (doc["base_tileset"])
	{
		m_base_tileset = doc["base_tileset"].as<uint8_t>();
	}
	return true;
}

void AnimatedTileset::SetBaseBytes(uint16_t base)
{
	m_base = base;
}

void AnimatedTileset::SetStartTile(Tile tile)
{
	m_base = static_cast<uint16_t>(tile.GetIndex() * Tileset::GetTileSizeBytes());
}

void AnimatedTileset::SetFrameSizeBytes(uint16_t bytes)
{
	m_length = bytes;
}

void AnimatedTileset::SetFrameSizeTiles(std::size_t count)
{
	m_length = static_cast<uint16_t>(count * Tileset::GetTileSizeBytes() / 2);
}

void AnimatedTileset::SetAnimationSpeed(uint8_t speed)
{
	m_speed = speed;
}

void AnimatedTileset::SetAnimationFrames(uint8_t frames)
{
	m_frames = frames;
}

void AnimatedTileset::SetBaseTileset(uint8_t base_tileset)
{
	m_base_tileset = base_tileset;
}

} // namespace Landstalker
