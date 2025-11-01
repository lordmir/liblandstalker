cmake_minimum_required(VERSION 3.16)

target_sources(${MODULE_NAME} PRIVATE
    "BlockmapIsometric.cpp"
    "Doors.cpp"
#    "MapToTmx.cpp"
#    "RoomToTmx.cpp"
    "Tilemap3DCmp.cpp"
    "TileSwaps.cpp"
)
