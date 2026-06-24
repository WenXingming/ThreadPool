
# ============================================================================
# TopLevel CMakeLists - ThreadPool project entry
# ============================================================================
# 核心功能和接口：
# 1. 配置线程池库的 C++ 标准。
# 2. 作为顶层项目时构建 examples 和 test。
# 3. 作为 FetchContent 子项目时只暴露 ThreadPool target。
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(ThreadPool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src)

# 判断当前是否作为顶层独立项目构建。如果是通过 FetchContent 或 add_subdirectory 引入，则跳过测试和示例的编译。
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    option(THREADPOOL_BUILD_EXAMPLES "Build ThreadPool examples" ON)

    include(CTest)

    if(THREADPOOL_BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif()

    if(BUILD_TESTING)
        add_subdirectory(test)
    endif()
endif()
