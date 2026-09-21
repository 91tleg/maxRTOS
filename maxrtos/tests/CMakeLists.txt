cmake_minimum_required(VERSION 3.20)

project(maxrtos_tests C)

enable_testing()

# Instrumentation, opt-in:
#   -DMAXRTOS_TEST_COVERAGE=ON          gcov-compatible line/branch coverage
#   -DMAXRTOS_TEST_SANITIZE=undefined   UBSan over everything, including SIL
#   -DMAXRTOS_TEST_SANITIZE=address     ASan + UBSan over the unit tests only:
#       the SIL virtual target runs process contexts on caller-provided
#       thread stacks, which AddressSanitizer does not support.
option(MAXRTOS_TEST_COVERAGE "Build tests with coverage instrumentation" OFF)
set(MAXRTOS_TEST_SANITIZE "" CACHE STRING "Sanitizer: '', 'undefined' or 'address'")

if(MAXRTOS_TEST_COVERAGE)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

set(MAXRTOS_BUILD_SIL ON)

if(MAXRTOS_TEST_SANITIZE STREQUAL "undefined")
    add_compile_options(-fsanitize=undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=undefined)
elseif(MAXRTOS_TEST_SANITIZE STREQUAL "address")
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
    set(MAXRTOS_BUILD_SIL OFF)
elseif(NOT MAXRTOS_TEST_SANITIZE STREQUAL "")
    message(FATAL_ERROR "MAXRTOS_TEST_SANITIZE must be '', 'undefined' or 'address'")
endif()

add_subdirectory(unit)
if(MAXRTOS_BUILD_SIL)
    add_subdirectory(sil)
endif()
