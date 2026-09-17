from __future__ import annotations

import pathlib

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from ..schema import RtosConfig


_TEMPLATE_DIR = pathlib.Path(__file__).parent.parent / "templates"


def generate_linker_script(
    cfg: RtosConfig,
    config_path: str,
    min_heap_size: int = 0x200,
    min_stack_size: int = 0x400,
) -> str:
    """
    Generate the linker script for an RTOS configuration.

    Partition stacks and static port buffers are grouped by their configured
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
        stack_regions.setdefault(
            partition.stack_region,
            [],
        ).append(partition)

    # Group static port buffers by memory region in configuration order.
    # Each port buffer is emitted as a separate NOLOAD section and aligned
    # to its configured size because the MPU requires an MPU region's base
    # address to be aligned to the region size.
    port_regions: dict[str, list] = {}

    for port_region in cfg.port_regions:
        port_regions.setdefault(
            port_region.region,
            [],
        ).append(port_region)

    # Select the memory region used for kernel .data, .bss, heap, and the
    # main stack. Flash is excluded, as are regions already dedicated to
    # partition stacks. The first remaining region is selected to keep
    # the configuration deterministic.
    stack_region_names = set(stack_regions.keys())

    kernel_candidates = [
        name
        for name in cfg.memory
        if name not in stack_region_names and name != "flash"
    ]

    if not kernel_candidates:
        raise ValueError(
            "no memory region available for kernel .data/.bss "
            "(all non-flash regions are used for partition stacks) -- "
            "add a dedicated region, e.g. 'dtcm'"
        )

    kernel_region = kernel_candidates[0]

    return template.render(
        config_path=config_path,
        memory=cfg.memory,
        stack_regions=stack_regions,
        port_regions=port_regions,
        kernel_region=kernel_region,
        min_heap_size=hex(min_heap_size),
        min_stack_size=hex(min_stack_size),
    )
