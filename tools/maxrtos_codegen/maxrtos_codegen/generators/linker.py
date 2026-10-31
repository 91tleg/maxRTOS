from __future__ import annotations

import pathlib
from types import SimpleNamespace

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from ..schema import RtosConfig, select_kernel_region


_TEMPLATE_DIR = pathlib.Path(__file__).parent.parent / "templates"


def generate_linker_script(
    cfg: RtosConfig,
    config_path: str,
    min_heap_size: int = 0x200,
    min_stack_size: int = 0x400,
) -> str:
    """
    Generate the linker script for an RTOS configuration.

    Partition domains (one per partition, holding all its process stacks) are grouped by their configured
    memory region so the linker template can emit deterministic NOLOAD
    sections. Kernel data, BSS, heap, and the main stack are placed in the
    first available non-flash memory region that is not assigned to
    partition stacks.

    Args:
        cfg: Validated RTOS configuration.
        config_path: Path to the source configuration file. Included in the
            generated linker script for traceability.
        min_heap_size: Minimum heap size in bytes.
        min_stack_size: Minimum main stack size in bytes.

    Returns:
        The generated linker script as a string.

    Raises:
        ValueError: If no memory region is available for kernel data,
            BSS, heap, and the main stack.
    """
    env = Environment(
        loader=FileSystemLoader(str(_TEMPLATE_DIR)),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )

    template = env.get_template("linker.ld.j2")

    # Group partition stacks by memory region. The template uses these
    # groups to emit sequential NOLOAD sections without hard-coded
    # addresses or manual offset calculations.
    stack_regions: dict[str, list] = {}

    for partition in cfg.partitions:
        # Processes in offset order so the linker places input sections
        # exactly where the schema's packing put them.
        ordered = [
            partition.processes[i]
            for i in sorted(
                range(len(partition.processes)),
                key=lambda k: partition.stack_offsets[k],
            )
        ]
        stack_regions.setdefault(
            partition.stack_region,
            [],
        ).append(SimpleNamespace(
            name=partition.name,
            domain_size=partition.domain_size,
            processes=ordered,
        ))

    # Kernel .data, .bss, heap and the main stack go in the region chosen by
    # the schema (it also checks that no unprivileged MPU region covers it).
    kernel_region = select_kernel_region(cfg.memory, set(stack_regions.keys()))

    if kernel_region is None:
        raise ValueError(
            "no memory region available for kernel .data/.bss "
            "(all non-flash regions are used for partition stacks) -- "
            "add a dedicated region, e.g. 'dtcm'"
        )

    return template.render(
        config_path=config_path,
        memory=cfg.memory,
        stack_regions=stack_regions,
        kernel_region=kernel_region,
        min_heap_size=hex(min_heap_size),
        min_stack_size=hex(min_stack_size),
    )
