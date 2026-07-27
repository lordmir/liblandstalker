#ifndef _ROOM_DATA_H_
#define _ROOM_DATA_H_

#include <map>
#include <optional>
#include <vector>
#include <memory>

#include <landstalker/main/DataManager.h>
#include <landstalker/main/DataTypes.h>
#include <landstalker/rooms/Room.h>
#include <landstalker/rooms/WarpList.h>
#include <landstalker/rooms/Chests.h>
#include <landstalker/3d_maps/Doors.h>
#include <landstalker/3d_maps/TileSwaps.h>
#include <landstalker/rooms/Flags.h>
#include <landstalker/rooms/Room.h>

namespace Landstalker {

class GameData;

class RoomData : public DataManager
{
public:
    enum class MiscPaletteType
    {
        LAVA,
        WARP,
        LANTERN
    };

    RoomData(const std::filesystem::path& asm_file);
    RoomData(const Rom& rom);

    virtual ~RoomData() {}

    virtual bool Save(const std::filesystem::path& dir);
    virtual bool Save();

	virtual bool HasBeenModified() const;
    virtual void RefreshPendingWrites(const Rom& rom);

    std::wstring GetTilesetDisplayName(uint8_t index) const;
    std::wstring GetAnimatedTilesetDisplayName(uint8_t tileset, uint8_t index) const;
    std::wstring GetRoomPaletteDisplayName(uint8_t index) const;
    std::wstring GetBlocksetDisplayName(uint8_t tileset, uint8_t pri, uint8_t sec) const;
    std::wstring GetRoomDisplayName(uint16_t room) const;
    std::wstring GetMapDisplayName(const std::string& map) const;

    std::vector<std::shared_ptr<TilesetEntry>> GetTilesets() const;
    std::vector<std::shared_ptr<AnimatedTilesetEntry>> GetAnimatedTilesets(const std::string& tileset) const;
    bool HasAnimatedTilesets(const std::string& tileset) const;
    std::shared_ptr<TilesetEntry> GetTileset(uint8_t index) const;
    std::shared_ptr<TilesetEntry> GetTileset(const std::string& name) const;
    std::map<std::string, std::shared_ptr<TilesetEntry>> GetAllTilesets() const;
    // Both return nullptr when there is no such animation, so they double as existence checks.
    std::shared_ptr<AnimatedTilesetEntry> GetAnimatedTileset(uint8_t tileset, uint8_t idx) const;
    std::shared_ptr<AnimatedTilesetEntry> GetAnimatedTileset(const std::string& name) const;
    std::map<std::string, std::shared_ptr<AnimatedTilesetEntry>> GetAllAnimatedTilesets() const;
    std::shared_ptr<TilesetEntry> GetIntroFont() const;

    // The tileset index is five bits wide. It is packed into the blockset primary key as
    // (primary << 5 | tileset) - see GetBlockset - and both of the pointer tables the game
    // indexes directly are sized to match: 32 tileset slots and 64 blockset primaries.
    // Slots past the last real tileset are padded with a dummy pointer on save.
    static constexpr std::size_t MAX_TILESETS = 32;
    // A tileset's graphics are DMAd into VRAM, which holds 2048 four-bit tiles in total.
    static constexpr std::size_t MAX_TILESET_TILES = 2048;
    // LoadAnimTiles fills two animation slots per room, so a third entry for the same
    // tileset would never be read - see code/maps/animtilesets.asm in the disassembly.
    static constexpr std::size_t MAX_ANIMS_PER_TILESET = 2;

