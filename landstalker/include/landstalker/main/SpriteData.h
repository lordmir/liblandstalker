#ifndef _SPRITE_DATA_H_
#define _SPRITE_DATA_H_

#include <set>
#include <cstdint>
#include <landstalker/main/DataManager.h>
#include <landstalker/main/DataTypes.h>
#include <landstalker/rooms/Entity.h>
#include <landstalker/rooms/Flags.h>
#include <landstalker/behaviours/Behaviours.h>
#include <landstalker/misc/Point.h>
#include <landstalker/main/StringData.h>
#include <landstalker/main/ImageBuffer.h>

namespace Landstalker {

class SpriteData : public DataManager
{
public:
	enum class FlagType
	{
		SPRITE_VISIBILITY
	};

	struct AnimationFlags
	{
		AnimationFlags();
		AnimationFlags(uint8_t packed);
		uint8_t Pack() const;
		bool IsDefault() const;
		bool operator==(const AnimationFlags& rhs) const;
		bool operator!=(const AnimationFlags& rhs) const;

		enum class IdleAnimationFrameCount
		{
			TWO_FRAMES = 0,
			ONE_FRAME = 1
		} idle_animation_frames;
		enum class IdleAnimationSource
		{
			USE_WALK_FRAMES = 0,
			DEDICATED = 1,
		} idle_animation_source;
		enum class JumpAnimationSource
		{
			USE_IDLE_FRAMES = 0,
			DEDICATED = 1,
		} jump_animation_source;
		enum class WalkAnimationFrameCount
		{
			FOUR_FRAMES = 0,
			TWO_FRAMES = 1,
		} walk_animation_frame_count;
		bool do_not_rotate;
		enum class TakeDamageAnimationSource
		{
			USE_IDLE_FRAMES = 0,
			DEDICATED = 1,
		} take_damage_animation_source;
		bool has_full_animations;
	};

	struct ItemProperties
	{
		uint8_t max_quantity;
		uint8_t verb;
		uint8_t equipment_index;
		uint16_t price;
		ItemProperties() : max_quantity(0), verb(12), equipment_index(0), price(0) {}
		ItemProperties(const std::array<uint8_t, 4>& elems);
		std::array<uint8_t, 4> Pack() const;
	};

	struct EnemyStats
	{
		uint8_t health;
		uint8_t defence;
		uint8_t attack;
		uint8_t gold_drop;
		uint8_t item_drop;
		enum class DropProbability : uint8_t
		{
			ONE_IN_64,
			ONE_IN_128,
			ONE_IN_256,
			ONE_IN_512,
			ONE_IN_1024,
			ONE_IN_2048,
			NO_DROP,
			GUARANTEED_DROP
		} drop_probability;
		static const std::unordered_map<DropProbability, std::string> DropProbabilityNames;
		EnemyStats() : health(0), defence(0), attack(0), gold_drop(0), item_drop(0), drop_probability(DropProbability::NO_DROP) {}
		EnemyStats(const std::array<uint8_t, 5>& elems);
		std::array<uint8_t, 5> Pack() const;
	};

	struct Hitbox
	{
		uint8_t base;
		uint8_t height;
		Hitbox() : base(0), height(0) {}
		Hitbox(uint8_t b, uint8_t h) : base(b), height(h) {}
	};

	struct SpriteMetadata
	{
		unsigned int frame_width;
		unsigned int frame_height;
		unsigned int frame_count;
		Point origin;
		AnimationFlags animation_flags;
		Hitbox hitbox;
		unsigned int volume;
		std::vector<int> compressed_frames;
		std::map<std::string, std::vector<int>> animations;
	};

	struct EntityMetadata
	{
		std::optional<std::string> low_palette = std::nullopt;
		std::optional<std::string> high_palette = std::nullopt;
		std::optional<uint8_t> talk_sfx = std::nullopt;
		std::optional<ItemProperties> item_properties = std::nullopt;
		std::optional<EnemyStats> enemy_stats = std::nullopt;
	};

	SpriteData(const std::filesystem::path& asm_file);
	SpriteData(const Rom& rom);

	virtual ~SpriteData();

	virtual bool Save(const std::filesystem::path& dir);
	virtual bool Save();

