#ifndef _CHARSET_H_
#define _CHARSET_H_

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

	inline RomOffsets::Region DeduceRegion(uint32_t printable_charset_size)
	{
		switch (printable_charset_size)
		{
		case 237:
			return RomOffsets::Region::JP;
		case 98:
			return RomOffsets::Region::FR;
		case 63:
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
