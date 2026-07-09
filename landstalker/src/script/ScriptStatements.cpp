#include <landstalker/script/ScriptStatements.h>
#include <landstalker/script/ScriptFunction.h>
#include <landstalker/misc/Literals.h>

namespace Landstalker {
namespace Statements {

YesNoPrompt::YesNoPrompt(AsmFile& file)
{
    AsmFile::ScriptAction s_prompt, s_on_yes, s_on_no;
    file >> s_prompt >> s_on_yes >> s_on_no;
    prompt = s_prompt;
    on_yes = s_on_yes;
    on_no = s_on_no;
}

YesNoPrompt::YesNoPrompt(const YAML::Node::const_iterator& it)
    : prompt((*it)["Question"]["Prompt"]),
      on_yes((*it)["Question"]["OnYes"]),
      on_no((*it)["Question"]["OnNo"])
{
}

bool YesNoPrompt::operator==(const YesNoPrompt& rhs) const
{
    return this->prompt == rhs.prompt && this->on_yes == rhs.on_yes && this->on_no == rhs.on_no;
}

bool YesNoPrompt::operator!=(const YesNoPrompt& rhs) const
{
    return !(*this == rhs);
}

void YesNoPrompt::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("bsr", AsmFile::Width::W, { "HandleYesNoPrompt" });
    prompt.ActionToAsm(file, 0);
    on_yes.ActionToAsm(file, 1);
    on_no.ActionToAsm(file, 2);
}

void YesNoPrompt::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "Question" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Prompt" << YAML::Value;
    prompt.ActionToYaml(out);
    out << YAML::Key << "OnYes" << YAML::Value;
    on_yes.ActionToYaml(out);
    out << YAML::Key << "OnNo" << YAML::Value;
    on_no.ActionToYaml(out);
    out << YAML::EndMap << YAML::EndMap;
}

std::string YesNoPrompt::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "Yes/No Prompt:" << std::endl;
    ss << std::string(indent + 2, ' ') << "Prompt:" << std::endl;
    ss << prompt.Print(indent + 4);
    ss << std::string(indent + 2, ' ') << "On Yes:" << std::endl;
    ss << on_yes.Print(indent + 4);
    ss << std::string(indent + 2, ' ') << "On No:" << std::endl;
    ss << on_no.Print(indent + 4);
    return ss.str();
}

bool YesNoPrompt::IsEndOfFunction() const
{
    return false;
}

SetFlagOnTalk::SetFlagOnTalk(AsmFile& file)
{
    AsmFile::ScriptAction s_on_clear, s_on_set;
    file >> flag >> s_on_clear >> s_on_set;
    on_clear = s_on_clear;
    on_set = s_on_set;
}

SetFlagOnTalk::SetFlagOnTalk(const YAML::Node::const_iterator& it)
    : flag((*it)["SetFlag"]["Flag"].as<uint16_t>()),
      on_clear((*it)["SetFlag"]["OnClear"]),
      on_set((*it)["SetFlag"]["OnSet"])
{
}

bool SetFlagOnTalk::operator==(const SetFlagOnTalk& rhs) const
{
    return this->flag == rhs.flag && this->on_clear == rhs.on_clear && this->on_set == rhs.on_set;
}

bool SetFlagOnTalk::operator!=(const SetFlagOnTalk& rhs) const
{
    return !(*this == rhs);
}

void SetFlagOnTalk::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("bsr", AsmFile::Width::W, { "SetFlagBitOnTalking" }) << flag;
    on_clear.ActionToAsm(file, 1);
    on_set.ActionToAsm(file, 2);
}

void SetFlagOnTalk::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "SetFlag" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Flag" << YAML::Value << Hex(flag);
    out << YAML::Key << "OnClear" << YAML::Value;
    on_clear.ActionToYaml(out);
    out << YAML::Key << "OnSet" << YAML::Value;
    on_set.ActionToYaml(out);
    out << YAML::EndMap << YAML::EndMap;
}

