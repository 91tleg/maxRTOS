"""Kernel limits (MAXRTOS_MAX_*) derived from a configuration."""

from __future__ import annotations

from ..schema import Limits


def limit_definitions(limits: Limits) -> list[tuple[str, int]]:
    """(macro name, value) for every limit the configuration sets.

    A runtime-created object count (mutexes, semaphores, buffers) that the
    configuration did not give is left out, so the kernel default applies.
    The port counts are known from the model and are set when there are ports.
    """
    pairs = [
        ("MAXRTOS_MAX_PARTITIONS", limits.max_partitions),
        ("MAXRTOS_MAX_PROCESSES", limits.max_processes),
        ("MAXRTOS_MAX_PRIORITY", limits.max_priority),
        ("MAXRTOS_MAX_READY_PER_PRIORITY", limits.max_ready_per_priority),
        ("MAXRTOS_MAX_FRAME_SLOTS", limits.max_frame_slots),
        ("MAXRTOS_MAX_MUTEXES", limits.max_mutexes),
        ("MAXRTOS_MAX_SEMAPHORES", limits.max_semaphores),
        ("MAXRTOS_MAX_BUFFERS", limits.max_buffers),
        ("MAXRTOS_MAX_QUEUE_PORTS", limits.max_queue_ports),
        ("MAXRTOS_MAX_SAMPLING_PORTS", limits.max_sampling_ports),
    ]
    return [(name, value) for name, value in pairs if value is not None]


def limit_minimums(limits: Limits) -> list[tuple[str, int]]:
    """(macro name, least value the configuration needs).

    The generated configuration header checks the limits in effect against
    these, whether or not maxrtos_limits.h was used.
    """
    minimums = [
        ("MAXRTOS_MAX_PARTITIONS", limits.max_partitions),
        ("MAXRTOS_MAX_PROCESSES", limits.max_processes),
        ("MAXRTOS_MAX_PRIORITY", limits.max_priority),
        ("MAXRTOS_MAX_READY_PER_PRIORITY", limits.max_processes_per_partition),
        ("MAXRTOS_MAX_FRAME_SLOTS", limits.max_frame_slots),
    ]

    if limits.max_queue_ports is not None:
        minimums.append(("MAXRTOS_MAX_QUEUE_PORTS", limits.max_queue_ports))

    if limits.max_sampling_ports is not None:
        minimums.append(("MAXRTOS_MAX_SAMPLING_PORTS", limits.max_sampling_ports))

    return minimums
