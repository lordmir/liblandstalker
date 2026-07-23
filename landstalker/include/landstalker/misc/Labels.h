#ifndef LABELS_H_
#define LABELS_H_

#include <yaml-cpp/yaml.h>
#include <optional>
#include <iostream>
#include <map>
#include <string>
#include <utility>

namespace Landstalker {

class Labels {
public:
    static void InitDefaults();
    static void LoadData(const std::string& filename);
    static void SaveData(const std::string& filename);
    static bool Exists(const std::wstring& what, int id);
    static std::optional<std::wstring> Get(const std::wstring& what, int id);
    static std::optional<std::wstring> NormalizePath(const std::wstring& what);
    static bool IsValidPath(const std::wstring& what);
    static bool IsValid(const std::wstring& what);
    static bool IsValid(const std::wstring& what, const std::wstring& category, int id);
    static bool Update(const std::wstring& category, int id, const std::wstring& updated);
    static bool Reorder(const std::wstring& category, std::size_t old_index,
        std::size_t new_index, std::size_t count);
    static bool Erase(const std::wstring& category, std::size_t index, std::size_t count);
    // Moves labels between ids within a category, for index spaces Reorder and Erase
    // cannot express. Animated tilesets and blocksets key off composite ids that embed
    // the tileset number - (tileset << 8 | index) and (tileset << 16 | pri << 8 | sec) -
    // so renumbering one tileset rewrites scattered keys rather than a contiguous run.
    // Ids absent from the mapping keep their labels; ids mapped to -1 are dropped.
    static bool Remap(const std::wstring& category, const std::map<int, int>& mapping);

    static const std::wstring C_ROOMS;
    static const std::wstring C_BGMS;
    static const std::wstring C_SOUNDS;
    static const std::wstring C_ENTITIES;
    static const std::wstring C_SPRITES;
    static const std::wstring C_SPRITE_FRAMES;
    static const std::wstring C_SPRITE_ANIMATIONS;
    static const std::wstring C_MAPS;
    static const std::wstring C_ROOM_PALETTES;
    static const std::wstring C_HIGH_PALETTES;
    static const std::wstring C_LOW_PALETTES;
    static const std::wstring C_TILESETS;
    static const std::wstring C_ANIM_TILESETS;
    static const std::wstring C_BLOCKSETS;
    static const std::wstring C_FLAGS;
    static const std::wstring C_BEHAVIOURS;
    static const std::wstring C_SCRIPT;
    static const std::wstring C_CUTSCENE;
    static const std::wstring C_CHARACTER;
    static const std::wstring C_GLOBAL_CHARACTER;
private:
    static bool IsValid(const std::wstring& what,
        const std::optional<std::pair<std::wstring, int>>& excluded);
    static std::map<std::pair<std::wstring, int>, std::wstring> m_data;
    static const std::map<std::wstring, std::wstring>& GetFormatStrings();

};

} // namespace Landstalker

#endif // LABELS_H_
