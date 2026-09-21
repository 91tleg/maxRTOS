# maxrtos_codegen

Generates the MPU/partition-table/frame-schedule glue and linker script for a
maxrtos partitioned application, from a single JSON config.

## What it generates

| File              | Contents                                                                 |
|-------------------|---------------------------------------------------------------------------|
| `link.ld`         | `MEMORY` map + one `NOLOAD` MPU-domain section per partition holding all its process stacks |
| `maxrtos_config.h`   | `PARTITION_ID_*`, `PARTITION_DOMAIN_SIZE_*`, per-process `extern` stacks + `PROCESS_STACK_SIZE_*`/`PROCESS_PRIORITY_*`, `maxrtos_config_init()` |
| `maxrtos_config.c`   | MPU region setup (one per partition), partition table wiring, frame schedule |

## What it deliberately does NOT generate

- **Board bring-up** (clocks, GPIO, pin muxing) — that's your `board_init()`.
- **SysTick timing** (`SYSTICK_HZ`, `CPU_CLOCK_HZ`, reload/enable) — this
  depends on your board's clock tree, not the partition config. Configure it
  yourself and route `SysTick_Handler()` to `maxrtos_arch_systick()` after
  calling `maxrtos_config_init()`.
- **Process entry-point bodies** — write `process_p0_entry()` etc. yourself
  and pass them to `maxrtos_process_create()`.

The generated code owns exactly one thing: turning your partition/memory/MPU
*declarations* into correct, boilerplate-free kernel wiring.

## Usage

```bash
pip install -e tools/maxrtos_codegen
maxrtos-codegen config.json --out-dir generated/
```

Re-run any time the config changes; output is deterministic and safe to
regenerate. Recommended: gitignore `generated/` and run this as a pre-build
step, or check it in and diff it in CI to catch drift from hand edits.

## Config format

See `examples/example_module.json`. Key sections:

- `memory`: named regions (`flash`, `dtcm`, `sram`, ...) with `origin`/`size`.
  Sizes accept `"512B"`, `"512"`, `"128K"`, `"2M"`.
- `stack_region`: which memory region holds partition stacks by default
  (override per-partition with a `"stack_region"` key on that partition).
- `partitions[]`: `id`, `name` (C identifier), `type`, `mpu.access` / `mpu.executable`
  for the partition's memory domain, and `processes[]`. Each process has
  `name` (C identifier, unique across the config), `stack_size`,
  `stack_alignment` (power of two; `stack_size` a multiple of it) and
  `priority` (0..`MAXRTOS_MAX_PRIORITY`), and optionally `period_ticks` (release
  interval, 0 = aperiodic) and `time_capacity_ticks` (deadline from each
  release, 0 = none; not more than a non-zero period). The old partition-level
  `stack_size`/`stack_alignment`/`priority`/`entry_symbol` keys are rejected.
- **Privilege is per partition.** `type` is `application` (the default) or
  `system`. Application partitions run unprivileged. A `system` partition runs
  privileged, all of its processes together (they share one memory domain, so
  a privileged process next to an unprivileged one would be an escalation
  path). There is no per-process privilege, and `maxrtos_process_create()` has
  no such parameter. Use `system` only for platform software such as a driver
  partition.
- **MPU isolation is per partition.** All processes of a partition share one
  MPU region, the *domain*: a naturally aligned power-of-two block covering all
  of that partition's stacks (sum of the stacks, rounded up). Processes in the
  same partition are therefore not isolated from each other's stacks.
- `health_monitor` (optional): recovery policy per fault class, as
  `{"default": {...}}` at the top level and optionally
  `partitions[].health_monitor` to override per partition. Fault classes:
  `memory_access`, `bus_error`, `illegal_instruction`, `divide_by_zero`.
  Actions: `restart_process` (default for all) and `halt_partition`.
  `maxrtos_config_init()` installs the table and enables the fault
  exceptions; without it an MPU violation would escalate to HardFault
  unclassified. `ignore` is rejected (it retries the faulting instruction
  forever). `unexpected_return` is not raised by the architecture layer yet, so
  it is not configurable.
  - `restart_process`: the faulting process gets a fresh initial context
    and is rescheduled; other processes and partitions are unaffected.
  - `halt_partition`: the partition is never scheduled again. Its frame
    slots run the architecture idle context (privileged WFI) so the rest of
    the schedule keeps its timing.
