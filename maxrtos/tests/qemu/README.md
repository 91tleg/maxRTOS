# QEMU emulation tests

The real kernel and Cortex-M7 architecture code, cross-compiled and run on an
emulated Cortex-M7 (QEMU `mps2-an500`). Unlike the host tests they execute the
actual SVC, PendSV, SysTick, MPU and fault-handler code paths.

Each test is one self-checking firmware. It reports its verdict through ARM
semihosting, so the QEMU exit status is the result and no debugger is needed.

| Test | What it checks |
|---|---|
| `emu_yield` | `maxrtos_yield()` alternates between the two processes of a partition and never starves either |
| `emu_queue_port_ipc` | blocking receive (same and other partition), timed receive expiry, blocked sender admitted in FIFO order, hostile pointers rejected |
| `emu_hm_restart_process` | a memory fault restarts only the faulting process; everything else keeps running |
| `emu_hm_halt_partition` | `halt_partition` stops that partition (including its healthy peer) but not the system |

## Run

Requires `arm-none-eabi-gcc`, `qemu-system-arm`, and Python 3 with `jinja2`.

```bash
cmake -S maxrtos/tests/qemu -B build/qemu -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/arm-none-eabi.cmake
cmake --build build/qemu
ctest --test-dir build/qemu --output-on-failure
```

## Writing a test

Add `test_<name>.c` implementing `emu_test_setup()` (create processes) and
`emu_test_poll()` (judge; called every tick from the privileged SysTick
handler), then register it with `add_emu_test()` in `CMakeLists.txt`.
Unprivileged processes cannot write kernel memory, so they record results in
`EMU_RES`, the memory of the `results` port, which the poll function reads.
The RTOS configuration is generated from `module.json` by `maxrtos_codegen`
at build time, so these tests also exercise the generator.

## Notes

- QEMU runs with `-icount shift=0`, so virtual time follows executed
  instructions and tick timing does not depend on host load.
- A test that never reaches a verdict fails after `EMU_TICK_LIMIT` ticks. A
  system that hangs before ticking (for example a stuck fault handler) is
  stopped by the CTest timeout.
