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
// but code/audio only ever needs a handful of things read back out of it: line labels ("Name:"),
// `equ` constants, `db`/`dw` data directives (read as a flat byte stream, matching AsmFile::ToBinary
// for the 68k side), `include "path"` (inlined recursively, relative to the including file's own
// directory), and `org ADDR` (pads the data stream with zero bytes up to ADDR, relative to the
// address of the first `org` seen - used by files like soundbank4.asm that jump to a fixed offset
// mid-file). Z80 instructions themselves are not parsed - any other line is skipped on read and is
// not round-tripped.
//
// A `dw` operand that isn't a number is treated as a label reference: the emitted word is a
// placeholder until the whole file (and everything it includes) has been parsed, at which point
// every such reference is resolved to the referenced label's own offset into the data stream (i.e.
// the same value Goto() would use) - this lets pointer tables be read back with dw operands like
// `dw MUSIC_20` before MUSIC_20 has been seen.
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
    bool GetLabelOffset(const std::string& label, std::size_t& offset) const;
    // The smallest label offset strictly greater than `offset`, or the data stream's total size if
    // no label follows it. Data structures whose streams have no explicit terminator (e.g. a music
    // channel that ends with a backward jump) are delimited by the next label in the source.
    std::size_t NextLabelOffsetAfter(std::size_t offset) const;
    // Moves the read position to the given label's offset into the data stream.
    bool Goto(const std::string& label);

    // Reads up to `count` bytes from the current read position (dw values split little-endian),
    // advancing it. Returns fewer than `count` bytes at the end of the data.
    std::vector<uint8_t> ReadBytes(std::size_t count);
    // Reads up to `count` bytes starting at a specific offset, without touching the read position.
    std::vector<uint8_t> ReadBytesAt(std::size_t offset, std::size_t count) const;
    // Every db/dw byte in the file, in order.
    const std::vector<uint8_t>& ToBinary() const { return m_data; }

    // Output: build a file with WriteComment/WriteLabel/WriteBytes, then WriteFile to save it.
    void WriteComment(const std::string& comment);
    void WriteLabel(const std::string& label);
    void WriteBytes(const std::vector<uint8_t>& bytes, std::size_t per_line = 8);
    // Emits a run of `dw NAME` lines, one per label, e.g. for symbolic pointer tables.
    void WriteWordRefs(const std::vector<std::string>& labels);
    // Emits a line verbatim - for boilerplate (cpu/phase/org/include) this class has no typed
    // writer for.
    void WriteRaw(const std::string& line);
    bool WriteFile(const std::filesystem::path& filename) const;

private:
    struct Fixup
    {
        std::size_t data_offset;
        std::string label;
    };

    void ParseFile(const std::filesystem::path& path);
    void ParseLine(const std::filesystem::path& current_dir, std::string line);
    void ResolveFixups();
    static bool ParseNumber(const std::string& token, uint32_t& value);

    std::vector<uint8_t> m_data;
    std::map<std::string, std::size_t> m_labels;  // label -> offset into m_data
    std::map<std::string, uint32_t> m_constants;   // equ name -> value
    std::vector<Fixup> m_fixups;                   // pending `dw LABEL` word references
    std::size_t m_readpos = 0;
    bool m_good = true;

    bool m_org_base_set = false;
    uint32_t m_org_base = 0; // address corresponding to m_data offset 0, set by the first `org` seen

    std::vector<std::string> m_out_lines; // buffered output lines for WriteFile
};

} // namespace Landstalker

#endif // _Z80_ASM_FILE_H_
