#include <landstalker/main/ScriptData.h>

#include <landstalker/main/AsmUtils.h>
#include <landstalker/main/RomLabels.h>
#include <landstalker/misc/Literals.h>
#include <landstalker/misc/Labels.h>

namespace Landstalker {

ScriptData::ScriptData(const std::filesystem::path& asm_file)
	: DataManager("Script Data", asm_file),
	  m_is_asm(true)
{
	if (!LoadAsmFilenames())
	{
		throw std::runtime_error(std::string("Unable to load file data from \'") + asm_file.string() + '\'');
	}
	if (!AsmLoadScript())
	{
		throw std::runtime_error(std::string("Unable to load script from \'") + m_script_filename.string() + '\'');
	}
	if (!AsmLoadScriptTables())
	{
		throw std::runtime_error(std::string("Unable to load script tables from \'") + asm_file.string() + '\'');
	}
	if (!AsmLoadScriptFunctions())
	{
		throw std::runtime_error(std::string("Unable to load script functions from \'") + asm_file.string() + '\'');
	}
	if (!AsmLoadCutsceneActions())
	{
		throw std::runtime_error(std::string("Unable to load cutscene actions from \'") + asm_file.string() + '\'');
	}
	if (!AsmLoadTriggerActions())
	{
		throw std::runtime_error(std::string("Unable to load trigger actions from \'") + asm_file.string() + '\'');
	}
	if (!AsmLoadRoomActions())
	{
		throw std::runtime_error(std::string("Unable to load room actions from \'") + asm_file.string() + '\'');
	}
	if (!AsmLoadItemUse())
	{
		throw std::runtime_error(std::string("Unable to load item use handlers from \'") + asm_file.string() + '\'');
	}
	if (!AsmLoadItemArticles())
	{
		throw std::runtime_error(std::string("Unable to load item articles from \'") + m_item_articles_filename.string() + '\'');
	}
	// The index of the first string script entry varies by region (e.g. 0x4D for US/JP,
	// 0x4E for FR/DE) - read it from the disassembly's SCRIPT_STRINGS_BEGIN constant,
	// falling back to the US value if the define is missing.
	const auto script_start = m_defines.find(RomLabels::Script::SCRIPT_STRINGS_BEGIN);
	if (script_start != m_defines.cend())
	{
		m_script_start = static_cast<uint16_t>(AsmFile::ParseValue(script_start->second, m_defines));
	}
	else
	{
		m_script_start = 0x4D;
	}
	InitCache();
}

ScriptData::ScriptData(const Rom& rom)
	: DataManager("Script Data", rom),
	  m_is_asm(false)
{
	SetDefaultFilenames();
	if (!RomLoadScript(rom))
	{
		throw std::runtime_error(std::string("Unable to load script from ROM"));
	}
	if (!RomLoadItemArticles(rom))
	{
		throw std::runtime_error(std::string("Unable to load item articles from ROM"));
	}
	InitCache();
}

bool ScriptData::Save(const std::filesystem::path& dir)
{
	std::filesystem::path directory = dir;
	if (std::filesystem::exists(directory) && std::filesystem::is_regular_file(directory))
	{
		directory = directory.parent_path();
	}
	if (!CreateDirectoryStructure(directory))
	{
		throw std::runtime_error(std::string("Unable to create directory structure at \'") + directory.string() + '\'');
	}
	if (!AsmSaveScript(dir))
	{
		throw std::runtime_error(std::string("Unable to save script to \'") + m_script_filename.string() + '\'');
	}
	if (!AsmSaveScriptTables(dir))
	{
		throw std::runtime_error(std::string("Unable to save script tables to \'") + directory.string() + '\'');
	}
	if (!AsmSaveScriptFunctions(dir))
	{
		throw std::runtime_error(std::string("Unable to save script functions to \'") + directory.string() + '\'');
	}
	if (!AsmSaveCutsceneActions(dir))
	{
		throw std::runtime_error(std::string("Unable to save cutscene actions to \'") + directory.string() + '\'');
	}
	if (!AsmSaveTriggerActions(dir))
	{
		throw std::runtime_error(std::string("Unable to save trigger actions to \'") + directory.string() + '\'');
	}
	if (!AsmSaveRoomActions(dir))
	{
		throw std::runtime_error(std::string("Unable to save room actions to \'") + directory.string() + '\'');
	}
	if (!AsmSaveItemUse(dir))
	{
		throw std::runtime_error(std::string("Unable to save item use handlers to \'") + directory.string() + '\'');
	}
	if (!AsmSaveItemArticles(dir))
	{
		throw std::runtime_error(std::string("Unable to save item articles to \'") + directory.string() + '\'');
	}
	CommitAllChanges();
	return true;
}

bool ScriptData::Save()
{
	return Save(GetBasePath());
}

bool ScriptData::HasBeenModified() const
{
	if (m_script_orig != *m_script)
	{
		return true;
	}
	if (m_is_asm && *m_chartable != *m_chartable_orig)
	{
		return true;
	}
	if (m_is_asm && *m_cutscene_table != *m_cutscene_table_orig)
	{
		return true;
	}
	if (m_is_asm && *m_shoptable != *m_shoptable_orig)
	{
		return true;
	}
	if (m_is_asm && *m_itemtable != *m_itemtable_orig)
	{
		return true;
	}
	if (m_is_asm && *m_charfuncs != *m_charfuncs_orig)
	{
		return true;
	}
	if (m_is_asm && *m_cutscenefuncs != *m_cutscenefuncs_orig)
	{
		return true;
	}
	if (m_is_asm && *m_shopfuncs != *m_shopfuncs_orig)
	{
		return true;
	}
	if (m_is_asm && *m_itemfuncs != *m_itemfuncs_orig)
	{
		return true;
	}
	if (m_is_asm && *m_flagprogress != *m_flagprogress_orig)
	{
		return true;
	}
	if (m_is_asm && m_cutscene_actions && m_cutscene_actions_orig
		&& *m_cutscene_actions != *m_cutscene_actions_orig)
	{
		return true;
	}
	if (m_is_asm && m_trigger_actions && m_trigger_actions_orig
		&& *m_trigger_actions != *m_trigger_actions_orig)
	{
		return true;
	}
	if (m_is_asm && m_room_actions && m_room_actions_orig
		&& *m_room_actions != *m_room_actions_orig)
	{
		return true;
	}
	if (m_is_asm && m_item_use && m_item_use_orig
		&& *m_item_use != *m_item_use_orig)
	{
		return true;
	}
	if (m_itemarticles != m_itemarticles_orig)
	{
		return true;
	}
	if (m_itemfoundarticles != m_itemfoundarticles_orig)
	{
		return true;
	}
	if (m_itemusearticles != m_itemusearticles_orig)
	{
		return true;
	}
	return false;
}

void ScriptData::RefreshPendingWrites(const Rom& rom)
{
	DataManager::RefreshPendingWrites(rom);
	if (!RomPrepareInjectScript(rom))
	{
		throw std::runtime_error(std::string("Unable to prepare script for ROM injection"));
	}
	if (!RomPrepareInjectItemArticles(rom))
	{
		throw std::runtime_error(std::string("Unable to prepare item articles for ROM injection"));
	}
}

std::wstring ScriptData::GetScriptEntryDisplayName(int script_id)
{
	std::wstring label = StrWPrintf("Script%04d", script_id);
	if (Labels::Exists(Labels::C_SCRIPT, script_id))
	{
		label += L" " + *Labels::Get(Labels::C_SCRIPT, script_id);
	}
	return label;
}

std::wstring ScriptData::GetFlagDisplayName(int flag)
{
	std::wstring label = StrWPrintf("[Flag %04d]", flag);
	if (Labels::Exists(Labels::C_FLAGS, flag))
	{
		label += L" (" + *Labels::Get(Labels::C_FLAGS, flag) + L")";
	}
	return label;
}

std::wstring ScriptData::GetCutsceneDisplayName(int cutscene)
{
	std::wstring label = StrWPrintf("Cutscene%03d", cutscene);
	if (Labels::Exists(Labels::C_CUTSCENE, cutscene))
	{
		label += L" " + *Labels::Get(Labels::C_CUTSCENE, cutscene);
	}
	return label;
}

uint16_t ScriptData::GetStringStart() const
{
	return m_script_start;
}

std::shared_ptr<Script> ScriptData::GetScript()
{
	return m_script;
}

std::shared_ptr<const Script> ScriptData::GetScript() const
{
	return m_script;
}

bool ScriptData::HasTables() const
{
	return m_is_asm;
}

std::shared_ptr<const std::vector<ScriptTable::Action>> ScriptData::GetCharTable() const
{
	return m_chartable;
}

std::shared_ptr<std::vector<ScriptTable::Action>> ScriptData::GetCharTable()
{
	return m_chartable;
}

std::shared_ptr<const std::vector<ScriptTable::Action>> ScriptData::GetCutsceneTable() const
{
	return m_cutscene_table;
}

std::shared_ptr<std::vector<ScriptTable::Action>> ScriptData::GetCutsceneTable()
{
	return m_cutscene_table;
}

std::shared_ptr<const std::vector<ScriptTable::Shop>> ScriptData::GetShopTable() const
{
	return m_shoptable;
}

std::shared_ptr<std::vector<ScriptTable::Shop>> ScriptData::GetShopTable()
{
	return m_shoptable;
}

void ScriptData::RemapRooms(const RoomIndexMap& mapping)
{
	if (!IsValidRoomRenumbering(mapping) || !m_shoptable)
	{
		return;
	}
	// A shop in a deleted room goes with it.
	RemapRoomRecords(mapping, *m_shoptable, { &ScriptTable::Shop::room });
}

std::shared_ptr<const std::vector<ScriptTable::Item>> ScriptData::GetItemTable() const
{
	return m_itemtable;
}

std::shared_ptr<std::vector<ScriptTable::Item>> ScriptData::GetItemTable()
{
	return m_itemtable;
}

std::shared_ptr<const ScriptFunctionTable> ScriptData::GetCharFuncs() const
{
	return m_charfuncs;
}

std::shared_ptr<ScriptFunctionTable> ScriptData::GetCharFuncs()
{
	return m_charfuncs;
}

void ScriptData::SetCharFuncs(const ScriptFunctionTable& funcs)
{
	*m_charfuncs = funcs;
}

std::shared_ptr<const ScriptFunctionTable> ScriptData::GetCutsceneFuncs() const
{
	return m_cutscenefuncs;
}

std::shared_ptr<ScriptFunctionTable> ScriptData::GetCutsceneFuncs()
{
	return m_cutscenefuncs;
}

void ScriptData::SetCutsceneFuncs(const ScriptFunctionTable& funcs)
{
	*m_cutscenefuncs = funcs;
}

std::shared_ptr<const ScriptFunctionTable> ScriptData::GetShopFuncs() const
{
	return m_shopfuncs;
}

std::shared_ptr<ScriptFunctionTable> ScriptData::GetShopFuncs()
{
	return m_shopfuncs;
}

void ScriptData::SetShopFuncs(const ScriptFunctionTable& funcs)
{
	*m_shopfuncs = funcs;
}

std::shared_ptr<const ScriptFunctionTable> ScriptData::GetItemFuncs() const
{
	return m_itemfuncs;
}

std::shared_ptr<ScriptFunctionTable> ScriptData::GetItemFuncs()
{
	return m_itemfuncs;
}

void ScriptData::SetItemFuncs(ScriptFunctionTable& funcs)
{
	*m_itemfuncs = funcs;
}

std::shared_ptr<const ScriptFunctionTable> ScriptData::GetProgressFlagsFuncs() const
{
	return m_flagprogress;
}

std::shared_ptr<ScriptFunctionTable> ScriptData::GetProgressFlagsFuncs()
{
	return m_flagprogress;
}

void ScriptData::SetProgressFlagsFuncs(const ScriptFunctionTable& funcs)
{
	*m_flagprogress = funcs;
}

std::shared_ptr<const AsmFunctionTable> ScriptData::GetCutsceneActions() const
{
	return m_cutscene_actions;
}

std::shared_ptr<AsmFunctionTable> ScriptData::GetCutsceneActions()
{
	return m_cutscene_actions;
}

void ScriptData::SetCutsceneActions(const AsmFunctionTable& actions)
{
	if (!m_cutscene_actions)
	{
		m_cutscene_actions = std::make_shared<AsmFunctionTable>();
	}
	*m_cutscene_actions = actions;
}

std::shared_ptr<const AsmFunctionTable> ScriptData::GetTriggerActions() const
{
	return m_trigger_actions;
}

std::shared_ptr<AsmFunctionTable> ScriptData::GetTriggerActions()
{
	return m_trigger_actions;
}

void ScriptData::SetTriggerActions(const AsmFunctionTable& actions)
{
	if (!m_trigger_actions)
	{
		m_trigger_actions = std::make_shared<AsmFunctionTable>();
	}
	*m_trigger_actions = actions;
}

std::shared_ptr<const RoomActionTable> ScriptData::GetRoomActions() const
{
	return m_room_actions;
}

std::shared_ptr<RoomActionTable> ScriptData::GetRoomActions()
{
	return m_room_actions;
}

void ScriptData::SetRoomActions(const RoomActionTable& actions)
{
	if (!m_room_actions)
	{
		m_room_actions = std::make_shared<RoomActionTable>();
	}
	*m_room_actions = actions;
}

std::shared_ptr<const ItemUseTable> ScriptData::GetItemUse() const { return m_item_use; }
std::shared_ptr<ItemUseTable> ScriptData::GetItemUse() { return m_item_use; }
void ScriptData::SetItemUse(const ItemUseTable& table)
{
	if (!m_item_use)
	{
		m_item_use = std::make_shared<ItemUseTable>();
	}
	*m_item_use = table;
}

bool ScriptData::HasItemArticles() const
{
	return m_itemarticles.has_value();
}

uint8_t ScriptData::GetItemArticle(uint8_t item) const
{
	if (!m_itemarticles || item >= m_itemarticles->size())
	{
		return 0;
	}
	return m_itemarticles->at(item);
}

void ScriptData::SetItemArticle(uint8_t item, uint8_t article)
{
	if (!m_itemarticles || item >= m_itemarticles->size())
	{
		return;
	}
	m_itemarticles->at(item) = article & 0x07;
}

void ScriptData::CommitAllChanges()
{
	m_pending_writes.clear();
}

bool ScriptData::LoadAsmFilenames()
{
	try
	{
		bool retval = true;
		AsmFile f(GetAsmFilename().string());
		retval = retval && GetFilenameFromAsm(f, RomLabels::DEFINES_SECTION, m_defines_filename);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Script::SCRIPT_SECTION, m_script_filename);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Script::CUTSCENE_TABLE_SECTION, m_cutscene_table_filename);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Script::CHAR_TABLE_SECTION, m_char_table_filename);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Script::SHOP_TABLE_SECTION, m_shop_table_filename);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Script::ITEM_TABLE_SECTION, m_item_table_filename);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Script::CHAR_FUNCS_SECTION, m_char_funcs_filename);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Script::CUTSCENE_FUNCS_SECTION, m_cutscene_funcs_filename);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Script::SHOP_FUNCS_SECTION, m_shop_funcs_filename);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Script::ITEM_FUNCS_SECTION, m_item_funcs_filename);
		retval = retval && GetFilenameFromAsm(f, RomLabels::Script::FLAG_PROGRESS_SECTION, m_flag_progress_filename);
		// Optional (older disassemblies lack these): don't fail the whole load if absent.
		if (f.LabelExists(RomLabels::Script::CUTSCENE_ACTIONS_SECTION))
		{
			retval = retval && GetFilenameFromAsm(f, RomLabels::Script::CUTSCENE_JUMPTABLE_SECTION, m_cutscene_jumptable_filename);
			retval = retval && GetFilenameFromAsm(f, RomLabels::Script::CUTSCENE_ACTIONS_SECTION, m_cutscene_actions_filename);
		}
		if (f.LabelExists(RomLabels::Script::TRIGGER_ACTIONS_SECTION))
		{
			retval = retval && GetFilenameFromAsm(f, RomLabels::Script::TRIGGER_JUMPTABLE_SECTION, m_trigger_jumptable_filename);
			retval = retval && GetFilenameFromAsm(f, RomLabels::Script::TRIGGER_ACTIONS_SECTION, m_trigger_actions_filename);
		}
		if (f.LabelExists(RomLabels::Script::ROOM_ACTIONS1_SECTION))
		{
			retval = retval && GetFilenameFromAsm(f, RomLabels::Script::ROOM_ACTIONS1_SECTION, m_room_actions1_filename);
			retval = retval && GetFilenameFromAsm(f, RomLabels::Script::ROOM_ACTIONS2_SECTION, m_room_actions2_filename);
		}
		if (f.LabelExists(RomLabels::Script::ITEM_USE1_SECTION))
		{
			retval = retval && GetFilenameFromAsm(f, RomLabels::Script::ITEM_PREUSE_TABLE_SECTION, m_item_preuse_table_filename);
			retval = retval && GetFilenameFromAsm(f, RomLabels::Script::ITEM_POSTUSE_TABLE_SECTION, m_item_postuse_table_filename);
			retval = retval && GetFilenameFromAsm(f, RomLabels::Script::ITEM_USE1_SECTION, m_itemuse1_filename);
			retval = retval && GetFilenameFromAsm(f, RomLabels::Script::ITEM_USE2_SECTION, m_itemuse2_filename);
			// itempostuse.asm has no section label of its own (SetDefaultFilenames isn't run on the
			// asm path), so set its default path here.
			m_itempostuse_filename = RomLabels::Script::ITEM_POSTUSE_FILE;
		}
		if (f.LabelExists(RomLabels::Script::ITEM_ARTICLES_SECTION))
		{
			retval = retval && GetFilenameFromAsm(f, RomLabels::Script::ITEM_ARTICLES_SECTION, m_item_articles_filename);
			retval = retval && GetFilenameFromAsm(f, RomLabels::Script::ITEM_FOUND_ARTICLE_TABLE_SECTION, m_item_found_article_table_filename);
			retval = retval && GetFilenameFromAsm(f, RomLabels::Script::ITEM_USE_ARTICLE_TABLE_SECTION, m_item_use_article_table_filename);
		}
		return retval;
	}
	catch (...)
	{
	}
	return false;
}

