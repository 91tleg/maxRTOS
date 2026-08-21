add_subdirectory(kernel)

add_library(maxrtos INTERFACE)
target_link_libraries(maxrtos INTERFACE
    maxrtos_kernel
)
target_include_directories(maxrtos INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
