set(SOURCES "")
set(LIBRARIES "tarxx" "utils")

if (WITH_LZ4)
    list(APPEND SOURCES tarxx-example-lz4.cpp)
endif()

list(APPEND SOURCES tarxx-example-tar.cpp)

set(TARGET component-tests)
find_package(GTest REQUIRED)
pkg_check_modules(GMock REQUIRED "gmock")
enable_testing()

add_executable(
        ${TARGET}
        ${SOURCES}
)
add_dependencies(${TARGET} ${EXAMPLE_NAME})


target_link_libraries(
        ${TARGET}
        GTest::GTest
        GTest::Main
        ${GMock_LIBRARIES}
        ${LIBRARIES}
)

target_compile_definitions(${TARGET} PRIVATE -DTARXX_EXAMPLE_BINARY_PATH="${CMAKE_BINARY_DIR}/${EXAMPLE_NAME}")
target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ctest grabs stdout, tests may fail in some runners i.e. clion due to this
add_custom_target(
        component-test-run
        ${TARGET}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS component-tests
)

set_target_properties(${TARGET} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

include(GoogleTest)
gtest_discover_tests(${TARGET})