void ScriptData::SetDefaultFilenames()
{
	if (m_defines_filename.empty()) m_defines_filename = RomLabels::DEFINES_FILE;
	if (m_script_filename.empty()) m_script_filename = RomLabels::Script::SCRIPT_FILE;
	if (m_cutscene_table_filename.empty()) m_cutscene_table_filename = RomLabels::Script::CUTSCENE_TABLE_FILE;
	if (m_char_table_filename.empty()) m_char_table_filename = RomLabels::Script::CHAR_TABLE_FILE;
	if (m_shop_table_filename.empty()) m_shop_table_filename = RomLabels::Script::SHOP_TABLE_FILE;
	if (m_item_table_filename.empty()) m_item_table_filename = RomLabels::Script::ITEM_TABLE_FILE;
	if (m_char_funcs_filename.empty()) m_char_funcs_filename = RomLabels::Script::CHAR_FUNCS_FILE;
	if (m_cutscene_funcs_filename.empty()) m_cutscene_funcs_filename = RomLabels::Script::CUTSCENE_FUNCS_FILE;
	if (m_shop_funcs_filename.empty()) m_shop_funcs_filename = RomLabels::Script::SHOP_FUNCS_FILE;
	if (m_item_funcs_filename.empty()) m_item_funcs_filename = RomLabels::Script::ITEM_FUNCS_FILE;
	if (m_flag_progress_filename.empty()) m_flag_progress_filename = RomLabels::Script::FLAG_PROGRESS_FILE;
	if (m_cutscene_actions_filename.empty()) m_cutscene_actions_filename = RomLabels::Script::CUTSCENE_ACTIONS_FILE;
	if (m_cutscene_jumptable_filename.empty()) m_cutscene_jumptable_filename = RomLabels::Script::CUTSCENE_JUMPTABLE_FILE;
	if (m_trigger_actions_filename.empty()) m_trigger_actions_filename = RomLabels::Script::TRIGGER_ACTIONS_FILE;
	if (m_trigger_jumptable_filename.empty()) m_trigger_jumptable_filename = RomLabels::Script::TRIGGER_JUMPTABLE_FILE;
	if (m_room_actions1_filename.empty()) m_room_actions1_filename = RomLabels::Script::ROOM_ACTIONS1_FILE;
	if (m_room_actions2_filename.empty()) m_room_actions2_filename = RomLabels::Script::ROOM_ACTIONS2_FILE;
	if (m_item_preuse_table_filename.empty()) m_item_preuse_table_filename = RomLabels::Script::ITEM_PREUSE_TABLE_FILE;
	if (m_item_postuse_table_filename.empty()) m_item_postuse_table_filename = RomLabels::Script::ITEM_POSTUSE_TABLE_FILE;
	if (m_itemuse1_filename.empty()) m_itemuse1_filename = RomLabels::Script::ITEM_USE1_FILE;
	if (m_itemuse2_filename.empty()) m_itemuse2_filename = RomLabels::Script::ITEM_USE2_FILE;
	if (m_itempostuse_filename.empty()) m_itempostuse_filename = RomLabels::Script::ITEM_POSTUSE_FILE;
	if (m_itemarticles.has_value() && m_itemfoundarticles.has_value() && m_itemusearticles.has_value())
	{
		if (m_item_articles_filename.empty()) m_item_articles_filename = RomLabels::Script::ITEM_ARTICLES_FILE;
		if (m_item_found_article_table_filename.empty()) m_item_found_article_table_filename = RomLabels::Script::ITEM_FOUND_ARTICLE_TABLE_FILE;
		if (m_item_use_article_table_filename.empty()) m_item_use_article_table_filename = RomLabels::Script::ITEM_USE_ARTICLE_TABLE_FILE;
	}
}