	virtual bool HasBeenModified() const;
	virtual void RefreshPendingWrites(const Rom& rom);

	static std::wstring GetEntityDisplayName(uint8_t id);
	static std::wstring GetSpriteDisplayName(uint8_t id);
	std::wstring GetSpriteAnimationDisplayName(uint8_t id, const std::string& name) const;
	std::wstring GetSpriteFrameDisplayName(uint8_t id, const std::string& name) const;
	// As GetSpriteFrameDisplayName, but annotated with the frame's role within a specific
	// animation, e.g. "<name> [Walk NE 2]". Frames past the animation's expected length, or
	// in an animation slot with no recognised role, are tagged "[Unused N]".
	std::wstring GetSpriteAnimationFrameDisplayName(uint8_t id, uint8_t anim_id, int frame_pos, const std::string& name) const;
	static std::wstring GetSpriteLowPaletteDisplayName(uint8_t id);
	static std::wstring GetSpriteHighPaletteDisplayName(uint8_t id);
	static std::wstring GetBehaviourDisplayName(int behav_id);
	SpriteMetadata GetSpriteMetadata(uint8_t id) const;
	std::string GetSpriteMetadataYaml(uint8_t id) const;

	// A single image holding every unique frame the sprite's animations reference, laid out in a
	// uniform grid. Every cell is the same size (the union bounding box of all frames) and each
	// frame is aligned so its origin lands on the shared origin, so cells line up when flipped
	// through animations. Cells follow the same frame order GetSpriteMetadataYaml() indexes, so the
	// YAML's per-animation frame indices map to sheet cells row-major.
	struct SpriteSheet
	{
		ImageBuffer image;    // palette index 0; supply the sprite palette when writing to PNG
		int columns = 0;
		int rows = 0;
		int cell_width = 0;
		int cell_height = 0;
		Point origin;         // position of the shared origin within every cell
		unsigned int frame_count = 0;
	};
	// columns <= 0 chooses a square-ish grid.
	SpriteSheet MakeSpriteSheet(uint8_t id, int columns = 0) const;
	EntityMetadata GetEntityMetadata(uint8_t id, std::shared_ptr<StringData> sd) const;
	std::string GetEntityMetadataYaml(uint8_t id, std::shared_ptr<StringData> sd) const;

	// The sprite graphics id is a byte, and the animation offset table the game indexes is
	// dense from 0, so sprites can be appended, moved or removed but never left with a gap.
	static constexpr std::size_t MAX_SPRITES = 256;

	// Sprite, animation and frame names all become assembly labels, so a name is only usable
	// if nothing of any of those kinds already carries it.
	static bool IsValidSpriteName(const std::string& name);
	bool IsSpriteNameInUse(const std::string& name) const;
	// Appends a sprite in the next free id, complete with the one animation, one frame and
	// one 1x1 subsprite a sprite needs to be drawable. Returns the new id, or nullopt if the
	// name is unusable or the id space is full.
	std::optional<uint8_t> AddSprite(const std::string& name);
	bool RenameSprite(uint8_t id, const std::string& new_name);
	// Exchanges the content of two sprite ids - each sprite's animations, frames, volume,
	// dimensions, flags and labels - while leaving the entity-to-sprite lookup untouched, so
	// the two sprites swap places in every entity that draws them. This "move by content" is
	// deliberately unlike a reference-renumbering reorder. Note that the disassembly's SpriteB_
	// constants name graphics ids by value; like any reorder, this changes which sprite an id
	// (and therefore such a constant) resolves to, and neither approach rewrites sprites.inc.
	bool SwapSprites(uint8_t a, uint8_t b);
	// A DeleteSprite refuses while any entity points at the sprite; GetEntitiesFromSprite
	// names them so a caller can warn first.
	bool IsSpriteUsedByEntities(uint8_t id) const;
	// Removes a sprite together with its animations and frames, pulling every higher id down
	// by one and renumbering their references. Refuses while an entity still points at it,
	// and refuses to remove the last remaining sprite. Not undoable.
	bool DeleteSprite(uint8_t id);
	// Recreates a sprite from the YAML produced by GetSpriteMetadataYaml plus the frame
	// binaries it names, appending it under new_name. frame_dir is where the .frm files sit.
	// Returns the new id, or nullopt on any failure (bad name, unreadable frames, full).
	std::optional<uint8_t> ImportSprite(const std::string& new_name, const std::string& yaml_data,
		const std::filesystem::path& frame_dir);

