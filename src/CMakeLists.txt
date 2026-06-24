# ============================================================================
# src/CMakeLists - ThreadPool library target
# 核心功能和接口：
# 1. 收集线程池源文件和头文件。
# 2. 生成 ThreadPool 库并配置头文件包含路径。
# 3. 链接线程库依赖。
# ============================================================================

file(GLOB_RECURSE THREAD_POOL_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file(GLOB_RECURSE THREAD_POOL_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(ThreadPool ${THREAD_POOL_SOURCES} ${THREAD_POOL_HEADERS})
target_include_directories(ThreadPool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(ThreadPool PUBLIC Threads::Threads)
