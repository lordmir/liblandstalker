#ifndef _SCRIPT_FUNCTION_TABLE_H_
#define _SCRIPT_FUNCTION_TABLE_H_

#include <landstalker/script/ScriptFunction.h>

namespace Landstalker {

// True if `label` appears in `text` as a whole word (not as a substring of a longer label) -
// used to find textual mentions of function labels inside raw custom ASM.
bool MentionsLabel(const std::string& text, const std::string& label);

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
	// Renames a function, updating every reference to it (jumps and branch targets) held by the
	// other functions in this table. Fails if old_name doesn't exist or new_name already does.
	// Note: mentions inside raw custom ASM blocks are NOT rewritten.
	bool RenameFunction(const std::string& old_name, const std::string& new_name);
	// Rewrites jump/branch references to old_name so they target new_name, WITHOUT touching any
	// definition. Used to keep references held by a *different* table in the same shared
	// function pool consistent when a function is renamed in its owning table.
	void RenameReferences(const std::string& old_name, const std::string& new_name);
	// Reorders the listed functions among the positions they already occupy in the table;
	// functions not in the list keep their exact positions (table order is semantically
	// significant - consecutive functions form fall-through pairs).
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