std::string SetFlagOnTalk::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "Set Flag On Talk:" << std::endl;
    ss << std::string(indent + 2, ' ') << "Flag: " << Hex(flag) << std::endl;
    ss << std::string(indent + 2, ' ') << "On Clear:" << std::endl;
    ss << on_clear.Print(indent + 4);
    ss << std::string(indent + 2, ' ') << "On Set:" << std::endl;
    ss << on_set.Print(indent + 4);
    return ss.str();
}

bool SetFlagOnTalk::IsEndOfFunction() const
{
    return false;
}

IsFlagSet::IsFlagSet(AsmFile& file)
{
    AsmFile::ScriptAction s_on_clear, s_on_set;
    file >> flag >> s_on_set >> s_on_clear;
    on_clear = s_on_clear;
    on_set = s_on_set;
}

IsFlagSet::IsFlagSet(const YAML::Node::const_iterator& it)
    : flag((*it)["CheckFlag"]["Flag"].as<uint16_t>()),
      on_set((*it)["CheckFlag"]["OnSet"]),
      on_clear((*it)["CheckFlag"]["OnClear"])
{
}

bool IsFlagSet::operator==(const IsFlagSet& rhs) const
{
    return this->flag == rhs.flag && this->on_clear == rhs.on_clear && this->on_set == rhs.on_set;
}

bool IsFlagSet::operator!=(const IsFlagSet& rhs) const
{
    return !(*this == rhs);
}

void IsFlagSet::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("bsr", AsmFile::Width::W, { "CheckFlagAndDisplayMessage" }) << flag;
    on_set.ActionToAsm(file, 1);
    on_clear.ActionToAsm(file, 2);
}

void IsFlagSet::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "CheckFlag" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Flag" << YAML::Value << Hex(flag);
    out << YAML::Key << "OnClear" << YAML::Value;
    on_clear.ActionToYaml(out);
    out << YAML::Key << "OnSet" << YAML::Value;
    on_set.ActionToYaml(out);
    out << YAML::EndMap << YAML::EndMap;
}

std::string IsFlagSet::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "Check Flag:" << std::endl;
    ss << std::string(indent + 2, ' ') << "Flag: " << Hex(flag) << std::endl;
    ss << std::string(indent + 2, ' ') << "On Set:" << std::endl;
    ss << on_set.Print(indent + 4);
    ss << std::string(indent + 2, ' ') << "On Clear:" << std::endl;
    ss << on_clear.Print(indent + 4);
    return ss.str();
}

bool IsFlagSet::IsEndOfFunction() const
{
    return false;
}

PlaySound::PlaySound(AsmFile& file)
{
    file >> sound;
}

PlaySound::PlaySound(const YAML::Node::const_iterator& it)
    : sound((*it)["PlaySound"].as<uint16_t>())
{
}

bool PlaySound::operator==(const PlaySound& rhs) const
{
    return this->sound == rhs.sound;
}

bool PlaySound::operator!=(const PlaySound& rhs) const
{
    return !(*this == rhs);
}

void PlaySound::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("trap", AsmFile::Width::NONE, { "#$00" }) << sound;
}

void PlaySound::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "PlaySound" << YAML::Value << Hex(sound);
    out << YAML::EndMap;
}

std::string PlaySound::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "Play Sound: " << Hex(sound) << std::endl;
    return ss.str();
}

bool PlaySound::IsEndOfFunction() const
{
    return false;
}

CustomItemScript::CustomItemScript(AsmFile& file)
{
    file >> custom_item_script;
}

CustomItemScript::CustomItemScript(const YAML::Node::const_iterator& it)
    : custom_item_script((*it)["CustomItemAction"].as<uint16_t>())
{
}

bool CustomItemScript::operator==(const CustomItemScript& rhs) const
{
    return this->custom_item_script == rhs.custom_item_script;
}

bool CustomItemScript::operator!=(const CustomItemScript& rhs) const
{
    return !(*this == rhs);
}

void CustomItemScript::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("trap", AsmFile::Width::NONE, { "#$02" }) << custom_item_script;
}

void CustomItemScript::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "CustomItemAction" << YAML::Value << Hex(custom_item_script);
    out << YAML::EndMap;
}