    // Tileset, animated tileset and blockset names all become asm labels in one global
    // namespace, so a name is only usable if no asset of any of those kinds has it.
    static bool IsValidTilesetName(const std::string& name);
    bool IsAssetNameInUse(const std::string& name) const;
    // Appends a tileset in the next free slot, sized to tile_count blank tiles, together
    // with the primary and secondary blockset a room needs to be able to use it (see
    // GetBlocksetsForRoom). Returns nullptr if the name is unusable, the tile count is out
    // of range, or all MAX_TILESETS slots are taken.
    std::shared_ptr<TilesetEntry> AddTileset(const std::string& name, std::size_t tile_count);
    bool RenameTileset(const std::string& old_name, const std::string& new_name);
    // Exchanges the whole ecosystem of two tileset slots - the tileset graphics, its animated
    // tilesets, its blocksets and every display label - while leaving room references untouched.
    // A room keeps its tileset/pri/sec fields, so the two tilesets (with their blocksets) simply
    // swap places in every room that used them. This "move by content" is deliberately unlike
    // ReorderTileset, which renumbers references so no room changes; the asset managers use it so
    // moving a tileset is a self-contained edit that never rewrites room data.
    bool SwapTilesets(uint8_t a, uint8_t b);
    // What a DeleteTileset would destroy or refuse over. Rooms are the blocking count -
    // a room cannot be left pointing at a slot that no longer exists - while the
    // blocksets and animated tilesets belonging to the tileset are removed with it.
    struct TilesetReferences
    {
        std::size_t rooms = 0;
        std::size_t blocksets = 0;
        std::size_t animated_tilesets = 0;
        std::size_t Total() const;
    };
    TilesetReferences CountTilesetReferences(uint8_t index) const;
    bool IsTilesetUsedByRooms(uint8_t index) const;
    // Removes a tileset along with its blocksets and animated tilesets, pulling every
    // higher slot down by one and renumbering their references. Refuses while any room
    // still uses it, and refuses to remove the last remaining tileset. Not undoable.
    bool DeleteTileset(uint8_t index);

    // Adds an animation to a tileset. base is the VRAM address the frame is DMAd to and
    // frame_size_bytes the length of one frame, both as the game stores them; speed is the
    // delay in frames between steps. Returns nullptr if the tileset already has
    // MAX_ANIMS_PER_TILESET animations, or the name is unusable.
    std::shared_ptr<AnimatedTilesetEntry> AddAnimatedTileset(uint8_t tileset, const std::string& name,
        uint16_t base, uint16_t frame_size_bytes, uint8_t speed, uint8_t frames);
    bool RenameAnimatedTileset(const std::string& old_name, const std::string& new_name);
    bool DeleteAnimatedTileset(uint8_t tileset, uint8_t index);

    std::shared_ptr<PaletteEntry> GetRoomPalette(const std::string& name) const;
    std::shared_ptr<PaletteEntry> GetRoomPalette(uint8_t index) const;
    const std::vector<std::shared_ptr<PaletteEntry>>& GetRoomPalettes() const;

    // Room palettes are a flat list the game indexes by a room's room_palette byte, so at most
    // 256 fit. These edit the list the way the string editor edits strings.
    static constexpr std::size_t MAX_ROOM_PALETTES = 256;
    // Rooms drawn with this palette, so a delete can refuse and name them.
    bool IsRoomPaletteUsed(uint8_t index) const;
    std::vector<uint16_t> GetRoomsUsingRoomPalette(uint8_t index) const;
    // Appends a new blank room palette at the end. Nothing references the new slot yet, so this
    // is always safe. Returns its index, or nullopt if the list is full.
    std::optional<uint8_t> AddRoomPalette();
    // Removes a room palette and pulls the higher slots down, renumbering the room_palette of
    // every room that pointed above it. Refuses while any room draws the palette itself.
    bool DeleteRoomPalette(uint8_t index);
    // Exchanges the colours (and display labels) of two palettes in place, leaving every room's
    // room_palette byte alone: the two palettes swap places in the rooms that use them, matching
    // the content-swap the asset managers use.
    bool SwapRoomPalettes(uint8_t a, uint8_t b);
    std::vector<std::shared_ptr<PaletteEntry>> GetMiscPalette(const MiscPaletteType& type) const;
    std::map<std::string, std::shared_ptr<PaletteEntry>> GetAllPalettes() const;
    std::shared_ptr<PaletteEntry> GetDefaultTilesetPalette(const std::string& name) const;
    std::shared_ptr<PaletteEntry> GetDefaultTilesetPalette(uint8_t index) const;
    std::list<std::shared_ptr<PaletteEntry>> GetTilesetRecommendedPalettes(const std::string& name) const;
    std::list<std::shared_ptr<PaletteEntry>> GetTilesetRecommendedPalettes(uint8_t index) const;

