#include <gtest/gtest.h>
#include <landstalker/script/ScriptFunctionTable.h>
#include <landstalker/main/AsmFile.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

using namespace Landstalker;
namespace S = Landstalker::Statements;

namespace {

S::Action IdAction(std::size_t script_id)
{
    return S::Action(AsmFile::ScriptAction(AsmFile::ScriptId(script_id)));
}

S::Action JumpAction(const std::string& func)
{
    return S::Action(AsmFile::ScriptAction(AsmFile::ScriptJump(func)));
}

ScriptFunction MakeFunction(const std::string& name, S::ScriptStatementVector&& statements)
{
    return ScriptFunction(name, statements);
}

// A table exercising every construct the editor round-trips: plain actions, jumps,
// nested prompts/flags, branches (narrow and wide), shop/church interactions,
// progress lists and custom ASM mentioning another function's label.
ScriptFunctionTable MakeSampleTable()
{
    ScriptFunctionTable table;

    table.AddFunction(MakeFunction("Intro", {
        S::PlaySound(0x12),
        S::SetFlagOnTalk(0x0040, IdAction(5), JumpAction("Helper")),
        S::YesNoPrompt(IdAction(10), IdAction(11), IdAction(12)),
        S::Rts()
    }));

    // Note: DisplayPrice, Branch, ShopInteraction, Rts, ProgressList and ActionTable all
    // terminate a function - each function below carries exactly one, as its last statement.
    table.AddFunction(MakeFunction("Helper", {
        S::IsFlagSet(0x0041, IdAction(20), IdAction(21)),
        S::Sleep(30),
        S::Branch("BranchTarget", false)
    }));

    table.AddFunction(MakeFunction("BranchTarget", {
        S::CustomItemScript(0x0100),
        S::Branch("Intro", true)
    }));

    table.AddFunction(MakeFunction("Price", {
        S::DisplayPrice(IdAction(33))
    }));

    table.AddFunction(MakeFunction("ShopStuff", {
        S::ShopInteraction(IdAction(1), IdAction(2), IdAction(3), IdAction(4))
    }));

    table.AddFunction(MakeFunction("ChurchStuff", {
        S::ChurchInteraction(IdAction(6), JumpAction("Helper")),
        S::Rts()
    }));

    table.AddFunction(MakeFunction("Progress", {
        S::ProgressList({
            { S::ProgressList::QuestProgress(0, 1), IdAction(40) },
            { S::ProgressList::QuestProgress(2, 3), JumpAction("Helper") }
        })
    }));

    table.AddFunction(MakeFunction("AsmMention", {
        S::CustomAsm(std::string("\tlea (BranchTarget).l, a0")),
        S::Rts()
    }));

    return table;
}

const S::ScriptStatement& StatementAt(const ScriptFunctionTable& table,
                                      const std::string& func, std::size_t index)
{
    const ScriptFunction* mapping = table.GetMapping(func);
    EXPECT_NE(mapping, nullptr) << "missing function " << func;
    EXPECT_GT(mapping->statements->size(), index);
    return mapping->statements->at(index);
}

} // namespace

TEST(ScriptStatementsTest, ValueConstructorEquality)
{
    EXPECT_EQ(S::PlaySound(0x12), S::PlaySound(0x12));
    EXPECT_NE(S::PlaySound(0x12), S::PlaySound(0x13));

    EXPECT_EQ(S::Branch("Label", false), S::Branch("Label", false));
    EXPECT_NE(S::Branch("Label", false), S::Branch("Label", true));
    EXPECT_NE(S::Branch("Label", false), S::Branch("Other", false));

    EXPECT_EQ(IdAction(5), IdAction(5));
    EXPECT_NE(IdAction(5), IdAction(6));
    EXPECT_EQ(JumpAction("Func"), JumpAction("Func"));
    EXPECT_NE(JumpAction("Func"), IdAction(5));

    EXPECT_EQ(S::SetFlagOnTalk(1, IdAction(2), IdAction(3)),
              S::SetFlagOnTalk(1, IdAction(2), IdAction(3)));
    EXPECT_NE(S::SetFlagOnTalk(1, IdAction(2), IdAction(3)),
              S::SetFlagOnTalk(1, IdAction(2), IdAction(4)));
}