- **Periodic processes and deadlines.** A process with `period_ticks` is released
  every period; `time_capacity_ticks` is its deadline, measured from the start
  of each release, in wall-clock ticks (not a CPU budget). The generated header
  has `PROCESS_PERIOD_TICKS_<name>` and `PROCESS_TIME_CAPACITY_TICKS_<name>`;
  pass them to `maxrtos_process_set_timing()` (`maxrtos/kernel/timing.h`) after
  creating the process, and call `maxrtos_periodic_wait()`
  (`maxrtos/timing.h`) at the end of every release. A process that has not
  completed a release by its deadline raises `deadline_exceeded` into its
  partition's health monitor (default `restart_process`, or `halt_partition`).
  While it waits for its release with nothing else ready, the CPU idles.
- `mpu.privileged_default_map` (optional, default `false`): MPU_CTRL.PRIVDEFENA.
  `false` denies every access no region permits, privileged included, so the
  kernel's own code, data and stacks need explicit privileged regions; the
  generator then requires flash and every other memory region to be covered by
  an `mpu.regions` entry. `true` lets privileged code use the default map: it
  is a stopgap while a missing region is being found, and it leaves any
  privileged code able to reach all memory no region covers. If a fault occurs,
  read `g_maxrtos_arch_last_fault` in a debugger: `mmfar` is the refused
  address.
- `schedule.major_frame[]`: ordered `{partition, duration_ticks}` slots.
- `mpu.regions[]`: background MPU regions (flash, peripheral space, etc.)
  independent of any partition. `size` must be a power of two and `base`
  aligned to it (hardware MPU region requirement).

## Validation

The tool rejects configs that would compile but fault at runtime:

- non-power-of-two stack alignment or MPU region size
- stack_size not a multiple of stack_alignment
- MPU region base not aligned to its own size
- partition domains that don't fit in their assigned memory region
- legacy single-stack partitions, duplicate process names, bad priorities,
  more processes than `MAXRTOS_MAX_PROCESSES`
- a `no_access` background MPU region overlapping a partition's stack region
- duplicate partition ids/names, frame slots referencing unknown partitions

## Generated API surface

```c
#include "maxrtos_config.h"
#include "maxrtos/scheduler.h"

static void control_main_entry(void *arg) { /* your logic */ }
static void application_main_entry(void *arg) { /* your logic */ }

int main(void)
{
    maxrtos_process_id_t id;

    board_init();            /* yours: clocks, GPIO, SysTick timing */
    maxrtos_config_init();   /* generated: MPU, partition table, frame schedule */

    /* You create the processes; the generated header supplies the stacks. */
    maxrtos_process_create(maxrtos_stack_control_main, PROCESS_STACK_SIZE_control_main,
                           PARTITION_ID_control, PROCESS_PRIORITY_control_main, control_main_entry, NULL, &id);
    maxrtos_process_create(maxrtos_stack_application_main, PROCESS_STACK_SIZE_application_main,
                           PARTITION_ID_application, PROCESS_PRIORITY_application_main, application_main_entry, NULL, &id);

    maxrtos_scheduler_start();   /* registers created processes, does not return */
}

void SysTick_Handler(void) { maxrtos_arch_systick(); }
```

## Tests

```bash
pip install -e tools/maxrtos_codegen pytest
pytest tools/maxrtos_codegen/tests
```

## Layout

```
maxrtos_codegen/
├── maxrtos_codegen/
│   ├── schema.py             # JSON -> validated dataclasses (size/addr parsing, invariants)
│   ├── cli.py                # maxrtos-codegen entry point
│   ├── generators/
│   │   ├── linker.py         # RtosConfig -> link.ld
│   │   └── c_config.py       # RtosConfig -> maxrtos_config.h / .c
│   └── templates/            # Jinja2 templates (the only place C/linker syntax lives)
├── tests/
└── examples/
    └── example_module.json
```
