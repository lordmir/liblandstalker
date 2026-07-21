#ifndef _END_CREDIT_STRING_H_
#define _END_CREDIT_STRING_H_

#include <vector>
#include <landstalker/main/RomOffsets.h>
#include <landstalker/text/LSString.h>

namespace Landstalker {

class EndCreditString : public LSString
{
public:
	EndCreditString();
	EndCreditString(int8_t fmt0, int8_t fmt1, const StringType& str);
	EndCreditString(const StringType& serialised);

	bool operator==(const EndCreditString& rhs) const;
	bool operator!=(const EndCreditString& rhs) const;

	// The credit fonts differ by region, so the charset does too. The German and French fonts carry
	// extra letters above the shared codes, the Japanese font redefines one of them, and the
	// kerning codes begin just above each font's last glyph and so move with it - see _rclChkKern
	// in endcredits2.asm.
	static const CharacterSet& GetDefaultCharset(RomOffsets::Region region = RomOffsets::Region::US);

	virtual size_t Decode(const uint8_t* buffer, size_t size);
	virtual size_t Encode(uint8_t* buffer, size_t size) const;
	virtual StringType Serialise() const;
	virtual void Deserialise(const StringType& ss);
	virtual StringType GetHeaderRow() const;

	int8_t GetHeight() const;
	int8_t GetColumn() const;
	void SetHeight(int8_t val);
	void SetColumn(int8_t val);
	void SetStr(const StringType& str);
protected:
	virtual size_t DecodeString(const uint8_t* string, size_t len);
	virtual size_t EncodeString(uint8_t* string, size_t len) const;
private:
	int8_t m_height;
	int8_t m_column;
};

} // namespace Landstalker

#endif // _END_CREDIT_STRING_H