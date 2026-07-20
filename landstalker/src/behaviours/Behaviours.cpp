#include <landstalker/behaviours/Behaviours.h>

#include <sstream>
#include <variant>
#include <yaml-cpp/yaml.h>
#include <cassert>
#include <numeric>

namespace Landstalker {

const std::unordered_map<Behaviours::ParamType, int> Behaviours::PARAM_SIZES =
{
    {ParamType::UINT8,           1},
    {ParamType::INT8,            1},
    {ParamType::SOUND,           1},
    {ParamType::LOW_CUTSCENE,    1},
    {ParamType::HIGH_CUTSCENE,   1},
    {ParamType::UINT16,          2},
    {ParamType::LABEL,           1},
    {ParamType::COORDINATE,      1},
    {ParamType::LONG_COORDINATE, 2},
    {ParamType::FLAG,            2}
};

const std::unordered_map<Behaviours::CommandType, Behaviours::CommandDefinition> Behaviours::COMMANDS_BY_ID =
{
    { CommandType::PAUSE                     , {  CommandType::PAUSE                     , {"Pause"},                    {{"Ticks",  ParamType::UINT8}} }},
    { CommandType::MOVE_TIMED                , {  CommandType::MOVE_TIMED                , {"MoveTimed"},                {{"Ticks",  ParamType::UINT8}} }},
    { CommandType::TURN_CW                   , {  CommandType::TURN_CW                   , {"TurnCW"},                   {} }},
    { CommandType::TURN_CCW                  , {  CommandType::TURN_CCW                  , {"TurnCCW"},                  {} }},
    { CommandType::TURN_NE                   , {  CommandType::TURN_NE                   , {"TurnNE"},                   {} }},
    { CommandType::TURN_SE                   , {  CommandType::TURN_SE                   , {"TurnSE"},                   {} }},
    { CommandType::TURN_SW                   , {  CommandType::TURN_SW                   , {"TurnSW"},                   {} }},
    { CommandType::TURN_NW                   , {  CommandType::TURN_NW                   , {"TurnNW"},                   {} }},
    { CommandType::SET_DIR_CW                , {  CommandType::SET_DIR_CW                , {"SetDirCW", "TurnCWNoUpdate"},    {} }},
    { CommandType::SET_DIR_CCW               , {  CommandType::SET_DIR_CCW               , {"SetDirCCW", "TurnCCWNoUpdate"},  {} }},
    { CommandType::SET_DIR_NE                , {  CommandType::SET_DIR_NE                , {"SetDirNE", "TurnNENoUpdate"},    {} }},
    { CommandType::SET_DIR_SE                , {  CommandType::SET_DIR_SE                , {"SetDirSE", "TurnSENoUpdate"},    {} }},
    { CommandType::SET_DIR_SW                , {  CommandType::SET_DIR_SW                , {"SetDirSW", "TurnSWNoUpdate"},    {} }},
    { CommandType::SET_DIR_NW                , {  CommandType::SET_DIR_NW                , {"SetDirNW", "TurnNWNoUpdate"},    {} }},
    { CommandType::MAKE_VISIBLE              , {  CommandType::MAKE_VISIBLE              , {"MakeVisible"},              {} }},
    { CommandType::MAKE_INVISIBLE            , {  CommandType::MAKE_INVISIBLE            , {"MakeInvisible"},            {} }},
    { CommandType::THROWN_OBJECT             , {  CommandType::THROWN_OBJECT             , {"ThrownObject", "UnknownB10"},    {{"ThrowType",  ParamType::UINT8}} }},
    { CommandType::MOVE_RELATIVE             , {  CommandType::MOVE_RELATIVE             , {"MoveRelative"},             {{"Steps", ParamType::COORDINATE}} }},
    { CommandType::GOTO_INSTRUCTION          , {  CommandType::GOTO_INSTRUCTION          , {"GotoInstruction", "GotoCommand"}, {{"Command",  ParamType::LABEL}} }},
    { CommandType::MOVE_UNTIL_COLLISION      , {  CommandType::MOVE_UNTIL_COLLISION      , {"MoveUntilCollision"},       {} }},
    { CommandType::TURN_RANDOM               , {  CommandType::TURN_RANDOM               , {"TurnRandom"},               {} }},
    { CommandType::SET_DIR_RANDOM            , {  CommandType::SET_DIR_RANDOM            , {"SetDirRandom", "TurnRandomNoUpdate"}, {} }},
    { CommandType::PUT_DOWN_OBJECT           , {  CommandType::PUT_DOWN_OBJECT           , {"PutDownObject", "UnknownB16"},   {{"Phase", ParamType::UINT8}} }},
    { CommandType::TURN_RANDOM_IMMEDIATE     , {  CommandType::TURN_RANDOM_IMMEDIATE     , {"TurnRandomImmediate"},      {} }},
    { CommandType::TURN_CW_IMMEDIATE         , {  CommandType::TURN_CW_IMMEDIATE         , {"TurnCWImmediate"},          {} }},
    { CommandType::TURN_CCW_IMMEDIATE        , {  CommandType::TURN_CCW_IMMEDIATE        , {"TurnCCWImmediate", "TurnCCWImmedate"}, {} }},
    { CommandType::TURN_NE_IMMEDIATE         , {  CommandType::TURN_NE_IMMEDIATE         , {"TurnNEImmediate"},          {} }},
    { CommandType::TURN_SE_IMMEDIATE         , {  CommandType::TURN_SE_IMMEDIATE         , {"TurnSEImmediate"},          {} }},
    { CommandType::TURN_SW_IMMEDIATE         , {  CommandType::TURN_SW_IMMEDIATE         , {"TurnSWImmediate"},          {} }},
    { CommandType::TURN_NW_IMMEDIATE         , {  CommandType::TURN_NW_IMMEDIATE         , {"TurnNWImmediate"},          {} }},
    { CommandType::FREEZE                    , {  CommandType::FREEZE                    , {"Freeze"},                   {} }},
    { CommandType::SET_SPEED_1               , {  CommandType::SET_SPEED_1               , {"SetSpeed1", "SlowSpeed"},   {} }},
    { CommandType::SET_SPEED_2               , {  CommandType::SET_SPEED_2               , {"SetSpeed2", "NormalSpeed"}, {} }},
    { CommandType::SET_SPEED_4               , {  CommandType::SET_SPEED_4               , {"SetSpeed4", "FastSpeed"},   {} }},
    { CommandType::SET_SPEED_8               , {  CommandType::SET_SPEED_8               , {"SetSpeed8", "XFastSpeed"},  {} }},
    { CommandType::TURN_180                  , {  CommandType::TURN_180                  , {"Turn180"},                  {} }},
    { CommandType::SET_DIR_180               , {  CommandType::SET_DIR_180               , {"SetDir180", "Turn180NoUpdate"}, {} }},
    { CommandType::TURN_180_IMMEDIATE        , {  CommandType::TURN_180_IMMEDIATE        , {"Turn180Immediate"},         {} }},
    { CommandType::FOLLOW_PLAYER_WITH_JUMP   , {  CommandType::FOLLOW_PLAYER_WITH_JUMP   , {"FollowPlayerWithJump"},     {{"Ticks", ParamType::UINT8}} }},
    { CommandType::JUMP                      , {  CommandType::JUMP                      , {"Jump"},                     {} }},
    { CommandType::ENABLE_ROTATION           , {  CommandType::ENABLE_ROTATION           , {"EnableRotation", "EnableFrameUpdate"},   {} }},
    { CommandType::DISABLE_ROTATION          , {  CommandType::DISABLE_ROTATION          , {"DisableRotation", "DisableFrameUpdate"}, {} }},
    { CommandType::MOVE_RANDOM_TIME          , {  CommandType::MOVE_RANDOM_TIME          , {"MoveRandomTime", "MoveRandomTimed"},     {{"Range", ParamType::UINT8}, {"BaseTicks", ParamType::UINT8}} }},
    { CommandType::RUN_SPECIAL_AI            , {  CommandType::RUN_SPECIAL_AI            , {"RunSpecialAI", "LoadSpecialAI"},         {} }},
    { CommandType::FOLLOW_PLAYER_NO_JUMP     , {  CommandType::FOLLOW_PLAYER_NO_JUMP     , {"FollowPlayerNoJump"},       {{"Ticks", ParamType::UINT8}} }},
    { CommandType::LOOT                      , {  CommandType::LOOT                      , {"Loot", "UnknownB2D"},       {} }},
    { CommandType::SHOP_ITEM                 , {  CommandType::SHOP_ITEM                 , {"ShopItem"},                 {} }},
    { CommandType::THROWN_SHOP_ITEM          , {  CommandType::THROWN_SHOP_ITEM          , {"ThrownShopItem", "UnknownB2F"},  {} }},
    { CommandType::MOVE_UP_RELATIVE          , {  CommandType::MOVE_UP_RELATIVE          , {"MoveUpRelative"},           {{"Steps", ParamType::COORDINATE}} }},
    { CommandType::MOVE_DOWN_RELATIVE        , {  CommandType::MOVE_DOWN_RELATIVE        , {"MoveDownRelative"},         {{"Steps", ParamType::COORDINATE}} }},
    { CommandType::PUT_DOWN_SHOP_ITEM        , {  CommandType::PUT_DOWN_SHOP_ITEM        , {"PutDownShopItem", "UnknownB32"}, {{"Phase", ParamType::UINT8}} }},
    { CommandType::FLEE_PLAYER               , {  CommandType::FLEE_PLAYER               , {"FleePlayer", "UnknownB33"},      {{"Ticks", ParamType::UINT8}} }},
    { CommandType::PLAYBACK_INPUT            , {  CommandType::PLAYBACK_INPUT            , {"PlaybackInput"},            {{"InputScript", ParamType::UINT8}} }},
    { CommandType::RESET_PLAYBACK            , {  CommandType::RESET_PLAYBACK            , {"ResetPlayback"},            {} }},
    { CommandType::WAIT_FOR_CONDITION        , {  CommandType::WAIT_FOR_CONDITION        , {"WaitForCondition"},         {{"Condition", ParamType::UINT8}} }},
    { CommandType::PAUSE_4_SECONDS           , {  CommandType::PAUSE_4_SECONDS           , {"Pause4s"},                  {} }},
    { CommandType::ENABLE_GRAVITY            , {  CommandType::ENABLE_GRAVITY            , {"EnableGravity"},            {} }},
    { CommandType::DISABLE_GRAVITY           , {  CommandType::DISABLE_GRAVITY           , {"DisableGravity"},           {} }},
    { CommandType::MOVE_UP_TIMED             , {  CommandType::MOVE_UP_TIMED             , {"MoveUpTimed"},              {{"Ticks", ParamType::UINT8}} }},
    { CommandType::MOVE_DOWN_TIMED           , {  CommandType::MOVE_DOWN_TIMED           , {"MoveDownTimed"},            {{"Ticks", ParamType::UINT8}} }},
    { CommandType::MOVE_UP_ABSOLUTE          , {  CommandType::MOVE_UP_ABSOLUTE          , {"MoveUpAbsolute"},           {{"Z", ParamType::COORDINATE}} }},
    { CommandType::MOVE_DOWN_ABSOLUTE        , {  CommandType::MOVE_DOWN_ABSOLUTE        , {"MoveDownAbsolute"},         {{"Z", ParamType::COORDINATE}} }},
    { CommandType::DESPAWN_ENTITY            , {  CommandType::DESPAWN_ENTITY            , {"DespawnEntity", "RemoveSprite"},         {} }},
    { CommandType::MOVE_UP_UNTIL_COLLISION   , {  CommandType::MOVE_UP_UNTIL_COLLISION   , {"MoveUpUntilCollision", "NudgeUp"},       {} }},
    { CommandType::MOVE_DOWN_UNTIL_COLLISION , {  CommandType::MOVE_DOWN_UNTIL_COLLISION , {"MoveDownUntilCollision"},   {} }},
    { CommandType::SET_FLAG                  , {  CommandType::SET_FLAG                  , {"SetFlag"},                  {{"Flag", ParamType::FLAG}} }},
    { CommandType::WAIT_FOR_FLAG_SET         , {  CommandType::WAIT_FOR_FLAG_SET         , {"WaitForFlagSet"},           {{"Flag", ParamType::FLAG}} }},
    { CommandType::CLEAR_FLAG                , {  CommandType::CLEAR_FLAG                , {"ClearFlag"},                {{"Flag", ParamType::FLAG}} }},
    { CommandType::HIDE                      , {  CommandType::HIDE                      , {"Hide"},                     {} }},
    { CommandType::SHOW_WHEN_COLLISION_CLEAR , {  CommandType::SHOW_WHEN_COLLISION_CLEAR , {"ShowWhenCollisionClear"},   {} }},
    { CommandType::WAIT_FOR_FLAG_CLEAR       , {  CommandType::WAIT_FOR_FLAG_CLEAR       , {"WaitForFlagClear"},         {{"Flag", ParamType::FLAG}} }},
    { CommandType::WAIT_SPRITE_NOT_HOSTILE   , {  CommandType::WAIT_SPRITE_NOT_HOSTILE   , {"WaitSpriteNotHostile", "UnknownB47"},    {{"Slot", ParamType::UINT8}} }},
    { CommandType::SET_ENTITY_SPEED          , {  CommandType::SET_ENTITY_SPEED          , {"SetEntitySpeed", "SetObjectSpeed"},      {{"Slot", ParamType::UINT8}, {"Speed", ParamType::UINT8}} }},
    { CommandType::ACTIVATE_SWITCH           , {  CommandType::ACTIVATE_SWITCH           , {"ActivateSwitch"},           {} }},
    { CommandType::RESET_SWITCH              , {  CommandType::RESET_SWITCH              , {"ResetSwitch"},              {} }},
    { CommandType::MOVE_TO_XY_POS_IMMEDIATE  , {  CommandType::MOVE_TO_XY_POS_IMMEDIATE  , {"MoveToXYPosImmediate", "MoveToXYPosImmedite"}, {{"X", ParamType::COORDINATE}, {"Y", ParamType::COORDINATE}} }},
    { CommandType::MOVE_TO_Z_POS_IMMEDIATE   , {  CommandType::MOVE_TO_Z_POS_IMMEDIATE   , {"MoveToZPosImmediate"},      {{"Z", ParamType::COORDINATE}} }},
    { CommandType::RESET_TO_INIT_PARAMS      , {  CommandType::RESET_TO_INIT_PARAMS      , {"ResetToInitParams", "ResetToInitialPos"},  {} }},
    { CommandType::START_LO_CUTSCENE         , {  CommandType::START_LO_CUTSCENE         , {"StartLoCutscene", "StartCutscene"},        {{"Cutscene", ParamType::LOW_CUTSCENE}} }},
    { CommandType::MOVE_NO_CLIP              , {  CommandType::MOVE_NO_CLIP              , {"MoveNoClip"},               {{"Steps", ParamType::UINT8}} }},
    { CommandType::ROTATE_PLAYER             , {  CommandType::ROTATE_PLAYER             , {"RotatePlayer"},             {{"Direction", ParamType::UINT8}} }},
    { CommandType::MAKE_HOSTILE              , {  CommandType::MAKE_HOSTILE              , {"MakeHostile"},              {} }},
    { CommandType::MAKE_NON_HOSTILE          , {  CommandType::MAKE_NON_HOSTILE          , {"MakeNonHostile"},           {} }},
    { CommandType::DISABLE_WALK_BACKWARDS    , {  CommandType::DISABLE_WALK_BACKWARDS    , {"DisableWalkBackwards", "EnableBackwardsMovement"},  {} }},
    { CommandType::ENABLE_WALK_BACKWARDS     , {  CommandType::ENABLE_WALK_BACKWARDS     , {"EnableWalkBackwards", "DisableBackwardsMovement"},  {} }},
    { CommandType::SPECIAL_ANIMATION         , {  CommandType::SPECIAL_ANIMATION         , {"SpecialAnimation"},         {{"Phase", ParamType::UINT8}} }},
    { CommandType::MOVE_UP_TO_INIT_POS       , {  CommandType::MOVE_UP_TO_INIT_POS       , {"MoveUpToInitPos"},          {} }},
    { CommandType::TRIGGER_TILE_SWAP         , {  CommandType::TRIGGER_TILE_SWAP         , {"TriggerTileSwap"},          {{"Swap", ParamType::UINT8}} }},
    { CommandType::UPDATE_SPRITE_FACING      , {  CommandType::UPDATE_SPRITE_FACING      , {"UpdateSpriteFacing", "UpdateSpriteOrientation"},    {} }},
    { CommandType::PRINT_TEXT                , {  CommandType::PRINT_TEXT                , {"PrintText"},                {{"String", ParamType::UINT16}} }},
    { CommandType::PROJECTILE_HIT_ENEMIES    , {  CommandType::PROJECTILE_HIT_ENEMIES    , {"ProjectileHitEnemies", "Unknown5A"},     {{"Ticks", ParamType::UINT8}} }},
    { CommandType::SET_TARGET_POSITION       , {  CommandType::SET_TARGET_POSITION       , {"SetTargetPosition"},        {{"X", ParamType::LONG_COORDINATE}, {"Y", ParamType::LONG_COORDINATE}} }},
    { CommandType::MOVE_TO_TARGET_POSITION   , {  CommandType::MOVE_TO_TARGET_POSITION   , {"MoveToTargetPosition"},     {} }},
    { CommandType::REPEAT_BEGIN              , {  CommandType::REPEAT_BEGIN              , {"RepeatBegin"},              {{"Repetitions", ParamType::UINT8}} }},
    { CommandType::REPEAT_END                , {  CommandType::REPEAT_END                , {"RepeatEnd"},                {} }},
    { CommandType::DECAY_FLASH               , {  CommandType::DECAY_FLASH               , {"DecayFlash"},               {{"Phase", ParamType::UINT8}} }},
    { CommandType::FLASH_SPIN_APPEAR         , {  CommandType::FLASH_SPIN_APPEAR         , {"FlashSpinAppear"},          {{"Phase", ParamType::UINT8}} }},
    { CommandType::FLASH_SPIN_DISAPPEAR      , {  CommandType::FLASH_SPIN_DISAPPEAR      , {"FlashSpinDisappear"},       {{"Phase", ParamType::UINT8}} }},
    { CommandType::PLAY_SOUND                , {  CommandType::PLAY_SOUND                , {"PlaySound"},                {{"Sound", ParamType::SOUND}} }},
    { CommandType::PROJECTILE_MOVE           , {  CommandType::PROJECTILE_MOVE           , {"ProjectileMove", "UnknownB63"},          {{"Ticks", ParamType::UINT8}} }},
    { CommandType::START_HI_CUTSCENE         , {  CommandType::START_HI_CUTSCENE         , {"StartHiCutscene"},          {{"Cutscene", ParamType::HIGH_CUTSCENE}} }},
    { CommandType::PROJECTILE_DIAG_RIGHT     , {  CommandType::PROJECTILE_DIAG_RIGHT     , {"ProjectileDiagRight"},      {{"Ticks", ParamType::UINT8}} }},
    { CommandType::PROJECTILE_DIAG_LEFT      , {  CommandType::PROJECTILE_DIAG_LEFT      , {"ProjectileDiagLeft"},       {{"Ticks", ParamType::UINT8}} }},
    { CommandType::PROJECTILE_LEVEL          , {  CommandType::PROJECTILE_LEVEL          , {"ProjectileLevel"},          {} }},
    { CommandType::NULL_COMMAND              , {  CommandType::NULL_COMMAND              , {"Null"},                     {} }}
};

std::unordered_map<std::string, Behaviours::CommandType> Behaviours::commands_by_name;

const Behaviours::CommandDefinition& Behaviours::GetCommand(CommandType cmd)
{
    return COMMANDS_BY_ID.at(cmd);
}

const Behaviours::CommandDefinition& Behaviours::GetCommandById(int id)
{
    return COMMANDS_BY_ID.at(static_cast<CommandType>(id));
}

const Behaviours::CommandDefinition& Behaviours::GetCommandByName(const std::string& name)
{
    if (commands_by_name.empty())
    {
        for (const auto& command : COMMANDS_BY_ID)
        {
            for (const auto& alias : command.second.aliases)
            {
                commands_by_name[alias] = command.first;
            }
        }
    }
    try
    {
        const auto& result = GetCommand(commands_by_name.at(name));
        return result;
    }
    catch (const std::exception&)
    {
        throw std::runtime_error("Unknown command \"" + name + "\"");
    }
}

std::map<int, std::pair<std::string, std::vector<Behaviours::Command>>> Behaviours::Unpack(const std::vector<uint8_t>& offsets, const std::vector<uint8_t>& behaviour_table)
{
    std::map<int, std::vector<uint8_t>> behaviours_raw;
    std::map<int, std::pair<std::string, std::vector<Behaviours::Command>>> behaviours_decoded;

    std::size_t file_pos = 0;
    int i = 0;
    for (const uint8_t offset : offsets)
    {
        std::vector<uint8_t> behav(behaviour_table.begin() + file_pos, behaviour_table.begin() + file_pos + offset);
        behaviours_raw[i++] = behav;
        file_pos += offset;
    }

    for (const auto& b : behaviours_raw)
    {
        int j = 0;
        std::unordered_map<int, int> labels;
        behaviours_decoded.insert({ b.first, {std::string("Behaviour") + std::to_string(b.first), {}} });
        while (j < static_cast<int>(b.second.size()))
        {
            labels[j] = static_cast<int>(labels.size() + 1);
            const auto& cmd = Behaviours::GetCommandById(b.second.at(j));
            behaviours_decoded[b.first].second.push_back({ cmd.id, {} });
            for (const auto& p : cmd.params)
            {
                Behaviours::ParameterValue value;
                switch (p.second)
                {
                case Behaviours::ParamType::UINT8:
                case Behaviours::ParamType::SOUND:
                case Behaviours::ParamType::LOW_CUTSCENE:
                    value = b.second[++j];
                    break;
                case Behaviours::ParamType::INT8:
                    value = static_cast<int8_t>(b.second[++j]);
                    break;
                case Behaviours::ParamType::HIGH_CUTSCENE:
                    value = b.second[++j] + 256;
                    break;
                case Behaviours::ParamType::UINT16:
                    value = (b.second[static_cast<uint8_t>(j + 1)] << 8) | b.second[static_cast<uint8_t>(j + 2)];
                    j += 2;
                    break;
                case Behaviours::ParamType::LABEL:
                {
                    int bpos = static_cast<int8_t>(b.second[++j]) - 1;
                    if (labels.find(j + bpos) != labels.cend())
                    {
                        value = labels.at(j + bpos);
                    }
                    break;
                }
                case Behaviours::ParamType::COORDINATE:
                    value = static_cast<double>(b.second[++j]) / 16.0;
                    break;
                case Behaviours::ParamType::FLAG:
                {
                    int byte = b.second[++j];
                    int bit = b.second[++j];
                    value = (byte << 3) | (bit & 7);
                    break;
                }
                case Behaviours::ParamType::LONG_COORDINATE:
                    value = static_cast<double>(((b.second[static_cast<uint8_t>(j + 1)] << 8) | b.second[static_cast<uint8_t>(j + 2)]) / 256.0);
                    j += 2;
                    break;
                case Behaviours::ParamType::NONE:
                    throw std::logic_error("Unexpected Parameter Type <NONE>");
                }
                behaviours_decoded[b.first].second.back().params.push_back({ p.first, value, p.second });
            }
            ++j;
        }
    }

    return behaviours_decoded;
}

std::pair<std::vector<uint8_t>, std::vector<uint8_t>> Behaviours::Pack(const std::map<int, std::pair<std::string, std::vector<Behaviours::Command>>>& behaviours)
{
    std::size_t cur_offset = 0;
    std::vector<uint8_t> offset_bytes;
    std::vector<uint8_t> behaviour_bytes;

    for (const auto& b : behaviours)
    {
        std::unordered_map<int, int> labels;
        for (const auto& c : b.second.second)
        {
            labels.insert({ static_cast<int>(labels.size() + 1), static_cast<int>(behaviour_bytes.size()) });
            behaviour_bytes.push_back(static_cast<uint8_t>(c.command));
            for (const auto& p : c.params)
            {
                const auto& param_value = std::get<1>(p);
                const auto& param_type = std::get<2>(p);
                switch (param_type)
                {
                case Behaviours::ParamType::UINT8:
                case Behaviours::ParamType::INT8:
                case Behaviours::ParamType::SOUND:
                case Behaviours::ParamType::LOW_CUTSCENE:
                    behaviour_bytes.push_back(static_cast<uint8_t>(std::get<int>(param_value)));
                    break;
                case Behaviours::ParamType::HIGH_CUTSCENE:
                    behaviour_bytes.push_back(static_cast<uint8_t>(std::get<int>(param_value) - 256));
                    break;
                case Behaviours::ParamType::UINT16:
                    behaviour_bytes.push_back(static_cast<uint8_t>(std::get<int>(param_value) >> 8));
                    behaviour_bytes.push_back(static_cast<uint8_t>(std::get<int>(param_value) & 0xFF));
                    break;
                case Behaviours::ParamType::LABEL:
                    if (labels.find(std::get<int>(param_value)) != labels.cend())
                    {
                        behaviour_bytes.push_back(static_cast<uint8_t>(labels.find(std::get<int>(param_value))->second - behaviour_bytes.size() + 1));
                    }
                    else
                    {
                        assert(false);
                    }
                    break;
                case Behaviours::ParamType::FLAG:
                    behaviour_bytes.push_back(static_cast<uint8_t>(std::get<int>(param_value) >> 3));
                    behaviour_bytes.push_back(static_cast<uint8_t>(std::get<int>(param_value) & 0x07));
                    break;
                case Behaviours::ParamType::COORDINATE:
                    behaviour_bytes.push_back(static_cast<uint8_t>(std::get<double>(param_value) * 16.0));
                    break;
                case Behaviours::ParamType::LONG_COORDINATE:
                    behaviour_bytes.push_back(static_cast<uint8_t>(std::get<double>(param_value)));
                    behaviour_bytes.push_back(static_cast<uint8_t>(std::get<double>(param_value) * 256.0));
                    break;
                case Behaviours::ParamType::NONE:
                    throw std::logic_error("Unexpected Parameter Type <NONE>");
                }
            }
        }
        offset_bytes.push_back(static_cast<uint8_t>(behaviour_bytes.size() - cur_offset));
        cur_offset = behaviour_bytes.size();
    }
    return { offset_bytes, behaviour_bytes };
}

} // namespace Landstalker
