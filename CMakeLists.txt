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
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 3. 全局包含路径（所有子模块都能访问）
include_directories(${CMAKE_SOURCE_DIR}/common)

# 4. 构建 samples 目录下的所有示例程序
file(GLOB SAMPLE_SOURCES "examples/*.cc")
foreach(sample_source ${SAMPLE_SOURCES})
    get_filename_component(sample_name ${sample_source} NAME_WE)
    add_executable(${sample_name} ${sample_source})
    target_link_directories(${sample_name} PRIVATE ${CMAKE_SOURCE_DIR}/common/lib)
    target_link_libraries(${sample_name} net base)  # 链接 net 和 base
endforeach()