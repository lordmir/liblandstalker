add_executable(lz77_tests test_lz77.cpp)
target_include_directories(lz77_tests PRIVATE ${CMAKE_SOURCE_DIR}/modules/liblandstalker/landstalker/include)
target_link_libraries(lz77_tests PRIVATE landstalker GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(lz77_tests)