    std::vector<std::shared_ptr<BlocksetEntry>> GetBlocksetList(const std::string& tileset) const;
    std::map<std::string, std::shared_ptr<BlocksetEntry>> GetAllBlocksets() const;
    std::shared_ptr<BlocksetEntry> GetBlockset(const std::string& name) const;
    std::shared_ptr<BlocksetEntry> GetBlockset(uint8_t pri, uint8_t sec) const;
    std::shared_ptr<BlocksetEntry> GetBlockset(uint8_t tileset, uint8_t pri, uint8_t sec) const;
    std::shared_ptr<BlocksetEntry> GetBlockset(const std::string& tileset, uint8_t pri, uint8_t sec) const;

    // Blocksets are grouped by (tileset, primary set). Within a group, entry 0 is the base
    // that every room in the group draws, and entries 1 upwards are the alternates a room
    // chooses between with its sec_blockset field - see GetBlocksetsForRoom, which
    // concatenates the base with the chosen alternate.
    //
    // Room::sec_blockset is three bits, so a group holds one base plus eight alternates.
    static constexpr std::size_t MAX_BLOCKSETS_PER_GROUP = 9;
    // Room::pri_blockset is a single bit, so a tileset has at most two groups.
    static constexpr std::size_t MAX_PRIMARY_SETS = 2;
    // Tilemap3D masks its block values to ten bits, so a room can address no more than this
    // many blocks in total across its base and alternate.
    static constexpr std::size_t MAX_COMBINED_BLOCKS = 0x400;

    bool HasBlocksetGroup(uint8_t tileset, uint8_t primary) const;
    // The group's entries in slot order, base first. Empty when the group does not exist.
    std::vector<std::shared_ptr<BlocksetEntry>> GetBlocksetGroup(uint8_t tileset, uint8_t primary) const;
    // Creates a group by adding its base blockset. A tileset needs a group before any room
    // can draw with it, and a second group is what lets rooms pick a different base through
    // pri_blockset. Returns nullptr if the group already exists or the name is unusable.
    std::shared_ptr<BlocksetEntry> AddBlocksetGroup(uint8_t tileset, uint8_t primary,
        const std::string& name, std::size_t block_count);
    // Removes a group and every blockset in it. Refuses while any room selects that group.
    bool DeleteBlocksetGroup(uint8_t tileset, uint8_t primary);
    // Appends an alternate to an existing group. Returns nullptr if the group is full, does
    // not exist, or the name is unusable.
    std::shared_ptr<BlocksetEntry> AddBlockset(uint8_t tileset, uint8_t primary,
        const std::string& name, std::size_t block_count);
    bool RenameBlockset(const std::string& old_name, const std::string& new_name);
    // Exchanges the content of two alternates within a group while leaving room references
    // untouched: a room keeps its sec_blockset, so the two alternates swap places in the rooms
    // that select them. The base (sec 0) is not one of the choices and cannot be swapped. This
    // "move by content" is deliberately unlike a reference-renumbering reorder.
    bool SwapBlockset(uint8_t tileset, uint8_t primary, uint8_t sec_a, uint8_t sec_b);
    // Removes an alternate and pulls the higher slots down, renumbering the rooms that
    // select them. Refuses while any room selects this slot, and refuses on the base -
    // use DeleteBlocksetGroup for that. Not undoable.
    bool DeleteBlockset(uint8_t tileset, uint8_t primary, uint8_t sec);
    // Rooms that would be affected by removing this blockset. For the base that is every
    // room in the group; for an alternate, the rooms whose sec_blockset selects it.
    std::vector<uint16_t> GetRoomsUsingBlockset(uint8_t tileset, uint8_t primary, uint8_t sec) const;

