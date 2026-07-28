#ifndef _GAME_DATA_H_
#define _GAME_DATA_H_

#include <vector>
#include <memory>
#include <list>
#include <atomic>

#include <landstalker/main/DataManager.h>
#include <landstalker/main/RoomData.h>
#include <landstalker/main/GraphicsData.h>
#include <landstalker/main/StringData.h>
#include <landstalker/main/SpriteData.h>
#include <landstalker/main/ScriptData.h>
#include <landstalker/main/AudioData.h>
#include <landstalker/main/MusicData.h>
#include <landstalker/main/DataTypes.h>

namespace Landstalker {

class GameData : public DataManager
{
public:
    GameData();
    GameData(const std::filesystem::path& asm_file);
    GameData(const Rom& rom);

    virtual ~GameData() {}

    bool Open(const std::filesystem::path& asm_file);
    bool Open(const Rom& rom);

    virtual bool Save(const std::filesystem::path& dir);
    virtual bool Save();

    virtual PendingWrites GetPendingWrites() const;
    virtual bool WillFitInRom(const Rom& rom) const;
    virtual bool HasBeenModified() const;
    virtual bool InjectIntoRom(Rom& rom);
    virtual void RefreshPendingWrites(const Rom& rom);

    std::shared_ptr<RoomData> GetRoomData() const { return m_ready ? m_rd : nullptr; }
    std::shared_ptr<GraphicsData> GetGraphicsData() const { return m_ready ? m_gd : nullptr; }
    std::shared_ptr<StringData> GetStringData() const { return m_ready ? m_sd : nullptr; }
    std::shared_ptr<SpriteData> GetSpriteData() const { return m_ready ? m_spd : nullptr; }
    std::shared_ptr<ScriptData> GetScriptData() const { return m_ready ? m_scd : nullptr; }
    std::shared_ptr<AudioData> GetAudioData() const { return m_ready ? m_ad : nullptr; }
    std::shared_ptr<MusicData> GetMusicData() const { return m_ready ? m_md : nullptr; }

    // Appends a room and extends the room-indexed tables owned by the other data
    // managers to match. Use this rather than RoomData::AddRoom directly. Returns
    // nullptr without touching anything if the room could not be created.
    std::shared_ptr<Room> AddRoom(const std::string& map, const std::string& name,
        const std::wstring& display_name, uint8_t tileset, uint8_t room_palette,
        uint8_t pri_blockset, uint8_t sec_blockset, uint8_t room_z_begin,
        uint8_t room_z_end, uint8_t bgm);

    // Moves a room to a new position in the room list, renumbering every reference to it
    // across all the data managers - warps, chests, doors, tile swaps, entity and flag
    // tables, visit flags, shops, display-name labels and the rooms.inc constants. Rooms
    // between the old and new positions shift along by one and are renumbered too.
    //
    // NOTE: the disassembly hardcodes room indices as equ constants in rooms.inc, and
    // this rewrites those to follow the move. Anything *outside* the project that names a
    // raw room number - hand-written asm, a patch, an external tool - is not covered and
    // will need updating by hand.
    bool MoveRoom(uint16_t room, uint16_t new_index);

    // Removes a room and pulls every room above it down by one, renumbering all their
    // references. Anything belonging to or pointing at the deleted room is destroyed:
    // its entities, chests, doors, tile swaps and per-room flags, any warp, fall, climb
    // or transition with it at either end, its shops, dialogue, visit flag, and save and
    // map locations. A rooms.inc constant naming it is reset to 0 rather than removed,
    // since game code refers to those by name and dropping the symbol would break the
    // build. This is not undoable - see IsRoomReferenced / CountRoomReferences to warn
    // the user first.
    //
    // Refuses to delete the last remaining room. The same caveat as MoveRoom applies:
    // raw room numbers outside the project are not covered.
    bool DeleteRoom(uint16_t room);

    // True if anything anywhere in the project points at this room. Short-circuits on the
    // first hit, so it is cheap enough to call per row while populating a list; use
    // CountRoomReferences when you need the breakdown.
    bool IsRoomReferenced(uint16_t room) const;

    // What a DeleteRoom would destroy, for a confirmation prompt. Counts references to
    // the room held elsewhere; it does not count the room's own map or name.
    struct RoomReferences
    {
        std::size_t warps = 0;
        std::size_t fall_climb_routes = 0;
        std::size_t transitions = 0;
        std::size_t entities = 0;
        std::size_t flags = 0;
        std::size_t chests = 0;
        std::size_t shops = 0;
        std::size_t constants = 0;
        std::size_t Total() const;
    };
    RoomReferences CountRoomReferences(uint16_t room) const;

    // The lookups below are served from caches built when the project is opened, merging
    // entries owned by several managers. Anything that adds, renames or removes a palette,
    // tileset, animated tileset or tilemap has to call this afterwards, or the caches will
    // still describe the project as it was loaded - a new entry would not be found at all,
    // and a renamed one would answer to its old name.
    void RefreshCaches();

    const std::map<std::string, std::shared_ptr<PaletteEntry>>& GetAllPalettes() const;
    const std::map<std::string, std::shared_ptr<TilesetEntry>>& GetAllTilesets() const;
    const std::map<std::string, std::shared_ptr<AnimatedTilesetEntry>>& GetAllAnimatedTilesets() const;
    const std::map<std::string, std::shared_ptr<Tilemap2DEntry>>& GetAllTilemaps() const;

    std::shared_ptr<PaletteEntry> GetPalette(const std::string& name) const;
    std::shared_ptr<TilesetEntry> GetTileset(const std::string& name) const;
    std::shared_ptr<AnimatedTilesetEntry> GetAnimatedTileset(const std::string& name) const;
    std::shared_ptr<Tilemap2DEntry> GetTilemap(const std::string& name) const;

private:
    void CacheData();
    void SetDefaults();

    std::shared_ptr<RoomData> m_rd;
    std::shared_ptr<GraphicsData> m_gd;
    std::shared_ptr<StringData> m_sd;
    std::shared_ptr<SpriteData> m_spd;
    std::shared_ptr<ScriptData> m_scd;
    std::shared_ptr<AudioData> m_ad;
    std::shared_ptr<MusicData> m_md;

    std::vector<std::shared_ptr<DataManager>> m_data;

    std::map<std::string, std::shared_ptr<PaletteEntry>> m_palettes;
    std::map<std::string, std::shared_ptr<TilesetEntry>> m_tilesets;
    std::map<std::string, std::shared_ptr<AnimatedTilesetEntry>> m_anim_tilesets;
    std::map<std::string, std::shared_ptr<Tilemap2DEntry>> m_tilemaps;
};

} // namespace Landstalker

#endif // _GAME_DATA_H_