std::string CustomItemScript::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "Custom Item Script: " << Hex(custom_item_script) << std::endl;
    return ss.str();
}

bool CustomItemScript::IsEndOfFunction() const
{
    return false;
}

Sleep::Sleep(AsmFile& file)
{
    file >> ticks;
}

Sleep::Sleep(const YAML::Node::const_iterator& it)
    : ticks((*it)["Sleep"].as<uint16_t>())
{
}

bool Sleep::operator==(const Sleep& rhs) const
{
    return this->ticks == rhs.ticks;
}

bool Sleep::operator!=(const Sleep& rhs) const
{
    return !(*this == rhs);
}

void Sleep::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("bsr", AsmFile::Width::W, { "Sleep_0" }) << ticks;
}

void Sleep::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "Sleep" << YAML::Value << static_cast<unsigned int>(ticks);
    out << YAML::EndMap;
}

std::string Sleep::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "Sleep: " << ticks << " frames" << std::endl;
    return ss.str();
}

bool Sleep::IsEndOfFunction() const
{
    return false;
}

DisplayPrice::DisplayPrice(AsmFile& file)
{
    // EN/JP have a single message; FR/DE follow the call with one ScriptID per grammatical
    // article form. Read every consecutive action - the list ends at the next label or
    // instruction, like ActionTable's.
    AsmFile::ScriptAction action;
    while (file.Read(action))
    {
        display_price.push_back(Action(action));
        if (file.IsLabel())
        {
            break;
        }
    }
    if (display_price.empty())
    {
        throw std::runtime_error("DisplayItemPriceMessage call is not followed by a script action");
    }
}

DisplayPrice::DisplayPrice(const YAML::Node::const_iterator& it)
{
    const YAML::Node node = (*it)["DisplayPriceMessage"];
    if (node.IsSequence())
    {
        for (const auto& elem : node)
        {
            display_price.push_back(Action(elem));
        }
    }
    else
    {
        // A single mapping (the EN/JP form, and the only shape older YAML files contain).
        display_price.push_back(Action(node));
    }
}

bool DisplayPrice::operator==(const DisplayPrice& rhs) const
{
    return this->display_price == rhs.display_price;
}

bool DisplayPrice::operator!=(const DisplayPrice& rhs) const
{
    return !(*this == rhs);
}

void DisplayPrice::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("bsr", AsmFile::Width::W, { "DisplayItemPriceMessage" });
    std::size_t counter = 0;
    for (const auto& action : display_price)
    {
        action.ActionToAsm(file, counter++);
    }
}

void DisplayPrice::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "DisplayPriceMessage" << YAML::Value;
    if (display_price.size() == 1)
    {
        // Keep the single-action shape older YAML files use.
        display_price.front().ActionToYaml(out);
    }
    else
    {
        out << YAML::BeginSeq;
        for (const auto& action : display_price)
        {
            action.ActionToYaml(out);
        }
        out << YAML::EndSeq;
    }
    out << YAML::EndMap;
}

std::string DisplayPrice::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "Display Price:" << std::endl;
    for (const auto& action : display_price)
    {
        ss << std::string(indent + 2, ' ') << "Prompt:" << std::endl;
        ss << action.Print(indent + 4);
    }
    return ss.str();
}

bool DisplayPrice::IsEndOfFunction() const
{
    return true;
}

Branch::Branch(const AsmFile::Instruction& ins)
{
    if (ins.mnemonic != "bra" || ins.operands.size() != 1 || !std::holds_alternative<std::string>(ins.operands.at(0)))
    {
        throw std::runtime_error("Branch: Unexpected instruction " + Trim(ins.ToLine()));
    }
    this->label = std::get<std::string>(ins.operands.at(0));
    this->wide = ins.width == AsmFile::Width::W;
}

Branch::Branch(const YAML::Node::const_iterator& it)
{
    // Compact form for the common case ("Branch: label"); a wide branch nests its details
    // ("Branch: {Label: x, Wide: true}").
    const YAML::Node node = (*it)["Branch"];
    if (node.IsMap())
    {
        label = node["Label"].as<std::string>();
        wide = node["Wide"].IsDefined() && node["Wide"].as<bool>();
    }
    else
    {
        label = node.as<std::string>();
        wide = false;
    }
}

