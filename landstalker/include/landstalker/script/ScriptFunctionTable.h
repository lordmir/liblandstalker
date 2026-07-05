#ifndef _SCRIPT_FUNCTION_TABLE_H_
#define _SCRIPT_FUNCTION_TABLE_H_

#include <landstalker/script/ScriptFunction.h>

namespace Landstalker {

class ScriptFunctionTable
{
public:
	ScriptFunctionTable() {}
	ScriptFunctionTable(AsmFile& file);
	ScriptFunctionTable(AsmFile&& file);
	ScriptFunctionTable(const std::string& yaml);

	bool operator== (const ScriptFunctionTable& rhs) const;
	bool operator!= (const ScriptFunctionTable& rhs) const;

	bool ReadAsm(AsmFile& file);
	bool WriteAsm(AsmFile& file);
	std::string Print() const;
	std::string ToYaml(const std::string& name = std::string("Script")) const;

	const std::vector<std::string>& GetFunctionNames() const;
	const ScriptFunction* GetMapping(const std::string& funcname) const;
	ScriptFunction* GetMapping(const std::string& funcname);
	ScriptFunction* GetMapping(std::size_t index);

	bool AddFunction(ScriptFunction&& func);
	bool RemoveFunction(const std::string& funcname);
	bool SetFunctionOrder(const std::vector<std::string>& order);
private:
	void Consolidate();
	void Unconsolidate();

	std::map<std::string, ScriptFunction> function_mapping;
	std::vector<std::string> funcnames;
};

std::ostream& operator<<(std::ostream& lhs, const ScriptFunctionTable& rhs);

} // namespace Landstalker

#endif // _SCRIPT_FUNCTION_TABLE_H_