bool ScriptData::CreateDirectoryStructure(const std::filesystem::path& dir)
{
	bool retval = true;

	retval = retval && CreateDirectoryTree(dir / m_script_filename);
	if (m_is_asm)
	{
		retval = retval && CreateDirectoryTree(dir / m_cutscene_table_filename);
		retval = retval && CreateDirectoryTree(dir / m_char_table_filename);
		retval = retval && CreateDirectoryTree(dir / m_shop_table_filename);
		retval = retval && CreateDirectoryTree(dir / m_item_table_filename);
		retval = retval && CreateDirectoryTree(dir / m_char_funcs_filename);
		retval = retval && CreateDirectoryTree(dir / m_cutscene_funcs_filename);
		retval = retval && CreateDirectoryTree(dir / m_shop_funcs_filename);
		retval = retval && CreateDirectoryTree(dir / m_item_funcs_filename);
		retval = retval && CreateDirectoryTree(dir / m_flag_progress_filename);
		if (m_cutscene_actions && m_cutscene_actions->IsValid()
			&& !m_cutscene_actions_filename.empty() && !m_cutscene_jumptable_filename.empty())
		{
			retval = retval && CreateDirectoryTree(dir / m_cutscene_actions_filename);
			retval = retval && CreateDirectoryTree(dir / m_cutscene_jumptable_filename);
		}
		if (m_trigger_actions && m_trigger_actions->IsValid()
			&& !m_trigger_actions_filename.empty() && !m_trigger_jumptable_filename.empty())
		{
			retval = retval && CreateDirectoryTree(dir / m_trigger_actions_filename);
			retval = retval && CreateDirectoryTree(dir / m_trigger_jumptable_filename);
		}
		if (m_room_actions && m_room_actions->IsValid()
			&& !m_room_actions1_filename.empty() && !m_room_actions2_filename.empty())
		{
			retval = retval && CreateDirectoryTree(dir / m_room_actions1_filename);
			retval = retval && CreateDirectoryTree(dir / m_room_actions2_filename);
		}
		if (m_item_use && m_item_use->IsValid())
		{
			retval = retval && CreateDirectoryTree(dir / m_item_preuse_table_filename);
			retval = retval && CreateDirectoryTree(dir / m_item_postuse_table_filename);
			retval = retval && CreateDirectoryTree(dir / m_itemuse1_filename);
			retval = retval && CreateDirectoryTree(dir / m_itemuse2_filename);
			retval = retval && CreateDirectoryTree(dir / m_itempostuse_filename);
		}
		if (!m_item_articles_filename.empty() && !m_item_use_article_table_filename.empty() && !m_item_found_article_table_filename.empty())
		{
			retval = retval && CreateDirectoryTree(dir / m_item_articles_filename);
			retval = retval && CreateDirectoryTree(dir / m_item_found_article_table_filename);
			retval = retval && CreateDirectoryTree(dir / m_item_use_article_table_filename);
		}
	}

	return retval;
}

