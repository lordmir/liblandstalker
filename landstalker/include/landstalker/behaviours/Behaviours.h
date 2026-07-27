#ifndef _BEHAVIOURS_H_
#define _BEHAVIOURS_H_

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <variant>
#include <tuple>

namespace Landstalker {

class Behaviours
{
public:
    // Names follow the disassembly's BHV_* constants
    // (code/include/constants/behaviours.inc) and the command reference in
    // docs/behaviour_commands.md.
    enum class CommandType : uint8_t
    {
        PAUSE                      = 0,   // $00
        MOVE_TIMED                 = 1,   // $01
        TURN_CW                    = 2,   // $02
        TURN_CCW                   = 3,   // $03
        TURN_NE                    = 4,   // $04
        TURN_SE                    = 5,   // $05
        TURN_SW                    = 6,   // $06
        TURN_NW                    = 7,   // $07
        SET_DIR_CW                 = 8,   // $08 (was TURN_CW_NO_UPDATE)
        SET_DIR_CCW                = 9,   // $09
        SET_DIR_NE                 = 10,  // $0A
        SET_DIR_SE                 = 11,  // $0B
        SET_DIR_SW                 = 12,  // $0C
        SET_DIR_NW                 = 13,  // $0D
        MAKE_VISIBLE               = 14,  // $0E
        MAKE_INVISIBLE             = 15,  // $0F
        THROWN_OBJECT              = 16,  // $10 engine state: thrown/dropped carried object in flight
        MOVE_RELATIVE              = 17,  // $11
        GOTO_INSTRUCTION           = 18,  // $12 (was GOTO_COMMAND)
        MOVE_UNTIL_COLLISION       = 19,  // $13
        TURN_RANDOM                = 20,  // $14
        SET_DIR_RANDOM             = 21,  // $15 (was TURN_RANDOM_NO_UPDATE)
        PUT_DOWN_OBJECT            = 22,  // $16 engine state: put-down object sliding to rest
        TURN_RANDOM_IMMEDIATE      = 23,  // $17
        TURN_CW_IMMEDIATE          = 24,  // $18
        TURN_CCW_IMMEDIATE         = 25,  // $19
        TURN_NE_IMMEDIATE          = 26,  // $1A
        TURN_SE_IMMEDIATE          = 27,  // $1B
        TURN_SW_IMMEDIATE          = 28,  // $1C
        TURN_NW_IMMEDIATE          = 29,  // $1D
        FREEZE                     = 30,  // $1E
        SET_SPEED_1                = 31,  // $1F (was SPEED_SLOW)
        SET_SPEED_2                = 32,  // $20 (was SPEED_NORMAL)
        SET_SPEED_4                = 33,  // $21 (was SPEED_FAST)
        SET_SPEED_8                = 34,  // $22 (was SPEED_X_FAST)
        TURN_180                   = 35,  // $23
        SET_DIR_180                = 36,  // $24 (was TURN_180_NO_UPDATE)
        TURN_180_IMMEDIATE         = 37,  // $25
        FOLLOW_PLAYER_WITH_JUMP    = 38,  // $26
        JUMP                       = 39,  // $27
        ENABLE_ROTATION            = 40,  // $28 (was ENABLE_FRAME_UPDATE)
        DISABLE_ROTATION           = 41,  // $29
        MOVE_RANDOM_TIME           = 42,  // $2A
        RUN_SPECIAL_AI             = 43,  // $2B (was LOAD_SPECIAL_AI)
        FOLLOW_PLAYER_NO_JUMP      = 44,  // $2C
        LOOT                       = 45,  // $2D engine state: dropped money bag/item box
        SHOP_ITEM                  = 46,  // $2E
        THROWN_SHOP_ITEM           = 47,  // $2F engine state: thrown shop item ($10 for shop items)
        MOVE_UP_RELATIVE           = 48,  // $30
        MOVE_DOWN_RELATIVE         = 49,  // $31
        PUT_DOWN_SHOP_ITEM         = 50,  // $32 engine state: put-down shop item ($16 for shop items)
        FLEE_PLAYER                = 51,  // $33 run directly away from the player
        PLAYBACK_INPUT             = 52,  // $34
        RESET_PLAYBACK             = 53,  // $35
        WAIT_FOR_CONDITION         = 54,  // $36
        PAUSE_4_SECONDS            = 55,  // $37
        ENABLE_GRAVITY             = 56,  // $38
        DISABLE_GRAVITY            = 57,  // $39
        MOVE_UP_TIMED              = 58,  // $3A
        MOVE_DOWN_TIMED            = 59,  // $3B
        MOVE_UP_ABSOLUTE           = 60,  // $3C
        MOVE_DOWN_ABSOLUTE         = 61,  // $3D
        DESPAWN_ENTITY             = 62,  // $3E (was REMOVE_SPRITE)
        MOVE_UP_UNTIL_COLLISION    = 63,  // $3F (was NUDGE_UP)
        MOVE_DOWN_UNTIL_COLLISION  = 64,  // $40
        SET_FLAG                   = 65,  // $41
        WAIT_FOR_FLAG_SET          = 66,  // $42
        CLEAR_FLAG                 = 67,  // $43
        HIDE                       = 68,  // $44
        SHOW_WHEN_COLLISION_CLEAR  = 69,  // $45
        WAIT_FOR_FLAG_CLEAR        = 70,  // $46
        WAIT_SPRITE_NOT_HOSTILE    = 71,  // $47 wait until sprite slot no longer hostile
        SET_ENTITY_SPEED           = 72,  // $48 (was SET_OBJECT_SPEED)
        ACTIVATE_SWITCH            = 73,  // $49
        RESET_SWITCH               = 74,  // $4A
        MOVE_TO_XY_POS_IMMEDIATE   = 75,  // $4B
        MOVE_TO_Z_POS_IMMEDIATE    = 76,  // $4C
        RESET_TO_INIT_PARAMS       = 77,  // $4D (was RESET_TO_INITIAL_POS)
        START_LO_CUTSCENE          = 78,  // $4E (was START_CUTSCENE)
        MOVE_NO_CLIP               = 79,  // $4F
        ROTATE_PLAYER              = 80,  // $50
        MAKE_HOSTILE               = 81,  // $51
        MAKE_NON_HOSTILE           = 82,  // $52
        DISABLE_WALK_BACKWARDS     = 83,  // $53 (was ENABLE_BACKWARDS_MOVEMENT: old name was swapped)
        ENABLE_WALK_BACKWARDS      = 84,  // $54 (was DISABLE_BACKWARDS_MOVEMENT: old name was swapped)
        SPECIAL_ANIMATION          = 85,  // $55
        MOVE_UP_TO_INIT_POS        = 86,  // $56
        TRIGGER_TILE_SWAP          = 87,  // $57
        UPDATE_SPRITE_FACING       = 88,  // $58 (was UPDATE_SPRITE_ORIENTATION)
        PRINT_TEXT                 = 89,  // $59
        PROJECTILE_HIT_ENEMIES     = 90,  // $5A player-side projectile: fly forward, damage a struck hostile
        SET_TARGET_POSITION        = 91,  // $5B
        MOVE_TO_TARGET_POSITION    = 92,  // $5C
        REPEAT_BEGIN               = 93,  // $5D
        REPEAT_END                 = 94,  // $5E
        DECAY_FLASH                = 95,  // $5F
        FLASH_SPIN_APPEAR          = 96,  // $60
        FLASH_SPIN_DISAPPEAR       = 97,  // $61
        PLAY_SOUND                 = 98,  // $62
        PROJECTILE_MOVE            = 99,  // $63 enemy projectile: fly forward with contact damage
        START_HI_CUTSCENE          = 100, // $64
        PROJECTILE_DIAG_RIGHT      = 101, // $65 as $63 plus a 90-degree-CW step each tick (undone after): straight diagonal, right of facing
        PROJECTILE_DIAG_LEFT       = 102, // $66 mirror of $65 (diagonally left)
        PROJECTILE_LEVEL           = 103, // $67 as $63 with a floor clamp: holds a constant height; despawns on landing
        NULL_COMMAND               = 104
    };

