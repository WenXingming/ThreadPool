# ============================================================================
# CMakeLists.txt for dedup tests
# ============================================================================

file(GLOB_RECURSE DEDUP_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(dedupTests
    ${DEDUP_TEST_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/dedup/FileWalker.cpp
    ${PROJECT_SOURCE_DIR}/src/dedup/Hasher.cpp
)

target_include_directories(dedupTests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/dedup
)

target_link_libraries(dedupTests PRIVATE
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(dedupTests)
