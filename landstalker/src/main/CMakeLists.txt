cmake_minimum_required(VERSION 3.28)

target_sources(${MODULE_NAME} PRIVATE
    "AsmFile.cpp"
    "AsmUtils.cpp"
    "DataManager.cpp"
    "DataTypes.cpp"
    "EntryPreferences.cpp"
    "GameData.cpp"
    "GraphicsData.cpp"
    "ImageBuffer.cpp"
    "Rom.cpp"
    "RomLabels.cpp"
    "RomOffsets.cpp"
    "RoomData.cpp"
    "ScriptData.cpp"
    "SpriteData.cpp"
    "StringData.cpp"
)
