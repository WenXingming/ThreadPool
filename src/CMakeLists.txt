cmake_minimum_required(VERSION 3.10)
project(thread_pool)


# 编译生成可执行文件
file(GLOB source_files ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp) # CMake 不会自动解析通配符，因此必须先通过 file(GLOB ...) 获取文件列表，再传递给 add_executable
add_executable(test ${source_files})

# 链接 -lpthread。Linux 下 std::thread 依赖 pthread
find_package(Threads REQUIRED)
target_link_libraries(test PRIVATE Threads::Threads)