    // The warp table packs the room number into 10 bits (WarpList::Warp::GetRaw masks
    // the high byte with 0x03), so 1024 is the hard ceiling regardless of how much
    // space the other tables have. The room dialogue table allows 11 bits, but a room
    // that no warp can reference is not much use.
    static constexpr std::size_t MAX_ROOMS = 0x400;

    const std::vector<std::shared_ptr<Room>>& GetRoomlist() const;
    std::size_t GetRoomCount() const;
    std::shared_ptr<Room> GetRoom(uint16_t index) const;
    // nullptr if no room carries that internal name.
    std::shared_ptr<Room> GetRoom(const std::string& name) const;
    static bool IsValidRoomName(const std::string& name);
    bool RenameRoom(uint16_t index, const std::string& name);
    // Appends a room to the end of the room list. Rooms can only be added at the end,
    // and existing rooms must never be moved or removed: every other room-indexed table
    // in the game (chests, doors, tile swaps, entities, visit flags, script and warp
    // references) keys off the room number, and the disassembly hardcodes ~60 room
    // indices as equ constants in code/include/constants/rooms.inc, which the editor
    // does not parse. Inserting or reordering would silently repoint all of those.
    // Prefer GameData::AddRoom, which also extends the tables owned by the other data
    // managers. Returns nullptr if any argument is out of range or the name is taken.
    std::shared_ptr<Room> AddRoom(const std::string& map, const std::string& name,
        const std::wstring& display_name, uint8_t tileset, uint8_t room_palette,
        uint8_t pri_blockset, uint8_t sec_blockset, uint8_t room_z_begin,
        uint8_t room_z_end, uint8_t bgm);
    // Renumbers every room reference this manager owns. Only rewrites what RoomData
    // stores - the entity, flag, visit-flag and shop tables live in the other managers,
    // so a reorder has to go through GameData::MoveRoom to stay consistent.
    void RemapRooms(const RoomIndexMap& mapping);
    // Room numbers are dense by contract: slots 0..N-1 are all occupied, and each room's
    // stored index equals its position. The game indexes the room table directly, so a
    // gap would be read as a real room. Rooms can therefore only be appended or moved,
    // never inserted at a number that does not exist yet or left unnumbered.
    bool IsRoomListSequential() const;
    // Room index constants from the disassembly's rooms.inc (ROOM_MERCATOR_CENTRE and
    // friends), which game code references by name. They are plain `equ` defines, so
    // they are not renumbered when the room list changes - appending is safe, but
    // inserting or reordering rooms would leave every one of these pointing at the
    // wrong room. Empty for ROM-loaded projects, which have no include file.
    const std::map<std::string, uint16_t>& GetRoomConstants() const;
    std::optional<uint16_t> GetRoomConstant(const std::string& name) const;
    std::vector<std::string> GetRoomConstantsForRoom(uint16_t room) const;
    static bool IsValidRoomConstantName(const std::string& name);
    // Adds the constant, or repoints it if the name is already in use.
    bool SetRoomConstant(const std::string& name, uint16_t room);
    bool RenameRoomConstant(const std::string& old_name, const std::string& new_name);
    bool DeleteRoomConstant(const std::string& name);

