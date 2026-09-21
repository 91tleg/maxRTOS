# Software-in-the-loop (SIL) tests

The production kernel and the hardware-independent part of the Cortex-M7
architecture layer, compiled **unchanged** for the host and run on a
**virtual Cortex-M7**. Scenarios drive it through the same public API an
application uses (`maxrtos_yield`, `maxrtos_queue_port_*`), inject faults, and
judge behaviour against requirements.

```
   scenario tests (test_*.c)            requirement-tagged, oracle-checked
   ───────────────────────────────────────────────────────────────────────
   sil_system  ── builds a partitioned system like the generated config
   ───────────────────────────────────────────────────────────────────────
   PRODUCTION CODE, unmodified:
     kernel/            scheduler, partitions, frame schedule, IPC, health monitor
     arch/cortex_m7/    svc.c  systick.c  start_scheduler.c  idle.c
                        mpu.c  mpu_hw.c   fault_dispatch.c
   ───────────────────────────────────────────────────────────────────────
   sim/  virtual target (the only thing standing in for hardware)
     CPU + exceptions   one thread per process, one "CPU" (baton); SysTick / SVC /
                        fault exceptions end with the pending PendSV, as on target
     virtual time       advances by sim_cpu_work(); SysTick every 1000 cycles;
                        fully deterministic
     MPU model          register-level PMSAv7; the production driver programs it
     32-bit memory      virtual address space over host memory
     fault injection    MPU violations, injected faults at a chosen tick
```

## What is real and what is simulated

| Real (production source) | Simulated (`sim/`) |
|---|---|
| Kernel: scheduling, partitions, frame, IPC, timeouts, health monitor | CPU registers, exception entry/return, PendSV context save/restore |
| SVC dispatch and **pointer validation** | The SysTick counter (virtual clock) |
| MPU **driver** (region programming, per-switch reconfiguration) | The MPU **hardware** (register model, access decisions) |
| Fault recovery enactment (restart / halt / idle) | Fault exception entry and CFSR classification |
| Scheduler start-up | Reset, vector table, linker layout |

The architecture layer reaches hardware only through `arch/cortex_m7/port.h`
(halt, WFI, barriers, MPU registers, address translation). The target uses the
defaults; SIL force-includes `sim/sim_port_overrides.h` to redirect them. No
production file has SIL-specific code.

What SIL cannot show — and QEMU (`../qemu`) and the board (`../hil`) do — is
the assembly: context save/restore, the SVC and PendSV entry sequences, the
generated linker layout and startup, FPU state, exception priorities, real
timing.

## Run

```bash
cmake -S maxrtos/tests -B build/tests
cmake --build build/tests
ctest --test-dir build/tests -L sil --output-on-failure
python3 maxrtos/tests/sil/tools/traceability.py build/tests/sil-reports
```

Each suite is one executable and writes a JUnit XML report to
`<build>/sil-reports/`.

| Suite | Requirements |
|---|---|
| `temporal_partitioning` | REQ-TP-001..003: exact slot allocation vs an independent oracle, zero jitter, a hogging partition cannot steal time |
| `spatial_partitioning` | REQ-SP-001..007: own memory, foreign/kernel/port/flash memory denied through the MPU model, MPU follows the running partition |
| `ipc_queue_port` | REQ-IPC-001..006: blocking and timed receive, cross-partition 500-message stream, capacity, hostile-pointer matrix |
| `fault_containment` | REQ-FT-001..008: restart vs halt, per-partition and per-class policy, fault storm, async fault attribution, no monitor, stranded peers |
| `scheduler_startup` | REQ-BOOT-001..004 |
| `process_yield` | REQ-YLD-001..003 |
| `fault_recovery_soak` | REQ-FT-009 (kernel level, 5000 faults) |
| `simulator_selfcheck` | SIM-001..004: qualifies the virtual target itself |

## How results are trusted

* **Requirements traceability.** [`REQUIREMENTS.md`](REQUIREMENTS.md) lists
  every requirement; every test case cites one. `tools/traceability.py` writes a
  requirement → test matrix and fails on an uncovered requirement or an unknown
  ID.
* **Mutation check.** `tools/mutation_check.py <build-dir>` injects 16 realistic
  defects into the production kernel and architecture code (timeouts that never
  expire, unchecked user pointers, a port region never enabled, halt that does
  not leave the faulting context, a frame off by one tick, ...) and requires
  the suites to catch each one. A suite that cannot fail proves nothing.
* **Independent oracles.** Expected schedules are computed in the test from the
  slot table, not read back from the code under test.
* **Simulator qualification.** The MPU model, address translation and virtual
  clock have their own tests (`SIM-*`).
* **Never compiled out.** Checks are `SIL_REQUIRE`/`SIL_EXPECT`, not `assert()`,
  so `-DNDEBUG` cannot make a run pass silently.
* **Coverage and sanitizers.** `-DMAXRTOS_TEST_COVERAGE=ON` (line and branch,
  gated in CI over the production kernel and portable arch sources) and
  `-DMAXRTOS_TEST_SANITIZE=undefined|address`. AddressSanitizer runs on the unit
  tests only: the virtual target runs contexts on caller-provided thread
  stacks, which ASan does not support.
* **Determinism.** Virtual time depends only on executed simulated work, so a
  run is bit-identical every time; there are no sleeps or wall-clock races.

## Writing a test

```c
static void test_something( void )
{
    sil_system_spec_t spec;

    sil_spec_two_partitions( &spec, entry_0, entry_1 );   // or fill in a spec
    sil_build( &spec );                                    // same steps as the generated config
    SIL_REQUIRE( sim_start( 200U ) );                      // real maxrtos_scheduler_start(), run 200 ticks

    SIL_EXPECT_EQ( shared_result, expected );              // judge on the bench thread
    sim_shutdown();
}
```

Process code runs on simulated contexts. It must burn time with
`sim_cpu_work()` or yield/block through the RTOS API (code that does neither
never lets time pass), must touch memory via `sim_read32`/`sim_write32` when it
should be MPU-checked, and records observations in shared variables. Checks
belong on the bench thread, never in process code.

Give the case a requirement ID from `REQUIREMENTS.md` in `SIL_CASE(...)`, add
the suite to `CMakeLists.txt` with `add_sil_test()`, and add a mutation to
`tools/mutation_check.py` if the test guards a defect the existing ones miss.

## Known limits

* Single core; no interrupt nesting model beyond SVC/SysTick ending in PendSV.
* Address translation stands in for the 32-bit target: pointers in `sim/`
  are translated between the virtual address space and host memory.
* The virtual target's context save/restore and resume-result delivery mirror,
  but are not, the assembly; QEMU covers the real thing.
* `maxrtos_scheduler_block_current()` / `maxrtos_scheduler_unblock_process()`
  are not called by any production path and are untested (dead code).
