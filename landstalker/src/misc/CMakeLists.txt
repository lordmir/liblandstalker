cmake_minimum_required(VERSION 3.16)

target_sources(${MODULE_NAME} PRIVATE
    "BitBarrel.cpp"
    "BitBarrelWriter.cpp"
    "DefaultLabels.cpp"
    "Labels.cpp"
    "LZ77.cpp"
    "Utils.cpp"
)
