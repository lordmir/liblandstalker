add_executable(lz77_tests test_lz77.cpp)
target_include_directories(lz77_tests PRIVATE ${CMAKE_SOURCE_DIR}/modules/liblandstalker/landstalker/include)
target_link_libraries(lz77_tests PRIVATE landstalker GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(lz77_tests)

add_executable(tilemap3d_tests test_tilemap3d.cpp)
target_include_directories(tilemap3d_tests PRIVATE ${CMAKE_SOURCE_DIR}/modules/liblandstalker/landstalker/include)
target_link_libraries(tilemap3d_tests PRIVATE landstalker GTest::gtest_main)

gtest_discover_tests(tilemap3d_tests)
