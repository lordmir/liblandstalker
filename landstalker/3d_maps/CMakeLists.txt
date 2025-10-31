cmake_minimum_required(VERSION 3.16)

target_sources(${MODULE_NAME} PRIVATE
    "src/BlockmapIsometric.cpp"
    "src/Doors.cpp"
#    "src/MapToTmx.cpp"
#    "src/RoomToTmx.cpp"
    "src/Tilemap3DCmp.cpp"
    "src/TileSwaps.cpp"
)
