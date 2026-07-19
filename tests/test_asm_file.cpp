#include <landstalker/main/AsmFile.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

class TemporaryAsmFile
{
public:
    explicit TemporaryAsmFile(const std::string& contents)
        : path(std::filesystem::temp_directory_path() /
               ("landstalker_asmfile_" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()) + ".asm"))
    {
        std::ofstream stream(path);
        stream << contents;
    }

    ~TemporaryAsmFile()
    {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    std::filesystem::path path;
};

TEST(AsmFileTest, ExpandsByteStringLiterals)
{
    TemporaryAsmFile source(
        "RegionErrorLine1:\t\tdc.b \"  DEVELOPED FOR USE ONLY WITH\",$00\n"
        "Punctuation:\t\tdc.b \"A,B;C\",\"\"\"\",$00 ; trailing comment\n"
        "MixedQuotes:\t\tdc.b '\"',\"'\",$00\n");

    Landstalker::AsmFile file(source.path);
    const std::vector<uint8_t> actual = file.ToBinary();

    std::vector<uint8_t> expected;
    const std::string first = "  DEVELOPED FOR USE ONLY WITH";
    expected.insert(expected.end(), first.begin(), first.end());
    expected.push_back(0);
    const std::string punctuation = "A,B;C\"";
    expected.insert(expected.end(), punctuation.begin(), punctuation.end());
    expected.push_back(0);
    expected.push_back('"');
    expected.push_back('\'');
    expected.push_back(0);

    EXPECT_EQ(actual, expected);
}

TEST(AsmFileTest, WritesPrintableRunsAndHexadecimalBytes)
{
    std::string value = "QUOTE\"END";
    value.push_back('\0');

    Landstalker::AsmFile file;
    file << Landstalker::AsmFile::Label("Encoded") << Landstalker::AsmFile::String(value);
    const std::string assembly = file.ToAssembly();

    EXPECT_NE(assembly.find("dc.b     \"QUOTE\",$22,\"END\",$00"), std::string::npos);

    TemporaryAsmFile encoded(assembly);
    Landstalker::AsmFile reread(encoded.path);
    const std::vector<uint8_t> expected(value.begin(), value.end());
    EXPECT_EQ(reread.ToBinary(), expected);
}

} // namespace
