# Context-switch latency results

NUCLEO-H753ZI, Cortex-M7, measured with the DWT cycle counter by
`run_bench.py` (see [README.md](README.md) for what each latency covers).
All partitions are privileged and the build is soft-float. Cycle counts are
exact; microseconds are cycles divided by the core clock. The -O2 builds use
link-time optimization (`-flto`), which is on by default above -O0;
`--no-lto` turns it off. The kernel tables are sized to the module by the
generated limits (`maxrtos_limits.h`).

## 480 MHz, -O2

`run_bench.py --480mhz --opt=-O2 [--caches] [--no-lto]`. The firmware sets up the LDO
supply, VOS0, HSE bypass (8 MHz from the ST-LINK), PLL1 at 480 MHz, AHB and
APB /2, and 4 flash wait states. Confirmed on the running target:
`RCC_CFGR` SWS = PLL1, `PLL1DIVR` N=480 P=Q=R=2, `FLASH_ACR` = 0x24.
15 s run, one run per configuration.

Cycles (min / mean / max) and worst case in µs:

| Latency | Caches on | max µs | Caches off | max µs |
|---|---|---|---|---|
| Yield to peer | 303 / 348 / 552 | 1.15 | 931 / 1,050 / 1,215 | 2.53 |
| Tick, partition switch | 464 / 622 / 1,513 | 3.15 | 1,586 / 2,043 / 5,042 | 10.50 |
| Queue send to receiver resumes | 869 / 870 / 1,855 | 3.86 | 3,067 / 3,068 / 4,705 | 9.80 |
| Mutex unlock to waiter resumes | 846 / 846 / 995 | 2.07 | 2,855 / 2,861 / 2,867 | 5.97 |

Measurement overhead is 1 cycle. The handoff rows include the send or unlock
plus the yield, not only the wake-up.

## 64 MHz (reset-default HSI), 7 flash wait states

Cycles (min / mean / max):

| Latency | -O0, caches on | -O2, caches on | -O0, caches off | -O2, caches off |
|---|---|---|---|---|
| Yield to peer | 1,651 / 1,658 / 1,860 | 290 / 341 / 511 | 3,436 / 3,443 / 3,465 | 709 / 862 / 1,032 |
| Tick, partition switch | 1,868 / 2,328 / 5,478 | 462 / 623 / 1,446 | 3,914 / 5,169 / 11,370 | 1,319 / 1,786 / 4,215 |
| Queue send to receiver resumes | 3,463 / 3,492 / 4,483 | 873 / 873 / 1,633 | 7,145 / 7,156 / 8,637 | 2,409 / 2,422 / 3,742 |
| Mutex unlock to waiter resumes | 4,078 / 4,139 / 4,266 | 850 / 850 / 962 | 8,242 / 8,243 / 8,255 | 2,231 / 2,241 / 2,244 |

Measurement overhead is 1 cycle at -O2 and 18 to 68 cycles at -O0.

## Observations

- With caches on, cycle counts at 480 MHz match those at 64 MHz (yield mean
  348 cycles at 480 MHz and 341 at 64 MHz, -O2), so the speedup is the clock
  ratio: 5.3 µs to 0.73 µs.
- With caches off, flash wait states raise the cycle counts at 480 MHz
  (yield mean 862 to 1,050 cycles).
- Processes are not time sliced, so a tick inside a partition's window
  switches nothing; the only tick latency is the switch to another partition,
  which includes reprogramming the MPU regions.
- The partition-switch tick maximum is above the mean (1,513 against 622
  cycles with caches on).

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
| Yield to peer | 348 / 552 | 173 / 205 | 338 / 394 |
| Queue send to receiver resumes | 870 / 1,855 | 521 / 907 | 954 / 1,501 |
| Mutex unlock to waiter resumes | 846 / 995 | 516 / 895 | 882 / 1,416 |
| **Caches off** | | | |
| Yield to peer | 1,050 / 1,215 | 462 / 496 | 703 / 820 |
| Queue send to receiver resumes | 3,068 / 4,705 | 1,779 / 3,201 | 3,009 / 3,337 |
| Mutex unlock to waiter resumes | 2,861 / 2,867 | 1,651 / 3,054 | 2,755 / 4,966 |

MAXRTOS cycles divided by FreeRTOS MPU cycles:

| Latency | Caches on, mean | Caches on, max | Caches off, mean | Caches off, max |
|---|---|---|---|---|
| Yield | 1.0x | 1.4x | 1.5x | 1.5x |
| Queue handoff | 0.9x | 1.2x | 1.0x | 1.4x |
| Mutex handoff | 1.0x | 0.7x | 1.0x | 0.6x |

Image size (bytes, whole firmware):

| | text | data | bss |
|---|---|---|---|
| MAXRTOS bench | 12,032 | 24 | 7,960 |
| FreeRTOS plain | 10,032 | 24 | 12,752 |
| FreeRTOS MPU | 15,060 | 952 | 23,512 |

Observations:

- The yield mean equals FreeRTOS MPU's with caches on (1.0x; the maximum is
  1.4x) and is 1.5x with caches off. The queue and mutex handoff means are
  within 10% of it. Against the plain port MAXRTOS is slower on the yield (2.0x
  with caches on, 2.3x with caches off) and on the handoffs (1.6x to 1.7x).
- The MPU roughly doubles FreeRTOS's yield (173 to 338 cycles with caches
  on) and adds about 370 to 430 cycles to the queue and mutex handoffs.
- The MAXRTOS bench image (with LTO) is about 20% larger in flash than
  FreeRTOS plain and about 20% smaller than FreeRTOS MPU. The bss figures
  are not comparable: the task stacks (1 KB in the FreeRTOS builds, 512 B
  here) and the kernel tables are sized differently.

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
  same cycles as the FreeRTOS MPU yield (348 against 342).

Limits:

- FreeRTOS has no partitions, so the groups run one at a time (a controller
  task suspends and resumes them every 250 ticks) and there is no equivalent
  of `TICK_SWITCH`. The FreeRTOS `TICK_SWITCH` slot holds a different
  measurement: SysTick entry to a higher-priority task woken from
  `vTaskDelay(1)`. It is not comparable and is left out of the tables.
- The FreeRTOS tasks are privileged, so the cost of the unprivileged system
  call path (SVC) is not measured.
- One run per configuration, so the maxima may move on longer runs.