	// --- Entity management ---
	// An entity is a {type -> sprite graphics} entry the game resolves by a linear search, so
	// entity ids need be neither dense nor contiguous. Items occupy the fixed range 0xC0..0xFE
	// (GetSpriteFromEntity maps them all to the item-box sprite) and cannot be added, moved or
	// deleted here - the 0xC0 boundary is the only thing that makes an entity an item.
	static constexpr uint8_t FIRST_ITEM_ENTITY = 0xC0;
	std::size_t GetEntityCount() const;
	// The entity ids in use, in ascending order.
	std::vector<uint8_t> GetEntityIds() const;
	// Lowest free non-item id, or nullopt when 0x00..0xBF are all taken.
	std::optional<uint8_t> GetFreeEntityId() const;
	// Appends a non-item entity at the lowest free id, pointing at sprite_id with the given
	// palette indices (-1 for none). Returns the new id, or nullopt if sprite_id is not a
	// sprite or no free id remains.
	std::optional<uint8_t> AddEntity(uint8_t sprite_id, int lo_palette, int hi_palette);
	// True while some room places an entity of this type; GetRoomsUsingEntity names them so a
	// delete can refuse and explain.
	bool IsEntityUsedInRooms(uint8_t id) const;
	std::vector<uint16_t> GetRoomsUsingEntity(uint8_t id) const;
	// Removes a non-item entity together with everything keyed to its id: the sprite lookup,
	// palette lookups, enemy stats, talk sfx (which lives in StringData) and display label.
	// Refuses for items and for any entity a room still uses.
	bool DeleteEntity(uint8_t id, const std::shared_ptr<StringData>& strings);
	// Exchanges everything keyed to two non-item entity ids while leaving room references
	// untouched: the rooms keep their type bytes, but the two entities swap sprite, palettes,
	// enemy stats, talk sfx and label. Refuses if either id is an item. This "move by content"
	// is deliberately unlike the sprite/tileset reorder, which renumbers references instead.
	bool SwapEntities(uint8_t a, uint8_t b, const std::shared_ptr<StringData>& strings);

	bool IsEntity(uint8_t id) const;
	bool IsSprite(uint8_t id) const;
	bool IsItem(uint8_t sprite_id) const;
	bool IsEntityItem(uint8_t entity_id) const;
	bool IsEntityEnemy(uint8_t entity_id) const;
	bool HasFrontAndBack(uint8_t id) const;
	bool CanRotate(uint8_t id) const;
	std::string GetSpriteName(uint8_t id) const;
	uint8_t GetSpriteId(const std::string& name) const;
	uint8_t GetSpriteFromEntity(uint8_t id) const;
	void SetEntitySprite(uint8_t entity, uint8_t sprite);
	bool EntityHasSprite(uint8_t id) const;
	std::vector<uint8_t> GetEntitiesFromSprite(uint8_t id) const;

	Hitbox GetSpriteHitbox(uint8_t id) const;
	Hitbox GetEntityHitbox(uint8_t id) const;
	void SetSpriteHitbox(uint8_t id, const Hitbox& hitbox);

	bool SpriteFrameExists(const std::string& name) const;
	void DeleteSpriteFrame(const std::string& name);
	void AddSpriteFrame(uint8_t sprite_id, const std::string& name);

	bool SpriteAnimationExists(const std::string& name) const;
	void DeleteSpriteAnimation(const std::string& name);
	void AddSpriteAnimation(uint8_t sprite_id, const std::string& name);
	void MoveSpriteAnimation(uint8_t sprite_id, const std::string& name, int pos);

	void DeleteSpriteAnimationFrame(const std::string& animation_name, int frame_id);
	void InsertSpriteAnimationFrame(const std::string& animation_name, int frame_id, const std::string& name);
	void ChangeSpriteAnimationFrame(const std::string& animation_name, int frame_id, const std::string& name);
	void MoveSpriteAnimationFrame(const std::string& animation_name, int old_pos, int new_pos);

