# ============================================================================
# CMakeLists.txt for ThreadPool Tests
# ============================================================================

# 引入 CMake 的 FetchContent 模块，用于下载和管理外部依赖
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # 确保 Windows 使用动态链接库
FetchContent_MakeAvailable(googletest)

# 构建测试可执行文件
file(GLOB_RECURSE THREAD_POOL_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(threadPoolTests ${THREAD_POOL_TEST_SOURCES})

target_link_libraries(threadPoolTests PRIVATE 
        ThreadPool 
        GTest::gtest_main
)

# 启用 CTest 测试框架，并自动发现和注册测试用例
include(GoogleTest)

gtest_discover_tests(threadPoolTests)