TEST(ScriptStatementsTest, ActionComparesInlineFunctionContents)
{
    // Two separately allocated inline functions with the same contents must compare equal:
    // Action::operator== compares the pointed-to functions, not shared_ptr identity.
    S::Action a{ MakeFunction("Inline", { S::PlaySound(1), S::Rts() }) };
    S::Action b{ MakeFunction("Inline", { S::PlaySound(1), S::Rts() }) };
    S::Action c{ MakeFunction("Inline", { S::PlaySound(2), S::Rts() }) };
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(ScriptFunctionTest, EqualityComparesStatementContents)
{
    ScriptFunction a = MakeFunction("F", { S::PlaySound(1), S::Rts() });
    ScriptFunction b = MakeFunction("F", { S::PlaySound(1), S::Rts() });
    ScriptFunction c = MakeFunction("F", { S::PlaySound(2), S::Rts() });
    ScriptFunction d = MakeFunction("G", { S::PlaySound(1), S::Rts() });
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
}

TEST(ScriptFunctionTableTest, YamlRoundTripPreservesContent)
{
    ScriptFunctionTable original = MakeSampleTable();
    const std::string yaml = original.ToYaml("Test");

    ScriptFunctionTable parsed(yaml);
    EXPECT_EQ(original, parsed);
}

TEST(ScriptFunctionTableTest, YamlEmissionIsStable)
{
    ScriptFunctionTable original = MakeSampleTable();
    const std::string first = original.ToYaml("Test");
    ScriptFunctionTable parsed(first);
    const std::string second = parsed.ToYaml("Test");
    EXPECT_EQ(first, second);
}

TEST(ScriptFunctionTableTest, AsmRoundTripPreservesContent)
{
    ScriptFunctionTable original = MakeSampleTable();

    AsmFile out;
    ASSERT_TRUE(original.WriteAsm(out));
    const auto path = std::filesystem::temp_directory_path() / "test_script_roundtrip.asm";
    ASSERT_TRUE(out.WriteFile(path));

    AsmFile in(path);
    ScriptFunctionTable reread(in);
    std::filesystem::remove(path);

    EXPECT_EQ(original, reread);
}

TEST(ScriptFunctionTableTest, ConsolidationLosesNothing)
{
    // ToYaml consolidates (inlines singly-referenced functions) and parsing unconsolidates.
    // Functions referenced by branches or mentioned in custom ASM must survive: they are
    // not relocatable, and a function must never be inlined into itself.
    ScriptFunctionTable original = MakeSampleTable();
    ScriptFunctionTable parsed(original.ToYaml("Test"));

    for (const std::string& name : { "Intro", "Helper", "BranchTarget", "Price",
                                     "ShopStuff", "ChurchStuff", "Progress", "AsmMention" })
    {
        EXPECT_NE(parsed.GetMapping(name), nullptr) << name << " lost in consolidation";
    }
    EXPECT_EQ(original.GetFunctionNames(), parsed.GetFunctionNames());
}

TEST(ScriptFunctionTableTest, RenameFunctionUpdatesReferences)
{
    ScriptFunctionTable table = MakeSampleTable();
    ASSERT_TRUE(table.RenameFunction("Helper", "Assistant"));

    EXPECT_EQ(table.GetMapping("Helper"), nullptr);
    ASSERT_NE(table.GetMapping("Assistant"), nullptr);

    // The jump held by Intro's SetFlagOnTalk must now target the new name.
    const auto& stmt = StatementAt(table, "Intro", 1);
    const auto* set_flag = std::get_if<S::SetFlagOnTalk>(&stmt);
    ASSERT_NE(set_flag, nullptr);
    const auto* jump = std::get_if<AsmFile::ScriptJump>(&set_flag->on_set.action);
    ASSERT_NE(jump, nullptr);
    EXPECT_EQ(jump->func, "Assistant");

    // Renaming to an existing name or from a missing name must fail.
    EXPECT_FALSE(table.RenameFunction("Assistant", "Intro"));
    EXPECT_FALSE(table.RenameFunction("DoesNotExist", "Whatever"));
}

TEST(ScriptFunctionTableTest, RenameReferencesUpdatesOtherTable)
{
    // Cross-pool scenario: other_table references a function defined in owner_table.
    ScriptFunctionTable owner = MakeSampleTable();
    ScriptFunctionTable other;
    other.AddFunction(MakeFunction("Caller", {
        S::SetFlagOnTalk(0x0001, IdAction(1), JumpAction("Helper")),
        S::Branch("Helper", false)
    }));

    ASSERT_TRUE(owner.RenameFunction("Helper", "Assistant"));
    other.RenameReferences("Helper", "Assistant");

    const auto& stmt = StatementAt(other, "Caller", 0);
    const auto* set_flag = std::get_if<S::SetFlagOnTalk>(&stmt);
    ASSERT_NE(set_flag, nullptr);
    const auto* jump = std::get_if<AsmFile::ScriptJump>(&set_flag->on_set.action);
    ASSERT_NE(jump, nullptr);
    EXPECT_EQ(jump->func, "Assistant");

    const auto& branch_stmt = StatementAt(other, "Caller", 1);
    const auto* branch = std::get_if<S::Branch>(&branch_stmt);
    ASSERT_NE(branch, nullptr);
    EXPECT_EQ(branch->label, "Assistant");

    // RenameReferences must not create a definition in the non-owning table.
    EXPECT_EQ(other.GetMapping("Assistant"), nullptr);
}

TEST(ScriptFunctionTableTest, SetFunctionOrderKeepsUnlistedPositions)
{
    ScriptFunctionTable table = MakeSampleTable();
    const auto before = table.GetFunctionNames();
    ASSERT_GE(before.size(), 3u);

    // Swap the first two functions; everything else keeps its exact position.
    ASSERT_TRUE(table.SetFunctionOrder({ before[1], before[0] }));
    const auto after = table.GetFunctionNames();
    EXPECT_EQ(after[0], before[1]);
    EXPECT_EQ(after[1], before[0]);
    for (std::size_t i = 2; i < before.size(); ++i)
    {
        EXPECT_EQ(after[i], before[i]);
    }
}

TEST(MentionsLabelTest, MatchesWholeWordsOnly)
{
    EXPECT_TRUE(MentionsLabel("jsr (MyFunc).l", "MyFunc"));
    EXPECT_TRUE(MentionsLabel("MyFunc", "MyFunc"));
    EXPECT_FALSE(MentionsLabel("jsr (MyFunc2).l", "MyFunc"));
    EXPECT_FALSE(MentionsLabel("jsr (NotMyFunc).l", "MyFunc"));
    EXPECT_FALSE(MentionsLabel("", "MyFunc"));
}

// Integration: round-trip the real disassembly's four script function files.
// Set LANDSTALKER_DISASM_PATH to the root of a landstalker_disasm checkout to enable.
TEST(ScriptFunctionTableTest, DisasmRoundTrip)
{
    const char* root = std::getenv("LANDSTALKER_DISASM_PATH");
    if (root == nullptr)
    {
        GTEST_SKIP() << "LANDSTALKER_DISASM_PATH not set";
    }

    const std::vector<std::string> relative_paths = {
        "code/script/characters/script_characters.asm",
        "code/script/cutscenes/script_cutscenes.asm",
        "code/script/shops/script_shops.asm",
        "code/script/shops/script_shopspecialitems.asm",
        "code/script/en/characters/script_characters.asm",
        "code/script/en/cutscenes/script_cutscenes.asm",
        "code/script/en/shops/script_shops.asm",
        "code/script/en/shops/script_shopspecialitems.asm"
    };

    // The script ASM uses named constants; parse the project's defines the same way
    // ScriptData::AsmLoad does. (ScriptData reads the exact path from the project ASM;
    // here we just probe the known layouts.)
    std::map<std::string, std::string> defines;
    for (const auto& candidate : { "code/include/landstalker.inc", "code/landstalker.inc" })
    {
        const auto defines_file = std::filesystem::path(root) / candidate;
        if (std::filesystem::exists(defines_file))
        {
            defines = AsmFile::ParseDefines(defines_file.string());
            break;
        }
    }

    int tested = 0;
    for (const auto& rel : relative_paths)
    {
        const auto path = std::filesystem::path(root) / rel;
        if (!std::filesystem::exists(path))
        {
            continue;
        }
        std::string stage = "parse original";
        try
        {
            AsmFile in(path, defines);
            ScriptFunctionTable table(in);

            stage = "write asm";
            AsmFile out;
            ASSERT_TRUE(table.WriteAsm(out)) << rel;
            const auto tmp = std::filesystem::temp_directory_path() / "test_script_disasm.asm";
            ASSERT_TRUE(out.WriteFile(tmp)) << rel;

            stage = "re-read";
            AsmFile reread_file(tmp, defines);
            ScriptFunctionTable reread(reread_file);
            std::filesystem::remove(tmp);

            EXPECT_EQ(table, reread) << rel;
            ++tested;
        }
        catch (const std::exception& e)
        {
            ADD_FAILURE() << rel << " (" << stage << "): " << e.what();
        }
    }
    EXPECT_GT(tested, 0) << "no script ASM files found under " << root;
}