    const std::map<std::string, std::shared_ptr<Tilemap3DEntry>>& GetMaps() const;
    const std::vector<std::string>& GetMapOrder() const;
    std::shared_ptr<Tilemap3DEntry> GetMap(const std::string& name) const;
    static bool IsValidMapName(const std::string& name);
    bool RenameMap(const std::string& old_name, const std::string& new_name);
    bool ReorderMap(const std::string& name, std::size_t new_index);
    std::shared_ptr<Tilemap3DEntry> CreateMap(const std::string& name,
        uint8_t width, uint8_t height, uint8_t heightmap_width, uint8_t heightmap_height,
        uint8_t heightmap_left, uint8_t heightmap_top);
    bool IsMapReferenced(const std::string& name) const;
    bool DeleteMap(const std::string& name);
    std::shared_ptr<PaletteEntry> GetPaletteForRoom(const std::string& name) const;
    std::shared_ptr<PaletteEntry> GetPaletteForRoom(uint16_t roomnum) const;
    std::shared_ptr<TilesetEntry> GetTilesetForRoom(const std::string& name) const;
    std::shared_ptr<TilesetEntry> GetTilesetForRoom(uint16_t roomnum) const;
    std::list<std::shared_ptr<BlocksetEntry>> GetBlocksetsForRoom(const std::string& name) const;
    std::list<std::shared_ptr<BlocksetEntry>> GetBlocksetsForRoom(uint16_t roomnum) const;
    std::shared_ptr<Blockset> GetCombinedBlocksetForRoom(const std::string& name) const;
    std::shared_ptr<Blockset> GetCombinedBlocksetForRoom(uint16_t roomnum) const;
    std::shared_ptr<Tilemap3DEntry> GetMapForRoom(const std::string& name) const;
    std::shared_ptr<Tilemap3DEntry> GetMapForRoom(uint16_t roomnum) const;
    std::vector<uint8_t> GetChestsForRoom(uint16_t roomnum) const;
    void SetChestsForRoom(uint16_t roomnum, const std::vector<uint8_t>& chests);
    uint8_t GetChestContentsFromFlag(int flag);
    int GetChestFlagBaseForRoom(uint16_t roomnum) const;
    bool GetNoChestFlagForRoom(uint16_t roomnum) const;
    void SetNoChestFlagForRoom(uint16_t roomnum, bool flag);
    bool CleanupChests(const GameData& g);

    std::vector<WarpList::Warp> GetWarpsForRoom(uint16_t roomnum);
    void SetWarpsForRoom(uint16_t roomnum, const std::vector<WarpList::Warp>& warps);
    bool HasFallDestination(uint16_t room) const;
    uint16_t GetFallDestination(uint16_t room) const;
    void SetHasFallDestination(uint16_t room, bool enabled);
    void SetFallDestination(uint16_t room, uint16_t dest);
    bool HasClimbDestination(uint16_t room) const;
    uint16_t GetClimbDestination(uint16_t room) const;
    void SetHasClimbDestination(uint16_t room, bool enabled);
    void SetClimbDestination(uint16_t room, uint16_t dest);
    std::vector<WarpList::Transition> GetTransitions(uint16_t room) const;
    std::vector<WarpList::Transition> GetSrcTransitions(uint16_t room) const;
    void SetTransitions(uint16_t room, const std::vector<WarpList::Transition>& data);
    void SetSrcTransitions(uint16_t room, const std::vector<WarpList::Transition>& data);

    bool IsShop(uint16_t room) const;
    void SetShop(uint16_t room, bool is_shop);
    bool IsTree(uint16_t room) const;
    void SetTree(uint16_t room, bool is_tree);
    bool HasLifestockSaleFlag(uint16_t room) const;
    uint16_t GetLifestockSaleFlag(uint16_t room) const;
    void SetLifestockSaleFlag(uint16_t room, uint16_t flag);
    void ClearLifestockSaleFlag(uint16_t room);
    bool HasLanternFlag(uint16_t room) const;
    uint16_t GetLanternFlag(uint16_t room) const;
    void SetLanternFlag(uint16_t room, uint16_t flag);
    void ClearLanternFlag(uint16_t room);
    bool HasTreeWarpFlag(uint16_t room) const;
    TreeWarpFlag GetTreeWarp(uint16_t room) const;
    void SetTreeWarp(const TreeWarpFlag& flag);
    void ClearTreeWarp(uint16_t room);

