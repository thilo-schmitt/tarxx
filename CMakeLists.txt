cmake_minimum_required(VERSION 3.8)
project(tarxx VERSION 0.1.0 LANGUAGES CXX)

add_library(tarxx INTERFACE tarxx.h)
set_target_properties(tarxx PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

add_executable(tarxx-example tarxx-example.cpp)
target_link_libraries(tarxx-example tarxx)
