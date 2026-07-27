cmake_minimum_required(VERSION 3.28)

target_sources(${MODULE_NAME} PRIVATE
    "AsmFunctionTable.cpp"
    "ItemUseTable.cpp"
    "ProgressFlags.cpp"
    "RoomActionTable.cpp"
    "Script.cpp"
    "ScriptFunction.cpp"
    "ScriptFunctionTable.cpp"
    "ScriptStatements.cpp"
    "ScriptTable.cpp"
    "ScriptTableEntry.cpp"
)
