# QEMU emulation tests

The real kernel and Cortex-M7 architecture code, cross-compiled and run on an
emulated Cortex-M7 (QEMU `mps2-an500`). Unlike the host tests they execute the
actual SVC, PendSV, SysTick, MPU and fault-handler code paths.

Each test is one self-checking firmware. It reports its verdict through ARM
semihosting, so the QEMU exit status is the result and no debugger is needed.

| Test | What it checks |
|---|---|
| `emu_yield` | `maxrtos_yield()` alternates between the two processes of a partition and never starves either |
| `emu_queue_port_ipc` | blocking receive (same and other partition), timed receive expiry, blocked sender admitted in FIFO order, hostile pointers, unknown IDs and the wrong partition or direction rejected |
| `emu_sampling_port` | a read before the first write, a write in one partition seen whole by a reader in another, a read that leaves the message, a write that replaces it, hostile pointers, unknown IDs and the wrong partition or direction rejected |
| `emu_fpu_context` | the FPU registers and FPSCR of four processes survive yields, SVCs, a blocking receive that is woken from another context, and the end of a partition's slot interrupting a loop at any instruction; double-precision arithmetic comes out exact; a process's first FPU instruction sees zero whatever the others left, including after a restart that discards a context with live FPU state and a pending lazy stacking |
| `emu_fpu_more` | the same in a privileged system partition and across timed waits; a process that clears `CONTROL.FPCA` (only privileged code can) is stopped at the first preemption, since the hardware then stacks a frame its context cannot hold; a process's first FPU instruction sees zero after the others have filled the registers |
| `emu_fpu_startup` | FPU registers, FPSCR and `CONTROL.FPCA` left by code that ran before the kernel never reach the first process |
| `emu_stack_integrity` | a process that moves its stack pointer (below its stack, past its top, to nonsense addresses, eight bytes either side of where its context fits) cannot make the context switch write outside its stack: its neighbours in other partitions are unchanged, none is restarted in its place, and a stack pointer exactly at the limit is left alone |
| `emu_stack_halt` | such a process is handled with the partition's memory access fault policy (here `halt_partition`), not just dropped |
| `emu_hm_restart_process` | a memory fault restarts only the faulting process; everything else keeps running |
| `emu_timed_wait` | `maxrtos_timed_wait()` returns after its delay and not before (a short delay that ends inside the partition's slot, and a long one that spans frames), the partition's other process keeps running meanwhile, a delay of 0 returns and gives way to its equal-priority peers, an infinite delay is rejected, and `maxrtos_get_time()` never goes back |
| `emu_fatal_hook` | A fault that cannot be attributed to a process (a `BKPT` with no debugger) ends in the application's own `maxrtos_arch_fatal()`, which replaces the kernel's weak halt |
| `emu_deadline_supervision` | periodic release through the real `maxrtos_periodic_wait()` SVC and idle switch; a punctual process never misses, an overrunning one is detected and restarted; the other partition is undisturbed |
| `emu_partition_privilege` | a system partition runs privileged and an application partition does not, through the real CONTROL.nPRIV switch; all tests run with PRIVDEFENA clear |
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
`EMU_RES`, a 2 KiB block that the test modules map read-write for every
partition (MPU region 5 in `module*.json`) and the poll function reads. It is
test scaffolding; IPC ports are never memory a partition can reach.
The RTOS configuration is generated from `module.json` by `maxrtos_codegen`
at build time, so these tests also exercise the generator.

## Notes

- QEMU runs with `-icount shift=0`, so virtual time follows executed
  instructions and tick timing does not depend on host load.
- A test that never reaches a verdict fails after `EMU_TICK_LIMIT` ticks. A
  system that hangs before ticking (for example a stuck fault handler) is
  stopped by the CTest timeout.
