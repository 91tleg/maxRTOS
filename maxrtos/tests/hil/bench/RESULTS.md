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
| Yield to peer | 282 / 339 / 529 | 1.10 | 920 / 1,115 / 1,212 | 2.53 |
| Tick, same partition | 226 / 370 / 1,192 | 2.48 | 840 / 1,175 / 3,890 | 8.10 |
| Tick, partition switch | 486 / 544 / 1,383 | 2.88 | 1,648 / 1,901 / 4,776 | 9.95 |
| Queue send to receiver resumes | 748 / 849 / 1,741 | 3.63 | 2,610 / 2,936 / 4,217 | 8.79 |
| Mutex unlock to waiter resumes | 737 / 819 / 1,034 | 2.15 | 2,510 / 2,727 / 2,764 | 5.76 |

Measurement overhead is 1 cycle. The handoff rows include the send or unlock
plus the yield, not only the wake-up.

## 64 MHz (reset-default HSI), 7 flash wait states

Cycles (min / mean / max):

| Latency | -O0, caches on | -O2, caches on | -O0, caches off | -O2, caches off |
|---|---|---|---|---|
| Yield to peer | 1,515 / 1,531 / 1,719 | 282 / 340 / 448 | 3,036 / 3,113 / 3,121 | 697 / 879 / 995 |
| Tick, same partition | 823 / 1,597 / 4,020 | 225 / 368 / 1,107 | 1,722 / 3,349 / 8,323 | 690 / 985 / 3,169 |
| Tick, partition switch | 1,796 / 2,131 / 5,078 | 483 / 545 / 1,374 | 3,847 / 4,669 / 10,367 | 1,319 / 1,529 / 4,108 |
| Queue send to receiver resumes | 2,432 / 3,142 / 3,813 | 748 / 849 / 1,522 | 4,966 / 6,323 / 7,450 | 2,130 / 2,376 / 3,415 |
| Mutex unlock to waiter resumes | 3,699 / 3,742 / 3,910 | 744 / 819 / 944 | 6,312 / 7,353 / 7,418 | 2,022 / 2,183 / 2,204 |

Measurement overhead is 1 cycle at -O2 and 18 to 68 cycles at -O0.

## Observations

- With caches on, cycle counts at 480 MHz match those at 64 MHz (yield mean
  339 cycles at 480 MHz and 340 at 64 MHz, -O2), so the speedup is the clock
  ratio: 5.3 µs to 0.71 µs.
- With caches off, flash wait states raise the cycle counts at 480 MHz
  (yield mean 879 to 1,115 cycles).
- A partition switch costs about 175 cycles more than a rotation
  inside one partition at -O2 with caches on, for reprogramming the MPU
  regions.
- The tick maxima are above the means (1,192 against 370 cycles with caches
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
| Yield to peer | 339 / 529 | 173 / 205 | 338 / 394 |
| Queue send to receiver resumes | 849 / 1,741 | 521 / 907 | 954 / 1,501 |
| Mutex unlock to waiter resumes | 819 / 1,034 | 516 / 895 | 882 / 1,416 |
| **Caches off** | | | |
| Yield to peer | 1,115 / 1,212 | 462 / 496 | 703 / 820 |
| Queue send to receiver resumes | 2,936 / 4,217 | 1,779 / 3,201 | 3,009 / 3,337 |
| Mutex unlock to waiter resumes | 2,727 / 2,764 | 1,651 / 3,054 | 2,755 / 4,966 |

MAXRTOS cycles divided by FreeRTOS MPU cycles:

| Latency | Caches on, mean | Caches on, max | Caches off, mean | Caches off, max |
|---|---|---|---|---|
| Yield | 1.0x | 1.3x | 1.6x | 1.5x |
| Queue handoff | 0.9x | 1.2x | 1.0x | 1.3x |
| Mutex handoff | 0.9x | 0.7x | 1.0x | 0.6x |

Image size (bytes, whole firmware):

| | text | data | bss |
|---|---|---|---|
| MAXRTOS bench | 11,364 | 24 | 7,984 |
| FreeRTOS plain | 10,032 | 24 | 12,752 |
| FreeRTOS MPU | 15,060 | 952 | 23,512 |

Observations:

- The yield mean equals FreeRTOS MPU's with caches on (1.0x; the maximum is
  1.3x) and is 1.6x with caches off. The queue and mutex handoff means are
  within 10% of it. Against the plain port MAXRTOS is slower on the yield (2.0x
  with caches on, 2.4x with caches off) and on the handoffs (1.6x to 1.7x).
- The MPU roughly doubles FreeRTOS's yield (173 to 338 cycles with caches
  on) and adds about 370 to 430 cycles to the queue and mutex handoffs.
- The MAXRTOS bench image (with LTO) is about 13% larger in flash than
  FreeRTOS plain and about 25% smaller than FreeRTOS MPU. The bss figures
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
  same cycles as the FreeRTOS MPU yield (339 against 342).

Limits:

- FreeRTOS has no partitions, so the groups run one at a time (a controller
  task suspends and resumes them every 250 ticks) and there is no equivalent
  of `TICK_SWITCH`. The FreeRTOS `TICK_SWITCH` slot holds a different
  measurement: SysTick entry to a higher-priority task woken from
  `vTaskDelay(1)`. It is not comparable and is left out of the tables.
- The FreeRTOS tasks are privileged, so the cost of the unprivileged system
  call path (SVC) is not measured.
- One run per configuration, so the maxima may move on longer runs.
