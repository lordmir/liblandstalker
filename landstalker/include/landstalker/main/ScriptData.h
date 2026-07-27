#ifndef _SCRIPT_DATA_H_
#define _SCRIPT_DATA_H_

#include <landstalker/main/DataManager.h>
#include <landstalker/main/DataTypes.h>
#include <landstalker/script/Script.h>
#include <landstalker/script/ScriptFunctionTable.h>
#include <landstalker/script/AsmFunctionTable.h>
#include <landstalker/script/RoomActionTable.h>
#include <landstalker/script/ItemUseTable.h>
#include <landstalker/script/ScriptTable.h>
#include <landstalker/rooms/RoomIndexMap.h>

namespace Landstalker {

class ScriptData : public DataManager
{
public:

    ScriptData(const std::filesystem::path& asm_file);
    ScriptData(const Rom& rom);

    virtual ~ScriptData() {}

    virtual bool Save(const std::filesystem::path& dir);
    virtual bool Save();

    virtual bool HasBeenModified() const;
    virtual void RefreshPendingWrites(const Rom& rom);

    static std::wstring GetScriptEntryDisplayName(int script_id);
    static std::wstring GetFlagDisplayName(int script_id);
    static std::wstring GetCutsceneDisplayName(int script_id);

    uint16_t GetStringStart() const;

    std::shared_ptr<Script> GetScript();
    std::shared_ptr<const Script> GetScript() const;

    bool HasTables() const;
    std::shared_ptr<const std::vector<ScriptTable::Action>> GetCharTable() const;
    std::shared_ptr<std::vector<ScriptTable::Action>> GetCharTable();
    std::shared_ptr<const std::vector<ScriptTable::Action>> GetCutsceneTable() const;
    std::shared_ptr<std::vector<ScriptTable::Action>> GetCutsceneTable();
    std::shared_ptr<const std::vector<ScriptTable::Shop>> GetShopTable() const;
    std::shared_ptr<std::vector<ScriptTable::Shop>> GetShopTable();
    // The shop table names the room each shop sits in. Go through GameData::MoveRoom
    // rather than calling this directly.
    void RemapRooms(const RoomIndexMap& mapping);
    std::shared_ptr<const std::vector<ScriptTable::Item>> GetItemTable() const;
    std::shared_ptr<std::vector<ScriptTable::Item>> GetItemTable();

    std::shared_ptr<const ScriptFunctionTable> GetCharFuncs() const;
    std::shared_ptr<ScriptFunctionTable> GetCharFuncs();
    void SetCharFuncs(const ScriptFunctionTable& funcs);
    std::shared_ptr<const ScriptFunctionTable> GetCutsceneFuncs() const;
    std::shared_ptr<ScriptFunctionTable> GetCutsceneFuncs();
    void SetCutsceneFuncs(const ScriptFunctionTable& funcs);
    std::shared_ptr<const ScriptFunctionTable> GetShopFuncs() const;
    std::shared_ptr<ScriptFunctionTable> GetShopFuncs();
    void SetShopFuncs(const ScriptFunctionTable& funcs);
    std::shared_ptr<const ScriptFunctionTable> GetItemFuncs() const;
    std::shared_ptr<ScriptFunctionTable> GetItemFuncs();
    void SetItemFuncs(ScriptFunctionTable& funcs); 
    std::shared_ptr<const ScriptFunctionTable> GetProgressFlagsFuncs() const;
    std::shared_ptr<ScriptFunctionTable> GetProgressFlagsFuncs();
    void SetProgressFlagsFuncs(const ScriptFunctionTable& funcs);

    // The cutscene action code ("Cutscenes"): CSA_xxxx handlers + their dispatch table. Only
    // available on the ASM path (nullptr / invalid when loaded from ROM). See
    // [[cutscene-two-layer-architecture]].
    std::shared_ptr<const AsmFunctionTable> GetCutsceneActions() const;
    std::shared_ptr<AsmFunctionTable> GetCutsceneActions();
    void SetCutsceneActions(const AsmFunctionTable& actions);

    // The behaviour trigger action code ("Trigger Actions"): TA_xx handlers + their dispatch table,
    // dispatched by a behaviour WaitForCondition. Same structure/availability as the cutscene actions.
    std::shared_ptr<const AsmFunctionTable> GetTriggerActions() const;
    std::shared_ptr<AsmFunctionTable> GetTriggerActions();
    void SetTriggerActions(const AsmFunctionTable& actions);

    // The per-room fixup chain ("Room Actions"): customroomactions1/2.asm. Same availability as the
    // other action tables (nullptr / invalid on the ROM path or an older disassembly).
    std::shared_ptr<const RoomActionTable> GetRoomActions() const;
    std::shared_ptr<RoomActionTable> GetRoomActions();
    void SetRoomActions(const RoomActionTable& actions);

    // The item pre-use / post-use handlers + their two dispatch tables (itemuse1/2.asm,
    // itempostuse.asm). Same availability as the other action tables.
    std::shared_ptr<const ItemUseTable> GetItemUse() const;
    std::shared_ptr<ItemUseTable> GetItemUse();
    void SetItemUse(const ItemUseTable& table);

    bool HasItemArticles() const;
    uint8_t GetItemArticle(uint8_t item) const;
    void SetItemArticle(uint8_t item, uint8_t article);
protected:
    virtual void CommitAllChanges();
private:
    bool LoadAsmFilenames();
    void SetDefaultFilenames();
    bool CreateDirectoryStructure(const std::filesystem::path& dir);
    void InitCache();

