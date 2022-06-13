cmake_minimum_required(VERSION 3.8)
project(tarxx VERSION 0.1.0 LANGUAGES CXX)

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

add_subdirectory(lib/lz4/build/cmake)

add_executable(tarxx-example tarxx-example.cpp)
set_target_properties(tarxx-example PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

target_link_libraries(tarxx-example tarxx lz4_static)