void ScriptData::InitCache()
{
	m_script_orig = *m_script;
	if (m_is_asm)
	{
		m_cutscene_table_orig = std::make_shared<std::vector<ScriptTable::Action>>(*m_cutscene_table);
		m_chartable_orig = std::make_shared<std::vector<ScriptTable::Action>>(*m_chartable);
		m_shoptable_orig = std::make_shared<std::vector<ScriptTable::Shop>>(*m_shoptable);
		m_itemtable_orig = std::make_shared<std::vector<ScriptTable::Item>>(*m_itemtable);
		m_charfuncs_orig = std::make_shared<ScriptFunctionTable>(*m_charfuncs);
		m_cutscenefuncs_orig = std::make_shared<ScriptFunctionTable>(*m_cutscenefuncs);
		m_shopfuncs_orig = std::make_shared<ScriptFunctionTable>(*m_shopfuncs);
		m_itemfuncs_orig = std::make_shared<ScriptFunctionTable>(*m_itemfuncs);
		m_flagprogress_orig = std::make_shared<ScriptFunctionTable>(*m_flagprogress);
		if (m_cutscene_actions)
		{
			m_cutscene_actions_orig = std::make_shared<AsmFunctionTable>(*m_cutscene_actions);
		}
		if (m_trigger_actions)
		{
			m_trigger_actions_orig = std::make_shared<AsmFunctionTable>(*m_trigger_actions);
		}
		if (m_room_actions)
		{
			m_room_actions_orig = std::make_shared<RoomActionTable>(*m_room_actions);
		}
		if (m_item_use)
		{
			m_item_use_orig = std::make_shared<ItemUseTable>(*m_item_use);
		}
	}
	// Copies nullopt for regions without the article tables. Cached for the ROM path too,
	// which also loads (and can modify) the article data.
	m_itemarticles_orig = m_itemarticles;
	m_itemfoundarticles_orig = m_itemfoundarticles;
	m_itemusearticles_orig = m_itemusearticles;
}

