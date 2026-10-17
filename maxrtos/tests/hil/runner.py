#!/usr/bin/env python3
"""Build, flash and run the hardware-in-the-loop tests on a NUCLEO-H753ZI.

    runner.py [-k SUBSTRING]... [--list] [--no-build] [--junit FILE]

Each test is one firmware (see CMakeLists.txt). The runner flashes it, lets it
run, and reads the verdict block the harness keeps in RAM (hil_result.h) over
SWD while the core runs. No debugger session is needed: it talks to OpenOCD
through the Tcl port.

Needs arm-none-eabi-gcc, arm-none-eabi-nm, cmake, ninja, openocd, and Python 3
with jinja2 (the code generator needs it at build time). The runner itself uses
only the standard library.
"""

import argparse
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]

# Layout of hil_result_t in hil_result.h: magic, state, ticks, tick_hz, then
# the NUL-terminated reason text.
HIL_RESULT_FORMAT = "<4I64s"
HIL_RESULT_SIZE = struct.calcsize(HIL_RESULT_FORMAT)
HIL_MAGIC = 0x48494C31
STATE_RUNNING, STATE_PASS, STATE_FAIL = 0, 1, 2

# g_maxrtos_arch_last_fault: cfsr, hfsr, mmfar, bfar, count.
LAST_FAULT_FORMAT = "<5I"

OPENOCD_CONFIG = ("interface/stlink.cfg", "target/stm32h7x.cfg")

STARTUP_SECONDS = 5.0
POLL_SECONDS = 0.05
HALT_CHECK_SECONDS = 0.5


class HilError(Exception):
    """The test could not be run or did not pass."""


def run(cmd, **kwargs):
    try:
        return subprocess.run(cmd, check=True, text=True, capture_output=True, **kwargs)
    except subprocess.CalledProcessError as exc:
        raise SystemExit(f"command failed: {' '.join(map(str, cmd))}\n{exc.stdout}\n{exc.stderr}")
    except FileNotFoundError:
        raise SystemExit(f"{cmd[0]} not found; is it installed and on PATH?")


def build(build_dir):
    try:
        import jinja2  # noqa: F401  (needed by the code generator)
    except ImportError:
        raise SystemExit("jinja2 is required by the code generator: pip install jinja2")

    print(f"building into {build_dir} ...", flush=True)
    run(["cmake", "-S", str(HERE), "-B", str(build_dir), "-G", "Ninja",
         f"-DCMAKE_TOOLCHAIN_FILE={ROOT / 'cmake' / 'arm-none-eabi.cmake'}",
         # The code generator needs jinja2: use the interpreter running this
         # script, not whichever python CMake finds first.
         f"-DPython3_EXECUTABLE={sys.executable}"])
    run(["cmake", "--build", str(build_dir)])


def symbol(elf, name):
    out = run(["arm-none-eabi-nm", str(elf)]).stdout
    match = re.search(rf"^([0-9a-f]+) \S {name}$", out, re.M)
    if not match:
        raise HilError(f"symbol {name} not found in {elf.name}")
    return int(match.group(1), 16)


