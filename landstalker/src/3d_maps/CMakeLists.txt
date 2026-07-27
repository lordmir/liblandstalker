cmake_minimum_required(VERSION 3.28)

target_sources(${MODULE_NAME} PRIVATE
    "BlockmapIsometric.cpp"
    "Doors.cpp"
    "MapToTmx.cpp"
    "RoomToTmx.cpp"
    "RoomToYaml.cpp"
    "Tilemap3D.cpp"
    "Tilemap3DCompressor.cpp"
    "TileSwaps.cpp"
)
