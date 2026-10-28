# Context-switch latency results

NUCLEO-H753ZI, Cortex-M7, measured with the DWT cycle counter by
`run_bench.py` (see [README.md](README.md) for what each latency covers).
All partitions are privileged and the build is soft-float. Cycle counts are
exact; microseconds are cycles divided by the core clock. Measured on commit
`2d9758a`.

## 480 MHz, -O2

`run_bench.py --480mhz --opt=-O2 [--caches]`. The firmware sets up the LDO
supply, VOS0, HSE bypass (8 MHz from the ST-LINK), PLL1 at 480 MHz, AHB and
APB /2, and 4 flash wait states. Confirmed on the running target:
`RCC_CFGR` SWS = PLL1, `PLL1DIVR` N=480 P=Q=R=2, `FLASH_ACR` = 0x24.
15 s run, one run per configuration.

Cycles (min / mean / max) and worst case in µs:

| Latency | Caches on | max µs | Caches off | max µs |
|---|---|---|---|---|
| Yield to peer | 595 / 628 / 816 | 1.70 | 2,240 / 2,322 / 2,356 | 4.91 |
| Tick, same partition | 342 / 659 / 2,631 | 5.48 | 1,392 / 2,272 / 9,590 | 19.98 |
| Tick, partition switch | 719 / 828 / 2,775 | 5.78 | 2,720 / 3,073 / 8,176 | 17.03 |
| Queue send to receiver resumes | 1,143 / 1,317 / 2,075 | 4.32 | 4,393 / 5,018 / 6,351 | 13.23 |
| Mutex unlock to waiter resumes | 1,263 / 1,382 / 1,583 | 3.30 | 4,623 / 5,066 / 5,177 | 10.79 |

Measurement overhead is 1 cycle. The handoff rows include the send or unlock
plus the yield, not only the wake-up.

## 64 MHz (reset-default HSI), 7 flash wait states

Cycles (min / mean / max):

| Latency | -O0, caches on | -O2, caches on | -O0, caches off | -O2, caches off |
|---|---|---|---|---|
| Yield to peer | 1,434 / 1,441 / 1,634 | 595 / 628 / 734 | 2,861 / 2,945 / 2,964 | 1,927 / 1,979 / 1,991 |
| Tick, same partition | 812 / 2,130 / 7,048 | 341 / 646 / 2,630 | 1,794 / 3,447 / 12,746 | 1,146 / 2,024 / 6,876 |
| Tick, partition switch | 1,660 / 2,780 / 7,697 | 719 / 846 / 2,772 | 3,772 / 5,561 / 14,639 | 2,267 / 2,683 / 7,975 |
| Queue send to receiver resumes | 2,315 / 3,058 / 3,532 | 1,143 / 1,317 / 1,935 | 4,864 / 6,234 / 6,839 | 3,463 / 4,048 / 5,079 |
| Mutex unlock to waiter resumes | 3,047 / 3,751 / 3,925 | 1,263 / 1,382 / 1,502 | 6,467 / 7,553 / 7,582 | 3,833 / 4,312 / 4,327 |

Measurement overhead is 1 cycle at -O2 and 18 to 68 cycles at -O0.

## Observations

- With caches on, cycle counts at 480 MHz match those at 64 MHz (yield mean
  628 cycles at -O2), so the speedup is the clock ratio: 9.8 µs to 1.3 µs.
- With caches off, flash wait states raise the cycle counts at 480 MHz
  (yield mean 1,979 to 2,322 cycles).
- A partition switch costs about 170 to 200 cycles more than a rotation
  inside one partition at -O2 with caches on, for reprogramming the MPU
  regions.
- The tick maxima are far above the means (2,631 against 659 cycles with
  caches on). They are ticks on which a timeout, release or deadline falls
  due and the kernel scans every process; ordinary ticks skip the scan.

## Comparison with FreeRTOS V11.1.0

Same board, 480 MHz, -O2, DWT cycle counter, 100000-cycle tick, same sample
bookkeeping and result layout, one 15 s run per configuration. The FreeRTOS
port is not in this repository.

- **plain**: `ARM_CM7/r0p1` port, no MPU.
- **MPU**: `ARM_CM4_MPU` port with MPU wrappers v2, 16 regions, MPU
  confirmed enabled on the target (`MPU_CTRL` = 0x5). All tasks are
  privileged (the DWT is privileged-only, as for our "system" partitions).
  Each task's stack region is reprogrammed on every context switch. Region
  attributes are write-back, non-shareable, as ours; the port default is
  shareable, which the M7 does not cache.

Cycles, mean / max:

| Latency | MAXRTOS | FreeRTOS plain | FreeRTOS MPU |
|---|---|---|---|
| **Caches on** | | | |
| Yield to peer | 628 / 816 | 173 / 205 | 338 / 394 |
| Tick, same group | 659 / 2,631 | 325 / 1,493 | 566 / 2,252 |
| Queue send to receiver resumes | 1,317 / 2,075 | 521 / 907 | 954 / 1,501 |
| Mutex unlock to waiter resumes | 1,382 / 1,583 | 516 / 895 | 882 / 1,416 |
| **Caches off** | | | |
| Yield to peer | 2,322 / 2,356 | 462 / 496 | 703 / 820 |
| Tick, same group | 2,272 / 9,590 | 1,641 / 4,870 | 2,392 / 7,522 |
| Queue send to receiver resumes | 5,018 / 6,351 | 1,779 / 3,201 | 3,009 / 3,337 |
| Mutex unlock to waiter resumes | 5,066 / 5,177 | 1,651 / 3,054 | 2,755 / 4,966 |

MAXRTOS cycles divided by FreeRTOS MPU cycles:

| Latency | Caches on, mean | Caches on, max | Caches off, mean | Caches off, max |
|---|---|---|---|---|
| Yield | 1.9x | 2.1x | 3.3x | 2.9x |
| Tick | 1.2x | 1.2x | 0.9x | 1.3x |
| Queue handoff | 1.4x | 1.4x | 1.7x | 1.9x |
| Mutex handoff | 1.6x | 1.1x | 1.8x | 1.0x |

Image size (bytes, whole firmware):

| | text | data | bss |
|---|---|---|---|
| MAXRTOS bench | 13,692 | 20 | 24,080 |
| FreeRTOS plain | 10,032 | 24 | 12,752 |
| FreeRTOS MPU | 15,060 | 952 | 23,512 |

Observations:

- FreeRTOS is faster in every row but one, by up to 3.3x against the MPU port
  and up to 5.0x against the plain port. The exception is the tick with caches
  off, where the MAXRTOS mean is 0.9x the FreeRTOS MPU mean (its maximum is
  still 1.3x). The largest gap is the yield.
- The MPU roughly doubles FreeRTOS's yield (173 to 338 cycles with caches
  on) and adds about 370 to 430 cycles to the queue and mutex handoffs.
- FreeRTOS plain is smaller than the MAXRTOS bench image in flash; FreeRTOS
  MPU is about 13% larger. The bss figures are not comparable: task stacks
  (1 KB in the FreeRTOS builds, 512 B here) and buffers dominate them.

Limits:

- FreeRTOS has no partitions, so the groups run one at a time (a controller
  task suspends and resumes them every 250 ticks) and there is no equivalent
  of `TICK_SWITCH`. The FreeRTOS `TICK_SWITCH` slot holds a different
  measurement: SysTick entry to a higher-priority task woken from
  `vTaskDelay(1)`. It is not comparable and is left out of the tables.
- The FreeRTOS tasks are privileged, so the cost of the unprivileged system
  call path (SVC) is not measured.
- One run per configuration, so the maxima may move on longer runs.
