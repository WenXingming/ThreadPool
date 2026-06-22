# ============================================================================
# CMakeLists.txt for dedup tests
# ============================================================================

file(GLOB_RECURSE DEDUP_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(dedupTests
    ${DEDUP_TEST_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/dedup/DuplicateFinder.cpp
    ${PROJECT_SOURCE_DIR}/src/dedup/FileWalker.cpp
    ${PROJECT_SOURCE_DIR}/src/dedup/Hasher.cpp
)

target_include_directories(dedupTests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/dedup
)

target_link_libraries(dedupTests PRIVATE
    ThreadPool
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(dedupTests)

find_program(BASH_EXECUTABLE bash REQUIRED)
add_test(
    NAME dedupCliTests
    COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_cli_tests.sh $<TARGET_FILE:dedup>
)
