cmake_minimum_required(VERSION 3.28)

target_sources(${MODULE_NAME} PRIVATE
    "ProgressFlags.cpp"
    "Script.cpp"
    "ScriptFunction.cpp"
    "ScriptTable.cpp"
    "ScriptTableEntry.cpp"
)