	uint32_t GetSpriteAnimationCount(uint8_t id) const;
	std::vector<std::string> GetSpriteAnimations(uint8_t id) const;
	std::vector<std::string> GetSpriteAnimations(const std::string& name) const;
	uint32_t GetSpriteFrameCount(uint8_t id) const;
	std::vector<std::string> GetSpriteFrames(uint8_t id) const;
	std::vector<std::string> GetSpriteFrames(const std::string& name) const;
	int GetSpriteFrameId(uint8_t sprite_id, const std::string& name) const;
	int GetDefaultEntityAnimationId(uint8_t id) const;
	int GetDefaultEntityFrameId(uint8_t id) const;
	int GetDefaultAbsFrameId(uint8_t sprite_id) const;
	std::shared_ptr<SpriteFrameEntry> GetDefaultEntityFrame(uint8_t id) const;
	std::shared_ptr<SpriteFrameEntry> GetSpriteFrame(const std::string& name) const;
	std::shared_ptr<SpriteFrameEntry> GetSpriteFrame(uint8_t id, uint8_t frame) const;
	std::shared_ptr<SpriteFrameEntry> GetSpriteFrame(uint8_t id, uint8_t anim, uint8_t frame) const;
	std::shared_ptr<SpriteFrameEntry> GetSpriteFrame(const std::string& anim_name, uint8_t frame) const;
	uint32_t GetSpriteAnimationFrameCount(uint8_t id, uint8_t anim_id) const;
	uint32_t GetSpriteAnimationFrameCount(const std::string& name) const;
	std::vector<std::string> GetSpriteAnimationFrames(uint8_t id, uint8_t anim_id) const;
	std::vector<std::string> GetSpriteAnimationFrames(const std::string& anim) const;
	std::vector<std::string> GetSpriteAnimationFrames(const std::string& name, uint8_t anim_id) const;
	AnimationFlags GetSpriteAnimationFlags(uint8_t id) const;
	void SetSpriteAnimationFlags(uint8_t id, const AnimationFlags& flags);
	uint16_t GetSpriteVolume(uint8_t id) const;
	void SetSpriteVolume(uint8_t id, uint16_t val);

	std::vector<Entity> GetRoomEntities(uint16_t room) const;
	void SetRoomEntities(uint16_t room, const std::vector<Entity>& entities);
	// The game indexes the room entity offset table by room number with no bounds check,
	// so the table has to cover every room even when the trailing ones have no entities.
	std::size_t GetRoomEntityTableSize() const;
	void SetRoomEntityTableSize(std::size_t rooms);
	// Renumbers the room-keyed entity and flag tables this manager owns. Go through
	// GameData::MoveRoom rather than calling this directly.
	void RemapRooms(const RoomIndexMap& mapping);
	std::vector<EntityFlag> GetEntityVisibilityFlagsForRoom(uint16_t room);
	void SetEntityVisibilityFlagsForRoom(uint16_t room, const std::vector<EntityFlag>& data);
	std::vector<OneTimeEventFlag> GetOneTimeEventFlagsForRoom(uint16_t room);
	void SetOneTimeEventFlagsForRoom(uint16_t room, const std::vector<OneTimeEventFlag>& data);
	std::vector<RoomClearFlag> GetMultipleEntityHideFlagsForRoom(uint16_t room);
	void SetMultipleEntityHideFlagsForRoom(uint16_t room, const std::vector<RoomClearFlag>& data);
	std::vector<RoomClearFlag> GetLockedDoorFlagsForRoom(uint16_t room);
	void SetLockedDoorFlagsForRoom(uint16_t room, const std::vector<RoomClearFlag>& data);
	std::vector<RoomClearFlag> GetPermanentSwitchFlagsForRoom(uint16_t room);
	void SetPermanentSwitchFlagsForRoom(uint16_t room, const std::vector<RoomClearFlag>& data);
	std::vector<SacredTreeFlag> GetSacredTreeFlagsForRoom(uint16_t room);
	void SetSacredTreeFlagsForRoom(uint16_t room, const std::vector<SacredTreeFlag>& data);

