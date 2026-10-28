#!/usr/bin/env python3
"""Build, flash and run the context-switch benchmark, then print the results.

    run_bench.py --cpu-hz 64000000 [--caches] [--seconds N] [--no-build]

Needs arm-none-eabi-gcc, cmake, ninja, openocd and a NUCLEO-H753ZI. The board
runs at whatever clock the startup code leaves it on (the reset default HSI is
64 MHz); pass that as --cpu-hz so cycles can be shown in microseconds.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SRC = Path(__file__).resolve().parent

NAMES = ["YIELD", "TICK_ROTATE", "TICK_SWITCH", "QUEUE_HANDOFF", "MUTEX_HANDOFF"]
MAGIC = 0xB3AC0001
STAT_WORDS = 7
WORDS = 4 + STAT_WORDS * len(NAMES)  # header words + words per statistic


def run(cmd, **kw):
    try:
        return subprocess.run(cmd, check=True, text=True, capture_output=True, **kw)
    except subprocess.CalledProcessError as exc:
        sys.exit(f"command failed: {' '.join(cmd)}\n{exc.stdout}\n{exc.stderr}")
    except FileNotFoundError:
        sys.exit(f"{cmd[0]} not found; is it installed and on PATH?")


def build(build_dir, caches, opt, mhz480=False, lto=True):
    try:
        import jinja2  # noqa: F401  (needed by the code generator)
    except ImportError:
        sys.exit("jinja2 is required by the code generator: pip install jinja2")

    run(["cmake", "-S", str(SRC), "-B", str(build_dir), "-G", "Ninja",
         f"-DCMAKE_TOOLCHAIN_FILE={ROOT / 'cmake' / 'arm-none-eabi.cmake'}",
         f"-DBENCH_CACHES={'ON' if caches else 'OFF'}",
         f"-DBENCH_480MHZ={'ON' if mhz480 else 'OFF'}",
         f"-DMAXRTOS_OPT_LEVEL={opt}",
         f"-DMAXRTOS_LTO={'ON' if lto and opt != '-O0' else 'OFF'}",
         # The code generator needs jinja2: use the interpreter running this
         # script, not whichever python CMake finds first.
         f"-DPython3_EXECUTABLE={sys.executable}"])
    run(["cmake", "--build", str(build_dir)])
    return build_dir / "bench_context_switch"


def symbol(elf, name):
    out = run(["arm-none-eabi-nm", str(elf)]).stdout
    m = re.search(rf"^([0-9a-f]+) \S {name}$", out, re.M)
    if not m:
        sys.exit(f"symbol {name} not found in {elf}")
    return int(m.group(1), 16)


def read_words(elf, address, seconds):
    """Flash, run for a while, then read g_bench through OpenOCD."""
    # The extra halt/resume after reset lets the core past a halt at the first
    # PendSV entry that was seen after flashing the caches-off -O2 image.
    cmds = (f"program {elf} verify; reset run; sleep 500; halt; resume; "
            f"sleep {seconds * 1000}; halt; "
            f"puts [read_memory 0x{address:08x} 32 {WORDS}]; exit")
    out = run(["openocd", "-f", "interface/stlink.cfg",
               "-f", "target/stm32h7x.cfg", "-c", cmds])
    text = out.stdout + out.stderr
    # `mdw` prints nothing with -c, so the words are read with read_memory,
    # which returns a Tcl list of hex numbers on the last line.
    words = []
    for line in text.splitlines():
        if re.fullmatch(r"(0x[0-9a-fA-F]+\s*)+", line.strip()):
            words = [int(w, 16) for w in line.split()]
    if len(words) < WORDS:
        sys.exit("could not read the results:\n" + text)
    return words[:WORDS]


def report(words, cpu_hz):
    magic, done, caches, overhead = words[:4]
    if magic != MAGIC:
        sys.exit("results block not found (did the firmware start?)")

    print(f"caches: {'on' if caches else 'off'}   measurement overhead: "
          f"{overhead} cycles   finished: {'yes' if done else 'NO (partial)'}")
    print(f"{'latency':<15}{'samples':>9}{'min':>9}{'mean':>11}{'max':>9}"
          f"{'max us':>10}")

    for i, name in enumerate(NAMES):
        count, lo, hi, sum_lo, sum_hi, tag, over = words[4 + STAT_WORDS * i: 4 + STAT_WORDS * (i + 1)]
        if count == 0:
            print(f"{name:<15}{0:>9}")
            continue
        mean = ((sum_hi << 32) | sum_lo) / count
        print(f"{name:<15}{count:>9}{lo:>9}{mean:>11.1f}{hi:>9}"
              f"{hi / cpu_hz * 1e6:>10.2f}"
              + (f"   outliers>30k: {over}, max at partition {tag >> 24}, "
                   f"tick {tag & 0xFFFFFF}" if name.startswith("TICK") else ""))

    print("\ncycles; subtract the overhead for a single event. Handoff figures "
          "include the send/unlock and the yield.")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--cpu-hz", type=int, default=64_000_000)
    p.add_argument("--480mhz", dest="mhz480", action="store_true",
                   help="firmware sets up the PLL for a 480 MHz core (implies --cpu-hz 480000000)")
    p.add_argument("--caches", action="store_true")
    p.add_argument("--opt", default="-O0",
                   help="optimization flag for kernel, arch and bench (default -O0)")
    p.add_argument("--no-lto", action="store_true",
                   help="build without link-time optimization (it is on above -O0)")
    p.add_argument("--seconds", type=int, default=15,
                   help="how long to let the benchmark run before reading")
    p.add_argument("--no-build", action="store_true")
    p.add_argument("--build-dir", type=Path, default=ROOT / "build" / "bench")
    args = p.parse_args()

    if args.mhz480:
        args.cpu_hz = 480_000_000

    elf = args.build_dir / "bench_context_switch"
    if not args.no_build:
        elf = build(args.build_dir, args.caches, args.opt, args.mhz480,
                    not args.no_lto)

    report(read_words(elf, symbol(elf, "g_bench"), args.seconds), args.cpu_hz)


if __name__ == "__main__":
    main()
