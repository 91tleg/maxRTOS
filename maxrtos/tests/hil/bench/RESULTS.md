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
