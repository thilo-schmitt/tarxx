add_subdirectory(util)
add_subdirectory(unit-tests)

if (WITH_EXAMPLE)
    add_subdirectory(component-tests)
endif()
