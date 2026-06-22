# ============================================================================
# dedup benchmark target
# ============================================================================

include(FetchContent)

find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googlebenchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.9.5.tar.gz
    )
    FetchContent_MakeAvailable(googlebenchmark)
endif()

add_executable(dedupBenchmark
    DedupBenchmark.cpp
    ${PROJECT_SOURCE_DIR}/src/dedup/DuplicateFinder.cpp
    ${PROJECT_SOURCE_DIR}/src/dedup/FileWalker.cpp
    ${PROJECT_SOURCE_DIR}/src/dedup/Hasher.cpp
)

target_include_directories(dedupBenchmark PRIVATE
    ${PROJECT_SOURCE_DIR}/src/dedup
)

target_compile_features(dedupBenchmark PRIVATE cxx_std_17)

target_link_libraries(dedupBenchmark PRIVATE
    ThreadPool
    benchmark::benchmark
)
