cmake_minimum_required(VERSION 3.8)
project(tarxx VERSION 0.1.0 LANGUAGES CXX)

option(WITH_LZ4 "Set to ON to enable LZ4 compression support" OFF)
option(WITH_EXAMPLE "Set to ON to build the example" OFF)

set(LIB_NAME tarxx)

add_library(${LIB_NAME} INTERFACE)
target_include_directories(${LIB_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
if(${CMAKE_VERSION} VERSION_LESS "3.19.0")
    target_compile_features(${LIB_NAME} INTERFACE cxx_std_17)
else()
    set_target_properties(${LIB_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
endif()

if (WITH_LZ4)
    target_compile_definitions(${LIB_NAME} INTERFACE ENABLE_LZ4=ON)

    find_package(PkgConfig REQUIRED)
    find_library(LZ4_LIBRARY
            NAMES lz4_static
            PATHS ${LZ4_LIBRARY_DIRS} ${LZ4_INCLUDE_DIRS}
            REQUIRED)
endif()

message(${LZ4_LIBRARY_DIRS})
if (WITH_EXAMPLE)
    add_executable(tarxx-example tarxx-example.cpp)
    set_target_properties(tarxx-example PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF)
endif()
target_link_libraries(tarxx-example tarxx lz4)
