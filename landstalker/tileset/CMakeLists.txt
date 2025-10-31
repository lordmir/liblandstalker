cmake_minimum_required(VERSION 3.16)

target_sources(${MODULE_NAME} PRIVATE
    "src/AnimatedTileset.cpp"
    "src/Tile.cpp"
    "src/TileAttributes.cpp"
    "src/Tileset.cpp"
)
