add_library(maxrtos_kernel STATIC
    src/scheduler.c
    src/process.c
    src/frame.c
    src/dispatch.c
    src/partition.c
    src/health_monitor.c
)

target_include_directories(maxrtos_kernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_options(maxrtos_kernel PRIVATE
    -Wall -Wextra -Wpedantic
)

if(MAXRTOS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
