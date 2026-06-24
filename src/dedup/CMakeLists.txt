# ============================================================================
# dedup/CMakeLists - dedup CLI target
# ============================================================================

file(GLOB_RECURSE DEDUP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file(GLOB_RECURSE DEDUP_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_executable(dedup ${DEDUP_SOURCES} ${DEDUP_HEADERS})
target_include_directories(dedup PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dedup PRIVATE ThreadPool CLI11::CLI11)
