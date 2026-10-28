# Context-switch latency results

NUCLEO-H753ZI, Cortex-M7, measured with the DWT cycle counter by
`run_bench.py` (see [README.md](README.md) for what each latency covers).
All partitions are privileged and the build is soft-float. Cycle counts are
exact; microseconds are cycles divided by the core clock. The -O2 builds use
link-time optimization (`-flto`), which is on by default above -O0;
`--no-lto` turns it off.

## 480 MHz, -O2

`run_bench.py --480mhz --opt=-O2 [--caches] [--no-lto]`. The firmware sets up the LDO
supply, VOS0, HSE bypass (8 MHz from the ST-LINK), PLL1 at 480 MHz, AHB and
APB /2, and 4 flash wait states. Confirmed on the running target:
`RCC_CFGR` SWS = PLL1, `PLL1DIVR` N=480 P=Q=R=2, `FLASH_ACR` = 0x24.
15 s run, one run per configuration.

Cycles (min / mean / max) and worst case in µs:

| Latency | Caches on | max µs | Caches off | max µs |
|---|---|---|---|---|
| Yield to peer | 334 / 334 / 559 | 1.16 | 1,090 / 1,117 / 1,312 | 2.73 |
| Tick, same partition | 229 / 393 / 1,125 | 2.34 | 882 / 1,144 / 1,612 | 3.36 |
| Tick, partition switch | 514 / 570 / 1,434 | 2.99 | 1,716 / 1,878 / 5,434 | 11.32 |
| Queue send to receiver resumes | 762 / 860 / 1,654 | 3.45 | 2,588 / 2,991 / 4,119 | 8.58 |
| Mutex unlock to waiter resumes | 838 / 884 / 1,104 | 2.30 | 2,698 / 2,828 / 2,828 | 5.89 |

Measurement overhead is 1 cycle. The handoff rows include the send or unlock
plus the yield, not only the wake-up.

## 64 MHz (reset-default HSI), 7 flash wait states

Cycles (min / mean / max):

| Latency | -O0, caches on | -O2, caches on | -O0, caches off | -O2, caches off |
|---|---|---|---|---|
| Yield to peer | 1,523 / 1,534 / 1,703 | 297 / 325 / 476 | 3,105 / 3,132 / 3,133 | 893 / 896 / 1,072 |
| Tick, same partition | 816 / 1,647 / 4,051 | 232 / 390 / 1,122 | 1,687 / 3,427 / 8,299 | 625 / 984 / 3,431 |
| Tick, partition switch | 1,798 / 2,194 / 5,311 | 513 / 572 / 1,424 | 4,002 / 4,771 / 10,614 | 1,323 / 1,561 / 4,096 |
| Queue send to receiver resumes | 2,433 / 3,165 / 3,756 | 763 / 860 / 1,493 | 5,063 / 6,319 / 7,348 | 2,140 / 2,379 / 3,387 |
| Mutex unlock to waiter resumes | 3,161 / 3,884 / 4,048 | 842 / 885 / 1,030 | 7,209 / 7,770 / 7,838 | 2,030 / 2,342 / 2,368 |

Measurement overhead is 1 cycle at -O2 and 18 to 68 cycles at -O0.

## Observations

- With caches on, cycle counts at 480 MHz match those at 64 MHz (yield mean
  334 cycles at 480 MHz and 325 at 64 MHz, -O2), so the speedup is the clock
  ratio: 5.1 µs to 0.70 µs.
- With caches off, flash wait states raise the cycle counts at 480 MHz
  (yield mean 896 to 1,117 cycles).
- A partition switch costs about 180 cycles more than a rotation
  inside one partition at -O2 with caches on, for reprogramming the MPU
  regions.
- The tick maxima are above the means (1,125 against 393 cycles with caches
  on).

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
| Yield to peer | 334 / 559 | 173 / 205 | 338 / 394 |
| Queue send to receiver resumes | 860 / 1,654 | 521 / 907 | 954 / 1,501 |
| Mutex unlock to waiter resumes | 884 / 1,104 | 516 / 895 | 882 / 1,416 |
| **Caches off** | | | |
| Yield to peer | 1,117 / 1,312 | 462 / 496 | 703 / 820 |
| Queue send to receiver resumes | 2,991 / 4,119 | 1,779 / 3,201 | 3,009 / 3,337 |
| Mutex unlock to waiter resumes | 2,828 / 2,828 | 1,651 / 3,054 | 2,755 / 4,966 |

