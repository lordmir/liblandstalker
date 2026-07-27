#ifndef _SCRIPT_FUNCTION_H_
#define _SCRIPT_FUNCTION_H_

#include <landstalker/main/AsmFile.h>
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <variant>
#include <yaml-cpp/yaml.h>

#include <landstalker/script/ScriptStatements.h>

namespace Landstalker
{

class ScriptFunction
{
public:
	ScriptFunction(const std::string& name, const Statements::ScriptStatementVector& statements);
	ScriptFunction(AsmFile& file);
	// Parses a function mapping ("Function" + "Statements" keys) - a top-level function list
	// item, or an action's nested inline definition.
	ScriptFunction(const YAML::Node& node);
	ScriptFunction(const ScriptFunction& other);

	ScriptFunction& operator= (const ScriptFunction& rhs);
	bool operator== (const ScriptFunction& rhs) const;
	bool operator!= (const ScriptFunction& rhs) const;

	void ToAsm(AsmFile& file) const;
	// Emits the function as one item of the enclosing function list: a mapping with "Function"
	// (the name) and "Statements" (the statement sequence) keys - the exact form the YAML
	// constructor reads back, whether at the top level or nested inline inside an action.
	void ToYaml(YAML::Emitter& out) const;
	std::string Print(int indent = 0) const;
	bool ProcessScriptFunction(const AsmFile::Instruction& ins, AsmFile& file);
	bool ProcessScriptTrap(const AsmFile::Instruction& ins, AsmFile& file);
	bool ProcessScriptMiscInstructions(const AsmFile::Instruction& ins);

	std::string name;
	std::unique_ptr<Statements::ScriptStatementVector> statements;
};

std::ostream& operator<<(std::ostream& lhs, const ScriptFunction& rhs);

} // namespace Landstalker

#endif // _SCRIPT_FUNCTION_H_
