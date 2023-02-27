cmake_minimum_required(VERSION 3.8)
project(tarxx VERSION 0.2.0 LANGUAGES CXX)

IF (WIN32)
    message(FATAL_ERROR Windows is not support at the moment)
ENDIF()

option(WITH_COMPRESSION "Set to ON to enable compression support without any algorithms" OFF)
option(WITH_EXAMPLE "Set to ON to build the example" OFF)
option(WITH_TESTS "Set to ON to build tests" OFF)
option(WITH_LZ4 "Set to ON to enable lz4 support" OFF)

set(LIB_NAME tarxx)
set(${LIB_NAME}_COMPILE_DEFINITIONS "")
set(${LIB_NAME}_LINK_LIBRARIES "")
set(${LIB_NAME}_INCLUDE_DIRECTORIES "")

find_package(PkgConfig REQUIRED)

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

if (WITH_COMPRESSION)
    list(APPEND ${LIB_NAME}_COMPILE_DEFINITIONS WITH_COMPRESSION=ON)
endif()

if (WITH_LZ4)
    if (NOT WITH_COMPRESSION)
        message(FATAL_ERROR "LZ4 option needs compression to be turned on with '-DCOMPRESSION=ON'")
    endif()
    find_library(LZ4 liblz4.a REQUIRED)

    list(APPEND ${LIB_NAME}_COMPILE_DEFINITIONS WITH_LZ4=ON)
    list(APPEND ${LIB_NAME}_LINK_LIBRARIES ${LZ4})
    list(APPEND ${LIB_NAME}_INCLUDE_DIRECTORIES ${LZ4_INCLUDE_DIRS})
endif()

target_compile_definitions(${LIB_NAME} INTERFACE ${${LIB_NAME}_COMPILE_DEFINITIONS})
target_link_libraries(${LIB_NAME} INTERFACE ${${LIB_NAME}_LINK_LIBRARIES})
target_include_directories(${LIB_NAME} INTERFACE ${${LIB_NAME}_INCLUDE_DIRECTORIES})

if (WITH_EXAMPLE)
    set(EXAMPLE_NAME tarxx-example)
    add_executable(${EXAMPLE_NAME} tarxx-example.cpp)
    set_target_properties(${EXAMPLE_NAME} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF)

    target_link_libraries(${EXAMPLE_NAME} tarxx)
    target_compile_definitions(${EXAMPLE_NAME} PRIVATE ${${LIB_NAME}_COMPILE_DEFINITIONS})
endif()

if (WITH_TESTS)
    add_subdirectory(tests)
endif()

message(STATUS "WITH_EXAMPLE = ${WITH_EXAMPLE}" )
message(STATUS "WITH_TESTS = ${WITH_TESTS}" )
message(STATUS "WITH_COMPRESSION = ${WITH_COMPRESSION}" )
message(STATUS "WITH_LZ4 = ${WITH_LZ4}" )

