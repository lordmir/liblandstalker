cmake_minimum_required(VERSION 3.16)

target_sources(${MODULE_NAME} PRIVATE
    "src/Chests.cpp"
    "src/Entity.cpp"
    "src/Flags.cpp"
    "src/Room.cpp"
    "src/RoomDialogueTable.cpp"
    "src/WarpList.cpp"
)
