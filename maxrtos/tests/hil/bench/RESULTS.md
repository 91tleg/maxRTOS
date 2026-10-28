# Context-switch latency results

NUCLEO-H753ZI, Cortex-M7, measured with the DWT cycle counter by
`run_bench.py` (see [README.md](README.md) for what each latency covers).
All partitions are privileged and the build is soft-float. Cycle counts are
exact; microseconds are cycles divided by the core clock.

## 480 MHz, -O2

`run_bench.py --480mhz --opt=-O2 [--caches]`. The firmware sets up the LDO
supply, VOS0, HSE bypass (8 MHz from the ST-LINK), PLL1 at 480 MHz, AHB and
APB /2, and 4 flash wait states. Confirmed on the running target:
`RCC_CFGR` SWS = PLL1, `PLL1DIVR` N=480 P=Q=R=2, `FLASH_ACR` = 0x24.
15 s run, one run per configuration.

Cycles (min / mean / max) and worst case in µs:

| Latency | Caches on | max µs | Caches off | max µs |
|---|---|---|---|---|
| Yield to peer | 758 / 764 / 960 | 2.00 | 2,894 / 2,930 / 2,972 | 6.19 |
| Tick, same partition | 1,577 / 1,885 / 3,884 | 8.09 | 7,204 / 7,664 / 15,274 | 31.82 |
| Tick, partition switch | 1,953 / 2,092 / 4,033 | 8.40 | 8,406 / 8,586 / 16,316 | 33.99 |
| Queue send to receiver resumes | 1,270 / 1,437 / 2,296 | 4.78 | 4,915 / 5,562 / 6,955 | 14.49 |
| Mutex unlock to waiter resumes | 1,398 / 1,503 / 1,705 | 3.55 | 5,173 / 5,634 / 5,733 | 11.94 |

Measurement overhead is 1 cycle. The handoff rows include the send or unlock
plus the yield, not only the wake-up.

## 64 MHz (reset-default HSI), 7 flash wait states

Cycles (min / mean / max):

| Latency | -O0, caches on | -O2, caches on | -O0, caches off | -O2, caches off |
|---|---|---|---|---|
| Yield to peer | 2,038 / 2,058 / 2,255 | 762 / 764 / 913 | 4,066 / 4,150 / 4,154 | 2,473 / 2,521 / 2,536 |
| Tick, same partition | 6,705 / 7,541 / 11,358 | 1,575 / 1,818 / 3,372 | 12,683 / 15,052 / 23,614 | 5,917 / 6,826 / 11,270 |
| Tick, partition switch | 7,519 / 8,573 / 11,903 | 1,949 / 2,053 / 3,566 | 14,708 / 16,875 / 23,647 | 6,934 / 7,669 / 12,760 |
| Queue send to receiver resumes | 2,941 / 3,616 / 4,134 | 1,277 / 1,440 / 2,090 | 6,137 / 7,304 / 8,392 | 4,161 / 4,672 / 5,854 |
| Mutex unlock to waiter resumes | 3,044 / 3,688 / 3,850 | 1,228 / 1,337 / 1,462 | 6,453 / 7,388 / 7,433 | 3,980 / 4,311 / 4,330 |

Measurement overhead is 1 cycle at -O2 and 18 to 68 cycles at -O0.

## Observations

- With caches on, cycle counts at 480 MHz match those at 64 MHz (yield mean
  764 cycles at -O2), so the speedup is the clock ratio: 12 µs to 1.6 µs.
- With caches off, flash wait states raise the cycle counts at 480 MHz
  (yield mean 2,521 to 2,930 cycles).
- A partition switch costs about 200 to 235 cycles more than a rotation
  inside one partition at -O2 with caches on, for reprogramming the MPU
  regions.

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
| Yield to peer | 764 / 960 | 173 / 205 | 338 / 394 |
| Tick, same group | 1,885 / 3,884 | 325 / 1,493 | 566 / 2,252 |
| Queue send to receiver resumes | 1,437 / 2,296 | 521 / 907 | 954 / 1,501 |
| Mutex unlock to waiter resumes | 1,503 / 1,705 | 516 / 895 | 882 / 1,416 |
| **Caches off** | | | |
| Yield to peer | 2,930 / 2,972 | 462 / 496 | 703 / 820 |
| Tick, same group | 7,664 / 15,274 | 1,641 / 4,870 | 2,392 / 7,522 |
| Queue send to receiver resumes | 5,562 / 6,955 | 1,779 / 3,201 | 3,009 / 3,337 |
| Mutex unlock to waiter resumes | 5,634 / 5,733 | 1,651 / 3,054 | 2,755 / 4,966 |

MAXRTOS cycles divided by FreeRTOS MPU cycles:

| Latency | Caches on, mean | Caches on, max | Caches off, mean | Caches off, max |
|---|---|---|---|---|
| Yield | 2.3x | 2.4x | 4.2x | 3.6x |
| Tick | 3.3x | 1.7x | 3.2x | 2.0x |
| Queue handoff | 1.5x | 1.5x | 1.9x | 2.1x |
| Mutex handoff | 1.7x | 1.2x | 2.1x | 1.2x |

Image size (bytes, whole firmware):

| | text | data | bss |
|---|---|---|---|
| MAXRTOS bench | 13,284 | 20 | 24,040 |
| FreeRTOS plain | 10,032 | 24 | 12,752 |
| FreeRTOS MPU | 15,060 | 952 | 23,512 |

Observations:

- FreeRTOS is faster in every row, by 1.2x to 4.2x against the MPU port and
  1.9x to 6.3x against the plain port. The largest gap is the yield.
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