bool ScriptData::AsmLoadScript()
{
	std::filesystem::path path = GetBasePath() / m_script_filename;
	m_script = std::make_shared<Script>(ReadBytes(path));
	return true;
}

bool ScriptData::AsmLoadScriptTables()
{
	m_defines = AsmFile::LoadDefines(GetAsmFilename(), GetBasePath(), RomLabels::DEFINES_SECTION);
	if (m_defines.empty() && !m_defines_filename.empty())
	{
		m_defines = AsmFile::ParseDefines((GetBasePath() / m_defines_filename).string(), GetBasePath());
	}
	m_cutscene_table = ScriptTable::ReadTable((GetBasePath() / m_cutscene_table_filename).string(), m_defines);
	m_chartable = ScriptTable::ReadTable((GetBasePath() / m_char_table_filename).string(), m_defines);
	m_shoptable = ScriptTable::ReadShopTable((GetBasePath() / m_shop_table_filename).string(), m_defines);
	m_itemtable = ScriptTable::ReadItemTable((GetBasePath() / m_item_table_filename).string(), m_defines);
	return true;
}

bool ScriptData::AsmLoadScriptFunctions()
{
	m_charfuncs = std::make_shared<ScriptFunctionTable>(AsmFile(GetBasePath() / m_char_funcs_filename, m_defines));
	m_cutscenefuncs = std::make_shared<ScriptFunctionTable>(AsmFile(GetBasePath() / m_cutscene_funcs_filename, m_defines));
	m_shopfuncs = std::make_shared<ScriptFunctionTable>(AsmFile(GetBasePath() / m_shop_funcs_filename, m_defines));
	m_itemfuncs = std::make_shared<ScriptFunctionTable>(AsmFile(GetBasePath() / m_item_funcs_filename, m_defines));
	m_flagprogress = std::make_shared<ScriptFunctionTable>(AsmFile(GetBasePath() / m_flag_progress_filename, m_defines));
	return true;
}

bool ScriptData::AsmLoadCutsceneActions()
{
	// Parsed as verbatim text rather than via AsmFile - see AsmFunctionTable /
	// [[dialogueactions-asm-format]]. Absent in older disassemblies: leave the table invalid
	// (the editor stays disabled) rather than failing the whole project load.
	m_cutscene_actions = std::make_shared<AsmFunctionTable>();
	if (m_cutscene_actions_filename.empty() || m_cutscene_jumptable_filename.empty())
	{
		return true;
	}
	const auto jump_bytes = ReadBytes(GetBasePath() / m_cutscene_jumptable_filename);
	const auto action_bytes = ReadBytes(GetBasePath() / m_cutscene_actions_filename);
	if (jump_bytes.empty() || action_bytes.empty())
	{
		return true;
	}
	const std::string jump_text(jump_bytes.begin(), jump_bytes.end());
	const std::string action_text(action_bytes.begin(), action_bytes.end());
	m_cutscene_actions->Parse(jump_text, action_text, RomLabels::Script::CUTSCENE_ACTION_DISPATCH_LABEL);
	return true;
}

