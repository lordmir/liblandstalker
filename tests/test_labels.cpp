#include <gtest/gtest.h>
#include <landstalker/misc/Labels.h>
#include <filesystem>
#include <string>

using namespace Landstalker;

class LabelsTest : public ::testing::Test {
protected:
    void SetUp() override {
        Labels::InitDefaults();
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(TempPath(), ec);
        Labels::InitDefaults();
    }

    static std::string TempPath() {
        return (std::filesystem::temp_directory_path() / "landstalker_labels_test.yaml").string();
    }

    // Saves, wipes the table, reloads, and hands back whatever survived.
    static std::optional<std::wstring> RoundTrip(const std::wstring& category, int id) {
        Labels::SaveData(TempPath());
        Labels::InitDefaults();
        Labels::LoadData(TempPath());
        return Labels::Get(category, id);
    }

    // Ids well clear of the defaults, and paths that cannot collide with them - IsValid
    // enforces uniqueness against every existing label, parent paths included.
    static constexpr int TEST_ID = 900;
};

TEST_F(LabelsTest, PlainLabelRoundTrips) {
    const std::wstring label = L"ZZTest/1F/Entrance Hall";
    ASSERT_TRUE(Labels::Update(Labels::C_ROOMS, TEST_ID, label));
    EXPECT_EQ(label, RoundTrip(Labels::C_ROOMS, TEST_ID));
}

// Apostrophes were rejected outright while SaveData quoted values by hand, which silently
// hid every label containing one - 226 room labels in the shipped label file.
TEST_F(LabelsTest, ApostropheLabelRoundTrips) {
    const std::wstring label = L"ZZTest/1F/Guard's Rest Room";
    ASSERT_TRUE(Labels::Update(Labels::C_ROOMS, TEST_ID, label));
    EXPECT_TRUE(Labels::Get(Labels::C_ROOMS, TEST_ID).has_value());
    EXPECT_EQ(label, RoundTrip(Labels::C_ROOMS, TEST_ID));
}

TEST_F(LabelsTest, QuotesAndBackslashesRoundTrip) {
    const std::wstring quoted = L"ZZTest/Room called \"Home\"";
    const std::wstring escaped = L"ZZTest/Back\\slash room";
    ASSERT_TRUE(Labels::Update(Labels::C_ROOMS, TEST_ID, quoted));
    ASSERT_TRUE(Labels::Update(Labels::C_ROOMS, TEST_ID + 1, escaped));
    Labels::SaveData(TempPath());
    Labels::InitDefaults();
    Labels::LoadData(TempPath());
    EXPECT_EQ(quoted, Labels::Get(Labels::C_ROOMS, TEST_ID));
    EXPECT_EQ(escaped, Labels::Get(Labels::C_ROOMS, TEST_ID + 1));
}

TEST_F(LabelsTest, YamlPunctuationRoundTrips) {
    const std::wstring label = L"ZZTest/Room: the sequel #2";
    ASSERT_TRUE(Labels::Update(Labels::C_ROOMS, TEST_ID, label));
    EXPECT_EQ(label, RoundTrip(Labels::C_ROOMS, TEST_ID));
}

TEST_F(LabelsTest, NonAsciiRoundTrips) {
    const std::wstring label = L"ZZTest/Chambre de Grégoire";
    ASSERT_TRUE(Labels::Update(Labels::C_ROOMS, TEST_ID, label));
    EXPECT_EQ(label, RoundTrip(Labels::C_ROOMS, TEST_ID));
}

// Three categories write their keys as hex, which must still parse back as ints.
TEST_F(LabelsTest, HexFormattedKeysRoundTrip) {
    const std::wstring label = L"ZZTest/Blockset's Label";
    ASSERT_TRUE(Labels::Update(Labels::C_BLOCKSETS, 0x123456, label));
    EXPECT_EQ(label, RoundTrip(Labels::C_BLOCKSETS, 0x123456));
}

TEST_F(LabelsTest, UnprintableCharactersAreStillRejected) {
    EXPECT_FALSE(Labels::NormalizePath(L"bad\tlabel").has_value());
    EXPECT_FALSE(Labels::NormalizePath(L"bad\nlabel").has_value());
    EXPECT_FALSE(Labels::NormalizePath(L"nbsp\u00A0space").has_value());   // non-breaking space
    EXPECT_FALSE(Labels::NormalizePath(L"").has_value());
    EXPECT_FALSE(Labels::NormalizePath(L"/leading").has_value());
    EXPECT_FALSE(Labels::NormalizePath(L"trailing/").has_value());
    EXPECT_FALSE(Labels::NormalizePath(L"double//slash").has_value());
}

TEST_F(LabelsTest, QuotesAndBackslashesArePermitted) {
    EXPECT_TRUE(Labels::NormalizePath(L"Guard's Room").has_value());
    EXPECT_TRUE(Labels::NormalizePath(L"say \"hi\"").has_value());
    EXPECT_TRUE(Labels::NormalizePath(L"back\\slash").has_value());
}

TEST_F(LabelsTest, PathSegmentsAreTrimmed) {
    const auto normalized = Labels::NormalizePath(L"  Outer  /  Inner  ");
    ASSERT_TRUE(normalized.has_value());
    EXPECT_EQ(std::wstring(L"Outer/Inner"), *normalized);
}
