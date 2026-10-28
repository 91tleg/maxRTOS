# Context-switch latency benchmark

Measures, on the NUCLEO-H753ZI, how long the kernel takes to hand the CPU
from one process to another. Times are exact core cycles from the Cortex-M7
DWT cycle counter, so this only runs on hardware (QEMU and the SIL simulator
do not model cycles).

| Latency | From | To |
|---|---|---|
| `YIELD` | `maxrtos_yield()` in a process | first instruction of its partition peer |
| `TICK_ROTATE` | SysTick entry | next process of the same partition |
| `TICK_SWITCH` | SysTick entry | first process of a different partition (adds the MPU domain switch) |
| `QUEUE_HANDOFF` | `t0; send; yield` | woken receiver returns from `receive` |
| `MUTEX_HANDOFF` | `t0; unlock; yield` | woken waiter returns from `lock` |

The handoff figures include the send/unlock and the yield, so subtract
`YIELD` for the wake-up cost alone. Samples that span a tick are dropped.
SysTick exception entry (~12 cycles) is not included, and every figure
contains one cycle-counter read (`overhead`, reported).

Each `TICK_*` sample ends at the first `observe_tick()` call after the tick.
Three things keep that sample about the kernel:

- The processes loop with a fixed period, so a fixed tick period would
  always land on the same few phases of the loop and the mean would depend on
  the loop length. Each tick is lengthened by a pseudo-random 0 to 511 cycles.
- A process the tick preempted resumes where it was. Each process calls
  `observe_tick()` before each kernel call that is outside a timed window, so
  it sees the new tick before the call and the sample does not include it.
- The mutex group is not sampled: its timed window (stamp, unlock, yield)
  cannot hold an observation, so a tick that preempts it is followed by
  kernel calls before the next one.

## Run

```
maxrtos/tests/hil/bench/run_bench.py --cpu-hz 64000000            # caches off
maxrtos/tests/hil/bench/run_bench.py --cpu-hz 64000000 --caches   # caches on
maxrtos/tests/hil/bench/run_bench.py --480mhz --opt=-O2 --caches   # 480 MHz
```

It builds, flashes with OpenOCD, lets the board run (`--seconds`, default 15),
halts it, and prints min / mean / max per latency. `--cpu-hz` is the actual
core clock (the reset default is the 64 MHz HSI unless your startup changes
it); it only converts cycles to microseconds.

Report the **max**, not the mean: a partitioned system is judged on its worst
case. Latest figures: [RESULTS.md](RESULTS.md). Run with the caches on and off (`--caches`), and note the optimization level: `--opt=-O2` builds the kernel, arch and bench at -O2 (default -O0). Above -O0 the build uses link-time optimization (`-flto`); `--no-lto` turns it off. The build is soft-float.

## Limits

- All partitions are `system` (privileged) because unprivileged code cannot
  read the DWT, so `nPRIV` switching is not measured.
- With `--caches`, D-cache gains depend on the MPU memory attributes the
  configuration gives SRAM.
- The kernel scans every process on a tick where a timeout, release or
  deadline falls due, and skips the scan otherwise. This bench arms none of
  them.
