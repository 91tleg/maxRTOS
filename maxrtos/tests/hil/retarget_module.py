#!/usr/bin/env python3
"""Retarget a QEMU test module configuration to the NUCLEO-H753ZI.

    retarget_module.py in.json out.json [--region NUMBER:BASE:SIZE:ACCESS[:x]]...

The QEMU tests describe partitions, processes, ports and health-monitor policy
in module*.json for the mps2-an500 memory map. The HIL build reuses them so the
two suites stay in step, and swaps in the memory map and the background MPU
regions of the board. Each --region adds one more background MPU region (for
example one over an address that has no memory behind it). ACCESS is one of
the generator's access names; a trailing ":x" makes the region executable.

The regions written here are what test_mpu_regs.c expects to read back from the
MPU hardware; change them together.
"""

import argparse
import json
import sys

MEMORY = {
    "flash": {"origin": "0x08000000", "size": "2M"},
    "dtcm": {"origin": "0x20000000", "size": "128K"},
    "sram": {"origin": "0x24000000", "size": "256K"},
}

REGIONS = [
    {"number": 0, "base": "0x08000000", "size": "2M",
     "access": "read_only", "executable": True},
    {"number": 1, "base": "0x58000000", "size": "512K",
     "access": "priv_read_write", "executable": False},
    {"number": 2, "base": "0x20000000", "size": "128K",
     "access": "priv_read_write", "executable": False},
    {"number": 3, "base": "0x24000000", "size": "512K",
     "access": "priv_read_write", "executable": False},
    # The tests' result block, in the half of the SRAM that "sram" above leaves
    # free (see EMU_RESULTS_BASE in the HIL CMakeLists.txt).
    {"number": 5, "base": "0x24040000", "size": "2K",
     "access": "read_write", "executable": False},
]


def parse_region(text):
    parts = text.split(":")
    if len(parts) not in (4, 5) or (len(parts) == 5 and parts[4] != "x"):
        raise argparse.ArgumentTypeError(
            f"bad --region {text!r}, expected NUMBER:BASE:SIZE:ACCESS[:x]")
    return {"number": int(parts[0]), "base": parts[1], "size": parts[2],
            "access": parts[3], "executable": len(parts) == 5}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("source")
    ap.add_argument("target")
    ap.add_argument("--region", action="append", default=[], type=parse_region)
    args = ap.parse_args()

    with open(args.source) as f:
        module = json.load(f)

    module["memory"] = MEMORY
    mpu = module.setdefault("mpu", {})
    mpu["regions"] = REGIONS + args.region

    with open(args.target, "w") as f:
        json.dump(module, f, indent=2)
        f.write("\n")


if __name__ == "__main__":
    sys.exit(main())
