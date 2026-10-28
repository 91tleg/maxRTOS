"""Tests for the kernel limits derived from a configuration."""

from __future__ import annotations

import json

import pytest

from maxrtos_codegen.cli import main
from maxrtos_codegen.generators.c_config import generate_header, generate_limits_header
from maxrtos_codegen.generators.limits import limit_definitions
from maxrtos_codegen.schema import ConfigError, load_config


def _proc(name, prio=5):
    return {"name": name, "stack_size": "512B", "stack_alignment": "512B", "priority": prio}


def _base(partitions=None, schedule=None):
    return {
        "module": {"name": "m"},
        "memory": {
            "flash": {"origin": "0x08000000", "size": "2M"},
            "dtcm": {"origin": "0x20000000", "size": "128K"},
            "sram": {"origin": "0x24000000", "size": "512K"},
        },
        "stack_region": "sram",
        "partitions": partitions
        or [
            {"id": 0, "name": "a", "mpu": {}, "processes": [_proc("a1", 2), _proc("a2", 4)]},
            {"id": 1, "name": "b", "mpu": {}, "processes": [_proc("b1", 7)]},
        ],
        "schedule": schedule
        or {"major_frame": [
            {"partition": 0, "duration_ticks": 2},
            {"partition": 1, "duration_ticks": 2},
        ]},
        "mpu": {"regions": [
            {"number": 0, "base": "0x08000000", "size": "2M", "access": "read_only", "executable": True},
            {"number": 2, "base": "0x20000000", "size": "128K", "access": "priv_read_write"},
            {"number": 3, "base": "0x24000000", "size": "512K", "access": "priv_read_write"},
        ]},
    }


def test_limits_are_derived_from_the_model():
    limits = load_config(_base()).limits
    assert limits.max_partitions == 2
    assert limits.max_processes == 3
    assert limits.max_priority == 7  # the highest declared
    assert limits.max_ready_per_priority == 2  # most processes in one partition
    assert limits.max_frame_slots == 2
    assert limits.max_mutexes is None  # not countable from the model


def test_partition_count_follows_the_highest_id():
    raw = _base([
        {"id": 0, "name": "a", "mpu": {}, "processes": [_proc("a1")]},
        {"id": 3, "name": "b", "mpu": {}, "processes": [_proc("b1")]},
    ], {"major_frame": [{"partition": 0, "duration_ticks": 1}, {"partition": 3, "duration_ticks": 1}]})
    assert load_config(raw).limits.max_partitions == 4


@pytest.mark.parametrize("count,depth", [(1, 1), (2, 2), (3, 4), (5, 8)])
def test_ready_queue_depth_is_a_power_of_two(count, depth):
    raw = _base([
        {"id": 0, "name": "a", "mpu": {}, "processes": [_proc(f"p{i}") for i in range(count)]},
    ], {"major_frame": [{"partition": 0, "duration_ticks": 1}]})
    assert load_config(raw).limits.max_ready_per_priority == depth


def test_runtime_object_limits_are_taken_from_the_model():
    raw = _base()
    raw["limits"] = {"mutexes": 1, "semaphores": 3, "buffers": 8}
    limits = load_config(raw).limits
    assert (limits.max_mutexes, limits.max_semaphores, limits.max_buffers) == (1, 3, 8)


def test_max_priority_can_be_raised_but_not_lowered():
    raw = _base()
    raw["limits"] = {"max_priority": 15}
    assert load_config(raw).limits.max_priority == 15

    raw["limits"] = {"max_priority": 3}  # a process is declared with 7
    with pytest.raises(ConfigError, match="max_priority"):
        load_config(raw)


@pytest.mark.parametrize("limits", [
    {"mutexes": 0},
    {"mutexes": 9},
    {"semaphores": True},
    {"buffers": "2"},
    {"max_priority": 32},
    {"colour": 1},
])
def test_bad_limits_are_rejected(limits):
    raw = _base()
    raw["limits"] = limits
    with pytest.raises(ConfigError, match="limits"):
        load_config(raw)


def test_limits_must_be_an_object():
    raw = _base()
    raw["limits"] = [1]
    with pytest.raises(ConfigError, match="limits must be an object"):
        load_config(raw)


def test_partition_id_beyond_the_kernel_maximum_is_rejected():
    raw = _base([
        {"id": 8, "name": "a", "mpu": {}, "processes": [_proc("a1")]},
    ], {"major_frame": [{"partition": 8, "duration_ticks": 1}]})
    with pytest.raises(ConfigError, match="MAXRTOS_MAX_PARTITIONS"):
        load_config(raw)


def test_too_many_frame_slots_are_rejected():
    raw = _base(schedule={"major_frame": [{"partition": 0, "duration_ticks": 1}] * 17})
    with pytest.raises(ConfigError, match="MAXRTOS_MAX_FRAME_SLOTS"):
        load_config(raw)


def test_definitions_omit_limits_the_model_does_not_set():
    names = [n for n, _ in limit_definitions(load_config(_base()).limits)]
    assert names == [
        "MAXRTOS_MAX_PARTITIONS",
        "MAXRTOS_MAX_PROCESSES",
        "MAXRTOS_MAX_PRIORITY",
        "MAXRTOS_MAX_READY_PER_PRIORITY",
        "MAXRTOS_MAX_FRAME_SLOTS",
    ]

    raw = _base()
    raw["limits"] = {"mutexes": 2}
    names = [n for n, _ in limit_definitions(load_config(raw).limits)]
    assert names[-1] == "MAXRTOS_MAX_MUTEXES"


def test_header_checks_the_limits_in_effect_against_the_model():
    header = generate_header(load_config(_base()), "m.json")
    assert "_Static_assert( MAXRTOS_MAX_PARTITIONS >= 2U," in header
    assert "_Static_assert( MAXRTOS_MAX_PROCESSES >= 3U," in header
    assert "_Static_assert( MAXRTOS_MAX_PRIORITY >= 7U," in header
    # The queue must hold the most processes of one partition (2), not the
    # power of two the generated limit rounds it up to.
    assert "_Static_assert( MAXRTOS_MAX_READY_PER_PRIORITY >= 2U," in header
    assert "_Static_assert( MAXRTOS_MAX_FRAME_SLOTS >= 2U," in header
    assert "MAXRTOS_MAX_MUTEXES" not in header


def test_limits_header_defines_each_limit():
    cfg = load_config(_base())
    header = generate_limits_header(cfg, "m.json")
    assert "#define MAXRTOS_MAX_PARTITIONS ( 2U )" in header
    assert "#define MAXRTOS_MAX_PROCESSES ( 3U )" in header
    assert "#define MAXRTOS_MAX_PRIORITY ( 7U )" in header
    assert "#define MAXRTOS_MAX_READY_PER_PRIORITY ( 2U )" in header
    assert "#define MAXRTOS_MAX_FRAME_SLOTS ( 2U )" in header
    assert "MAXRTOS_MAX_MUTEXES" not in header
    assert header.count("#ifndef M_RTOS_LIMITS_H") == 1


def test_limits_header_is_written_only_when_asked(tmp_path):
    config = tmp_path / "m.json"
    config.write_text(json.dumps(_base()))

    out = tmp_path / "plain"
    assert main([str(config), "--out-dir", str(out)]) == 0
    assert not (out / "maxrtos_limits.h").exists()

    out = tmp_path / "limits"
    assert main([str(config), "--out-dir", str(out), "--emit-limits"]) == 0
    assert "MAXRTOS_MAX_PARTITIONS" in (out / "maxrtos_limits.h").read_text()
