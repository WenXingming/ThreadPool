
# ============================================================================
# TopLevel CMakeLists - 项目顶层构建入口
# ============================================================================
# 核心功能和接口：
# 1. 配置全局 C++ 标准和测试开关。
# 2. 组织核心库、examples 和 test 子目录构建。
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(ThreadPool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_BENCHMARKS "Build benchmark targets" OFF)

include(FetchContent)
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG        v2.4.1
)
FetchContent_MakeAvailable(cli11)

include(CTest)

add_subdirectory(src/threadpool)
add_subdirectory(src/dedup)
add_subdirectory(examples)
if(BUILD_TESTING)
    add_subdirectory(test/threadpool)
    add_subdirectory(test/dedup)
endif()
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmark/dedup)
endif()