    enum class ParamType
    {
        NONE,
        UINT8,
        INT8,
        UINT16,
        LABEL,
        COORDINATE,
        LONG_COORDINATE,
        FLAG,
        SOUND,
        LOW_CUTSCENE,
        HIGH_CUTSCENE
    };

    using ParameterValue = std::variant<int, double>;
    using Parameter = std::tuple<std::string, ParameterValue, ParamType>;

    struct CommandDefinition
    {
        CommandType id;
        std::vector<std::string> aliases;
        std::vector<std::pair<std::string, ParamType>> params;
    };

    struct Command
    {
        CommandType command = CommandType::PAUSE;
        std::vector<Parameter> params;

        bool operator==(const Command& rhs) const
        {
            return (this->command == rhs.command && this->params == rhs.params);
        }
        bool operator!=(const Command& rhs) const
        {
            return !(*this == rhs);
        }
    };

    static const CommandDefinition& GetCommand(CommandType cmd);
    static const CommandDefinition& GetCommandById(int id);
    static const CommandDefinition& GetCommandByName(const std::string& name);

    static std::map<int, std::pair<std::string, std::vector<Command>>> Unpack(const std::vector<uint8_t>& offsets, const std::vector<uint8_t>& behaviour_table);
    static std::pair<std::vector<uint8_t>, std::vector<uint8_t>> Pack(const std::map<int, std::pair<std::string, std::vector<Command>>>& behaviours);
private:
    static const std::unordered_map<ParamType, int> PARAM_SIZES;
    static const std::unordered_map<CommandType, CommandDefinition> COMMANDS_BY_ID;
    static std::unordered_map<std::string, CommandType> commands_by_name;
};

} // namespace Landstalker

#endif // _BEHAVIOURS_H_
