#ifndef _CHARSET_H_
#define _CHARSET_H_

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <landstalker/main/RomOffsets.h>
#include <landstalker/text/LSString.h>
#include <landstalker/misc/Literals.h>

namespace Landstalker {

namespace Charset
{
	extern const LSString::CharacterSet DEFAULT_ENGLISH_CHARSET;
	extern const LSString::CharacterSet DEFAULT_FRENCH_CHARSET;
	extern const LSString::CharacterSet DEFAULT_GERMAN_CHARSET;
	extern const LSString::CharacterSet DEFAULT_JAPANESE_CHARSET;
	extern const LSString::DiacriticMap JAPANESE_DIACRITIC_MAP;
	extern const LSString::DiacriticMap DEFAULT_DIACRITIC_MAP;

	// A single named code-point constant (CHR_*) that the disassembly's
	// assembler source references and that the editor emits into the generated
	// charset include file. The value is stored explicitly (loaded from the
	// charset YAML) rather than derived, so region-specific layouts - e.g. the
	// Japanese ROM, whose control characters live in the menu-charset code range
	// and are not contiguous with the string marker - round-trip exactly.
	struct CharsetConstant
	{
		std::string name;
		int value = 0;

		bool operator==(const CharsetConstant& rhs) const
		{
			return name == rhs.name && value == rhs.value;
		}
		bool operator!=(const CharsetConstant& rhs) const { return !(*this == rhs); }
	};

	// Complete set of character mappings used by a particular ROM/disassembly,
	// as stored in a charset YAML file (Main/Menu/Intro/Credits sections plus
	// Diacritics, ControlChars and Constants).
	struct Charsets
	{
		LSString::CharacterSet main;
		LSString::CharacterSet menu;
		LSString::CharacterSet intro;
		LSString::CharacterSet credits;
		LSString::DiacriticMap diacritics;
		std::map<std::string, LSString::StringType> control_chars;
		std::vector<CharsetConstant> constants;
		uint8_t eos_marker = LSString::DEFAULT_EOS_MARKER;

		bool operator==(const Charsets& rhs) const
		{
			return main == rhs.main && menu == rhs.menu && intro == rhs.intro && credits == rhs.credits
			    && diacritics == rhs.diacritics && control_chars == rhs.control_chars
			    && constants == rhs.constants && eos_marker == rhs.eos_marker;
		}
		bool operator!=(const Charsets& rhs) const { return !(*this == rhs); }
	};

	// Built-in charsets for the given region, used when no charset YAML exists.
	Charsets GetDefaultCharsets(RomOffsets::Region region);

	// Language suffix used in metadata charset filenames, e.g. "en" -> charset_en.yaml
	std::string GetCharsetYamlName(RomOffsets::Region region);

	// Merges the contents of a charset YAML file over the top of the supplied
	// charsets. Sections absent from the file are left untouched. Returns false
	// (leaving charsets in an unspecified state) if the file cannot be parsed.
	bool LoadCharsetsFromYaml(const std::filesystem::path& path, Charsets& charsets);

	// Writes the full set of charsets out as a charset YAML file.
	bool SaveCharsetsToYaml(const std::filesystem::path& path, const Charsets& charsets);

	inline RomOffsets::Region DeduceRegion(uint32_t printable_charset_size)
	{
		switch (printable_charset_size)
		{
		case 237:
			return RomOffsets::Region::JP;
		case 98:
		case 100:
			return RomOffsets::Region::FR;
		case 63:
		case 65:
			return RomOffsets::Region::DE;
		case 85:
		default:
			return RomOffsets::Region::US;
		}
	}

	inline const LSString::CharacterSet& GetDefaultCharset(RomOffsets::Region region)
	{
		switch (region)
		{
		case RomOffsets::Region::JP:
			return DEFAULT_JAPANESE_CHARSET;
		case RomOffsets::Region::FR:
			return DEFAULT_FRENCH_CHARSET;
		case RomOffsets::Region::DE:
			return DEFAULT_GERMAN_CHARSET;
		case RomOffsets::Region::US:
		case RomOffsets::Region::UK:
		case RomOffsets::Region::US_BETA:
		default:
			return DEFAULT_ENGLISH_CHARSET;
		}
	}

	inline uint8_t GetEOSChar(RomOffsets::Region region)
	{
		switch (region)
		{
		case RomOffsets::Region::JP:
			return 0xE9_u8;
		case RomOffsets::Region::FR:
			return 0x64_u8;
		case RomOffsets::Region::DE:
			return 0x41_u8;
		case RomOffsets::Region::US:
		case RomOffsets::Region::UK:
		case RomOffsets::Region::US_BETA:
		default:
			return 0x55_u8;
		}
	}

	inline const LSString::DiacriticMap& GetDiacriticMap(RomOffsets::Region region)
	{
		if (region == RomOffsets::Region::JP)
		{
			return JAPANESE_DIACRITIC_MAP;
		}
		return DEFAULT_DIACRITIC_MAP;
	}

} // namespace Charset

} // namespace Landstalker

#endif // _CHARSET_H_