class OpenOCD:
    """One OpenOCD process, driven over its Tcl RPC port."""

    def __init__(self, port):
        self.port = port
        self.log = tempfile.NamedTemporaryFile("w+", prefix="openocd_", suffix=".log")
        self.proc = None
        self.sock = None

    def __enter__(self):
        args = ["openocd"]
        for config in OPENOCD_CONFIG:
            args += ["-f", config]
        args += ["-c", "gdb_port disabled", "-c", "telnet_port disabled",
                 "-c", f"tcl_port {self.port}"]
        try:
            self.proc = subprocess.Popen(args, stdout=self.log, stderr=self.log)
        except FileNotFoundError:
            raise SystemExit("openocd not found; is it installed and on PATH?")

        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                break
            try:
                self.sock = socket.create_connection(("127.0.0.1", self.port), timeout=5)
                return self
            except OSError:
                time.sleep(0.1)

        self.close()
        raise SystemExit("could not start OpenOCD; is the board connected?\n" + self.output())

    def __exit__(self, *exc):
        self.close()

    def output(self):
        self.log.flush()
        self.log.seek(0)
        return self.log.read()

    def close(self):
        if self.sock is not None:
            try:
                self.command("shutdown")
            except (OSError, HilError):
                pass
            self.sock.close()
            self.sock = None
        if self.proc is not None:
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
            self.proc = None

    def command(self, text, timeout=60):
        """Run a Tcl command and return its result; raise if it failed."""
        # OpenOCD's RPC does not report failure, so catch it in Tcl.
        wrapped = "set rc [catch {%s} res]; return \"$rc $res\"" % text
        self.sock.settimeout(timeout)
        try:
            self.sock.sendall(wrapped.encode() + b"\x1a")
            reply = b""
            while not reply.endswith(b"\x1a"):
                chunk = self.sock.recv(65536)
                if not chunk:
                    raise HilError("OpenOCD closed the connection\n" + self.output())
                reply += chunk
        except OSError as exc:
            raise HilError(f"OpenOCD connection failed ({exc})\n" + self.output())
        code, _, result = reply[:-1].decode(errors="replace").partition(" ")
        if code != "0":
            raise HilError(f"OpenOCD: {text}: {result}")
        return result

    def read_words(self, address, count):
        out = self.command(f"read_memory 0x{address:08x} 32 {count}")
        return [int(word, 16) for word in out.split()]

    def read_bytes(self, address, size):
        return struct.pack(f"<{size // 4}I", *self.read_words(address, size // 4))

    def write_word(self, address, value):
        self.command(f"write_memory 0x{address:08x} 32 {{0x{value:08x}}}")

    def state(self):
        return self.command("[target current] curstate")

    def pc(self):
        match = re.search(r"0x[0-9a-fA-F]+", self.command("reg pc"))
        return int(match.group(0), 16) if match else None


@dataclass
class Result:
    name: str
    passed: bool = False
    detail: str = ""
    seconds: float = 0.0
    samples: list = field(default_factory=list)  # (host time, ticks)


@dataclass
class TestInfo:
    timeout: float = 20.0
    check: object = None  # fn(samples, final block) -> None, raises HilError


def check_timebase(samples, block):
    """The kernel tick must run at its nominal rate in real time.

    The firmware has already shown that a tick is exactly the programmed
    number of core cycles. Comparing ticks with the host clock shows the core
    clock is the one the harness assumed.
    """
    window = [(t, ticks) for t, ticks in samples if ticks >= 500]
    if len(window) < 2 or window[-1][0] - window[0][0] < 2.0:
        raise HilError("too few samples to measure the tick rate")
    (t0, n0), (t1, n1) = window[0], window[-1]
    rate = (n1 - n0) / (t1 - t0)
    nominal = block["tick_hz"]
    error = (rate - nominal) / nominal
    note = f"{rate:.1f} Hz measured vs {nominal} Hz ({error * 100:+.2f}%)"
    if abs(error) > 0.03:
        raise HilError(f"tick rate off: {note}; is the core clock the 64 MHz the harness assumes?")
    return note


# Per-test settings; tests not listed use the defaults.
TESTS = {
    "timebase": TestInfo(timeout=30.0, check=check_timebase),
}


def parse_block(raw):
    magic, state, ticks, tick_hz, reason = struct.unpack(HIL_RESULT_FORMAT, raw)
    return {"magic": magic, "state": state, "ticks": ticks, "tick_hz": tick_hz,
            "reason": reason.split(b"\0", 1)[0].decode(errors="replace")}


def diagnostics(oc, elf):
    """What the core was doing, for a test that never finished."""
    lines = []
    try:
        if oc.state() == "running":
            oc.command("halt")
        pc = oc.pc()
        if pc is not None:
            lines.append(f"pc=0x{pc:08x}")
        fault = struct.unpack(LAST_FAULT_FORMAT,
                              oc.read_bytes(symbol(elf, "g_maxrtos_arch_last_fault"), 20))
        lines.append("last fault: cfsr=0x%08x hfsr=0x%08x mmfar=0x%08x bfar=0x%08x count=%d" % fault)
    except (HilError, OSError) as exc:
        lines.append(f"(no diagnostics: {exc})")
    return "; ".join(lines)


def run_test(oc, name, elf, timeout_scale, info):
    result = Result(name)
    started = time.monotonic()
    result_address = symbol(elf, "g_hil_result")
    timeout = info.timeout * timeout_scale

    try:
        oc.command(f"program {{{elf}}} verify")
        # Clear the verdict so a block left by the previous test is not read
        # back before this firmware has started.
        oc.write_word(result_address, 0)
        oc.write_word(result_address + 4, 0)
        oc.command("reset run")

        block = None
        last_halt_check = time.monotonic()
        start_deadline = time.monotonic() + STARTUP_SECONDS
        run_deadline = None

        while True:
            now = time.monotonic()
            block = parse_block(oc.read_bytes(result_address, HIL_RESULT_SIZE))

            if block["magic"] != HIL_MAGIC:
                if now > start_deadline:
                    raise HilError("firmware did not start (no verdict block); "
                                   + diagnostics(oc, elf))
            else:
                if run_deadline is None:
                    run_deadline = now + timeout
                if block["state"] == STATE_RUNNING:
                    result.samples.append((now, block["ticks"]))
                else:
                    break
                if now > run_deadline:
                    raise HilError(f"no verdict after {timeout:.0f}s "
                                   f"({block['ticks']} ticks); " + diagnostics(oc, elf))

            if now - last_halt_check > HALT_CHECK_SECONDS:
                last_halt_check = now
                if oc.state() == "halted":
                    raise HilError("core halted (HALT or breakpoint) before a verdict; "
                                   + diagnostics(oc, elf))

            time.sleep(POLL_SECONDS)

        if block["state"] == STATE_FAIL:
            raise HilError(f"{block['reason'] or 'failed (no reason given)'} (tick {block['ticks']})")

        result.detail = f"{block['ticks']} ticks"
        if info.check is not None:
            result.detail += "; " + (info.check(result.samples, block) or "")
        result.passed = True
    except HilError as exc:
        result.detail = str(exc)

    result.seconds = time.monotonic() - started
    return result


def write_junit(path, results):
    suite = ET.Element("testsuite", name="hil", tests=str(len(results)),
                       failures=str(sum(not r.passed for r in results)),
                       time=f"{sum(r.seconds for r in results):.3f}")
    for r in results:
        case = ET.SubElement(suite, "testcase", classname="hil", name=r.name,
                             time=f"{r.seconds:.3f}")
        if not r.passed:
            ET.SubElement(case, "failure", message=r.detail.splitlines()[0]).text = r.detail
    ET.ElementTree(suite).write(path, encoding="utf-8", xml_declaration=True)


def discover(build_dir):
    manifest = build_dir / "hil_tests.txt"
    if not manifest.exists():
        raise SystemExit(f"{manifest} not found; build first (drop --no-build)")
    return [line for line in manifest.read_text().split() if line]


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("-k", dest="filters", action="append", default=[], metavar="SUBSTRING",
                   help="run only tests whose name contains this (repeatable)")
    p.add_argument("--list", action="store_true", help="list the tests and exit")
    p.add_argument("--no-build", action="store_true", help="use the existing build")
    p.add_argument("--build-dir", type=Path, default=ROOT / "build" / "hil")
    p.add_argument("--junit", type=Path, metavar="FILE", help="also write a JUnit XML report")
    p.add_argument("--timeout-scale", type=float, default=1.0,
                   help="multiply every test's time limit (slow host, slow clock)")
    p.add_argument("--openocd-port", type=int, default=6666)
    args = p.parse_args()

    if not args.no_build:
        build(args.build_dir)

    names = discover(args.build_dir)
    if args.filters:
        names = [n for n in names if any(f in n for f in args.filters)]
    if args.list:
        print("\n".join(names))
        return 0
    if not names:
        raise SystemExit("no tests selected")

    results = []
    with OpenOCD(args.openocd_port) as oc:
        for name in names:
            elf = args.build_dir / f"hil_{name}"
            print(f"{name:<26}", end="", flush=True)
            result = run_test(oc, name, elf, args.timeout_scale, TESTS.get(name, TestInfo()))
            results.append(result)
            print(f"{'PASS' if result.passed else 'FAIL'}  {result.seconds:5.1f}s  {result.detail}",
                  flush=True)

    failed = [r for r in results if not r.passed]
    print(f"\n{len(results) - len(failed)} of {len(results)} passed")
    if args.junit:
        write_junit(args.junit, results)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
