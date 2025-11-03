#ifndef _ROM_OFFSETS_H_
#define _ROM_OFFSETS_H_

#include <cstdint>
#include <unordered_map>
#include <string>
#include <optional>

namespace Landstalker {

class RomOffsets
{
public:
	struct Section
	{
		uint32_t begin;
		uint32_t end;
		inline uint32_t size() const
		{
			return end - begin;
		};
	};

	enum class Region
	{
		JP,
		US,
		UK,
		FR,
		DE,
		US_BETA,
		COUNT
	};

	static const uint32_t CHECKSUM_ADDRESS  = 0x00018E;
	static const uint32_t CHECKSUM_BEGIN    = 0x000200;
	static const uint32_t BUILD_DATE_BEGIN  = 0x000202;
	static const uint32_t BUILD_DATE_LENGTH = 0x00000E;
	static const uint32_t EXPECTED_SIZE     = 0x200000;

	static std::optional<std::string> GetRegionName(Region region);
	static std::optional<Region> GetRegionFromReleaseDate(const std::string& release_date);
	static std::optional<uint32_t> GetAddress(const std::string& label, std::optional<Region> region = std::nullopt);
	static std::optional<Section> GetSection(const std::string& label, std::optional<Region> region = std::nullopt);
	static bool SectionExists(const std::string& label);
	static bool AddressExists(const std::string& label);
private: 
	RomOffsets();

	static const RomOffsets& GetInstance();

	const std::unordered_map<Region, std::string> REGION_NAMES;
	const std::unordered_map<std::string, Region> RELEASE_BUILD_DATE;
	const std::unordered_map<std::string, std::unordered_map<Region, uint32_t>> ADDRESS;
	const std::unordered_map<std::string, std::unordered_map<Region, Section>> SECTION;
};
} // namespace Landstalker

#endif // _ROM_OFFSETS_H_
