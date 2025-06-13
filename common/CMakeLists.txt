cmake_minimum_required(VERSION 3.12)
project(MyProject
    VERSION 1.0.0
    LANGUAGES C CXX
)

# 1. 设置编译选项
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 2. 设置输出目录（可执行文件、库文件）
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 3. 全局包含路径（所有子模块都能访问）
include_directories(${CMAKE_SOURCE_DIR}/common)

# 4. 自动扫描 base 和 net 目录下的所有源文件
file(GLOB_RECURSE BASE_SOURCES "base/*.cc" "base/*.cpp")
file(GLOB_RECURSE BASE_HEADERS "base/*.h" "base/*.hpp")

file(GLOB_RECURSE NET_SOURCES "net/*.cpp" "net/*.cc" "net/poller/*.cpp" "net/poller/*.cc")
file(GLOB_RECURSE NET_HEADERS "net/*.h" "net/*.hpp"  "net/poller/*.h" "net/poller/*.hpp")

# 5. 构建 base 库（静态库）
add_library(base STATIC ${BASE_SOURCES} ${BASE_HEADERS})
target_include_directories(base PUBLIC
    ${CMAKE_SOURCE_DIR}/common/base
)

# 6. 构建 net 库（静态库，依赖 base）
add_library(net STATIC ${NET_SOURCES} ${NET_HEADERS})
target_include_directories(net PUBLIC
    ${CMAKE_SOURCE_DIR}/common/net
)
target_link_libraries(net PUBLIC base)  # net 依赖 base