bool Branch::operator==(const Branch& rhs) const
{
    return this->label == rhs.label && this->wide == rhs.wide;
}

bool Branch::operator!=(const Branch& rhs) const
{
    return !(*this == rhs);
}

void Branch::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("bra", wide ? AsmFile::Width::W : AsmFile::Width::S, { label });
}

void Branch::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "Branch" << YAML::Value;
    if (wide)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "Label" << YAML::Value << label;
        out << YAML::Key << "Wide" << YAML::Value << true;
        out << YAML::EndMap;
    }
    else
    {
        out << label;
    }
    out << YAML::EndMap;
}

std::string Branch::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "Branch: " << label << "  " << (wide ? "[Wide]" : "[Short]") << std::endl;
    return ss.str();
}

bool Branch::IsEndOfFunction() const
{
    return true;
}

ShopInteraction::ShopInteraction(AsmFile& file)
{
    AsmFile::ScriptAction s_on_sale_prompt, s_on_sale_confirm, s_on_no_money, s_on_sale_decline;
    file >> s_on_sale_prompt >> s_on_sale_confirm >> s_on_no_money >> s_on_sale_decline;
    on_sale_prompt = s_on_sale_prompt;
    on_sale_confirm = s_on_sale_confirm;
    on_no_money = s_on_no_money;
    on_sale_decline = s_on_sale_decline;
}

ShopInteraction::ShopInteraction(const YAML::Node::const_iterator& it)
    : on_sale_prompt((*it)["Shop"]["OnSalePrompt"]),
      on_sale_confirm((*it)["Shop"]["OnSaleConfirm"]),
      on_no_money((*it)["Shop"]["OnNoMoney"]),
      on_sale_decline((*it)["Shop"]["OnSaleDecline"])
{
}

bool ShopInteraction::operator==(const ShopInteraction& rhs) const
{
    return this->on_sale_prompt == rhs.on_sale_prompt
        && this->on_sale_confirm == rhs.on_sale_confirm
        && this->on_sale_decline == rhs.on_sale_decline
        && this->on_no_money == rhs.on_no_money;
}

bool ShopInteraction::operator!=(const ShopInteraction& rhs) const
{
    return !(*this == rhs);
}

void ShopInteraction::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("bsr", AsmFile::Width::W, { "HandleShopInterraction" });
    on_sale_prompt.ActionToAsm(file, 0);
    on_sale_confirm.ActionToAsm(file, 1);
    on_no_money.ActionToAsm(file, 2);
    on_sale_decline.ActionToAsm(file, 3);
}

void ShopInteraction::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "Shop" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "OnSalePrompt" << YAML::Value;
    on_sale_prompt.ActionToYaml(out);
    out << YAML::Key << "OnSaleConfirm" << YAML::Value;
    on_sale_confirm.ActionToYaml(out);
    out << YAML::Key << "OnNoMoney" << YAML::Value;
    on_no_money.ActionToYaml(out);
    out << YAML::Key << "OnSaleDecline" << YAML::Value;
    on_sale_decline.ActionToYaml(out);
    out << YAML::EndMap << YAML::EndMap;
}

std::string ShopInteraction::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "Shop:" << std::endl;
    ss << std::string(indent + 2, ' ') << "On Sale Prompt:" << std::endl;
    ss << on_sale_prompt.Print(indent + 4);
    ss << std::string(indent + 2, ' ') << "On Sale Confirm:" << std::endl;
    ss << on_sale_confirm.Print(indent + 4);
    ss << std::string(indent + 2, ' ') << "On Sale Decline:" << std::endl;
    ss << on_sale_decline.Print(indent + 4);
    ss << std::string(indent + 2, ' ') << "On Not Enough Money:" << std::endl;
    ss << on_no_money.Print(indent + 4);
    return ss.str();
}