	const std::map<std::string, std::shared_ptr<PaletteEntry>>& GetAllPalettes() const;
	std::shared_ptr<PaletteEntry> GetPalette(const std::string& name) const;
	std::shared_ptr<Palette> GetSpritePalette(int lo, int hi = -1) const;
	std::shared_ptr<Palette> GetEntityPalette(uint8_t idx) const;
	void SetEntityPalette(uint8_t entity, int lo, int hi);
	std::pair<int, int> GetEntityPaletteIdxs(uint8_t idx) const;
	uint8_t GetLoPaletteCount() const;
	std::shared_ptr<PaletteEntry> GetLoPalette(uint8_t idx) const;
	uint8_t GetHiPaletteCount() const;
	std::shared_ptr<PaletteEntry> GetHiPalette(uint8_t idx) const;

	// Sprite low/high palettes are flat lists the entity palette LUT indexes. The LUT keeps the
	// low/high distinction in bit 7, so each list holds at most 128. Add/remove/swap edit these
	// lists the way the string editor edits strings; a swap exchanges colours in place, leaving
	// the entity references pointing where they point.
	static constexpr std::size_t MAX_SPRITE_PALETTES = 128;
	bool IsLoPaletteUsed(uint8_t index) const;
	bool IsHiPaletteUsed(uint8_t index) const;
	std::vector<uint8_t> GetEntitiesUsingLoPalette(uint8_t index) const;
	std::vector<uint8_t> GetEntitiesUsingHiPalette(uint8_t index) const;
	std::optional<uint8_t> AddLoPalette();
	std::optional<uint8_t> AddHiPalette();
	bool DeleteLoPalette(uint8_t index);
	bool DeleteHiPalette(uint8_t index);
	bool SwapLoPalettes(uint8_t a, uint8_t b);
	bool SwapHiPalettes(uint8_t a, uint8_t b);

	uint8_t GetProjectile1PaletteCount() const;
	std::shared_ptr<PaletteEntry> GetProjectile1Palette(uint8_t idx) const;
	uint8_t GetProjectile2PaletteCount() const;
	std::shared_ptr<PaletteEntry> GetProjectile2Palette(uint8_t idx) const;

	ItemProperties GetItemProperties(uint8_t entity_index) const;
	void SetItemProperties(uint8_t entity_index, const ItemProperties& props);
	EnemyStats GetEnemyStats(uint8_t entity_index) const;
	void SetEnemyStats(uint8_t entity_index, const EnemyStats& stats);
	void ClearEnemyStats(uint8_t entity_index);

	std::map<int, std::string> GetScriptNames() const;
	std::pair<std::string, std::vector<Behaviours::Command>> GetScript(int id) const;
	void SetScript(int id, const std::string& name, const std::vector<Behaviours::Command>& cmds);
	void SetScript(int id, const std::vector<Behaviours::Command>& cmds);

protected:
	virtual void CommitAllChanges();
private:
	bool LoadAsmFilenames();
	void SetDefaultFilenames();
	bool CreateDirectoryStructure(const std::filesystem::path& dir);
	void InitCache();

	// Semantic role of one animation slot (ordinal) of a sprite, derived from its AnimationFlags.
	// The game reaches each slot as AnimationIndex/4 in UpdateSpriteFrame (spritefuncs1.asm); a
	// logical action occupies two consecutive ordinals - the NE bank (even) and SW bank (odd) -
	// with NW/SE produced at runtime by h-flip. Some flag combinations overload one slot with
	// several actions (e.g. a shared idle/walk bank), so label can list more than one role.
	struct AnimationRole
	{
		std::string label;    // bracket text, e.g. "Idle/Walk NE" or "Unused 1"
		bool unused;          // true when no game action maps to this slot
		int expected_frames;  // frame count this action plays; extra frames are flagged unused; 0 = variable
		int min_ok_frames;    // at/below this count the slot is a valid shorter role (idle), so no missing warning
	};
	// One entry per animation ordinal of the sprite, in list order.
	std::vector<AnimationRole> ComputeSpriteAnimationRoles(uint8_t id) const;
	// Rewrites every sprite id in the project to follow the given old -> new mapping, which
	// must cover each affected id exactly once. An id mapped to -1 is being deleted and its
	// animations and frames must already have been removed. Shared by SwapSprites and
	// DeleteSprite, which differ only in the mapping they build.
	// remap_entity_references false leaves the entity -> sprite lookup alone, so the content
	// moves between ids but the references do not follow it - SwapSprites relies on this.
	void RemapSprites(const std::map<uint8_t, int>& mapping, bool remap_entity_references = true);
	// Shared low/high sprite-palette list edits (the lo and hi variants differ only in the list,
	// palette type, name format and display-label category they pass). The entity palette LUT
	// resolves references through each entry's stored index, so a delete re-indexes the entries
	// above the hole and the pointer-based lookups follow automatically.
	std::optional<uint8_t> AddSpritePalette(std::vector<std::shared_ptr<PaletteEntry>>& pals,
		Palette::Type type, const std::string& name_format);
	bool DeleteSpritePalette(std::vector<std::shared_ptr<PaletteEntry>>& pals,
		const std::wstring& label_category, uint8_t index, bool used);
	bool SwapSpritePalettes(std::vector<std::shared_ptr<PaletteEntry>>& pals,
		const std::wstring& label_category, uint8_t a, uint8_t b);

