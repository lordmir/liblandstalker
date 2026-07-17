#include <landstalker/script/ScriptFunction.h>
#include <landstalker/script/ScriptStatements.h>
#include <landstalker/misc/Literals.h>

namespace Landstalker {

ScriptFunction::ScriptFunction(const std::string& p_name, const std::vector<Statements::ScriptStatement>& p_statements)
    : name(p_name), statements(std::make_unique<Statements::ScriptStatementVector>(p_statements))
{
}

ScriptFunction::ScriptFunction(AsmFile& file)
  : statements(std::make_unique<Statements::ScriptStatementVector>())
{
    while (file.IsGood())
    {
        AsmFile::Instruction ins;
        AsmFile::ScriptAction action;
        if (file.IsLabel())
        {
            if (!name.empty())
            {
                break;
            }
            name = file.ReadLabel();
        }
        if (file.Read(ins))
        {
            if (name.empty())
            {
                throw std::runtime_error("Instruction outside function:\n" + Trim(ins.ToLine()));
            }
            if (!ProcessScriptFunction(ins, file) && !ProcessScriptTrap(ins, file) && !ProcessScriptMiscInstructions(ins))
            {
                throw std::runtime_error("Unable to process instruction:\n" + Trim(ins.ToLine()));
            }
        }
        else if (std::holds_alternative<AsmFile::ScriptId>(file.Peek()) || std::holds_alternative<AsmFile::ScriptJump>(file.Peek()))
        {
            statements->push_back(Statements::ActionTable(file));
        }
        if (std::visit([](const auto& arg) { return arg.IsEndOfFunction(); }, statements->back()))
        {
            break;
        }
    }
}

ScriptFunction::ScriptFunction(const YAML::Node& node)
  : statements(std::make_unique<Statements::ScriptStatementVector>())
{
    name = node["Function"].as<std::string>();
    for (auto statement_it = node["Statements"].begin(); statement_it != node["Statements"].end(); ++statement_it)
    {
        std::string statement_type;

        if (statement_it->IsScalar())
        {
            statement_type = statement_it->as<std::string>();
        }
        else if (statement_it->IsMap())
        {
            statement_type = statement_it->begin()->first.as<std::string>();
        }
        if (statement_type == "PlaySound")
        {
            statements->push_back(Statements::PlaySound(statement_it));
        }
        else if (statement_type == "Action")
        {
            statements->push_back(Statements::Action((*statement_it)["Action"]));
        }
        else if (statement_type == "CustomItemAction")
        {
            statements->push_back(Statements::CustomItemScript(statement_it));
        }
        else if (statement_type == "Sleep")
        {
            statements->push_back(Statements::Sleep(statement_it));
        }
        else if (statement_type == "DisplayPriceMessage")
        {
            statements->push_back(Statements::DisplayPrice(statement_it));
        }
        else if (statement_type == "Branch")
        {
            statements->push_back(Statements::Branch(statement_it));
        }
        else if (statement_type == "ActionTable")
        {
            statements->push_back(Statements::ActionTable(statement_it));
        }
        else if (statement_type == "Question")
        {
            statements->push_back(Statements::YesNoPrompt(statement_it));
        }
        else if (statement_type == "ProgressDependent")
        {
            statements->push_back(Statements::ProgressList(statement_it));
        }
        else if (statement_type == "SetFlag")
        {
            statements->push_back(Statements::SetFlagOnTalk(statement_it));
        }
        else if (statement_type == "CheckFlag")
        {
            statements->push_back(Statements::IsFlagSet(statement_it));
        }
        else if (statement_type == "Shop")
        {
            statements->push_back(Statements::ShopInteraction(statement_it));
        }
        else if (statement_type == "Church")
        {
            statements->push_back(Statements::ChurchInteraction(statement_it));
        }
        else if (statement_type == "Return")
        {
            statements->push_back(Statements::Rts());
        }
        else if (statement_type == "Asm")
        {
            statements->push_back(Statements::CustomAsm(statement_it));
        }
        else if (statement_type == "QuestProgress")
        {
            statements->push_back(Statements::ProgressFlagMapping(statement_it));
        }
        else
        {
            throw std::runtime_error(std::string("Unexpected label ") + statement_type);
        }
    }
}

ScriptFunction::ScriptFunction(const ScriptFunction& other)
    : name(other.name), statements(std::make_unique<Statements::ScriptStatementVector>(*other.statements))
{}

ScriptFunction& ScriptFunction::operator= (const ScriptFunction& rhs)
{
    if (this != &rhs)
    {
        name = rhs.name;
        statements = std::make_unique<Statements::ScriptStatementVector>(*rhs.statements);
    }
    return *this;
}

bool ScriptFunction::operator==(const ScriptFunction& rhs) const
{
    // Compare the statement CONTENTS - comparing the unique_ptrs themselves (as this used to)
    // makes any two distinct functions unequal regardless of their statements.
    if (this->name != rhs.name)
    {
        return false;
    }
    if (this->statements == rhs.statements)
    {
        return true;
    }
    if (!this->statements || !rhs.statements)
    {
        return false;
    }
    return *this->statements == *rhs.statements;
}

bool ScriptFunction::operator!=(const ScriptFunction& rhs) const
{
    return !(*this == rhs);
}

void ScriptFunction::ToAsm(AsmFile& file) const
{
    file << AsmFile::Label(name);
    for (const auto& statement : *statements)
    {
        std::visit([&file](const auto& arg)
            {
                arg.ToAsm(file);
            }, statement);
    }
}

void ScriptFunction::ToYaml(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "Function" << YAML::Value << name;
    out << YAML::Key << "Statements" << YAML::Value << YAML::BeginSeq;
    for (const auto& statement : *statements)
    {
        std::visit([&](const auto& arg)
            {
                arg.ToYaml(out);
            }, statement);
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;
}

std::string ScriptFunction::Print(int indent) const
{
    std::ostringstream ss;
    ss << std::string(indent, ' ') << name << std::endl;
    for (const auto& statement : *statements)
    {
        std::visit([&](const auto& arg)
            {
                ss << arg.Print(indent + 2);
            }, statement);
    }
    return ss.str();
}

bool ScriptFunction::ProcessScriptFunction(const AsmFile::Instruction& ins, AsmFile& file)
{
    if (!(ins.mnemonic == "bsr" && ins.operands.size() == 1 && std::holds_alternative<std::string>(ins.operands.at(0))))
    {
        return false;
    }
    std::string funcname = std::get<std::string>(ins.operands.at(0));

    if (funcname == "HandleYesNoPrompt")
    {
        statements->push_back(Statements::YesNoPrompt(file));
        return true;
    }
    else if (funcname == "HandleProgressDependentDialogue")
    {
        statements->push_back(Statements::ProgressList(file));
        return true;
    }
    else if (funcname == "SetFlagBitOnTalking")
    {
        statements->push_back(Statements::SetFlagOnTalk(file));
        return true;
    }
    else if (funcname == "CheckFlagAndDisplayMessage")
    {
        statements->push_back(Statements::IsFlagSet(file));
        return true;
    }
    else if (funcname == "Sleep_0")
    {
        statements->push_back(Statements::Sleep(file));
        return true;
    }
    else if (funcname == "DisplayItemPriceMessage")
    {
        statements->push_back(Statements::DisplayPrice(file));
        return true;
    }
    else if (funcname == "HandleShopInterraction")
    {
        statements->push_back(Statements::ShopInteraction(file));
        return true;
    }
    else if (funcname == "HandleChurchInterraction")
    {
        statements->push_back(Statements::ChurchInteraction(file));
        return true;
    }
    else if (funcname == "PickValueBasedOnFlags")
    {
        statements->push_back(Statements::ProgressFlagMapping(file));
        return true;
    }
    return false;
}

bool ScriptFunction::ProcessScriptTrap(const AsmFile::Instruction& ins, AsmFile& file)
{
    if (!(ins.mnemonic == "trap" && ins.operands.size() == 1 && std::holds_alternative<AsmFile::Immediate>(ins.operands.front())))
    {
        return false;
    }

    int trap_number = static_cast<int>(std::get<AsmFile::Immediate>(ins.operands.front()));
    switch (trap_number)
    {
    case 0:
        statements->push_back(Statements::PlaySound(file));
        return true;
    case 1:
        statements->push_back(Statements::Action(file));
        return true;
    case 2:
        statements->push_back(Statements::CustomItemScript(file));
        return true;
    default:
        return false;
    }
}

bool ScriptFunction::ProcessScriptMiscInstructions(const AsmFile::Instruction& ins)
{
    if (ins.mnemonic == "bra" && ins.operands.size() == 1 && std::holds_alternative<std::string>(ins.operands[0]))
    {
        statements->push_back(Statements::Branch(ins));
        return true;
    }
    else if (ins.mnemonic == "rts")
    {
        statements->push_back(Statements::Rts());
        return true;
    }
    else
    {
        if (!statements->empty() && std::holds_alternative<Statements::CustomAsm>(statements->back()))
        {
            std::get<Statements::CustomAsm>(statements->back()).Append(ins);
        }
        else
        {
            statements->push_back(Statements::CustomAsm(ins));
        }
        return true;
    }
}


} // namespace Landstalker