    bool HasNormalTileSwaps(uint16_t room) const;
    std::vector<TileSwapFlag> GetNormalTileSwaps(uint16_t room) const;
    void SetNormalTileSwaps(uint16_t room, const std::vector<TileSwapFlag>& swaps);
    bool HasLockedDoorTileSwaps(uint16_t room) const;
    std::vector<TileSwapFlag> GetLockedDoorTileSwaps(uint16_t room) const;
    void SetLockedDoorTileSwaps(uint16_t room, const std::vector<TileSwapFlag>& swaps);
    bool HasTileSwaps(uint16_t room) const;
    std::vector<TileSwap> GetTileSwaps(uint16_t room) const;
    void SetTileSwaps(uint16_t room, const std::vector<TileSwap>& swaps);
    bool HasDoors(uint16_t room) const;
    std::vector<Door> GetDoors(uint16_t room) const;
    void SetDoors(uint16_t room, const std::vector<Door>& swaps);


protected:
    virtual void CommitAllChanges();
private:
    bool LoadAsmFilenames();
    void SetDefaultFilenames();
    bool CreateDirectoryStructure(const std::filesystem::path& dir);

    bool AsmLoadRoomTable();
    bool AsmLoadRoomConstants();
    bool AsmLoadMaps();
    bool AsmLoadRoomPalettes();
    bool AsmLoadWarpData();
    bool AsmLoadMiscPaletteData();
    bool AsmLoadBlocksetData();
    bool AsmLoadBlocksetPtrData();
    bool AsmLoadAnimatedTilesetData();
    bool AsmLoadTilesetData();
    bool AsmLoadChestData();
    bool AsmLoadDoorData();
    bool AsmLoadGfxSwapData();
    bool AsmLoadMiscData();

    bool RomLoadRoomData(const Rom& rom);
    bool RomLoadRoomPalettes(const Rom& rom);
    bool RomLoadWarpData(const Rom& rom);
    bool RomLoadMiscPaletteData(const Rom& rom);
    bool RomLoadBlocksetData(const Rom& rom);
    bool RomLoadBlockset(const Rom& rom, uint8_t pri, uint8_t sec, uint32_t begin, uint32_t end);
    bool RomLoadAllTilesetData(const Rom& rom);
    bool RomLoadChestData(const Rom& rom);
    bool RomLoadDoorData(const Rom& rom);
    bool RomLoadGfxSwapData(const Rom& rom);
    bool RomLoadMiscData(const Rom& rom);

    bool AsmSaveMaps(const std::filesystem::path& dir);
    bool AsmSaveRoomData(const std::filesystem::path& dir);
    bool AsmSaveRoomConstants(const std::filesystem::path& dir);
    bool AsmSaveWarpData(const std::filesystem::path& dir);
    bool AsmSaveRoomPalettes(const std::filesystem::path& dir);
    bool AsmSaveMiscPaletteData(const std::filesystem::path& dir);
    bool AsmSaveBlocksetPointerData(const std::filesystem::path& dir);
    bool AsmSaveBlocksetData(const std::filesystem::path& dir);
    bool AsmSaveTilesetData(const std::filesystem::path& dir);
    bool AsmSaveTilesetPointerData(const std::filesystem::path& dir);
    bool AsmSaveAnimatedTilesetData(const std::filesystem::path& dir);
    bool AsmSaveChestData(const std::filesystem::path& dir);
    bool AsmSaveDoorData(const std::filesystem::path& dir);
    bool AsmSaveGfxSwapData(const std::filesystem::path& dir);
    bool AsmSaveMiscData(const std::filesystem::path& dir);

    bool RomPrepareInjectMiscWarp(const Rom& rom);
    bool RomPrepareInjectRoomData(const Rom& rom);
    bool RomPrepareInjectMiscPaletteData(const Rom& rom);
    bool RomPrepareInjectBlocksetData(const Rom& rom);
    bool RomPrepareInjectTilesetData(const Rom& rom);
    bool RomPrepareInjectAnimatedTilesetData(const Rom& rom);
    bool RomPrepareInjectChestData(const Rom& rom);
    bool RomPrepareInjectDoorData(const Rom& rom);
    bool RomPrepareInjectGfxSwapData(const Rom& rom);
    bool RomPrepareInjectMiscData(const Rom& rom);

    void UpdateTilesetRecommendedPalettes();
    void ResetTilesetDefaultPalettes();