	ByteVector SerialisePaletteLUT() const;
	void DeserialisePaletteLUT(const ByteVector& bytes);
	ByteVector SerialisePalArray(const std::vector<std::shared_ptr<PaletteEntry>>& pals) const;
	std::vector<std::shared_ptr<PaletteEntry>> DeserialisePalArray(const ByteVector& bytes, const std::string& name,
		const std::filesystem::path& path, Palette::Type type, bool unique_path = false);
	void DeserialiseRoomEntityTable(const ByteVector& offsets, const ByteVector& bytes);
	std::pair<ByteVector, ByteVector> SerialiseRoomEntityTable() const;

	bool AsmLoadSpriteFrames();
	bool AsmLoadSpritePointers();
	bool AsmLoadSpritePalettes();
	bool AsmLoadSpriteData();

	bool RomLoadSpriteFrames(const Rom& rom);
	bool RomLoadSpritePalettes(const Rom& rom);
	bool RomLoadSpriteData(const Rom& rom);

	bool AsmSaveSpriteFrames(const std::filesystem::path& dir);
	bool AsmSaveSpritePointers(const std::filesystem::path& dir);
	bool AsmSaveSpritePalettes(const std::filesystem::path& dir);
	bool AsmSaveSpriteData(const std::filesystem::path& dir);

	bool RomPrepareInjectSpriteFrames(const Rom& rom);
	bool RomPrepareInjectSpritePalettes(const Rom& rom);
	bool RomPrepareInjectSpriteData(const Rom& rom);

	std::filesystem::path m_palette_data_file;
	std::filesystem::path m_palette_lut_file;
	std::filesystem::path m_proj1_pal_file;
	std::filesystem::path m_proj2_pal_file;
	std::filesystem::path m_sprite_lut_file;
	std::filesystem::path m_sprite_anims_file;
	std::filesystem::path m_sprite_anim_frames_file;
	std::filesystem::path m_sprite_frames_data_file;
	std::filesystem::path m_item_properties_file;
	std::filesystem::path m_sprite_behaviour_offset_file;
	std::filesystem::path m_sprite_behaviour_table_file;
	std::filesystem::path m_sprite_anim_flags_lookup_file;
	std::filesystem::path m_sprite_visibility_flags_file;
	std::filesystem::path m_one_time_event_flags_file;
	std::filesystem::path m_room_clear_flags_file;
	std::filesystem::path m_locked_door_sprite_flags_file;
	std::filesystem::path m_permanent_switch_flags_file;
	std::filesystem::path m_sacred_tree_flags_file;
	std::filesystem::path m_sprite_gfx_idx_lookup_file;
	std::filesystem::path m_sprite_dimensions_lookup_file;
	std::filesystem::path m_room_sprite_table_offsets_file;
	std::filesystem::path m_enemy_stats_file;
	std::filesystem::path m_room_sprite_table_file;

	std::map<uint8_t, std::string> m_names;
	std::map<std::string, uint8_t> m_ids;
	std::map<uint8_t, std::set<std::string>> m_sprite_frames;

