add_library(maxrtos_kernel STATIC
    src/scheduler.c
    src/process.c
)

target_include_directories(maxrtos_kernel PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_options(maxrtos_kernel PRIVATE
    -Wall -Wextra -Wpedantic
)

if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(tests)
endif()
