#include <landstalker/misc/Labels.h>
#include <landstalker/misc/DefaultLabels.h>
#include <landstalker/misc/Utils.h>
#include <climits>
#include <cstdint>
#include <cwctype>
#include <codecvt>
#include <fstream>
#include <functional>
#include <vector>

namespace Landstalker {

std::map<std::pair<std::wstring, int>, std::wstring> Labels::m_data;

const std::map<std::wstring, std::wstring>& Labels::GetFormatStrings()
{
    static const std::map<std::wstring, std::wstring> FORMAT_STRINGS
    {
        {Labels::C_SPRITE_ANIMATIONS, L"0x%04X"},
        {Labels::C_SPRITE_FRAMES, L"0x%04X"},
        {Labels::C_BLOCKSETS, L"0x%06X"}
    };
    return FORMAT_STRINGS;
}

const std::wstring Labels::C_ROOMS(L"rooms");
const std::wstring Labels::C_BGMS(L"bgms");
const std::wstring Labels::C_SOUNDS(L"sounds");
const std::wstring Labels::C_ENTITIES(L"entities");
const std::wstring Labels::C_SPRITES(L"sprites");
const std::wstring Labels::C_SPRITE_FRAMES(L"sprite_frames");
const std::wstring Labels::C_SPRITE_ANIMATIONS(L"sprite_animations");
const std::wstring Labels::C_MAPS(L"maps");
const std::wstring Labels::C_ROOM_PALETTES(L"room_palettes");
const std::wstring Labels::C_HIGH_PALETTES(L"sprite_high_palettes");
const std::wstring Labels::C_LOW_PALETTES(L"sprite_low_palettes");
const std::wstring Labels::C_TILESETS(L"tilesets");
const std::wstring Labels::C_ANIM_TILESETS(L"animated_tilesets");
const std::wstring Labels::C_BLOCKSETS(L"blocksets");
const std::wstring Labels::C_FLAGS(L"flags");
const std::wstring Labels::C_BEHAVIOURS(L"behaviours");
const std::wstring Labels::C_SCRIPT(L"script");
const std::wstring Labels::C_CUTSCENE(L"cutscene");
const std::wstring Labels::C_CHARACTER(L"character");
const std::wstring Labels::C_GLOBAL_CHARACTER(L"global_character");

void Labels::InitDefaults()
{
    m_data = DefaultLabels::DEFAULT_LABELS;
}

void Labels::LoadData(const std::string& filename)
{
    try {
        InitDefaults();
        YAML::Node config = YAML::LoadFile(filename);
        std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;

        // Iterate through all top-level keys in the YAML
        for (const auto& category : config) {
            const std::wstring category_name = cvt.from_bytes(category.first.as<std::string>());
            try {
                for (const auto& entry : category.second) {
                    m_data[{category_name, entry.first.as<int>()}] =
                        cvt.from_bytes(entry.second.as<std::string>());
                }
            } catch (const YAML::Exception& e) {
                Debug("Error parsing " + cvt.to_bytes(category_name) + " in YAML: " + e.what());
            }
        }
    } catch (const YAML::Exception& e) {
        Debug("Error loading YAML file '" + filename + "': " + e.what());
    } catch (const std::exception& e) {
        Debug("Error accessing file '" + filename + "': " + e.what());
    }
}

void Labels::SaveData(const std::string& filename)
{
    std::wofstream ofs(filename, std::ios::binary | std::ios::out);
    ofs.imbue(std::locale(std::locale(), new std::codecvt_utf8<wchar_t>));
    std::wstring category = L"";
    bool begin = true;
    for (const auto& label : m_data)
    {
        if (category != label.first.first)
        {
            category = label.first.first;
            if (begin)
            {
                begin = false;
            }
            else
            {
                ofs << std::endl;
            }
            ofs << category << L":" << std::endl;
        }
        const std::wstring fmt = GetFormatStrings().count(label.first.first) > 0 ? GetFormatStrings().at(label.first.first) : L"%d";
        ofs << L"    " << StrWPrintf(fmt, label.first.second) << L": \"" << label.second << "\"" << std::endl;
    }
}

bool Labels::Exists(const std::wstring& what, int id)
{
    return m_data.find({what, id}) != m_data.cend() && IsValidPath(m_data.at({what, id}));
}

std::optional<std::wstring> Labels::Get(const std::wstring& what, int id) {
    auto it = m_data.find({what, id});
    if (it != m_data.end() && IsValidPath(it->second))
    {
        return it->second;
    }
    return std::nullopt;
}

bool Labels::IsValid(const std::wstring& what)
{
    return IsValid(what, std::nullopt);
}

bool Labels::IsValid(const std::wstring& what, const std::wstring& category, int id)
{
    return IsValid(what, std::make_optional(std::make_pair(category, id)));
}

bool Labels::IsValid(const std::wstring& what,
    const std::optional<std::pair<std::wstring, int>>& excluded)
{
    if (!IsValidPath(what))
    {
        return false;
    }
    for(const auto& lbl : m_data)
    {
        if (excluded && lbl.first == *excluded)
        {
            continue;
        }
        if (lbl.second == what)
        {
            Debug("Duplicate label found");
            return false;
        }
        if (lbl.second.length() > what.length() && lbl.second.rfind(what, 0) == 0)
        {
            if (lbl.second.at(what.length()) == L'/')
            {
                Debug("Label same name as subdirectory");
                return false;
            }
        }
        if (what.length() > lbl.second.length() && what.rfind(lbl.second, 0) == 0 &&
            what.at(lbl.second.length()) == L'/')
        {
            Debug("Label would place a subdirectory beneath an existing item");
            return false;
        }
    }
    return true;
}

bool Labels::Update(const std::wstring& what, int id, const std::wstring& updated)
{
    if (!IsValidPath(updated))
    {
        return false;
    }
    if (m_data.count({ what, id }) > 0 && m_data.at({ what, id }) == updated)
    {
        // No update needed
        return true;
    }
    if (!IsValid(updated, what, id))
    {
        return false;
    }
    m_data[{what, id}] = updated;
    return true;
}

std::optional<std::wstring> Labels::NormalizePath(const std::wstring& what)
{
    if (what.empty())
    {
        Debug("Empty string");
        return std::nullopt;
    }
    if (what.front() == L'/' || what.back() == L'/' || what.find(L"//") != std::wstring::npos)
    {
        Debug("Invalid Path");
        return std::nullopt;
    }
    for (std::size_t i = 0; i < what.size(); ++i)
    {
        std::uint32_t codepoint = static_cast<std::uint32_t>(what[i]);
#if WCHAR_MAX <= 0xFFFF
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF)
        {
            if (++i >= what.size())
            {
                Debug("Invalid Unicode");
                return std::nullopt;
            }
            const auto low = static_cast<std::uint32_t>(what[i]);
            if (low < 0xDC00 || low > 0xDFFF)
            {
                Debug("Invalid Unicode");
                return std::nullopt;
            }
            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
        }
        else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF)
        {
            Debug("Invalid Unicode");
            return std::nullopt;
        }
#endif
        const bool control = codepoint <= 0x1F || (codepoint >= 0x7F && codepoint <= 0x9F);
        const bool non_space_whitespace = codepoint == 0x85 || codepoint == 0xA0 ||
            codepoint == 0x1680 || (codepoint >= 0x2000 && codepoint <= 0x200A) ||
            codepoint == 0x2028 || codepoint == 0x2029 || codepoint == 0x202F ||
            codepoint == 0x205F || codepoint == 0x3000;
        const bool noncharacter = (codepoint >= 0xFDD0 && codepoint <= 0xFDEF) ||
            (codepoint & 0xFFFF) == 0xFFFE || (codepoint & 0xFFFF) == 0xFFFF;
        if (control || non_space_whitespace || noncharacter || codepoint > 0x10FFFF || codepoint == L'\\' ||
            codepoint == L'\'' || codepoint == L'"')
        {
            Debug("Invalid characters");
            return std::nullopt;
        }
    }
    std::wstring tmp;
    std::wstringstream ss(what);
    std::vector<std::wstring> elems;
    while (std::getline(ss, tmp, L'/'))
    {
        const auto first = tmp.find_first_not_of(L' ');
        const auto last = tmp.find_last_not_of(L' ');
        if (first == std::wstring::npos)
        {
            Debug("Empty path segment");
            return std::nullopt;
        }
        elems.push_back(tmp.substr(first, last - first + 1));
    }
    std::wstring normalized;
    for (const auto& elem : elems)
    {
        if (!normalized.empty())
        {
            normalized += L'/';
        }
        normalized += elem;
    }
    return normalized;
}

