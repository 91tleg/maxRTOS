# maxrtos_codegen

Generates the MPU/partition-table/frame-schedule glue and linker script for a
maxrtos partitioned application, from a single JSON config.

## What it generates

| File              | Contents                                                                 |
|-------------------|---------------------------------------------------------------------------|
| `link.ld`         | `MEMORY` map + per-partition `NOLOAD` stack sections, packed automatically |
| `maxrtos_config.h`   | `PARTITION_ID_*` constants, `extern` stack arrays, `maxrtos_config_init/start()` |
| `maxrtos_config.c`   | MPU region setup, partition table wiring, frame schedule, scheduler start |

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
- `partitions[]`: `id`, `name` (C identifier), `stack_size`, `stack_alignment`
  (must be a power of two; `stack_size` must be a multiple of it — this is an
  MPU sub-region alignment requirement, not a style choice), and `mpu.access`
  / `mpu.executable` for that partition's own memory protection.
- `schedule.major_frame[]`: ordered `{partition, duration_ticks}` slots.
- `mpu.regions[]`: background MPU regions (flash, peripheral space, etc.)
  independent of any partition. `size` must be a power of two and `base`
  aligned to it (hardware MPU region requirement).

## Validation

The tool rejects configs that would compile but fault at runtime:

- non-power-of-two stack alignment or MPU region size
- stack_size not a multiple of stack_alignment
- MPU region base not aligned to its own size
- partition stacks that don't fit in their assigned memory region
- a `no_access` background MPU region overlapping a partition's stack region
- duplicate partition ids/names, frame slots referencing unknown partitions

## Generated API surface

```c
#include "maxrtos_config.h"

static void process_control_entry(void *arg)     { /* your logic */ }
static void process_application_entry(void *arg)  { /* your logic */ }

int main(void)
{
    board_init();            /* yours: clocks, GPIO, SysTick timing */
    maxrtos_config_init();      /* generated: MPU, partition table, context switch init */

    maxrtos_process_id_t id_control, id_application;
    maxrtos_process_create(maxrtos_stack_control, PARTITION_STACK_SIZE_control,
                            PARTITION_ID_control, 5U, process_control_entry, NULL, &id_control);
    maxrtos_process_create(maxrtos_stack_application, PARTITION_STACK_SIZE_application,
                            PARTITION_ID_application, 5U, process_application_entry, NULL, &id_application);

    maxrtos_config_start(id_control, id_application);  /* generated: binds + starts scheduler, does not return */
}

void SysTick_Handler(void) { maxrtos_arch_systick(); }
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
└── examples/
    └── example_module.json
```