    bool AsmLoadScript();
    bool AsmLoadScriptTables();
    bool AsmLoadScriptFunctions();
    bool AsmLoadCutsceneActions();
    bool AsmLoadTriggerActions();
    bool AsmLoadRoomActions();
    bool AsmLoadItemUse();
    bool AsmLoadItemArticles();

    bool RomLoadScript(const Rom& rom);
    bool RomLoadItemArticles(const Rom& rom);

    bool AsmSaveScript(const std::filesystem::path& dir);
    bool AsmSaveScriptTables(const std::filesystem::path& dir);
    bool AsmSaveScriptFunctions(const std::filesystem::path& dir);
    bool AsmSaveCutsceneActions(const std::filesystem::path& dir);
    bool AsmSaveTriggerActions(const std::filesystem::path& dir);
    bool AsmSaveRoomActions(const std::filesystem::path& dir);
    bool AsmSaveItemUse(const std::filesystem::path& dir);
    bool AsmSaveItemArticles(const std::filesystem::path& dir);

    bool RomPrepareInjectScript(const Rom& rom);
    bool RomPrepareInjectItemArticles(const Rom& rom);

    std::filesystem::path m_defines_filename;
    std::filesystem::path m_script_filename;
    std::filesystem::path m_cutscene_table_filename;
    std::filesystem::path m_char_table_filename;
    std::filesystem::path m_shop_table_filename;
    std::filesystem::path m_item_table_filename;

    std::filesystem::path m_char_funcs_filename;
    std::filesystem::path m_cutscene_funcs_filename;
    std::filesystem::path m_shop_funcs_filename;
    std::filesystem::path m_item_funcs_filename;
    std::filesystem::path m_flag_progress_filename;

    std::filesystem::path m_cutscene_actions_filename;
    std::filesystem::path m_cutscene_jumptable_filename;

    std::filesystem::path m_trigger_actions_filename;
    std::filesystem::path m_trigger_jumptable_filename;

    std::filesystem::path m_room_actions1_filename;
    std::filesystem::path m_room_actions2_filename;

    std::filesystem::path m_item_preuse_table_filename;
    std::filesystem::path m_item_postuse_table_filename;
    std::filesystem::path m_itemuse1_filename;
    std::filesystem::path m_itemuse2_filename;
    std::filesystem::path m_itempostuse_filename;

    std::filesystem::path m_item_articles_filename;
    std::filesystem::path m_item_found_article_table_filename;
    std::filesystem::path m_item_use_article_table_filename;

    std::map<std::string, std::string> m_defines;
    std::shared_ptr<Script> m_script;
    Script m_script_orig;
    uint16_t m_script_start;
    bool m_is_asm;

    std::shared_ptr<std::vector<ScriptTable::Action>> m_chartable;
    std::shared_ptr<std::vector<ScriptTable::Action>> m_chartable_orig;
    std::shared_ptr<std::vector<ScriptTable::Action>> m_cutscene_table;
    std::shared_ptr<std::vector<ScriptTable::Action>> m_cutscene_table_orig;
    std::shared_ptr<std::vector<ScriptTable::Shop>> m_shoptable;
    std::shared_ptr<std::vector<ScriptTable::Shop>> m_shoptable_orig;
    std::shared_ptr<std::vector<ScriptTable::Item>> m_itemtable;
    std::shared_ptr<std::vector<ScriptTable::Item>> m_itemtable_orig;

    std::shared_ptr<ScriptFunctionTable> m_charfuncs;
    std::shared_ptr<ScriptFunctionTable> m_charfuncs_orig;
    std::shared_ptr<ScriptFunctionTable> m_cutscenefuncs;
    std::shared_ptr<ScriptFunctionTable> m_cutscenefuncs_orig;
    std::shared_ptr<ScriptFunctionTable> m_shopfuncs;
    std::shared_ptr<ScriptFunctionTable> m_shopfuncs_orig;
    std::shared_ptr<ScriptFunctionTable> m_itemfuncs;
    std::shared_ptr<ScriptFunctionTable> m_itemfuncs_orig;
    std::shared_ptr<ScriptFunctionTable> m_flagprogress;
    std::shared_ptr<ScriptFunctionTable> m_flagprogress_orig;

    std::shared_ptr<AsmFunctionTable> m_cutscene_actions;
    std::shared_ptr<AsmFunctionTable> m_cutscene_actions_orig;

    std::shared_ptr<AsmFunctionTable> m_trigger_actions;
    std::shared_ptr<AsmFunctionTable> m_trigger_actions_orig;

    std::shared_ptr<RoomActionTable> m_room_actions;
    std::shared_ptr<RoomActionTable> m_room_actions_orig;

    std::shared_ptr<ItemUseTable> m_item_use;
    std::shared_ptr<ItemUseTable> m_item_use_orig;

    std::optional<std::vector<uint8_t>> m_itemarticles;
    std::optional<std::vector<uint8_t>> m_itemarticles_orig;
    std::optional<std::vector<uint16_t>> m_itemfoundarticles;
    std::optional<std::vector<uint16_t>> m_itemfoundarticles_orig;
    std::optional<std::vector<uint16_t>> m_itemusearticles;
    std::optional<std::vector<uint16_t>> m_itemusearticles_orig;
};

} // namespace Landstalker

#endif // _SCRIPT_DATA_H_