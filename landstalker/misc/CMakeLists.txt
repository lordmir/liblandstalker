cmake_minimum_required(VERSION 3.16)

target_sources(${MODULE_NAME} PRIVATE
    "src/BitBarrel.cpp"
    "src/BitBarrelWriter.cpp"
    "src/DefaultLabels.cpp"
    "src/Labels.cpp"
    "src/LZ77.cpp"
    "src/Utils.cpp"
)
