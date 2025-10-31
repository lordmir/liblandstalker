cmake_minimum_required(VERSION 3.16)

target_sources(${MODULE_NAME} PRIVATE
    "src/AsmFile.cpp"
    "src/AsmUtils.cpp"
    "src/DataManager.cpp"
    "src/DataTypes.cpp"
    "src/EntryPreferences.cpp"
    "src/GameData.cpp"
    "src/GraphicsData.cpp"
    "src/ImageBuffer.cpp"
    "src/Rom.cpp"
    "src/RomLabels.cpp"
    "src/RomOffsets.cpp"
    "src/RoomData.cpp"
    "src/ScriptData.cpp"
    "src/SpriteData.cpp"
    "src/StringData.cpp"
)
