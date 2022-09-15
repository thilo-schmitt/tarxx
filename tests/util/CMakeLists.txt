set(LIB_NAME utils)

add_library(${LIB_NAME} INTERFACE)
target_include_directories(${LIB_NAME} INTERFACE "${CMAKE_CURRENT_LIST_DIR}/..")