bool ShopInteraction::IsEndOfFunction() const
{
    return true;
}

ChurchInteraction::ChurchInteraction(AsmFile& file)
{
    AsmFile::ScriptAction s_script_normal_priest, s_script_skeleton_priest;
    file >> s_script_normal_priest >> s_script_skeleton_priest;
    script_normal_priest = s_script_normal_priest;
    script_skeleton_priest = s_script_skeleton_priest;
}

ChurchInteraction::ChurchInteraction(const YAML::Node::const_iterator& it)
    : script_normal_priest((*it)["Church"]["NormalPriest"]),
      script_skeleton_priest((*it)["Church"]["SkeletonPriest"])
{
}

bool ChurchInteraction::operator==(const ChurchInteraction& rhs) const
{
    return this->script_normal_priest == rhs.script_normal_priest
        && this->script_skeleton_priest == rhs.script_skeleton_priest;
}

bool ChurchInteraction::operator!=(const ChurchInteraction& rhs) const
{
    return !(*this == rhs);
}

void ChurchInteraction::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("bsr", AsmFile::Width::S, { "HandleChurchInterraction" });
    script_normal_priest.ActionToAsm(file, 0);
    script_skeleton_priest.ActionToAsm(file, 1);
}

void ChurchInteraction::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "Church" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "NormalPriest" << YAML::Value;
    script_normal_priest.ActionToYaml(out);
    out << YAML::Key << "SkeletonPriest" << YAML::Value;
    script_skeleton_priest.ActionToYaml(out);
    out << YAML::EndMap << YAML::EndMap;
}

std::string ChurchInteraction::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "Church:" << std::endl;
    ss << std::string(indent + 2, ' ') << "Normal Priest:" << std::endl;
    ss << script_normal_priest.Print(indent + 4);
    ss << std::string(indent + 2, ' ') << "Skeleton Priest:" << std::endl;
    ss << script_skeleton_priest.Print(indent + 4);
    return ss.str();
}

bool ChurchInteraction::IsEndOfFunction() const
{
    return false;
}

Rts::Rts()
{
}

Rts::Rts(const YAML::Node::const_iterator& /*it*/)
{
}

bool Rts::operator==(const Rts& /*rhs*/) const
{
    return true;
}

bool Rts::operator!=(const Rts& /*rhs*/) const
{
    return false;
}

void Rts::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("rts");
}

void Rts::ToYaml(YAML::Emitter& out) const
{
    // The one statement with no payload - a bare scalar item, not a mapping.
    out << "Return";
}

std::string Rts::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "Return" << std::endl;
    return ss.str();
}

bool Rts::IsEndOfFunction() const
{
    return true;
}

CustomAsm::CustomAsm(const AsmFile::Instruction& ins)
{
    Append(ins);
}

CustomAsm::CustomAsm(const YAML::Node::const_iterator& it)
{
    std::stringstream ss((*it)["Asm"].as<std::string>());
    std::string line;
    while (std::getline(ss, line))
    {
        line = Trim(line);
        if (line.empty())
        {
            continue;
        }
        instructions.push_back(AsmFile::Instruction::FromLine("\t" + line));
    }
}

CustomAsm::CustomAsm(const std::string& inst)
{
    std::stringstream ss(inst);
    std::string line;
    while (std::getline(ss, line))
    {
        if (!Trim(line).empty())
        {
            instructions.push_back(AsmFile::Instruction::FromLine(line));
        }
    }
}

CustomAsm::CustomAsm(const std::vector<AsmFile::Instruction>& inst)
    : instructions(inst)
{
}

bool CustomAsm::operator==(const CustomAsm& rhs) const
{
    return this->instructions == rhs.instructions;
}

bool CustomAsm::operator!=(const CustomAsm& rhs) const
{
    return !(*this == rhs);
}

void CustomAsm::Append(const AsmFile::Instruction& ins)
{
    instructions.push_back(ins);
}

void CustomAsm::ToAsm(AsmFile& file) const
{
    for (const auto& ins : instructions)
    {
        file << ins;
    }
}

