set(SOURCES "")
set(LIBRARIES "tarxx" "utils")

if (WITH_LZ4)
    list(APPEND SOURCES lz4.cpp)
endif()

list(APPEND SOURCES tar.cpp)

set(TARGET unit-tests)
find_package(GTest REQUIRED)
pkg_check_modules(GMock REQUIRED "gmock")
list(APPEND LIBRARIES GTest::GTest GTest::Main)

enable_testing()

add_executable(
        ${TARGET}
        ${SOURCES}
)

target_link_libraries(
        ${TARGET}
        ${LIBRARIES}
)

add_custom_target(
        unit-test-run
        ${TARGET}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS unit-tests
)

set_target_properties(${TARGET} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
