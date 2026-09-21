from __future__ import annotations

import pathlib
from types import SimpleNamespace

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from ..schema import HM_ACTIONS, HM_FAULTS, RtosConfig


_TEMPLATE_DIR = pathlib.Path(__file__).parent.parent / "templates"

_ACCESS_ENUM_SUFFIX = {
    "read_only": "READ_ONLY",
    "read_write": "READ_WRITE",
    "no_access": "NONE",
    "priv_read_only": "PRIV_READ_ONLY",
    "priv_read_write": "PRIV_READ_WRITE",
}


def _env() -> Environment:
    return Environment(
        loader=FileSystemLoader(str(_TEMPLATE_DIR)),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )


def _stack_regions(cfg: RtosConfig) -> dict[str, list]:
    grouped: dict[str, list] = {}

    for partition in cfg.partitions:
        grouped.setdefault(partition.stack_region, []).append(partition)

    return grouped


def _with_access_enum(obj):
    """
    Return a shallow copy of obj with .access_enum set to the correct
    MAXRTOS_MPU_ACCESS_* suffix for its .access/.mpu_access string.
    """
    access_str = (
        getattr(obj, "access", None)
        or getattr(obj, "mpu_access", None)
    )

    return SimpleNamespace(
        **vars(obj),
        access_enum=_ACCESS_ENUM_SUFFIX[access_str],
    )


def _partitions_with_access_enum(
    cfg: RtosConfig,
) -> list[SimpleNamespace]:
    result = []

    for partition in cfg.partitions:
        result.append(
            SimpleNamespace(
                id=partition.id,
                name=partition.name,
                domain_size=partition.domain_size,
                mpu_executable=partition.mpu_executable,
                stack_region=partition.stack_region,
                processes=partition.processes,
                health=[
                    SimpleNamespace(
                        fault=HM_FAULTS[fault],
                        action=HM_ACTIONS[action],
                    )
                    for fault, action in (
                        (f, partition.health[f]) for f in HM_FAULTS
                    )
                ],
                access_enum=_ACCESS_ENUM_SUFFIX[partition.mpu_access],
            )
        )

    return result


def _mpu_regions_with_access_enum(
    cfg: RtosConfig,
) -> list[SimpleNamespace]:
    result = []

    for region in cfg.mpu_regions:
        result.append(
            SimpleNamespace(
                number=region.number,
                base=region.base,
                size=region.size,
                executable=region.executable,
                access_enum=_ACCESS_ENUM_SUFFIX[region.access],
            )
        )

    return result


def _port_regions_with_mask_expr(
    cfg: RtosConfig,
) -> list[SimpleNamespace]:
    """
    Attach a precomputed C bitmask expression to each port.

    For example, partitions {0, 2} becomes:
    "(1UL << 0U) | (1UL << 2U)".

    Computed here in Python rather than in the template because Jinja2's
    built-in filters do not include a regex/format-map operation suited
    to this.
    """
    result = []

    for port_region in cfg.port_regions:
        mask_expr = " | ".join(
            f"(1UL << {partition_id}U)"
            for partition_id in port_region.member_partition_ids
        )

        result.append(
            SimpleNamespace(
                name=port_region.name,
                size=port_region.size,
                message_size=port_region.message_size,
                capacity=port_region.capacity,
                region=port_region.region,
                member_partition_ids=port_region.member_partition_ids,
                mask_expr=mask_expr,
            )
        )

    return result


def generate_header(cfg: RtosConfig, config_path: str) -> str:
    template = _env().get_template("maxrtos_config.h.j2")

    return template.render(
        config_path=config_path,
        module_name=cfg.module_name,
        partitions=cfg.partitions,
        port_regions=_port_regions_with_mask_expr(cfg),
    )


def generate_source(cfg: RtosConfig, config_path: str) -> str:
    template = _env().get_template("maxrtos_config.c.j2")

    return template.render(
        config_path=config_path,
        module_name=cfg.module_name,
        partitions=_partitions_with_access_enum(cfg),
        stack_regions=_stack_regions(cfg),
        mpu_regions=_mpu_regions_with_access_enum(cfg),
        frame_schedule=cfg.frame_schedule,
        port_regions=_port_regions_with_mask_expr(cfg),
    )