void CustomAsm::ToYaml(YAML::Emitter& out) const
{
    std::string block;
    for (const auto& line : instructions)
    {
        if (!block.empty())
        {
            block += "\n";
        }
        block += Trim(line.ToLine());
    }
    out << YAML::BeginMap;
    out << YAML::Key << "Asm" << YAML::Value << YAML::Literal << block;
    out << YAML::EndMap;
}

std::string CustomAsm::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "Custom Assembler:" << std::endl;
    for (const auto& line : instructions)
    {
        ss << std::string(indent + 2, ' ') << Trim(line.ToLine()) << std::endl;
    }
    return ss.str();
}

bool CustomAsm::IsEndOfFunction() const
{
    return false;
}

ProgressList::ProgressList(AsmFile& file)
{
    AsmFile::ScriptAction action;
    uint8_t quest = 0;
    uint8_t progress_count = 0;
    while (true)
    {
        file >> quest >> progress_count;
        if (quest == 0xFF)
        {
            break;
        }
        file >> action;
        progress.emplace_back(QuestProgress(quest, progress_count), action);
    }
}

ProgressList::ProgressList(const YAML::Node::const_iterator& it)
{
    for (const auto& elem : (*it)["ProgressDependent"])
    {
        uint8_t quest = elem["Quest"].as<uint8_t>();
        uint8_t prog = elem["Progress"].as<uint8_t>();
        auto action = Action(elem["Action"]);
        progress.emplace_back(QuestProgress(quest, prog), action);
    }
}

bool ProgressList::operator==(const ProgressList& rhs) const
{
    return this->progress == rhs.progress;
}

bool ProgressList::operator!=(const ProgressList& rhs) const
{
    return !(*this == rhs);
}

void ProgressList::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("bsr", AsmFile::Width::W, { "HandleProgressDependentDialogue" });
    std::size_t offset = 1;
    for (const auto& p : progress)
    {
        file << std::vector<uint8_t>{p.first.quest, p.first.progress};
        p.second.ActionToAsm(file, offset);
        offset += 2;
    }
    file << 0xFFFF_u16;
}

void ProgressList::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "ProgressDependent" << YAML::Value << YAML::BeginSeq;
    for (const auto& line : progress)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "Quest" << YAML::Value << static_cast<int>(line.first.quest);
        out << YAML::Key << "Progress" << YAML::Value << static_cast<int>(line.first.progress);
        out << YAML::Key << "Action" << YAML::Value;
        line.second.ActionToYaml(out);
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;
}

std::string ProgressList::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "ProgressList:" << std::endl;
    for (const auto& prog : progress)
    {
        ss << std::string(indent + 2, ' ') << "Quest: " << static_cast<int>(prog.first.quest)
           << ", Progress: " << static_cast<int>(prog.first.progress) << ":" << std::endl;
        ss << prog.second.Print(indent + 4);
    }
    return ss.str();
}

bool ProgressList::IsEndOfFunction() const
{
    return true;
}

ProgressFlagMapping::ProgressFlagMapping(AsmFile& file)
{
    uint16_t flag;
    uint8_t dummy, progress;
    while (true)
    {
        file >> flag;
        if (flag == 0xFFFF)
        {
            break;
        }
        file >> progress >> dummy;
        this->mapping[progress] = flag;
    }
}

ProgressFlagMapping::ProgressFlagMapping(const YAML::Node::const_iterator& it)
{
    for (const auto& elem : (*it)["QuestProgress"])
    {
        uint16_t flag = elem["OnFlagSet"].as<uint16_t>();
        uint8_t progress = elem["SetProgress"].as<uint8_t>();
        mapping.insert({ progress, flag });
    }
}

bool ProgressFlagMapping::operator==(const ProgressFlagMapping& rhs) const
{
    return this->mapping == rhs.mapping;
}

bool ProgressFlagMapping::operator!=(const ProgressFlagMapping& rhs) const
{
    return !(*this == rhs);
}

void ProgressFlagMapping::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("bsr", AsmFile::Width::W, { "PickValueBasedOnFlags" });
    for (const auto& p : mapping)
    {
        file << std::vector<uint16_t>{p.second, static_cast<uint16_t>(p.first << 8)};
    }
    file << 0xFFFF_u16;
}