bool ScriptData::AsmLoadTriggerActions()
{
	// Same verbatim-text handling as the cutscene actions (see AsmLoadCutsceneActions). Absent in
	// older disassemblies: leave the table invalid rather than failing the project load.
	m_trigger_actions = std::make_shared<AsmFunctionTable>();
	if (m_trigger_actions_filename.empty() || m_trigger_jumptable_filename.empty())
	{
		return true;
	}
	const auto jump_bytes = ReadBytes(GetBasePath() / m_trigger_jumptable_filename);
	const auto action_bytes = ReadBytes(GetBasePath() / m_trigger_actions_filename);
	if (jump_bytes.empty() || action_bytes.empty())
	{
		return true;
	}
	const std::string jump_text(jump_bytes.begin(), jump_bytes.end());
	const std::string action_text(action_bytes.begin(), action_bytes.end());
	m_trigger_actions->Parse(jump_text, action_text, RomLabels::Script::TRIGGER_ACTION_DISPATCH_LABEL);
	return true;
}

bool ScriptData::AsmLoadRoomActions()
{
	// The per-room fixup chain, parsed as verbatim text (RoomActionTable). Absent in older
	// disassemblies: leave the table invalid rather than failing the project load. m_defines was
	// loaded by AsmLoadScriptTables and resolves the ROOM_x tokens in the branch guards.
	m_room_actions = std::make_shared<RoomActionTable>();
	if (m_room_actions1_filename.empty() || m_room_actions2_filename.empty())
	{
		return true;
	}
	const auto bytes1 = ReadBytes(GetBasePath() / m_room_actions1_filename);
	const auto bytes2 = ReadBytes(GetBasePath() / m_room_actions2_filename);
	if (bytes1.empty() || bytes2.empty())
	{
		return true;
	}
	const std::string text1(bytes1.begin(), bytes1.end());
	const std::string text2(bytes2.begin(), bytes2.end());
	m_room_actions->Parse(text1, text2, m_defines);
	return true;
}

bool ScriptData::AsmLoadItemUse()
{
	// Item pre-use / post-use handlers + their two dispatch tables (verbatim blocks; see ItemUseTable).
	// Absent in older disassemblies: leave the table invalid rather than failing the project load.
	m_item_use = std::make_shared<ItemUseTable>();
	if (m_itemuse1_filename.empty() || m_itemuse2_filename.empty() || m_itempostuse_filename.empty()
		|| m_item_preuse_table_filename.empty() || m_item_postuse_table_filename.empty())
	{
		return true;
	}
	const auto pre_tbl = ReadBytes(GetBasePath() / m_item_preuse_table_filename);
	const auto post_tbl = ReadBytes(GetBasePath() / m_item_postuse_table_filename);
	const auto u1 = ReadBytes(GetBasePath() / m_itemuse1_filename);
	const auto u2 = ReadBytes(GetBasePath() / m_itemuse2_filename);
	const auto pu = ReadBytes(GetBasePath() / m_itempostuse_filename);
	if (pre_tbl.empty() || post_tbl.empty() || u1.empty() || u2.empty() || pu.empty())
	{
		return true;
	}
	m_item_use->Parse(std::string(pre_tbl.begin(), pre_tbl.end()), std::string(post_tbl.begin(), post_tbl.end()),
		std::string(u1.begin(), u1.end()), std::string(u2.begin(), u2.end()),
		std::string(pu.begin(), pu.end()), m_defines);
	return true;
}

bool ScriptData::AsmLoadItemArticles()
{
	if (!m_item_articles_filename.empty() && !m_item_use_article_table_filename.empty() && !m_item_found_article_table_filename.empty())
	{
		std::filesystem::path articles_path = GetBasePath() / m_item_articles_filename;
		auto data = ReadBytes(articles_path);
		m_itemarticles = std::vector<uint8_t>(data.size() * 2);
		std::size_t i = 0;
		for (const auto& byte : data)
		{
			(*m_itemarticles)[i++] = (byte >> 5) & 0x07;
			(*m_itemarticles)[i++] = (byte >> 1) & 0x07;
		}
		std::filesystem::path itemuse_path = GetBasePath() / m_item_use_article_table_filename;
		data = ReadBytes(itemuse_path);
		m_itemusearticles = std::vector<uint16_t>(data.size() / 2);
		for (i = 0; i + 1 < data.size(); i += 2)
		{
			(*m_itemusearticles)[i / 2] = (data[i] << 8) | data[i + 1];
		}
		std::filesystem::path itemfound_path = GetBasePath() / m_item_found_article_table_filename;
		data = ReadBytes(itemfound_path);
		m_itemfoundarticles = std::vector<uint16_t>(data.size() / 2);
		for (i = 0; i + 1 < data.size(); i += 2)
		{
			(*m_itemfoundarticles)[i / 2] = (data[i] << 8) | data[i + 1];
		}
	}
	else
	{
		m_itemarticles.reset();
		m_itemfoundarticles.reset();
		m_itemusearticles.reset();
	}
	return true;
}

bool ScriptData::RomLoadScript(const Rom& rom)
{
	uint32_t script_begin = rom.get_section(RomLabels::Script::SCRIPT_SECTION).begin;
	uint32_t script_end = Disasm::ReadOffset16(rom, RomLabels::Script::SCRIPT_END);
	uint32_t script_size = script_end - script_begin;
	m_script = std::make_shared<Script>(rom.read_array<uint8_t>(script_begin, script_size));
	m_script_start = rom.read<uint16_t>(RomLabels::Script::SCRIPT_STRINGS_BEGIN);
	return true;
}

