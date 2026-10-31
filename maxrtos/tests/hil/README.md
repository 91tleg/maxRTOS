# Hardware-in-the-loop tests

The real kernel and Cortex-M7 architecture code, running on a NUCLEO-H753ZI.
These tests exist for what the host, SIL and QEMU suites cannot see: how the
real MPU, the real exception hardware and the real clock behave.

Each test is one self-checking firmware. It keeps its verdict in RAM
(`hil_result.h`); `runner.py` flashes it, lets it run and reads the verdict over
SWD through OpenOCD while the core runs. The LEDs show it too: yellow while
running, then green (pass) or red (fail).

## Run

Needs `arm-none-eabi-gcc`, `cmake`, `ninja`, `openocd`, Python 3 with `jinja2`
(for the code generator), and a NUCLEO-H753ZI on USB.

```bash
python3 maxrtos/tests/hil/runner.py                  # build, flash and run everything
python3 maxrtos/tests/hil/runner.py -k mpu -k fault  # only tests with these in the name
python3 maxrtos/tests/hil/runner.py --list
python3 maxrtos/tests/hil/runner.py --no-build --junit hil.xml
```

The exit status is non-zero if any test fails. A test that never reaches a
verdict is reported with the program counter and the last fault the core took.

## Tests

**Only meaningful on silicon**

| Test | What it checks |
|---|---|
| `timebase` | The core counts exactly `reload + 1` cycles per tick, and the tick count follows the host's wall clock (the runner compares them), which shows the core clock is the 64 MHz the harness assumes |
| `mpu_regs` | The 16 MPU regions read back from the hardware match the configuration for the running partition: the region count, the size encoding, how base and region number read back, the partition domain switching, and that unused regions are off, including every one a port could once have used |
| `mpu_isolation` | Every kind of forbidden access (other partition read/write, flash, kernel memory, a peripheral, executing data) raises a MemManage fault with exactly the right `CFSR` bits and fault address, never reaches HardFault, leaves the victim memory unchanged, restarts only the offender, and leaves the rest running |
| `fault_status` | Undefined instruction, divide by zero, unaligned `LDRD`, branch to ARM state and a real bus error leave the `CFSR`/`BFAR` values the ARMv7-M manual lists, are classified, and are recovered by the health monitor |
| `context_integrity` | r1-r12 hold their values across preemption by the real exception entry/return and the MPU switch, for processes that are cut by the tick and for ones that also yield |

**The QEMU tests, unchanged, on the chip**

`yield`, `queue_port_ipc`, `sampling_port`, `fpu_context`, `fpu_more`, `fpu_startup`, `stack_integrity`, `stack_halt`, `stack_slot`, `stack_lazy`, `hm_restart_process`, `hm_halt_partition`,
`deadline_supervision`, `partition_privilege`, `hardfault_escalation`, `mutex`,
`mutex_release_on_restart`, `semaphore` and `timed_wait` are built from `../qemu/test_*.c`
and run against the same checks. Their module configurations are the QEMU ones
with the memory map swapped by `retarget_module.py`, so the two suites cannot
drift apart.

## How it fits together

- `harness.c` implements the `emu_test_setup()` / `emu_test_poll()` interface of
  [`../qemu/harness.h`](../qemu/harness.h) for the board: it starts SysTick,
  calls `emu_test_poll()` every tick from the SysTick handler, and publishes the
  verdict. The one thing it must do that QEMU does not is clear the result words
  (`EMU_RES`): a reset keeps the board's SRAM, so a test would otherwise start
  with the previous test's numbers.
- `retarget_module.py` rewrites a QEMU `module*.json` for the board: memory
  map, and background MPU regions (flash, AHB4 peripherals, DTCM, AXI SRAM).
  `test_mpu_regs.c` expects exactly these regions; change them together.
- `hil_stages.h` is the one-fault-per-stage protocol of `mpu_isolation` and
  `fault_status`: an injector process provokes one fault per stage and is
  restarted by the health monitor, and the privileged poll function checks
  `g_maxrtos_arch_last_fault` against the stage's expectation.
- The core runs from the reset-default HSI at 64 MHz and the tick is 1 kHz.
  Caches are off.

## Writing a test

Add `tests/test_<name>.c` implementing `emu_test_setup()` and `emu_test_poll()`
(see [`../qemu/README.md`](../qemu/README.md)), and register it with
`add_hil_test()` in `CMakeLists.txt`. Pass `REGION` for extra background MPU
regions, `CONFIG` for another module JSON, and `DEFINES HIL_TICK_LIMIT=n` for a
test that needs more than 1000 ticks. Settings the runner needs for a test, such
as a longer time limit or a host-side check, go in `TESTS` in `runner.py`.
`hil.h` has the CFSR bit names and helpers.

A test that fails should say why: use `hil_fail_values("what", got, want)`.

## Notes

- `maxrtos_yield()` round trips cost thousands of cycles at `-O0`, so a
  process that only yields makes a few beats per tick, not thousands. Judge
  progress over a window of ticks, not against a count.
- The benchmark in [`bench/`](bench/README.md) is separate and measures
  context-switch latency.