bool Labels::Reorder(const std::wstring& category, std::size_t old_index,
    std::size_t new_index, std::size_t count)
{
    if (old_index >= count || new_index >= count)
    {
        return false;
    }
    if (old_index == new_index)
    {
        return true;
    }

    std::vector<std::optional<std::wstring>> labels;
    labels.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        labels.push_back(Get(category, static_cast<int>(i)));
    }
    const auto moved = labels[old_index];
    labels.erase(labels.begin() + static_cast<std::ptrdiff_t>(old_index));
    labels.insert(labels.begin() + static_cast<std::ptrdiff_t>(new_index), moved);
    for (std::size_t i = 0; i < count; ++i)
    {
        m_data.erase({ category, static_cast<int>(i) });
        if (labels[i])
        {
            m_data[{ category, static_cast<int>(i) }] = *labels[i];
        }
    }
    return true;
}

bool Labels::Erase(const std::wstring& category, std::size_t index, std::size_t count)
{
    if (index >= count)
    {
        return false;
    }
    for (std::size_t i = index; i + 1 < count; ++i)
    {
        const auto next = Get(category, static_cast<int>(i + 1));
        m_data.erase({ category, static_cast<int>(i) });
        if (next)
        {
            m_data[{ category, static_cast<int>(i) }] = *next;
        }
    }
    m_data.erase({ category, static_cast<int>(count - 1) });
    return true;
}

bool Labels::IsValidPath(const std::wstring& what)
{
    const auto normalized = NormalizePath(what);
    return normalized && *normalized == what;
}

} // namespace Landstalker
