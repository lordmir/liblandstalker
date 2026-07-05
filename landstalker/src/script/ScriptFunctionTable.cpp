#include <landstalker/script/ScriptFunctionTable.h>
#include <landstalker/misc/Literals.h>

#include <algorithm>

namespace Landstalker {

namespace {

bool IsLabelNameChar(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

} // namespace

bool MentionsLabel(const std::string& text, const std::string& label)
{
    std::size_t pos = text.find(label);
    while (pos != std::string::npos)
    {
        const bool before_ok = pos == 0 || !IsLabelNameChar(text.at(pos - 1));
        const std::size_t after = pos + label.size();
        const bool after_ok = after >= text.size() || !IsLabelNameChar(text.at(after));
        if (before_ok && after_ok)
        {
            return true;
        }
        pos = text.find(label, pos + 1);
    }
    return false;
}

ScriptFunctionTable::ScriptFunctionTable(AsmFile& file)
{
    ReadAsm(file);
}

ScriptFunctionTable::ScriptFunctionTable(AsmFile&& file)
{
    ReadAsm(file);
}

ScriptFunctionTable::ScriptFunctionTable(const std::string& yaml)
{
    YAML::Node node = YAML::Load(yaml);
    const auto funclist = node.begin()->second;
    for (auto it = funclist.begin(); it != funclist.end(); ++it)
    {
        ScriptFunction func(*it);
        if (function_mapping.find(func.name) != function_mapping.cend())
        {
            throw std::runtime_error("Duplicate Label: " + func.name);
        }
        function_mapping.insert({ func.name, func });
        funcnames.push_back(func.name);
    }
    // Nesting is purely a YAML-format concern: the in-memory table is always kept flat (one
    // standalone entry per function, references stored as Jumps), so hoist any nested inline
    // definitions back out immediately on parse. The inverse (Consolidate()) happens only when
    // generating YAML - see ToYaml().
    Unconsolidate();
}

bool ScriptFunctionTable::operator==(const ScriptFunctionTable& rhs) const
{
    return this->function_mapping == rhs.function_mapping && this->funcnames == rhs.funcnames;
}

bool ScriptFunctionTable::operator!=(const ScriptFunctionTable& rhs) const
{
    return !(*this == rhs);
}

bool ScriptFunctionTable::ReadAsm(AsmFile& file)
{
    while (file.IsGood())
    {
        ScriptFunction func(file);
        if (function_mapping.find(func.name) != function_mapping.cend())
        {
            throw std::runtime_error("Duplicate Label: " + func.name);
        }
        function_mapping.insert({ func.name, func });
        funcnames.push_back(func.name);
    }
    return true;
}

bool ScriptFunctionTable::WriteAsm(AsmFile& file)
{
    Unconsolidate();
    for (const auto& funcname : funcnames)
    {
        function_mapping.at(funcname).ToAsm(file);
    }
    return true;
}

std::string ScriptFunctionTable::Print() const
{
    std::ostringstream ss;
    ss << "Function Mapping:" << std::endl;
    for (const auto& funcname : funcnames)
    {
        ss << function_mapping.at(funcname).Print(2);
    }
    return ss.str();
}

std::string ScriptFunctionTable::ToYaml(const std::string& name) const
{
    // Nesting is purely a YAML-format concern: the in-memory table is always kept flat, and
    // single-use functions are nested inline (Consolidate()) only here, on a copy, when
    // generating the YAML output. The YAML parse constructor applies the inverse
    // (Unconsolidate()) so a round trip always lands back on the flat internal form.
    ScriptFunctionTable nested(*this);
    nested.Consolidate();
    // Built with yaml-cpp's emitter, so quoting is handled properly - in particular a name
    // like "Shops : Custom Items" becomes a quoted key instead of unparseable output.
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << name << YAML::Value << YAML::BeginSeq;
    for (const auto& funcname : nested.funcnames)
    {
        nested.function_mapping.at(funcname).ToYaml(out);
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;
    return std::string(out.c_str()) + "\n";
}

const std::vector<std::string>& ScriptFunctionTable::GetFunctionNames() const
{
    return funcnames;
}

const ScriptFunction* ScriptFunctionTable::GetMapping(const std::string& funcname) const
{
    if (function_mapping.find(funcname) == function_mapping.cend())
    {
        return nullptr;
    }
    return &function_mapping.find(funcname)->second;
}

ScriptFunction* ScriptFunctionTable::GetMapping(const std::string& funcname)
{
    if (function_mapping.find(funcname) == function_mapping.cend())
    {
        return nullptr;
    }
    return &function_mapping.find(funcname)->second;
}

ScriptFunction* ScriptFunctionTable::GetMapping(std::size_t index)
{
    if (index >= funcnames.size())
    {
        return nullptr;
    }
    return &function_mapping.find(funcnames.at(index))->second;
}

bool ScriptFunctionTable::AddFunction(ScriptFunction&& func)
{
    if (function_mapping.find(func.name) != function_mapping.cend())
    {
        return false;
    }
    funcnames.push_back(func.name);
    function_mapping.emplace(func.name, func);
    return true;
}

bool ScriptFunctionTable::InsertFunctionAfter(ScriptFunction&& func, const std::string& after)
{
    if (function_mapping.find(func.name) != function_mapping.cend())
    {
        return false;
    }
    auto pos = funcnames.end();
    if (!after.empty())
    {
        const auto it = std::find(funcnames.begin(), funcnames.end(), after);
        if (it != funcnames.end())
        {
            pos = std::next(it);
        }
    }
    funcnames.insert(pos, func.name);
    function_mapping.emplace(func.name, func);
    return true;
}


bool ScriptFunctionTable::SetFunctionOrder(const std::vector<std::string>& order)
{
    // Reorder the listed functions among the positions they already occupy, leaving every
    // unlisted function exactly where it is. The list typically covers only the functions one
    // part of the UI happens to display, while table order is semantically significant for all
    // of them (consecutive functions form fall-through pairs), so the rest must not be
    // disturbed as a side effect.
    std::vector<std::string> listed;
    std::set<std::string> seen;
    for (const auto& name : order)
    {
        if (function_mapping.find(name) != function_mapping.end() && seen.insert(name).second)
        {
            listed.push_back(name);
        }
    }

    std::vector<std::size_t> positions;
    for (std::size_t i = 0; i < funcnames.size(); ++i)
    {
        if (seen.find(funcnames.at(i)) != seen.end())
        {
            positions.push_back(i);
        }
    }
    if (positions.size() != listed.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < listed.size(); ++i)
    {
        funcnames.at(positions.at(i)) = listed.at(i);
    }
    return true;
}
bool ScriptFunctionTable::RenameFunction(const std::string& old_name, const std::string& new_name)
{
    if (new_name == old_name)
    {
        return true;
    }
    auto mapping_it = function_mapping.find(old_name);
    if (mapping_it == function_mapping.end() || function_mapping.find(new_name) != function_mapping.end())
    {
        return false;
    }

    ScriptFunction renamed(mapping_it->second);
    renamed.name = new_name;
    function_mapping.erase(mapping_it);
    function_mapping.emplace(new_name, renamed);
    std::replace(funcnames.begin(), funcnames.end(), old_name, new_name);

    RenameReferences(old_name, new_name);
    return true;
}

void ScriptFunctionTable::RenameReferences(const std::string& old_name, const std::string& new_name)
{
    auto Update = [&](Statements::Action& action)
    {
        if (std::holds_alternative<AsmFile::ScriptJump>(action.action))
        {
            auto& jump = std::get<AsmFile::ScriptJump>(action.action);
            if (jump.func == old_name)
            {
                jump.func = new_name;
            }
        }
        else if (std::holds_alternative<std::shared_ptr<ScriptFunction>>(action.action))
        {
            // Shouldn't occur in the (always flat) in-memory form, but keep any nested inline
            // definition consistent too.
            auto& nested = std::get<std::shared_ptr<ScriptFunction>>(action.action);
            if (nested && nested->name == old_name)
            {
                nested->name = new_name;
            }
        }
    };
    for (auto& func : function_mapping)
    {
        for (auto& statement : *func.second.statements)
        {
            std::visit([&](auto& arg)
                {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, Statements::Action>)
                    {
                        Update(arg);
                    }
                    else if constexpr (std::is_same_v<T, Statements::ActionTable>)
                    {
                        for (auto& action : arg.actions)
                        {
                            Update(action);
                        }
                    }
                    else if constexpr (std::is_same_v<T, Statements::ProgressList>)
                    {
                        for (auto& flagaction : arg.progress)
                        {
                            Update(flagaction.second);
                        }
                    }
                    else if constexpr (std::is_same_v<T, Statements::YesNoPrompt>)
                    {
                        Update(arg.prompt);
                        Update(arg.on_yes);
                        Update(arg.on_no);
                    }
                    else if constexpr (std::is_same_v<T, Statements::SetFlagOnTalk>)
                    {
                        Update(arg.on_set);
                        Update(arg.on_clear);
                    }
                    else if constexpr (std::is_same_v<T, Statements::IsFlagSet>)
                    {
                        Update(arg.on_set);
                        Update(arg.on_clear);
                    }
                    else if constexpr (std::is_same_v<T, Statements::DisplayPrice>)
                    {
                        Update(arg.display_price);
                    }
                    else if constexpr (std::is_same_v<T, Statements::ShopInteraction>)
                    {
                        Update(arg.on_sale_prompt);
                        Update(arg.on_sale_confirm);
                        Update(arg.on_no_money);
                        Update(arg.on_sale_decline);
                    }
                    else if constexpr (std::is_same_v<T, Statements::ChurchInteraction>)
                    {
                        Update(arg.script_normal_priest);
                        Update(arg.script_skeleton_priest);
                    }
                    else if constexpr (std::is_same_v<T, Statements::Branch>)
                    {
                        if (arg.label == old_name)
                        {
                            arg.label = new_name;
                        }
                    }
                }, statement);
        }
    }
}

bool ScriptFunctionTable::RemoveFunction(const std::string& funcname)
{
    auto mapping_it = function_mapping.find(funcname);
    if (mapping_it == function_mapping.end())
    {
        return false;
    }

    function_mapping.erase(mapping_it);
    funcnames.erase(std::remove(funcnames.begin(), funcnames.end(), funcname), funcnames.end());
    return true;
}
void ScriptFunctionTable::Consolidate()
{
    std::map<std::string, int> func_call_counts;
    std::set<std::string> non_relocatable_funcs;
    auto Increment = [&func_call_counts](const Statements::Action& action)
    {
        if (std::holds_alternative<AsmFile::ScriptJump>(action.action))
        {
            func_call_counts[std::get<AsmFile::ScriptJump>(action.action).func]++;
        }
    };
    auto MarkNonRelocatable = [&](const std::string& funcname)
    {
        non_relocatable_funcs.insert(funcname);
        // We also can't relocate the function that is fallen into
        auto next = std::find(funcnames.cbegin(), funcnames.cend(), funcname);
        next = next != funcnames.cend() ? std::next(next) : next;
        if (next != funcnames.cend())
        {
            non_relocatable_funcs.insert(*next);
        }
    };
    for (const auto& func : function_mapping)
    {
        for (const auto& statement : *func.second.statements)
        {
            std::visit([&](const auto& arg)
                {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, Statements::Action>)
                    {
                        Increment(arg);
                    }
                    else if constexpr (std::is_same_v<T, Statements::ActionTable>)
                    {
                        for (const auto& action : arg.actions)
                        {
                            Increment(action);
                        }
                    }
                    else if constexpr (std::is_same_v<T, Statements::ProgressList>)
                    {
                        for (const auto& flagaction : arg.progress)
                        {
                            Increment(flagaction.second);
                        }
                    }
                    else if constexpr (std::is_same_v<T, Statements::YesNoPrompt>)
                    {
                        Increment(arg.prompt);
                        Increment(arg.on_yes);
                        Increment(arg.on_no);
                    }
                    else if constexpr (std::is_same_v<T, Statements::SetFlagOnTalk>)
                    {
                        Increment(arg.on_set);
                        Increment(arg.on_clear);
                    }
                    else if constexpr (std::is_same_v<T, Statements::IsFlagSet>)
                    {
                        Increment(arg.on_set);
                        Increment(arg.on_clear);
                    }
                    else if constexpr (std::is_same_v<T, Statements::DisplayPrice>)
                    {
                        Increment(arg.display_price);
                    }
                    else if constexpr (std::is_same_v<T, Statements::ShopInteraction>)
                    {
                        Increment(arg.on_sale_prompt);
                        Increment(arg.on_sale_confirm);
                        Increment(arg.on_no_money);
                        Increment(arg.on_sale_decline);
                    }
                    else if constexpr (std::is_same_v<T, Statements::ChurchInteraction>)
                    {
                        Increment(arg.script_normal_priest);
                        Increment(arg.script_skeleton_priest);
                    }
                    else if constexpr (std::is_same_v<T, Statements::Branch>)
                    {
                        // A branch target needs its standalone label in the output ASM - it
                        // can never be nested inline, even if only jumped to once.
                        non_relocatable_funcs.insert(arg.label);
                    }
                    else if constexpr (std::is_same_v<T, Statements::CustomAsm>)
                    {
                        // Raw ASM can reference function labels textually - keep any function
                        // it mentions standalone.
                        for (const auto& ins : arg.instructions)
                        {
                            const std::string line = ins.ToLine();
                            for (const auto& funcname : funcnames)
                            {
                                if (MentionsLabel(line, funcname))
                                {
                                    non_relocatable_funcs.insert(funcname);
                                }
                            }
                        }
                    }
                }, statement);
        }
    }
    for (const auto& func : function_mapping)
    {
        if (func.second.statements->size() == 0)
        {
            continue;
        }
        const auto& last_statement = func.second.statements->back();
        std::visit([&](const auto& arg)
            {
                if (!arg.IsEndOfFunction())
                {
                    MarkNonRelocatable(func.first);
                }
            }, last_statement);
    }
    std::vector<std::pair<std::string, Statements::Action&>> action_list;
    // The function currently being scanned by the do-loop below - a function must never be
    // nested into its own body (a recursive single-use function would otherwise have its only
    // definition copied inside itself and then erased, vanishing from the output entirely).
    std::string container;
    auto Replace = [&](Statements::Action& action)
    {
        if (std::holds_alternative<AsmFile::ScriptJump>(action.action))
        {
            std::string func_name = std::get<AsmFile::ScriptJump>(action.action).func;
            if (func_name != container
                && func_call_counts.find(func_name) != func_call_counts.cend() && func_call_counts.at(func_name) == 1
                && function_mapping.find(func_name) != function_mapping.cend()
                && non_relocatable_funcs.find(func_name) == non_relocatable_funcs.cend())
            {
                action.action = std::make_shared<ScriptFunction>(function_mapping.at(func_name));
                action_list.push_back({ func_name, action });
            }
        }
    };
    do
    {
        action_list.clear();
        for (auto& func : function_mapping)
        {
            container = func.first;
            for (auto& statement : *func.second.statements)
            {
                std::visit([&](auto& arg)
                    {
                        using T = std::decay_t<decltype(arg)>;
                        if constexpr (std::is_same_v<T, Statements::Action>)
                        {
                            Replace(arg);
                        }
                        else if constexpr (std::is_same_v<T, Statements::ActionTable>)
                        {
                            for (auto& action : arg.actions)
                            {
                                Replace(action);
                            }
                        }
                        else if constexpr (std::is_same_v<T, Statements::ProgressList>)
                        {
                            for (auto& flagaction : arg.progress)
                            {
                                Replace(flagaction.second);
                            }
                        }
                        else if constexpr (std::is_same_v<T, Statements::YesNoPrompt>)
                        {
                            Replace(arg.prompt);
                            Replace(arg.on_yes);
                            Replace(arg.on_no);
                        }
                        else if constexpr (std::is_same_v<T, Statements::SetFlagOnTalk>)
                        {
                            Replace(arg.on_set);
                            Replace(arg.on_clear);
                        }
                        else if constexpr (std::is_same_v<T, Statements::IsFlagSet>)
                        {
                            Replace(arg.on_set);
                            Replace(arg.on_clear);
                        }
                        else if constexpr (std::is_same_v<T, Statements::DisplayPrice>)
                        {
                            Replace(arg.display_price);
                        }
                        else if constexpr (std::is_same_v<T, Statements::ShopInteraction>)
                        {
                            Replace(arg.on_sale_prompt);
                            Replace(arg.on_sale_confirm);
                            Replace(arg.on_no_money);
                            Replace(arg.on_sale_decline);
                        }
                        else if constexpr (std::is_same_v<T, Statements::ChurchInteraction>)
                        {
                            Replace(arg.script_normal_priest);
                            Replace(arg.script_skeleton_priest);
                        }
                    }, statement);
            }
        }
        for (const auto& action : action_list)
        {
            action.second.action = std::make_shared<ScriptFunction>(action.first, *function_mapping.at(action.first).statements);
        }
        for (const auto& action : action_list)
        {
            function_mapping.erase(action.first);
            funcnames.erase(std::find(funcnames.cbegin(), funcnames.cend(), action.first));
        }
    } while (!action_list.empty());
}

void ScriptFunctionTable::Unconsolidate()
{
    std::map<std::string, ScriptFunction> insert_list;
    std::vector<std::string> new_funcnames;
    auto Insert = [&](Statements::Action& action)
    {
        if (std::holds_alternative<std::shared_ptr<ScriptFunction>>(action.action))
        {
            ScriptFunction consolidated_func = *std::get<std::shared_ptr<ScriptFunction>>(action.action);
            std::string func_name = consolidated_func.name;
            action.action = AsmFile::ScriptJump(func_name, action.offset);
            if (std::find(new_funcnames.cbegin(), new_funcnames.cend(), func_name) == new_funcnames.cend() &&
                std::find(funcnames.cbegin(), funcnames.cend(), func_name) == funcnames.cend())
            {
                new_funcnames.push_back(func_name);
                insert_list.insert({ func_name, consolidated_func });
            }
        }
    };
    while (true)
    {
        for (auto& funcname : funcnames)
        {
            new_funcnames.push_back(funcname);
            for (auto& statement : *function_mapping.at(funcname).statements)
            {
                std::visit([&](auto& arg)
                    {
                        using T = std::decay_t<decltype(arg)>;
                        if constexpr (std::is_same_v<T, Statements::Action>)
                        {
                            Insert(arg);
                        }
                        else if constexpr (std::is_same_v<T, Statements::ActionTable>)
                        {
                            for (auto& action : arg.actions)
                            {
                                Insert(action);
                            }
                        }
                        else if constexpr (std::is_same_v<T, Statements::ProgressList>)
                        {
                            for (auto& flagaction : arg.progress)
                            {
                                Insert(flagaction.second);
                            }
                        }
                        else if constexpr (std::is_same_v<T, Statements::YesNoPrompt>)
                        {
                            Insert(arg.prompt);
                            Insert(arg.on_yes);
                            Insert(arg.on_no);
                        }
                        else if constexpr (std::is_same_v<T, Statements::SetFlagOnTalk>)
                        {
                            Insert(arg.on_set);
                            Insert(arg.on_clear);
                        }
                        else if constexpr (std::is_same_v<T, Statements::IsFlagSet>)
                        {
                            Insert(arg.on_set);
                            Insert(arg.on_clear);
                        }
                        else if constexpr (std::is_same_v<T, Statements::DisplayPrice>)
                        {
                            Insert(arg.display_price);
                        }
                        else if constexpr (std::is_same_v<T, Statements::ShopInteraction>)
                        {
                            Insert(arg.on_sale_prompt);
                            Insert(arg.on_sale_confirm);
                            Insert(arg.on_no_money);
                            Insert(arg.on_sale_decline);
                        }
                        else if constexpr (std::is_same_v<T, Statements::ChurchInteraction>)
                        {
                            Insert(arg.script_normal_priest);
                            Insert(arg.script_skeleton_priest);
                        }
                    }, statement);
            }
        }
        if (insert_list.empty())
        {
            break;
        }
        for (const auto& item : insert_list)
        {
            function_mapping.emplace(item);
        }
        insert_list.clear();
        funcnames = new_funcnames;
        new_funcnames.clear();
    }
}

std::ostream& operator<<(std::ostream& lhs, const Statements::Statement& rhs)
{
    lhs << rhs.Print();
    return lhs;
}

std::ostream& operator<<(std::ostream& lhs, const ScriptFunction& rhs)
{
    lhs << rhs.Print();
    return lhs;
}

std::ostream& operator<<(std::ostream& lhs, const ScriptFunctionTable& rhs)
{
    lhs << rhs.Print();
    return lhs;
}


} // namespace Landstalker
