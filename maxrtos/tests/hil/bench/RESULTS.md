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
| Yield to peer | 419 / 453 / 655 | 1.36 | 1,259 / 1,463 / 1,535 | 3.20 |
| Tick, same partition | 225 / 526 / 1,815 | 3.78 | 835 / 1,410 / 5,587 | 11.64 |
| Tick, partition switch | 511 / 814 / 1,922 | 4.00 | 1,791 / 1,976 / 6,149 | 12.81 |
| Queue send to receiver resumes | 840 / 907 / 1,715 | 3.57 | 2,784 / 3,002 / 4,156 | 8.66 |
| Mutex unlock to waiter resumes | 875 / 941 / 1,144 | 2.38 | 2,817 / 2,866 / 2,899 | 6.04 |

Measurement overhead is 1 cycle. The handoff rows include the send or unlock
plus the yield, not only the wake-up.

## 64 MHz (reset-default HSI), 7 flash wait states

Cycles (min / mean / max):

| Latency | -O0, caches on | -O2, caches on | -O0, caches off | -O2, caches off |
|---|---|---|---|---|
| Yield to peer | 1,496 / 1,514 / 1,669 | 415 / 455 / 582 | 2,988 / 3,093 / 3,097 | 956 / 1,147 / 1,197 |
| Tick, same partition | 810 / 1,986 / 7,196 | 229 / 551 / 1,751 | 1,781 / 3,076 / 12,274 | 634 / 1,252 / 4,547 |
| Tick, partition switch | 1,668 / 2,806 / 7,731 | 513 / 817 / 1,921 | 3,732 / 5,408 / 14,137 | 1,382 / 1,863 / 4,972 |
| Queue send to receiver resumes | 2,379 / 3,118 / 3,566 | 840 / 907 / 1,494 | 5,107 / 6,374 / 7,060 | 2,289 / 2,443 / 3,416 |
| Mutex unlock to waiter resumes | 3,131 / 3,869 / 4,029 | 883 / 941 / 1,031 | 6,549 / 7,604 / 7,682 | 2,321 / 2,330 / 2,347 |

Measurement overhead is 1 cycle at -O2 and 18 to 68 cycles at -O0.

## Observations

- With caches on, cycle counts at 480 MHz match those at 64 MHz (yield mean
  453 cycles at -O2), so the speedup is the clock ratio: 7.1 µs to 0.94 µs.
- With caches off, flash wait states raise the cycle counts at 480 MHz
  (yield mean 1,147 to 1,463 cycles).
- A partition switch costs about 270 to 290 cycles more than a rotation
  inside one partition at -O2 with caches on, for reprogramming the MPU
  regions.
- The tick maxima are far above the means (1,815 against 526 cycles with
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
| Yield to peer | 453 / 655 | 173 / 205 | 338 / 394 |
| Tick, same group | 526 / 1,815 | 325 / 1,493 | 566 / 2,252 |
| Queue send to receiver resumes | 907 / 1,715 | 521 / 907 | 954 / 1,501 |
| Mutex unlock to waiter resumes | 941 / 1,144 | 516 / 895 | 882 / 1,416 |
| **Caches off** | | | |
| Yield to peer | 1,463 / 1,535 | 462 / 496 | 703 / 820 |
| Tick, same group | 1,410 / 5,587 | 1,641 / 4,870 | 2,392 / 7,522 |
| Queue send to receiver resumes | 3,002 / 4,156 | 1,779 / 3,201 | 3,009 / 3,337 |
| Mutex unlock to waiter resumes | 2,866 / 2,899 | 1,651 / 3,054 | 2,755 / 4,966 |

MAXRTOS cycles divided by FreeRTOS MPU cycles:

| Latency | Caches on, mean | Caches on, max | Caches off, mean | Caches off, max |
|---|---|---|---|---|
| Yield | 1.3x | 1.7x | 2.1x | 1.9x |
| Tick | 0.9x | 0.8x | 0.6x | 0.7x |
| Queue handoff | 1.0x | 1.1x | 1.0x | 1.2x |
| Mutex handoff | 1.1x | 0.8x | 1.0x | 0.6x |

Image size (bytes, whole firmware):

| | text | data | bss |
|---|---|---|---|
| MAXRTOS bench | 11,160 | 20 | 24,064 |
| FreeRTOS plain | 10,032 | 24 | 12,752 |
| FreeRTOS MPU | 15,060 | 952 | 23,512 |

Observations:

- The yield is the one row where FreeRTOS MPU is clearly faster: MAXRTOS takes
  1.3x its cycles with caches on and 2.1x with caches off (mean). On the tick
  MAXRTOS is faster (0.6x to 0.9x of its cycles), and the queue and mutex
  handoff means are within 10% of it. Against the plain port MAXRTOS is slower
  on the yield (2.6x to 3.2x) and on the handoffs (1.7x to 1.8x), and faster
  on the tick with caches off (0.9x).
- The MPU roughly doubles FreeRTOS's yield (173 to 338 cycles with caches
  on) and adds about 370 to 430 cycles to the queue and mutex handoffs.
- The MAXRTOS bench image (with LTO) is about 11% larger in flash than
  FreeRTOS plain and about 26% smaller than FreeRTOS MPU. Without LTO it is
  13,692 bytes of text. The bss figures are not comparable: task stacks
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
- With LTO on both sides, MAXRTOS takes 1.3x the FreeRTOS MPU cycles for the
  yield with caches on (453 against 342).

Limits:

- FreeRTOS has no partitions, so the groups run one at a time (a controller
  task suspends and resumes them every 250 ticks) and there is no equivalent
  of `TICK_SWITCH`. The FreeRTOS `TICK_SWITCH` slot holds a different
  measurement: SysTick entry to a higher-priority task woken from
  `vTaskDelay(1)`. It is not comparable and is left out of the tables.
- The FreeRTOS tasks are privileged, so the cost of the unprivileged system
  call path (SVC) is not measured.
- One run per configuration, so the maxima may move on longer runs.
