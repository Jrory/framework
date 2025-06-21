cmake_minimum_required(VERSION 3.12)
project(MyProject
    VERSION 1.0.0
    LANGUAGES C CXX
)

find_package(ZLIB REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost 1.74 REQUIRED COMPONENTS thread)

# 1. 设置编译选项
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 2. 设置输出目录（可执行文件、库文件）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(PROJECT_DIR "${CMAKE_SOURCE_DIR}/..")

include_directories(
    ${PROJECT_DIR}
    /usr/include
)

set(PROTO_FILE "${PROJECT_DIR}/proto/message.proto")
get_filename_component(PROTO_DIR "${PROTO_FILE}" DIRECTORY)
get_filename_component(PROTO_NAME "${PROTO_FILE}" NAME_WE)

set(PROTO_SRCS "${PROJECT_DIR}/proto/${PROTO_NAME}.pb.cc")
set(PROTO_HDRS "${PROJECT_DIR}/proto/${PROTO_NAME}.pb.h")

add_library(common_objects OBJECT
    ${CMAKE_SOURCE_DIR}/ConnectionManager.cpp
    ${CMAKE_SOURCE_DIR}/MessageDispatcher.cpp
    ${CMAKE_SOURCE_DIR}/ThreadPool.cpp
    ${CMAKE_SOURCE_DIR}/MessageBufferPool.cpp
    ${CMAKE_SOURCE_DIR}/WrapTcpClient.cpp
    ${CMAKE_SOURCE_DIR}/WrapTcpServer.cpp
    ${PROTO_SRCS}
)

find_library(BASE_LIB base
    PATHS ${PROJECT_DIR}/common/lib
)

find_library(NET_LIB net
    PATHS ${PROJECT_DIR}/common/lib
)

file(GLOB SAMPLE_SOURCES "${PROJECT_DIR}/tcp/app*.cpp")
foreach(sample_source ${SAMPLE_SOURCES})
    get_filename_component(sample_name ${sample_source} NAME_WE)
    add_executable(${sample_name} ${sample_source} $<TARGET_OBJECTS:common_objects>)
    target_link_libraries(${sample_name}
        PRIVATE
        ${NET_LIB}
        ${BASE_LIB}
        Threads::Threads 
        Boost::thread
        protobuf
        ${ZLIB_LIBRARIES}
    )
endforeach()