    // Rewrites every tileset slot number in the project to follow the given old -> new
    // mapping, which must cover each affected slot exactly once. A slot mapped to -1 is
    // being deleted and its dependants must already have been removed. Shared by
    // SwapTilesets and DeleteTileset, which differ only in the mapping they build.
    // remap_room_tilesets false leaves each room's tileset field alone, so the content moves
    // between slots but the references do not follow it - the content-swap SwapTilesets relies
    // on this, where preserving room references is the whole point.
    void RemapTilesets(const std::map<uint8_t, int>& mapping, bool remap_room_tilesets = true);
    // Rewrites the slot numbers within one blockset group to follow the given old -> new
    // mapping, and renumbers the sec_blockset of every room selecting an affected slot. A
    // slot mapped to -1 is being deleted. Shared by SwapBlockset and DeleteBlockset.
    // remap_room_blocksets false leaves each room's sec_blockset alone, so the content moves
    // between slots but the references do not follow it - SwapBlockset relies on this.
    void RemapBlocksets(uint8_t tileset, uint8_t primary, const std::map<uint8_t, int>& mapping,
        bool remap_room_blocksets = true);
    // Blank blockset bytes in the stored (compressed) form, or empty if they will not encode.
    ByteVector MakeBlankBlockset(std::size_t block_count) const;
    // Picks an unused file path for a new asset, starting from the directory and
    // extension its siblings use and falling back to the stock layout when there are none.
    std::filesystem::path AllocateAssetFilename(const std::string& stem,
        const std::filesystem::path& sibling, const std::string& fallback_format) const;

    std::filesystem::path m_room_data_filename;
    std::filesystem::path m_room_constants_filename;
    std::filesystem::path m_map_data_filename;
    std::filesystem::path m_warp_data_filename;
    std::filesystem::path m_fall_data_filename;
    std::filesystem::path m_climb_data_filename;
    std::filesystem::path m_transition_data_filename;
    std::filesystem::path m_palette_data_filename;
    std::filesystem::path m_lava_pal_data_filename;
    std::filesystem::path m_warp_pal_data_filename;
    std::filesystem::path m_lantern_pal_data_filename;
    std::filesystem::path m_tileset_data_filename;
    std::filesystem::path m_tileset_ptrtab_filename;
    std::filesystem::path m_tileset_anim_filename;
    std::filesystem::path m_blockset_pri_ptr_filename;
    std::filesystem::path m_blockset_sec_ptr_filename;
    std::filesystem::path m_blockset_data_filename;
    std::filesystem::path m_chest_offset_data_filename;
    std::filesystem::path m_chest_data_filename;
    std::filesystem::path m_door_offset_data_filename;
    std::filesystem::path m_door_table_data_filename;
    std::filesystem::path m_gfxswap_flag_data_filename;
    std::filesystem::path m_gfxswap_locked_door_flag_data_filename;
    std::filesystem::path m_gfxswap_big_tree_flag_data_filename;
    std::filesystem::path m_gfxswap_table_data_filename;
    std::filesystem::path m_shop_table_data_filename;
    std::filesystem::path m_lifestock_sold_flag_data_filename;
    std::filesystem::path m_bigtree_data_filename;
    std::filesystem::path m_lantern_flag_data_filename;

    std::map<std::string, std::shared_ptr<TilesetEntry>> m_tilesets_by_name;
    std::map<std::string, std::shared_ptr<TilesetEntry>> m_tilesets_by_name_orig;
    std::map<uint8_t, std::shared_ptr<TilesetEntry>> m_tilesets;
    std::map<uint8_t, std::shared_ptr<TilesetEntry>> m_tilesets_orig;