	std::map<uint8_t, uint16_t> m_sprite_volume;
	std::map<uint8_t, uint16_t> m_sprite_volume_orig;
	std::map<uint8_t, std::vector<std::string>> m_animations;
	std::map<uint8_t, std::vector<std::string>> m_animations_orig;
	std::map<std::string, std::vector<std::string>> m_animation_frames;
	std::map<std::string, std::vector<std::string>> m_animation_frames_orig;
	std::map<std::string, std::shared_ptr<SpriteFrameEntry>> m_frames;
	std::map<std::string, std::shared_ptr<SpriteFrameEntry>> m_frames_orig;

	std::vector<std::shared_ptr<PaletteEntry>> m_lo_palettes;
	std::vector<std::shared_ptr<PaletteEntry>> m_lo_palettes_orig;
	std::vector<std::shared_ptr<PaletteEntry>> m_hi_palettes;
	std::vector<std::shared_ptr<PaletteEntry>> m_hi_palettes_orig;
	std::vector<std::shared_ptr<PaletteEntry>> m_projectile1_palettes;
	std::vector<std::shared_ptr<PaletteEntry>> m_projectile1_palettes_orig;
	std::vector<std::shared_ptr<PaletteEntry>> m_projectile2_palettes;
	std::vector<std::shared_ptr<PaletteEntry>> m_projectile2_palettes_orig;
	std::map<std::string, std::shared_ptr<PaletteEntry>> m_palettes_by_name;

	std::map<uint8_t, std::shared_ptr<PaletteEntry>> m_lo_palette_lookup;
	std::map<uint8_t, std::shared_ptr<PaletteEntry>> m_lo_palette_lookup_orig;
	std::map<uint8_t, std::shared_ptr<PaletteEntry>> m_hi_palette_lookup;
	std::map<uint8_t, std::shared_ptr<PaletteEntry>> m_hi_palette_lookup_orig;

	std::vector<EntityFlag> m_sprite_visibility_flags;
	std::vector<EntityFlag> m_sprite_visibility_flags_orig;
	std::vector<OneTimeEventFlag> m_one_time_event_flags;
	std::vector<OneTimeEventFlag> m_one_time_event_flags_orig;
	std::vector<RoomClearFlag> m_room_clear_flags;
	std::vector<RoomClearFlag> m_room_clear_flags_orig;
	std::vector<RoomClearFlag> m_locked_door_flags;
	std::vector<RoomClearFlag> m_locked_door_flags_orig;
	std::vector<RoomClearFlag> m_permanent_switch_flags;
	std::vector<RoomClearFlag> m_permanent_switch_flags_orig;
	std::vector<SacredTreeFlag> m_sacred_tree_flags;
	std::vector<SacredTreeFlag> m_sacred_tree_flags_orig;

	std::map<uint8_t, uint8_t> m_sprite_to_entity_lookup;
	std::map<uint8_t, uint8_t> m_sprite_to_entity_lookup_orig;
	std::map<uint8_t, std::array<uint8_t, 2>> m_sprite_dimensions;
	std::map<uint8_t, std::array<uint8_t, 2>> m_sprite_dimensions_orig;
	std::map<uint8_t, std::array<uint8_t, 5>> m_enemy_stats;
	std::map<uint8_t, std::array<uint8_t, 5>> m_enemy_stats_orig;
	std::vector<std::array<uint8_t, 4>> m_item_properties;
	std::vector<std::array<uint8_t, 4>> m_item_properties_orig;
	std::map<uint8_t, AnimationFlags> m_sprite_animation_flags;
	std::map<uint8_t, AnimationFlags> m_sprite_animation_flags_orig;

	std::map<uint16_t, std::vector<Entity>> m_room_entities;
	std::map<uint16_t, std::vector<Entity>> m_room_entities_orig;
	std::size_t m_room_entity_table_size = 0;
	std::size_t m_room_entity_table_size_orig = 0;

	std::map<int, std::pair<std::string, std::vector<Behaviours::Command>>> m_sprite_behaviours;
	std::map<int, std::pair<std::string, std::vector<Behaviours::Command>>> m_sprite_behaviours_orig;
};

} // namespace Landstalker

#endif // _SPRITE_DATA_H_