bool ScriptData::RomLoadItemArticles(const Rom& rom)
{
	uint32_t articles_begin = rom.get_section(RomLabels::Script::ITEM_ARTICLES_SECTION).begin;
	uint32_t articles_end = rom.get_section(RomLabels::Script::ITEM_ARTICLES_SECTION).end;
	uint32_t articles_size = articles_end - articles_begin;
	uint32_t itemuse_begin = rom.get_section(RomLabels::Script::ITEM_USE_ARTICLE_TABLE_SECTION).begin;
	uint32_t itemuse_end = rom.get_section(RomLabels::Script::ITEM_USE_ARTICLE_TABLE_SECTION).end;
	uint32_t itemuse_size = itemuse_end - itemuse_begin;
	uint32_t itemfound_begin = rom.get_section(RomLabels::Script::ITEM_FOUND_ARTICLE_TABLE_SECTION).begin;
	uint32_t itemfound_end = rom.get_section(RomLabels::Script::ITEM_FOUND_ARTICLE_TABLE_SECTION).end;
	uint32_t itemfound_size = itemfound_end - itemfound_begin;
	if (articles_size && itemuse_size && itemfound_size)
	{
		auto data = rom.read_array<uint8_t>(articles_begin, articles_size);
		std::size_t i = 0;
		m_itemarticles = std::vector<uint8_t>(data.size() * 2);
		for (const auto& byte : data)
		{
			(*m_itemarticles)[i++] = (byte >> 5) & 0x07;
			(*m_itemarticles)[i++] = (byte >> 1) & 0x07;
		}
		m_itemusearticles = rom.read_array<uint16_t>(itemuse_begin, itemuse_size / sizeof(uint16_t));
		m_itemfoundarticles = rom.read_array<uint16_t>(itemfound_begin, itemfound_size / sizeof(uint16_t));
	}
	else
	{
		m_itemarticles.reset();
		m_itemfoundarticles.reset();
		m_itemusearticles.reset();
	}
	return true;
}

bool ScriptData::AsmSaveScript(const std::filesystem::path& dir)
{
	WriteBytes(m_script->ToBytes(), dir / m_script_filename);
	return true;
}

bool ScriptData::AsmSaveScriptTables(const std::filesystem::path& dir)
{
	bool retval = true;
	retval = retval && ScriptTable::WriteTable(dir, m_cutscene_table_filename, "Cutscene Script Table", m_cutscene_table);
	retval = retval && ScriptTable::WriteTable(dir, m_char_table_filename, "Character Script Table", m_chartable);
	retval = retval && ScriptTable::WriteShopTable(dir, m_shop_table_filename, "Shop Script Table", m_shoptable);
	retval = retval && ScriptTable::WriteItemTable(dir, m_item_table_filename, "Item Script Table", m_itemtable);
	return retval;
}

bool ScriptData::AsmSaveScriptFunctions(const std::filesystem::path& dir)
{
	bool retval = true;
	AsmFile char_funcs_asm;
	AsmFile cutscene_funcs_asm;
	AsmFile shop_funcs_asm;
	AsmFile item_funcs_asm;
	AsmFile flag_progress_asm;

	char_funcs_asm.WriteFileHeader(m_char_funcs_filename, "Character Script Functions");
	retval = retval && m_charfuncs->WriteAsm(char_funcs_asm);
	retval = retval && char_funcs_asm.WriteFile(dir / m_char_funcs_filename);

	cutscene_funcs_asm.WriteFileHeader(m_cutscene_funcs_filename, "Cutscene Script Functions");
	retval = retval && m_cutscenefuncs->WriteAsm(cutscene_funcs_asm);
	retval = retval && cutscene_funcs_asm.WriteFile(dir / m_cutscene_funcs_filename);

	shop_funcs_asm.WriteFileHeader(m_shop_funcs_filename, "Shop Script Functions");
	retval = retval && m_shopfuncs->WriteAsm(shop_funcs_asm);
	retval = retval && shop_funcs_asm.WriteFile(dir / m_shop_funcs_filename);

	item_funcs_asm.WriteFileHeader(m_item_funcs_filename, "Special Shop Item Script Functions");
	retval = retval && m_itemfuncs->WriteAsm(item_funcs_asm);
	retval = retval && item_funcs_asm.WriteFile(dir / m_item_funcs_filename);

	flag_progress_asm.WriteFileHeader(m_flag_progress_filename, "Quest Progress Flag Mapping");
	retval = retval && m_flagprogress->WriteAsm(flag_progress_asm);
	retval = retval && flag_progress_asm.WriteFile(dir / m_flag_progress_filename);

	return retval;
}

bool ScriptData::AsmSaveCutsceneActions(const std::filesystem::path& dir)
{
	// Nothing to save if the disassembly didn't provide these files.
	if (!m_cutscene_actions || !m_cutscene_actions->IsValid()
		|| m_cutscene_actions_filename.empty() || m_cutscene_jumptable_filename.empty())
	{
		return true;
	}
	// Written verbatim (NOT via AsmFile), preserving comments/formatting byte-for-byte.
	const std::string jump_text = m_cutscene_actions->EmitDispatch();
	const std::string action_text = m_cutscene_actions->EmitActions();
	WriteBytes(ByteVector(jump_text.begin(), jump_text.end()), dir / m_cutscene_jumptable_filename);
	WriteBytes(ByteVector(action_text.begin(), action_text.end()), dir / m_cutscene_actions_filename);
	return true;
}

bool ScriptData::AsmSaveTriggerActions(const std::filesystem::path& dir)
{
	// Nothing to save if the disassembly didn't provide these files.
	if (!m_trigger_actions || !m_trigger_actions->IsValid()
		|| m_trigger_actions_filename.empty() || m_trigger_jumptable_filename.empty())
	{
		return true;
	}
	// Written verbatim (NOT via AsmFile), preserving comments/formatting byte-for-byte.
	const std::string jump_text = m_trigger_actions->EmitDispatch();
	const std::string action_text = m_trigger_actions->EmitActions();
	WriteBytes(ByteVector(jump_text.begin(), jump_text.end()), dir / m_trigger_jumptable_filename);
	WriteBytes(ByteVector(action_text.begin(), action_text.end()), dir / m_trigger_actions_filename);
	return true;
}

