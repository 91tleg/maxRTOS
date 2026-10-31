# MAXRTOS

A statically configured, time partitioned RTOS for ARM Cortex-M7.
Partitions get fixed time slots and MPU-enforced memory domains; each partition
runs its own set of processes. Everything (memory map, partitions, processes,
schedule, health-monitor policy) is described in one JSON file and turned into
C and a linker script at build time.

## What it provides

- **Time partitioning:** a fixed major frame of partition slots.
- **Spatial partitioning:** one MPU domain per partition, with an optional
  deny-by-default map (`privileged_default_map: false`).
- **Processes:** several per partition, each with its own stack, priority,
  and optional `period` / `time_capacity` (deadline supervision).
- **Privilege:** a partition attribute. Application partitions run
  unprivileged; system partitions run privileged.
- **IPC:** queuing ports with blocking send/receive and timeouts, and sampling
  ports that hold the latest message. A port is named by an ID and held by the
  kernel, in memory no partition can map; the module lists which partitions may
  send (write) on it and which may receive (read).
- **Health monitor:** per-partition recovery policy per fault class
  (restart process, halt partition, ignore), including deadline misses.
  When the kernel cannot continue (a fault it cannot attribute to a process,
  or a failure of the recovery path) it calls `maxrtos_arch_fatal()`, a weak
  function that halts by default. Define your own to log and reset. The kernel
  does not own a watchdog: choose one, configure it, and feed it from your
  `SysTick_Handler` or another place you control.

## Layout

| Path | Contents |
|---|---|
| `maxrtos/kernel` | Architecture-independent scheduling, IPC and recovery logic |
| `maxrtos/arch/cortex_m7` | Context switch, SVC, MPU, fault handlers, SysTick |
| `maxrtos/include/maxrtos` | Public application API |
| `tools/maxrtos_codegen` | JSON -> `link.ld`, `maxrtos_config.{c,h}` |
| `demo/nucleo_h753zi` | Example application for the NUCLEO-H753ZI |

The arch layer calls the kernel; the kernel never calls the arch layer.

## Using it

1. Describe the system in JSON and generate the configuration
   (schema and options: [tools/maxrtos_codegen/README.md](tools/maxrtos_codegen/README.md)):

   ```
   maxrtos-codegen module.json --out-dir generated
   ```

2. Provide the application-owned pieces: the **startup file** (vector table
   and reset handler; a reference is in
   [`maxrtos/arch/cortex_m7/examples/startup.S`](maxrtos/arch/cortex_m7/examples/startup.S),
   which also lists the handlers it must point at), the SysTick setup, and
   `main`.
3. In `main`: call `maxrtos_config_init()`, create your processes,
   then `maxrtos_scheduler_start()`.

See [`demo/nucleo_h753zi`](demo/nucleo_h753zi) for a complete example.

## FPU

Every process may use the Cortex-M7 FPU. The kernel enables it at
initialization and saves and restores the whole FPU state (s0-s31, FPSCR) with
each process's context, so a process that is switched out and in again finds
its registers as it left them, and one process's FPU values never reach
another's: a new or restarted process starts from zeros.

The kernel stays soft-float. Build code that uses the FPU with
`-mfpu=fpv5-d16 -mfloat-abi=softfp`, which has the soft-float calling
convention and so links with the kernel. Do not build the kernel that way: its
exception handlers must not touch the FPU registers. Under link-time
optimization, build such a file without it (`-fno-lto`): the link step builds
every unit with the target's options and cannot assemble FPU instructions that
were compiled for another set.

Each process needs stack for a 200-byte saved context and a 104-byte exception
frame on top of its own use; the code generator rejects stacks below 512 bytes.
The context switch checks a process's stack pointer against its stack before it
writes the context, and treats a process that left its stack as having faulted.

## Build

Host tests:

```
cmake -S maxrtos/tests -B build/tests -G Ninja
cmake --build build/tests && ctest --test-dir build/tests --output-on-failure
```

Firmware (needs `arm-none-eabi-gcc`):

```
cmake -S demo/nucleo_h753zi -B build/nucleo_h753zi -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/arm-none-eabi.cmake
cmake --build build/nucleo_h753zi
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program build/nucleo_h753zi/maxrtos_demo_nucleo_h753zi verify reset exit"
```

## Testing

| Level | What it runs | Docs |
|---|---|---|
| Unit | Kernel logic on the host | `maxrtos/tests/unit` |
| SIL | Kernel and arch code on a virtual Cortex-M7 | [tests/sil/README.md](maxrtos/tests/sil/README.md) |
| QEMU | Real firmware on `mps2-an500`; the exit code is the verdict | [tests/qemu/README.md](maxrtos/tests/qemu/README.md) |
| HIL | The kernel on the NUCLEO-H753ZI: MPU, faults, clock, context switch | [tests/hil/README.md](maxrtos/tests/hil/README.md) |
| Benchmark | Context-switch latency on the NUCLEO-H753ZI | [bench/README.md](maxrtos/tests/hil/bench/README.md), [results](maxrtos/tests/hil/bench/RESULTS.md) |

## Performance

Yield to a partition peer takes 348 cycles on average (0.73 µs), 552 at worst
(1.15 µs), at 480 MHz, -O2, caches on. The benchmark runs with every partition
privileged, so a switch into an unprivileged partition, which also changes
`CONTROL.nPRIV`, is not included.
Full tables, including 64 MHz and caches off: [RESULTS.md](maxrtos/tests/hil/bench/RESULTS.md).

SysTick and PendSV run at the lowest exception priority, so any interrupt
handler of higher priority delays a partition switch by at least its own
run time. The slot boundaries are exact in the tick count; the instant the
next partition starts running is not. Budget the longest interrupt handler
you add, plus the switch itself, as jitter at every slot boundary.
