#ifndef _Z80_ASM_FILE_H_
#define _Z80_ASM_FILE_H_

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Landstalker {

// A minimal reader/writer for the Z80 (asw-syntax) sound driver disassembly under code/audio.
// AsmFile targets 68k asm68k syntax (dc.b/dc.w, '$' hex literals) and cannot parse this source,
// but code/audio only ever needs three things read back out of it: line labels ("Name:"), `equ`
// constants, and `db`/`dw` data directives (read as a flat byte stream, matching AsmFile::ToBinary
// for the 68k side). Z80 instructions themselves are not parsed - any other line is skipped on
// read and is not round-tripped.
class Z80AsmFile
{
public:
    Z80AsmFile() = default;
    explicit Z80AsmFile(const std::filesystem::path& filename);

    bool Good() const { return m_good; }

    // Constants declared as `NAME equ VALUE`.
    bool ConstantExists(const std::string& name) const;
    uint32_t GetConstant(const std::string& name) const;

    // Line labels ("NAME:"), positioned by their byte offset into the db/dw data stream.
    bool LabelExists(const std::string& label) const;
    // Moves the read position to the given label's offset into the data stream.
    bool Goto(const std::string& label);

    // Reads up to `count` bytes from the current read position (dw values split little-endian),
    // advancing it. Returns fewer than `count` bytes at the end of the data.
    std::vector<uint8_t> ReadBytes(std::size_t count);
    // Every db/dw byte in the file, in order.
    const std::vector<uint8_t>& ToBinary() const { return m_data; }

    // Output: build a file with WriteComment/WriteLabel/WriteBytes, then WriteFile to save it.
    void WriteComment(const std::string& comment);
    void WriteLabel(const std::string& label);
    void WriteBytes(const std::vector<uint8_t>& bytes, std::size_t per_line = 8);
    bool WriteFile(const std::filesystem::path& filename) const;

private:
    void ParseLine(std::string line);
    static bool ParseNumber(const std::string& token, uint32_t& value);

    std::vector<uint8_t> m_data;
    std::map<std::string, std::size_t> m_labels;  // label -> offset into m_data
    std::map<std::string, uint32_t> m_constants;   // equ name -> value
    std::size_t m_readpos = 0;
    bool m_good = true;

    std::vector<std::string> m_out_lines; // buffered output lines for WriteFile
};

} // namespace Landstalker

#endif // _Z80_ASM_FILE_H_
