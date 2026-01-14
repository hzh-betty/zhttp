cmake_minimum_required(VERSION 3.14)
project(zhttp_project VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 编译选项
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# 测试选项
option(BUILD_TESTING "Build tests" ON)

# 添加子项目
add_subdirectory(zlog)
add_subdirectory(zcoroutine)
add_subdirectory(znet)
add_subdirectory(zhttp)
