cmake_minimum_required(VERSION 3.10)
project(thread_pool)

# 设置 C++ 标准。设置 C++ 标准是一个好习惯，这能确保你的代码在不同编译器上行为一致，并能使用最新的语言特性。
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译生成可执行文件
set(source_dir ${CMAKE_CURRENT_SOURCE_DIR})
file(GLOB source_files ${source_dir}/*.cpp) # CMake 不会自动解析通配符，因此必须先通过 file(GLOB ...) 获取文件列表
add_executable(test ${source_files})

# 链接 -lpthread。Linux 下 std::thread 依赖 pthread
find_package(Threads REQUIRED)
target_link_libraries(test PRIVATE Threads::Threads)