bool ScriptData::AsmSaveRoomActions(const std::filesystem::path& dir)
{
	if (!m_room_actions || !m_room_actions->IsValid()
		|| m_room_actions1_filename.empty() || m_room_actions2_filename.empty())
	{
		return true;
	}
	// Regenerated (renumbered labels, standard boilerplate) but assembling to an identical binary; each
	// file gets the standard AsmFile header comment block.
	AsmFile hdr1, hdr2;
	hdr1.WriteFileHeader(m_room_actions1_filename, "Custom Room Actions");
	hdr2.WriteFileHeader(m_room_actions2_filename, "Custom Room Actions (continued)");
	const std::string text1 = m_room_actions->EmitFile1(hdr1.ToAssembly());
	const std::string text2 = m_room_actions->EmitFile2(hdr2.ToAssembly());
	WriteBytes(ByteVector(text1.begin(), text1.end()), dir / m_room_actions1_filename);
	WriteBytes(ByteVector(text2.begin(), text2.end()), dir / m_room_actions2_filename);
	return true;
}

bool ScriptData::AsmSaveItemUse(const std::filesystem::path& dir)
{
	if (!m_item_use || !m_item_use->IsValid())
	{
		return true;
	}
	AsmFile h1, h2, h3;
	h1.WriteFileHeader(m_itemuse1_filename, "Item Pre-Use Handlers");
	h2.WriteFileHeader(m_itemuse2_filename, "Item Pre-Use Handlers (continued)");
	h3.WriteFileHeader(m_itempostuse_filename, "Item Post-Use Handlers");
	const std::string pre_tbl = m_item_use->EmitPreTable();
	const std::string post_tbl = m_item_use->EmitPostTable();
	const std::string u1 = m_item_use->EmitItemUse1(h1.ToAssembly());
	const std::string u2 = m_item_use->EmitItemUse2(h2.ToAssembly());
	const std::string pu = m_item_use->EmitItemPostUse(h3.ToAssembly());
	WriteBytes(ByteVector(pre_tbl.begin(), pre_tbl.end()), dir / m_item_preuse_table_filename);
	WriteBytes(ByteVector(post_tbl.begin(), post_tbl.end()), dir / m_item_postuse_table_filename);
	WriteBytes(ByteVector(u1.begin(), u1.end()), dir / m_itemuse1_filename);
	WriteBytes(ByteVector(u2.begin(), u2.end()), dir / m_itemuse2_filename);
	WriteBytes(ByteVector(pu.begin(), pu.end()), dir / m_itempostuse_filename);
	return true;
}

bool ScriptData::AsmSaveItemArticles(const std::filesystem::path& dir)
{
	bool retval = true;
	if (m_itemarticles.has_value() && m_itemfoundarticles.has_value() && m_itemusearticles.has_value())
	{
		auto article_data = ByteVector(m_itemarticles->size() / 2);
		for (std::size_t i = 0; i < article_data.size(); ++i)
		{
			article_data[i] = ((*m_itemarticles)[i * 2] & 0x07) << 5;
			article_data[i] |= ((*m_itemarticles)[i * 2 + 1] & 0x07) << 1;
		}
		auto found_data = ByteVector(m_itemfoundarticles->size() * 2);
		for (std::size_t i = 0; i < m_itemfoundarticles->size(); ++i)
		{
			found_data[i * 2] = (*m_itemfoundarticles)[i] >> 8;
			found_data[i * 2 + 1] = (*m_itemfoundarticles)[i] & 0xFF;
		}
		auto use_data = ByteVector(m_itemusearticles->size() * 2);
		for (std::size_t i = 0; i < m_itemusearticles->size(); ++i)
		{
			use_data[i * 2] = (*m_itemusearticles)[i] >> 8;
			use_data[i * 2 + 1] = (*m_itemusearticles)[i] & 0xFF;
		}
		WriteBytes(article_data, dir / m_item_articles_filename);
		WriteBytes(found_data, dir / m_item_found_article_table_filename);
		WriteBytes(use_data, dir / m_item_use_article_table_filename);
	}
	return retval;
}

bool ScriptData::RomPrepareInjectScript(const Rom& /*rom*/)
{
	m_pending_writes.push_back({ RomLabels::Script::SCRIPT_SECTION, std::make_shared<ByteVector>(m_script->ToBytes()) });
	return true;
}

bool ScriptData::RomPrepareInjectItemArticles(const Rom& /*rom*/)
{
	if (m_itemarticles.has_value() && m_itemfoundarticles.has_value() && m_itemusearticles.has_value())
	{
		auto article_data = ByteVector(m_itemarticles->size() / 2);
		for (std::size_t i = 0; i < article_data.size(); ++i)
		{
			article_data[i] = ((*m_itemarticles)[i * 2] & 0x07) << 5;
			article_data[i] |= ((*m_itemarticles)[i * 2 + 1] & 0x07) << 1;
		}
		auto found_data = ByteVector(m_itemfoundarticles->size() * 2);
		for (std::size_t i = 0; i < m_itemfoundarticles->size(); ++i)
		{
			found_data[i * 2] = (*m_itemfoundarticles)[i] >> 8;
			found_data[i * 2 + 1] = (*m_itemfoundarticles)[i] & 0xFF;
		}
		auto use_data = ByteVector(m_itemusearticles->size() * 2);
		for (std::size_t i = 0; i < m_itemusearticles->size(); ++i)
		{
			use_data[i * 2] = (*m_itemusearticles)[i] >> 8;
			use_data[i * 2 + 1] = (*m_itemusearticles)[i] & 0xFF;
		}
		m_pending_writes.push_back({ RomLabels::Script::ITEM_ARTICLES_SECTION, std::make_shared<ByteVector>(article_data) });
		m_pending_writes.push_back({ RomLabels::Script::ITEM_FOUND_ARTICLE_TABLE_SECTION, std::make_shared<ByteVector>(found_data) });
		m_pending_writes.push_back({ RomLabels::Script::ITEM_USE_ARTICLE_TABLE_SECTION, std::make_shared<ByteVector>(use_data) });
	}
	return true;
}

} // namespace Landstalker