MAXRTOS cycles divided by FreeRTOS MPU cycles:

| Latency | Caches on, mean | Caches on, max | Caches off, mean | Caches off, max |
|---|---|---|---|---|
| Yield | 1.0x | 1.4x | 1.6x | 1.6x |
| Queue handoff | 0.9x | 1.1x | 1.0x | 1.2x |
| Mutex handoff | 1.0x | 0.8x | 1.0x | 0.6x |

Image size (bytes, whole firmware):

| | text | data | bss |
|---|---|---|---|
| MAXRTOS bench | 11,720 | 24 | 24,064 |
| FreeRTOS plain | 10,032 | 24 | 12,752 |
| FreeRTOS MPU | 15,060 | 952 | 23,512 |

Observations:

- The yield mean equals FreeRTOS MPU's with caches on (1.0x; the maximum is
  1.4x) and is 1.6x with caches off. The queue and mutex handoff means are
  within 10% of it. Against the plain port MAXRTOS is slower on the yield (1.9x
  with caches on, 2.4x with caches off) and on the handoffs (1.7x).
- The MPU roughly doubles FreeRTOS's yield (173 to 338 cycles with caches
  on) and adds about 370 to 430 cycles to the queue and mutex handoffs.
- The MAXRTOS bench image (with LTO) is about 17% larger in flash than
  FreeRTOS plain and about 22% smaller than FreeRTOS MPU. The bss figures
  are not comparable: task stacks
  (1 KB in the FreeRTOS builds, 512 B here) and buffers dominate them.

## FreeRTOS with and without LTO

FreeRTOS V11.1.0 built with and without `-flto`, on the same board and
measured the same way: `-O2`, 480 MHz, 1 KB task stacks and control blocks in
DTCM, `configUSE_PORT_OPTIMISED_TASK_SELECTION` 1, and for the MPU port the
settings described above. In the MPU build `port.c` and the MPU wrapper files
are compiled without LTO, because their assembly calls C functions by name and
LTO drops them. Mean / max cycles, one 15 s run each:

| Latency | Plain, no LTO | Plain, LTO | MPU, no LTO | MPU, LTO |
|---|---|---|---|---|
| **Caches on** | | | | |
| Yield to peer | 174 / 204 | 174 / 202 | 342 / 391 | 342 / 402 |
| Queue send to receiver resumes | 585 / 731 | 522 / 901 | 904 / 1,440 | 950 / 1,494 |
| Mutex unlock to waiter resumes | 588 / 596 | 511 / 881 | 893 / 945 | 907 / 1,282 |
| **Caches off** | | | | |
| Yield to peer | 512 / 624 | 488 / 589 | 808 / 1,011 | 755 / 989 |
| Queue send to receiver resumes | 2,139 / 3,865 | 1,849 / 2,901 | 3,107 / 4,636 | 3,216 / 3,534 |
| Mutex unlock to waiter resumes | 2,217 / 4,024 | 1,733 / 1,743 | 3,003 / 3,003 | 2,879 / 4,227 |

- LTO does not change the FreeRTOS yield with caches on (174 cycles plain,
  342 MPU).
- It moves the caches-on handoff means by between -13% and +5%, and the
  caches-off yield by -5% (plain) and -7% (MPU).
- With LTO on both sides, the MAXRTOS yield with caches on takes about the
  same cycles as the FreeRTOS MPU yield (334 against 342).

Limits:

- FreeRTOS has no partitions, so the groups run one at a time (a controller
  task suspends and resumes them every 250 ticks) and there is no equivalent
  of `TICK_SWITCH`. The FreeRTOS `TICK_SWITCH` slot holds a different
  measurement: SysTick entry to a higher-priority task woken from
  `vTaskDelay(1)`. It is not comparable and is left out of the tables.
- The FreeRTOS tasks are privileged, so the cost of the unprivileged system
  call path (SVC) is not measured.
- One run per configuration, so the maxima may move on longer runs.
