cmake_minimum_required(VERSION 3.16)

target_sources(${MODULE_NAME} PRIVATE
    "src/Blockmap2D.cpp"
    "src/Tilemap.cpp"
    "src/Tilemap2DRLE.cpp"
)