void ProgressFlagMapping::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "QuestProgress" << YAML::Value << YAML::BeginSeq;
    for (const auto& line : mapping)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "OnFlagSet" << YAML::Value << Hex(line.second);
        out << YAML::Key << "SetProgress" << YAML::Value << static_cast<int>(line.first);
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;
}

std::string ProgressFlagMapping::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "Progress Flag Mapping:" << std::endl;
    for (const auto& flag : mapping)
    {
        ss << std::string(indent + 2, ' ') << "Progress: " << static_cast<int>(flag.first)
           << " -> Flag: " << Hex(flag.second) << std::endl;
    }
    return ss.str();
}

bool ProgressFlagMapping::IsEndOfFunction() const
{
    return false;
}

ActionTable::ActionTable(AsmFile& file)
{
    AsmFile::ScriptAction action;
    while (file.Read(action))
    {
        actions.push_back(action);
        if (file.IsLabel())
        {
            break;
        }
    }
}

ActionTable::ActionTable(const YAML::Node::const_iterator& it)
{
    for (const auto& elem : (*it)["ActionTable"])
    {
        actions.push_back(Action(elem));
    }
}

bool ActionTable::operator==(const ActionTable& rhs) const
{
    return this->actions == rhs.actions;
}

bool ActionTable::operator!=(const ActionTable& rhs) const
{
    return !(*this == rhs);
}

void ActionTable::ToAsm(AsmFile& file) const
{
    std::size_t counter = 0;
    for (const auto& action : actions)
    {
        action.ActionToAsm(file, counter++);
    }
}

void ActionTable::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "ActionTable" << YAML::Value << YAML::BeginSeq;
    for (const auto& action : actions)
    {
        action.ActionToYaml(out);
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;
}

std::string ActionTable::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << "ActionTable:" << std::endl;
    for (const auto& action : actions)
    {
        ss << action.Print(indent + 2);
    }
    return ss.str();
}

bool ActionTable::IsEndOfFunction() const
{
    return true;
}

Action::Action(const ScriptFunction& func)
    : action(std::make_shared<ScriptFunction>(func))
{
}

Action::Action(const AsmFile::ScriptAction& scriptaction)
{
    std::visit([&](const auto& arg)
        {
            action = arg;
        }, *scriptaction);
    offset = static_cast<std::size_t>(std::visit([](const auto& arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, AsmFile::ScriptId>)
            {
                return arg.offset;
            }
            else if constexpr (std::is_same_v<T, AsmFile::ScriptJump>)
            {
                return arg.offset;
            }
        }, *scriptaction));
}

Action::Action(AsmFile& file)
{
    AsmFile::ScriptAction s_action;
    file >> s_action;
    std::visit([&](const auto& arg)
        {
            action = arg;
        }, *s_action);
}

Action::Action(const YAML::Node& node)
{
    if (!node.IsDefined() || node.IsNull() || !node.IsMap())
    {
        throw std::runtime_error("Missing or malformed script action");
    }
    std::string type = node.begin()->first.as<std::string>();
    if (type == "ScriptID")
    {
        action = AsmFile::ScriptId(node.begin()->second.as<unsigned int>());
    }
    else if (type == "Jump")
    {
        action = AsmFile::ScriptJump(node.begin()->second.as<std::string>());
    }
    else if (type == "Function")
    {
        action = std::make_shared<ScriptFunction>(node);
    }
    else
    {
        throw std::runtime_error("Bad Script Action Type: " + type);
    }
}

