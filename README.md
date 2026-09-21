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
- **IPC:** queuing ports with blocking send/receive and timeouts.
- **Health monitor:** per-partition recovery policy per fault class
  (restart process, halt partition, ignore), including deadline misses.

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
| SIL | Kernel and arch code on a virtual Cortex-M7 | [tests/sil/README.md](maxrtos/tests/sil/README.md), [REQUIREMENTS.md](maxrtos/tests/sil/REQUIREMENTS.md) |
| QEMU | Real firmware on `mps2-an500`; the exit code is the verdict | [tests/qemu/README.md](maxrtos/tests/qemu/README.md) |
| HIL | Board tests | `maxrtos/tests/hil` |
