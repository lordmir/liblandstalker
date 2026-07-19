add_executable(lz77_tests test_lz77.cpp)
target_include_directories(lz77_tests PRIVATE ${CMAKE_SOURCE_DIR}/modules/liblandstalker/landstalker/include)
target_link_libraries(lz77_tests PRIVATE landstalker GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(lz77_tests)

add_executable(tilemap3d_tests test_tilemap3d.cpp)
target_include_directories(tilemap3d_tests PRIVATE ${CMAKE_SOURCE_DIR}/modules/liblandstalker/landstalker/include)
target_link_libraries(tilemap3d_tests PRIVATE landstalker GTest::gtest_main)

gtest_discover_tests(tilemap3d_tests)

add_executable(script_tests test_script.cpp)
target_include_directories(script_tests PRIVATE ${CMAKE_SOURCE_DIR}/modules/liblandstalker/landstalker/include)
target_link_libraries(script_tests PRIVATE landstalker yaml-cpp::yaml-cpp GTest::gtest_main)

gtest_discover_tests(script_tests)

add_executable(asm_file_tests test_asm_file.cpp)
target_include_directories(asm_file_tests PRIVATE ${CMAKE_SOURCE_DIR}/modules/liblandstalker/landstalker/include)
target_link_libraries(asm_file_tests PRIVATE landstalker GTest::gtest_main)

gtest_discover_tests(asm_file_tests)