    std::map<std::string, std::shared_ptr<AnimatedTilesetEntry>> m_animated_ts_by_name;
    std::map<std::string, std::shared_ptr<AnimatedTilesetEntry>> m_animated_ts_by_name_orig;
    std::map<std::string, std::shared_ptr<AnimatedTilesetEntry>> m_animated_ts_by_ptr;
    std::map<std::string, std::shared_ptr<AnimatedTilesetEntry>> m_animated_ts_by_ptr_orig;
    std::map<std::pair<uint8_t, uint8_t>, std::shared_ptr<AnimatedTilesetEntry>> m_animated_ts;
    std::map<std::pair<uint8_t, uint8_t>, std::shared_ptr<AnimatedTilesetEntry>> m_animated_ts_orig;

    std::unordered_map<std::string, std::shared_ptr<PaletteEntry>> m_tileset_defaultpal;
    std::unordered_map<std::string, std::list<std::shared_ptr<PaletteEntry>>> m_tileset_pals;

    std::shared_ptr<TilesetEntry> m_intro_font;

    std::map<std::string, std::shared_ptr<BlocksetEntry>> m_blocksets_by_name;
    std::map<std::string, std::shared_ptr<BlocksetEntry>> m_blocksets_by_name_orig;
    std::map<std::pair<uint8_t, uint8_t>, std::shared_ptr<BlocksetEntry>> m_blocksets;
    std::map<std::pair<uint8_t, uint8_t>, std::shared_ptr<BlocksetEntry>> m_blocksets_orig;

    std::vector<std::shared_ptr<Room>> m_roomlist;
    std::vector<std::shared_ptr<Room>> m_roomlist_orig;
    std::map<std::string, std::shared_ptr<Room>> m_roomlist_by_name;

    std::map<std::string, uint16_t> m_room_constants;
    std::map<std::string, uint16_t> m_room_constants_orig;

    std::map<std::string, std::shared_ptr<Tilemap3DEntry>> m_maps;
    std::map<std::string, std::shared_ptr<Tilemap3DEntry>> m_maps_orig;
    std::vector<std::string> m_map_order;
    std::vector<std::string> m_map_order_orig;

    WarpList m_warps;
    WarpList m_warps_orig;

    std::map<std::string, std::shared_ptr<PaletteEntry>> m_room_pals_by_name;
    std::vector<std::shared_ptr<PaletteEntry>> m_room_pals;
    std::vector<std::shared_ptr<PaletteEntry>> m_room_pals_orig;
    std::vector<std::shared_ptr<PaletteEntry>> m_lava_palette;
    std::vector<std::shared_ptr<PaletteEntry>> m_lava_palette_orig;
    std::vector<std::shared_ptr<PaletteEntry>> m_warp_palette;
    std::vector<std::shared_ptr<PaletteEntry>> m_warp_palette_orig;
    std::shared_ptr<PaletteEntry> m_labrynth_lit_palette;

    Chests m_chests;
    Chests m_chests_orig;
    Doors m_doors;
    Doors m_doors_orig;
    TileSwaps m_gfxswaps;
    TileSwaps m_gfxswaps_orig;

    std::map<uint16_t, std::vector<TileSwapFlag>> m_gfxswap_flags;
    std::map<uint16_t, std::vector<TileSwapFlag>> m_gfxswap_flags_orig;
    std::map<uint16_t, std::vector<TileSwapFlag>> m_gfxswap_locked_door_flags;
    std::map<uint16_t, std::vector<TileSwapFlag>> m_gfxswap_locked_door_flags_orig;
    std::vector<TreeWarpFlag> m_gfxswap_big_tree_flags;
    std::vector<TreeWarpFlag> m_gfxswap_big_tree_flags_orig;
    std::set<uint16_t> m_shop_list;
    std::set<uint16_t> m_shop_list_orig;
    std::map<uint16_t, uint16_t> m_lifestock_sold_flags;
    std::map<uint16_t, uint16_t> m_lifestock_sold_flags_orig;
    std::set<uint16_t> m_big_tree_list;
    std::set<uint16_t> m_big_tree_list_orig;
    std::map<uint16_t, uint16_t> m_lantern_flag_list;
    std::map<uint16_t, uint16_t> m_lantern_flag_list_orig;
};

} // namespace Landstalker

#endif // _ROOM_DATA_H_
