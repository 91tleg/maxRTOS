# Sizes the kernel's static tables to a module.
#
# The kernel sizes its tables (partition table, process pool, ready queues,
# buffer pool, ...) from the MAXRTOS_MAX_* macros in maxrtos/config.h, whose
# defaults suit the largest configuration and are skipped for any macro that is
# already defined. `maxrtos-codegen --emit-limits` writes the smallest values
# that fit a module.json as maxrtos_limits.h. The kernel does not know about
# that file: the build force-includes it, so its definitions come first.
#
#   maxrtos_use_generated_limits(<maxrtos_limits.h> [<generation target>])
#
# Call it after add_subdirectory() of maxrtos and maxrtos/arch/cortex_m7. The
# header becomes a PUBLIC `-include` of maxrtos_kernel, so the kernel, the
# architecture layer and every target that links them see the same limits:
# they change the size of structures those share, so the header must never
# reach only some of them. If the generation step is a target of this
# project, name it so the kernel is compiled after the header exists.
#
# A project that builds several modules against one copy of the kernel (the
# QEMU and HIL test projects) does not call this, runs maxrtos-codegen without
# --emit-limits, and keeps the defaults.
function(maxrtos_use_generated_limits limits_header)
    if(NOT TARGET maxrtos_kernel)
        message(FATAL_ERROR
            "maxrtos_use_generated_limits(): call it after add_subdirectory() "
            "of the maxrtos kernel")
    endif()

    # SHELL: keeps the option and its argument together.
    target_compile_options(maxrtos_kernel PUBLIC
        "SHELL:-include ${limits_header}")

    if(ARGC GREATER 1)
        add_dependencies(maxrtos_kernel ${ARGV1})
    endif()
endfunction()
