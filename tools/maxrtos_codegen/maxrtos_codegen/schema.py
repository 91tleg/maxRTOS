"""
Parsing and validation for RTOS partition-config JSON.

This module only knows how to turn human-friendly strings ("512K", "0x08000000")
into integers and check the structural invariants of the config. It does not
know anything about C or linker-script syntax generation (see generators/).
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Any


class ConfigError(ValueError):
    """Raised for any structural or semantic problem in the input JSON."""


_SIZE_RE = re.compile(r"^\s*(\d+)\s*([KkMmGg]?)[Bb]?\s*$")
_UNIT_MULT = {"": 1, "k": 1024, "m": 1024 * 1024, "g": 1024 * 1024 * 1024}


def parse_size(value: Any) -> int:
    """Parse '512B', '512', '2M', '128K' -> integer bytes."""
    if isinstance(value, int):
        return value
    if not isinstance(value, str):
        raise ConfigError(f"expected size string or int, got {value!r}")
    m = _SIZE_RE.match(value)
    if not m:
        raise ConfigError(f"unparseable size: {value!r} (expected e.g. '512', '512B', '2M', '128K')")
    num, unit = m.groups()
    return int(num) * _UNIT_MULT[unit.lower()]


def parse_addr(value: Any) -> int:
    """Parse '0x08000000' or a plain int -> integer address."""
    if isinstance(value, int):
        return value
    if not isinstance(value, str):
        raise ConfigError(f"expected address string or int, got {value!r}")
    v = value.strip()
    try:
        return int(v, 16) if v.lower().startswith("0x") else int(v, 10)
    except ValueError as e:
        raise ConfigError(f"unparseable address: {value!r}") from e


def is_power_of_two(n: int) -> bool:
    return n > 0 and (n & (n - 1)) == 0


_C_IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def require_c_ident(name: str, context: str) -> str:
    if not _C_IDENT_RE.match(name):
        raise ConfigError(f"{context} {name!r} is not a valid C identifier")
    return name


ACCESS_VALUES = {"read_only", "read_write", "no_access", "priv_read_only", "priv_read_write"}

# Must match MAXRTOS_MPU_SYSTEM_REGION_COUNT in
# arch/cortex_m7/include/maxrtos/arch/cortex_m7/mpu.h: background/
# system regions occupy 0-7, region 8 is the reusable partition
# slot, and 9-15 are static port regions.
MPU_SYSTEM_REGION_COUNT = 8


@dataclass
class MemoryRegion:
    name: str
    origin: int
    size: int

    @property
    def end(self) -> int:
        return self.origin + self.size


@dataclass
class Process:
    """One process of a partition, with its own stack."""
    name: str
    stack_size: int
    stack_alignment: int
    priority: int
    # Release interval in ticks; 0 = aperiodic.
    period_ticks: int = 0
    # Deadline in ticks, from the start of each release; 0 = none.
    time_capacity_ticks: int = 0


@dataclass
class Partition:
    """A partition and its MPU memory domain.

    All processes of a partition share one MPU region (the domain) that
    covers every one of their stacks. The domain is a single naturally
    aligned power-of-two block, so stacks are packed into it and the
    domain is padded up to domain_size.
    """
    id: int
    name: str
    mpu_access: str
    mpu_executable: bool
    stack_region: str
    processes: list[Process]
    type: str
    domain_size: int
    domain_alignment: int
    # Byte offset of each process stack from the domain base, in the
    # same order as processes.
    stack_offsets: list[int]
    # Fully resolved health-monitor policy: every key of HM_FAULTS maps to
    # a key of HM_ACTIONS.
    health: dict[str, str] = field(default_factory=dict)


@dataclass
class FrameSlot:
    partition_id: int
    duration_ticks: int


@dataclass
class MpuRegion:
    number: int
    base: int
    size: int
    access: str
    executable: bool


@dataclass
class PortRegion:
    """A statically declared, MPU-protected shared-memory region for
    inter-partition IPC.

    The generator owns the queue/sampling initialization contract because it
    knows the configured object shape for each static port and because the
    port must be ready before any partition can send or receive.
    """
    name: str
    size: int
    message_size: int
    capacity: int
    region: str
    member_partition_ids: list[int]


MAX_PORT_REGIONS = 7

# Must match MAXRTOS_MAX_PROCESSES / MAXRTOS_MAX_PRIORITY in
# include/maxrtos/config.h.
MAX_PROCESSES = 16
MAX_PRIORITY = 31

MPU_MIN_REGION_SIZE = 32

# Fault classes the architecture layer actually raises, mapped to the C
# enumerator suffix in maxrtos/kernel/health_monitor.h
# (MAXRTOS_FAULT_<suffix>). UNEXPECTED_RETURN exists in the kernel enum but
# nothing raises it yet, so it is not configurable. DEADLINE_EXCEEDED is
# raised by the kernel's deadline supervision (process time_capacity_ticks).
HM_FAULTS = {
    "memory_access": "MEMORY_ACCESS",
    "bus_error": "BUS_ERROR",
    "illegal_instruction": "ILLEGAL_INSTRUCTION",
    "divide_by_zero": "DIVIDE_BY_ZERO",
    "deadline_exceeded": "DEADLINE_EXCEEDED",
}

# "ignore" is deliberately absent: for every raised fault it resumes the
# faulting instruction, which faults again forever.
HM_ACTIONS = {
    "restart_process": "RESTART_PROCESS",
    "halt_partition": "HALT_PARTITION",
}

HM_DEFAULT_ACTION = "restart_process"

# A partition's privilege is fixed by its type. Application partitions run
# unprivileged; only a system partition (platform software such as a driver
# partition) runs privileged, for all of its processes together, because they
# share one memory domain.
PARTITION_TYPES = ("application", "system")

_LEGACY_PARTITION_KEYS = ("stack_size", "stack_alignment", "priority", "entry_symbol")


def _next_pow2(n: int) -> int:
    p = 1
    while p < n:
        p <<= 1
    return p


def _domain_bytes(parts: list["Partition"]) -> int:
    """Bytes a set of domains occupy when placed in id order, each
    aligned to its own size (MPU rule), assuming a region-aligned origin."""
    cursor = 0
    for p in sorted(parts, key=lambda x: x.id):
        cursor = (cursor + p.domain_alignment - 1) // p.domain_alignment * p.domain_alignment
        cursor += p.domain_size
    return cursor


def _pack_domain(processes: list[Process]) -> tuple[int, int, list[int]]:
    """Pack process stacks into one MPU domain.

    Stacks are placed in descending-alignment order (ties keep JSON order)
    to minimise padding. Returns (domain_size, domain_alignment, offsets)
    where offsets are indexed like `processes`. domain_size is a power of
    two >= the MPU minimum, and the domain must be aligned to itself.
    """
    order = sorted(range(len(processes)), key=lambda i: -processes[i].stack_alignment)
    offsets = [0] * len(processes)
    cursor = 0
    for i in order:
        pr = processes[i]
        cursor = (cursor + pr.stack_alignment - 1) // pr.stack_alignment * pr.stack_alignment
        offsets[i] = cursor
        cursor += pr.stack_size
    domain_size = max(_next_pow2(cursor), MPU_MIN_REGION_SIZE)
    return domain_size, domain_size, offsets


@dataclass
class RtosConfig:
    module_name: str
    memory: dict[str, MemoryRegion]
    partitions: list[Partition]
    frame_schedule: list[FrameSlot]
    mpu_regions: list[MpuRegion]
    stack_region_name: str
    port_regions: list[PortRegion]
    # Value of MPU_CTRL.PRIVDEFENA. False (the default) denies every access
    # not permitted by a region, privileged included.
    privileged_default_map: bool = False


def _parse_health_policy(raw: object, ctx: str) -> dict[str, str]:
    """Validate one health_monitor object -> {fault: action} (may be partial)."""
    if not isinstance(raw, dict):
        raise ConfigError(f"{ctx} must be an object mapping fault names to actions")
    policy: dict[str, str] = {}
    for fault, action in raw.items():
        if fault not in HM_FAULTS:
            hint = ""
            if fault == "unexpected_return":
                hint = " (this fault class is not raised by the architecture layer yet)"
            raise ConfigError(
                f"{ctx}.{fault}: unknown fault; expected one of {sorted(HM_FAULTS)}{hint}"
            )
        if action == "ignore":
            raise ConfigError(
                f"{ctx}.{fault}: 'ignore' is not allowed - it resumes the faulting "
                f"instruction, which faults again forever. Use one of {sorted(HM_ACTIONS)}"
            )
        if action not in HM_ACTIONS:
            raise ConfigError(
                f"{ctx}.{fault}: action must be one of {sorted(HM_ACTIONS)}, got {action!r}"
            )
        policy[fault] = action
    return policy


def load_config(raw: dict) -> RtosConfig:
    try:
        module_name = raw["module"]["name"]
    except KeyError as e:
        raise ConfigError("missing required key: module.name") from e
    require_c_ident(module_name, "module.name")

    mem_raw = raw.get("memory", {})
    if not mem_raw:
        raise ConfigError("missing required key: memory (at least one region)")
    memory: dict[str, MemoryRegion] = {}
    for mem_name, m in mem_raw.items():
        try:
            origin = parse_addr(m["origin"])
            size = parse_size(m["size"])
        except KeyError as e:
            raise ConfigError(f"memory.{mem_name} missing 'origin' or 'size'") from e
        memory[mem_name] = MemoryRegion(name=mem_name, origin=origin, size=size)

    partitions_raw = raw.get("partitions", [])
    if not partitions_raw:
        raise ConfigError("at least one partition is required")

    hm_raw = raw.get("health_monitor", {})
    if not isinstance(hm_raw, dict):
        raise ConfigError("health_monitor must be an object")
    for key in hm_raw:
        if key != "default":
            raise ConfigError(
                f"health_monitor.{key}: unknown key; only 'default' is supported "
                f"(per-partition overrides go on partitions[].health_monitor)"
            )
    hm_default = {f: HM_DEFAULT_ACTION for f in HM_FAULTS}
    hm_default.update(_parse_health_policy(hm_raw.get("default", {}), "health_monitor.default"))

    seen_ids: set[int] = set()
    seen_names: set[str] = set()
    seen_process_names: set[str] = set()
    partitions: list[Partition] = []
    for i, p in enumerate(partitions_raw):
        ctx = f"partitions[{i}]"
        for req in ("id", "name", "mpu", "processes"):
            if req not in p:
                raise ConfigError(f"{ctx} missing required key '{req}'")
        for legacy in _LEGACY_PARTITION_KEYS:
            if legacy in p:
                raise ConfigError(
                    f"{ctx}.{legacy} is no longer supported on a partition: a "
                    f"partition owns several processes, each with its own stack. "
                    f"Move it into partitions[{i}].processes[] "
                    f"(name, stack_size, stack_alignment, priority)"
                )
        pid = p["id"]
        name = require_c_ident(p["name"], f"{ctx}.name")
        if pid in seen_ids:
            raise ConfigError(f"duplicate partition id {pid}")
        if name in seen_names:
            raise ConfigError(f"duplicate partition name {name!r}")
        seen_ids.add(pid)
        seen_names.add(name)

        ptype = p.get("type", "application")
        if ptype not in PARTITION_TYPES:
            raise ConfigError(
                f"{ctx}.type must be one of {PARTITION_TYPES}, got {ptype!r}"
            )

        mpu = p["mpu"]
        access = mpu.get("access", "read_write")
        if access not in ACCESS_VALUES:
            raise ConfigError(
                f"{ctx}.mpu.access must be one of {ACCESS_VALUES}, got {access!r}"
            )
        executable = bool(mpu.get("executable", False))

        stack_region = p.get("stack_region") or raw.get("stack_region")
        if not stack_region:
            raise ConfigError(
                f"{ctx}: no 'stack_region' set on the partition and no top-level "
                f"'stack_region' default - specify which memory region holds partition stacks"
            )
        if stack_region not in memory:
            raise ConfigError(
                f"{ctx}.stack_region {stack_region!r} not found in 'memory'"
            )

        health = dict(hm_default)
        health.update(
            _parse_health_policy(p.get("health_monitor", {}), f"{ctx}.health_monitor")
        )

        procs_raw = p["processes"]
        if not isinstance(procs_raw, list) or not procs_raw:
            raise ConfigError(f"{ctx}.processes must be a non-empty list")

        processes: list[Process] = []
        for j, pr in enumerate(procs_raw):
            pctx = f"{ctx}.processes[{j}]"
            for req in ("name", "stack_size", "stack_alignment", "priority"):
                if req not in pr:
                    raise ConfigError(f"{pctx} missing required key '{req}'")
            pname = require_c_ident(pr["name"], f"{pctx}.name")
            if pname in seen_process_names:
                raise ConfigError(f"duplicate process name {pname!r}")
            seen_process_names.add(pname)

            stack_size = parse_size(pr["stack_size"])
            stack_align = parse_size(pr["stack_alignment"])
            if not is_power_of_two(stack_align):
                raise ConfigError(
                    f"{pctx}.stack_alignment ({stack_align}) must be a power of two"
                )
            if stack_size <= 0 or stack_size % stack_align != 0:
                raise ConfigError(
                    f"{pctx}.stack_size ({stack_size}) must be a positive multiple of "
                    f"stack_alignment ({stack_align})"
                )

            prio = pr["priority"]
            if isinstance(prio, bool) or not isinstance(prio, int) or not (0 <= prio <= MAX_PRIORITY):
                raise ConfigError(
                    f"{pctx}.priority must be an integer in [0, {MAX_PRIORITY}], got {prio!r}"
                )

            period = pr.get("period_ticks", 0)
            capacity = pr.get("time_capacity_ticks", 0)
            for key, value in (("period_ticks", period), ("time_capacity_ticks", capacity)):
                if isinstance(value, bool) or not isinstance(value, int) or value < 0:
                    raise ConfigError(
                        f"{pctx}.{key} must be a non-negative integer number of ticks, "
                        f"got {value!r}"
                    )
            if period > 0 and capacity > period:
                raise ConfigError(
                    f"{pctx}.time_capacity_ticks ({capacity}) must not exceed "
                    f"period_ticks ({period}): a deadline later than the next release "
                    f"could never be met"
                )

            processes.append(
                Process(pname, stack_size, stack_align, prio, period, capacity)
            )

        domain_size, domain_align, offsets = _pack_domain(processes)

        partitions.append(
            Partition(
                id=pid,
                name=name,
                mpu_access=access,
                mpu_executable=executable,
                stack_region=stack_region,
                processes=processes,
                type=ptype,
                domain_size=domain_size,
                domain_alignment=domain_align,
                stack_offsets=offsets,
                health=health,
            )
        )

    total_processes = sum(len(p.processes) for p in partitions)
    if total_processes > MAX_PROCESSES:
        raise ConfigError(
            f"{total_processes} processes declared but MAXRTOS_MAX_PROCESSES is {MAX_PROCESSES}"
        )

    by_region: dict[str, list[Partition]] = {}
    for p in partitions:
        by_region.setdefault(p.stack_region, []).append(p)

    stack_region_name = partitions[0].stack_region
    for region_name, parts in by_region.items():
        need = _domain_bytes(parts)
        if need > memory[region_name].size:
            raise ConfigError(
                f"partition domains assigned to memory region {region_name!r} "
                f"require {need} bytes but the region is only "
                f"{memory[region_name].size} bytes"
            )

    schedule_raw = raw.get("schedule", {}).get("major_frame", [])
    if not schedule_raw:
        raise ConfigError(
            "missing required key: schedule.major_frame (at least one slot)"
        )

    frame_schedule: list[FrameSlot] = []
    for i, s in enumerate(schedule_raw):
        ctx = f"schedule.major_frame[{i}]"
        for req in ("partition", "duration_ticks"):
            if req not in s:
                raise ConfigError(f"{ctx} missing required key '{req}'")
        if s["partition"] not in seen_ids:
            raise ConfigError(
                f"{ctx}.partition={s['partition']} does not match any partition id"
            )
        if s["duration_ticks"] <= 0:
            raise ConfigError(f"{ctx}.duration_ticks must be positive")
        frame_schedule.append(
            FrameSlot(partition_id=s["partition"], duration_ticks=s["duration_ticks"])
        )

    mpu_regions_raw = raw.get("mpu", {}).get("regions", [])
    seen_region_numbers: set[int] = set()
    mpu_regions: list[MpuRegion] = []
    for i, r in enumerate(mpu_regions_raw):
        ctx = f"mpu.regions[{i}]"
        for req in ("number", "base", "size", "access"):
            if req not in r:
                raise ConfigError(f"{ctx} missing required key '{req}'")
        num = r["number"]
        if not (0 <= num < MPU_SYSTEM_REGION_COUNT):
            raise ConfigError(
                f"{ctx}.number ({num}) must be in [0, {MPU_SYSTEM_REGION_COUNT - 1}] - "
                f"region {MPU_SYSTEM_REGION_COUNT} is reserved by the kernel for the "
                f"currently-scheduled partition, and regions above it are unused"
            )
        if num in seen_region_numbers:
            raise ConfigError(f"{ctx}: duplicate MPU region number {num}")
        seen_region_numbers.add(num)
        access = r["access"]
        if access not in ACCESS_VALUES:
            raise ConfigError(f"{ctx}.access must be one of {ACCESS_VALUES}")
        base = parse_addr(r["base"])
        size = parse_size(r["size"])
        if not is_power_of_two(size):
            raise ConfigError(f"{ctx}.size ({size}) must be a power of two")

        if base % size != 0:
            raise ConfigError(
                f"{ctx}.base (0x{base:X}) must be aligned to its size ({size})"
            )

        mpu_regions.append(
            MpuRegion(
                number=num,
                base=base,
                size=size,
                access=access,
                executable=bool(r.get("executable", False)),
            )
        )

    # Partition stack regions must not silently collide with declared background MPU regions
    # that are marked no_access.
    for mr in mpu_regions:
        if mr.access != "no_access":
            continue

        for p in partitions:
            region = memory[p.stack_region]
            if mr.base < region.end and (mr.base + mr.size) > region.origin:
                raise ConfigError(
                    f"mpu region #{mr.number} (no_access) overlaps memory region "
                    f"{p.stack_region!r} used for partition stacks"
                )

    # Every memory region used as a partition stack_region must be fully covered by
    # at least one priv_read_write background region. maxrtos_arch_init_stack() runs
    # privileged (called from maxrtos_config_start() at boot, before any context
    # switch has happened, and again from fault_handlers.c's restart-process path)
    # and writes directly into that memory. Region 8 (the reusable partition-window
    # slot) only describes whichever partition is *currently scheduled* -  at boot,
    # and for any non-scheduled partition, it describes nothing. Without a covering
    # background region, that write has no permitting MPU region at all and faults
    # with DACCVIOL on real hardware (confirmed twice: once for kernel DTCM/MSP
    # stack, once for this exact SRAM/partition-stack case). This check exists so
    # that gap is caught here, at generation time, instead of as a runtime fault.
    stack_region_names_used = {p.stack_region for p in partitions}
    for region_name in stack_region_names_used:
        region = memory[region_name]
        covered = any(
            mr.access == "priv_read_write" and
            mr.base <= region.origin and
            (mr.base + mr.size) >= region.end
            for mr in mpu_regions
        )
        if not covered:
            raise ConfigError(
                f"memory region {region_name!r} is used as a partition stack_region "
                f"but is not fully covered by any 'priv_read_write' entry in "
                f"mpu.regions -- add one spanning at least "
                f"[0x{region.origin:X}, 0x{region.end:X}) or maxrtos_arch_init_stack() "
                f"will fault (DACCVIOL) writing partition stacks at boot"
            )

    ports_raw = raw.get("ports", [])
    if len(ports_raw) > MAX_PORT_REGIONS:
        raise ConfigError(
            f"{len(ports_raw)} ports declared but only {MAX_PORT_REGIONS} static "
            f"port MPU regions exist in hardware (16 regions - 8 background - "
            f"1 reusable partition slot -- see mpu.h)"
        )

    name_to_id = {p.name: p.id for p in partitions}
    seen_port_names: set[str] = set()
    port_regions: list[PortRegion] = []
    for i, prt in enumerate(ports_raw):
        ctx = f"ports[{i}]"
        for req in ("name", "size", "message_size", "capacity", "region", "members"):
            if req not in prt:
                raise ConfigError(f"{ctx} missing required key '{req}'")

        name = require_c_ident(prt["name"], f"{ctx}.name")
        if name in seen_port_names:
            raise ConfigError(f"duplicate port name {name!r}")
        seen_port_names.add(name)

        size = parse_size(prt["size"])
        if not is_power_of_two(size):
            raise ConfigError(f"{ctx}.size ({size}) must be a power of two -- MPU region size rule")
        if size < 32:
            raise ConfigError(f"{ctx}.size ({size}) must be at least 32 bytes -- ARMv7-M MPU minimum region size")

        message_size = parse_size(prt["message_size"])
        if message_size <= 0:
            raise ConfigError(f"{ctx}.message_size must be greater than zero")

        capacity = int(prt["capacity"])
        if capacity <= 0:
            raise ConfigError(f"{ctx}.capacity must be greater than zero")

        region_name = prt["region"]
        if region_name not in memory:
            raise ConfigError(f"{ctx}.region {region_name!r} not found in 'memory'")

        members_raw = prt["members"]
        if not isinstance(members_raw, list) or len(members_raw) == 0:
            raise ConfigError(f"{ctx}.members must be a non-empty list of partition names")
        member_ids: list[int] = []
        for m in members_raw:
            if m not in name_to_id:
                raise ConfigError(f"{ctx}.members references unknown partition {m!r}")
            member_ids.append(name_to_id[m])
        if len(set(member_ids)) != len(member_ids):
            raise ConfigError(f"{ctx}.members lists the same partition more than once")

        port_regions.append(PortRegion(
            name=name,
            size=size,
            message_size=message_size,
            capacity=capacity,
            region=region_name,
            member_partition_ids=sorted(member_ids),
        ))

    # Combined capacity check: partition stacks and port buffers sharing
    # a memory region are both linker-packed sequentially into it, so
    # they compete for the same space. Each is padded to its own
    # required alignment the same way the linker will place it -- ports
    # must additionally be aligned to their own size, since the MPU
    # requires a region's base address be a multiple of its size.
    port_regions_by_memory: dict[str, list[PortRegion]] = {}
    for prt in port_regions:
        port_regions_by_memory.setdefault(prt.region, []).append(prt)

    all_region_names = set(by_region.keys()) | set(port_regions_by_memory.keys())
    for region_name in all_region_names:
        region = memory[region_name]

        stack_bytes = _domain_bytes(by_region.get(region_name, []))

        port_bytes = 0
        for prt in port_regions_by_memory.get(region_name, []):
            port_bytes = (port_bytes + prt.size - 1) // prt.size * prt.size
            port_bytes += prt.size

        total = stack_bytes + port_bytes
        if total > region.size:
            raise ConfigError(
                f"memory region {region_name!r} needs {total} bytes for partition "
                f"domains ({stack_bytes}) and port buffers ({port_bytes}) together "
                f"but is only {region.size} bytes"
            )

    mpu_section = raw.get("mpu", {})
    privdefena = mpu_section.get("privileged_default_map", False)
    if not isinstance(privdefena, bool):
        raise ConfigError("mpu.privileged_default_map must be true or false")

    if not privdefena:
        # Deny-by-default: privileged kernel code (handlers, the scheduler,
        # the MPU driver) then needs regions too. Check the ones we can know.
        flash = memory.get("flash")
        if flash is not None and not any(
            mr.access in ("read_only", "priv_read_only", "read_write", "priv_read_write")
            and mr.base <= flash.origin
            and (mr.base + mr.size) >= flash.end
            for mr in mpu_regions
        ):
            raise ConfigError(
                f"mpu.privileged_default_map is false but memory region 'flash' "
                f"[0x{flash.origin:X}, 0x{flash.end:X}) is not covered by a readable "
                f"entry in mpu.regions: the kernel could not fetch its own code"
            )
        for name, region in memory.items():
            if name == "flash":
                continue
            if not any(
                mr.access in ("priv_read_write", "read_write")
                and mr.base <= region.origin
                and (mr.base + mr.size) >= region.end
                for mr in mpu_regions
            ):
                raise ConfigError(
                    f"mpu.privileged_default_map is false but memory region {name!r} "
                    f"[0x{region.origin:X}, 0x{region.end:X}) is not fully covered by a "
                    f"'priv_read_write' (or 'read_write') entry in mpu.regions: kernel "
                    f"data, stacks and IPC objects placed there would fault"
                )

    return RtosConfig(
        module_name=module_name,
        memory=memory,
        partitions=sorted(partitions, key=lambda x: x.id),
        frame_schedule=frame_schedule,
        mpu_regions=mpu_regions,
        stack_region_name=stack_region_name,
        port_regions=port_regions,
        privileged_default_map=privdefena,
    )