bool Action::operator==(const Action& rhs) const
{
    // The nested-function alternative holds a shared_ptr - compare the pointed-to functions,
    // not the pointer identities (a straight variant compare would report two structurally
    // identical inline functions as different). Action::offset is deliberately not compared:
    // like ScriptId/ScriptJump offsets, it's ASM layout metadata, not part of the meaning.
    if (this->action.index() != rhs.action.index())
    {
        return false;
    }
    if (std::holds_alternative<std::shared_ptr<ScriptFunction>>(this->action))
    {
        const auto& lhs_func = std::get<std::shared_ptr<ScriptFunction>>(this->action);
        const auto& rhs_func = std::get<std::shared_ptr<ScriptFunction>>(rhs.action);
        if (lhs_func == rhs_func)
        {
            return true;
        }
        if (!lhs_func || !rhs_func)
        {
            return false;
        }
        return *lhs_func == *rhs_func;
    }
    return this->action == rhs.action;
}

bool Action::operator!=(const Action& rhs) const
{
    return !(*this == rhs);
}

Action::operator AsmFile::ScriptAction() const
{
    return std::visit([](const auto& arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>)
            {
                return AsmFile::ScriptAction();
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ScriptFunction>>)
            {
                return AsmFile::ScriptAction();
            }
            else if constexpr (std::is_same_v<T, AsmFile::ScriptId>)
            {
                return AsmFile::ScriptAction(arg);
            }
            else if constexpr (std::is_same_v<T, AsmFile::ScriptJump>)
            {
                return AsmFile::ScriptAction(arg);
            }
        }, action);
}

void Action::ToAsm(AsmFile& file) const
{
    file << AsmFile::Instruction("trap", AsmFile::Width::NONE, { "#$01" });
    ActionToAsm(file, 0);
}

void Action::ActionToAsm(AsmFile& file, int p_offset) const
{
    return std::visit([&](const auto& arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>)
            {
                throw std::runtime_error("Invalid Action");
            }
            else if constexpr (std::is_same_v<T, std::pair<std::string, ScriptFunction>>)
            {
                file << AsmFile::ScriptAction();
            }
            else if constexpr (std::is_same_v<T, AsmFile::ScriptId>)
            {
                file << AsmFile::ScriptAction(AsmFile::ScriptId(arg.script_id, p_offset));
            }
            else if constexpr (std::is_same_v<T, AsmFile::ScriptJump>)
            {
                file << AsmFile::ScriptAction(AsmFile::ScriptJump(arg.func, p_offset));
            }
        }, action);
}

void Action::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap << YAML::Key << "Action" << YAML::Value;
    ActionToYaml(out);
    out << YAML::EndMap;
}

void Action::ActionToYaml(YAML::Emitter& out) const
{
    std::visit([&](const auto& arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>)
            {
                // The old string writer silently emitted nothing here, which produced a null
                // node the parser then crashed on - fail at the writing end instead, like
                // ActionToAsm() does.
                throw std::runtime_error("Cannot serialize an invalid (empty) script action");
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ScriptFunction>>)
            {
                if (!arg)
                {
                    throw std::runtime_error("Cannot serialize a null inline function");
                }
                arg->ToYaml(out);
            }
            else if constexpr (std::is_same_v<T, AsmFile::ScriptId>)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "ScriptID" << YAML::Value << static_cast<unsigned int>(arg.script_id);
                out << YAML::EndMap;
            }
            else if constexpr (std::is_same_v<T, AsmFile::ScriptJump>)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "Jump" << YAML::Value << arg.func;
                out << YAML::EndMap;
            }
        }, action);
}

std::string Action::Print(int indent) const
{
    std::ostringstream ss;
    std::visit([&](const auto& arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>)
            {
                ss << std::string(indent, ' ') << "Unknown Action" << std::endl;
            }
            else if constexpr (std::is_same_v<T, AsmFile::ScriptId>)
            {
                ss << std::string(indent, ' ') << StrPrintf("Load Script ID %d", arg.script_id) << std::endl;
            }
            else if constexpr (std::is_same_v<T, AsmFile::ScriptJump>)
            {
                ss << std::string(indent, ' ') << "Jump to function " << arg.func << std::endl;
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ScriptFunction>>)
            {
                if (arg)
                {
                    ss << arg->Print(indent);
                }
            }
        }, action);
    return ss.str();
}

bool Action::IsEndOfFunction() const
{
    return false;
}

} // namespace Statements

} // namespace Landstalker
