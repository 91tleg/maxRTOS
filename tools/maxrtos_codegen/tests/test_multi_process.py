"""Tests for multi-process partitions with one MPU domain per partition."""

from __future__ import annotations

import copy

import pytest

from maxrtos_codegen.generators.c_config import generate_header, generate_source
from maxrtos_codegen.generators.linker import generate_linker_script
from maxrtos_codegen.schema import ConfigError, load_config


def _proc(name, size="512B", align="512B", prio=5):
    return {"name": name, "stack_size": size, "stack_alignment": align, "priority": prio}


def _base(partitions=None):
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
            {"id": 0, "name": "a", "mpu": {}, "processes": [_proc("a1")]},
            {"id": 1, "name": "b", "mpu": {}, "processes": [_proc("b1")]},
        ],
        "schedule": {"major_frame": [
            {"partition": 0, "duration_ticks": 2},
            {"partition": 1, "duration_ticks": 2},
        ]},
        "mpu": {"regions": [
            {"number": 3, "base": "0x24000000", "size": "512K", "access": "priv_read_write"},
        ]},
    }


def test_single_process_partition_domain_equals_stack():
    cfg = load_config(_base())
    assert cfg.partitions[0].domain_size == 512
    assert cfg.partitions[0].stack_offsets == [0]


def test_multiple_processes_share_one_pow2_domain():
    raw = _base([
        {"id": 0, "name": "a", "mpu": {}, "processes": [
            _proc("a_small"), _proc("a_big", "1K", "1K", 7), _proc("a_mid", "512B", "512B", 6),
        ]},
    ])
    raw["schedule"]["major_frame"] = [{"partition": 0, "duration_ticks": 1}]
    p = load_config(raw).partitions[0]
    # 1K + 512 + 512 = 2K exactly; biggest alignment placed first.
    assert p.domain_size == 2048
    assert p.stack_offsets[1] == 0
    assert sorted(p.stack_offsets) == [0, 1024, 1536]
    for pr, off in zip(p.processes, p.stack_offsets):
        assert off % pr.stack_alignment == 0
        assert off + pr.stack_size <= p.domain_size


def test_domain_rounds_up_to_power_of_two():
    raw = _base([
        {"id": 0, "name": "a", "mpu": {}, "processes": [_proc("x"), _proc("y"), _proc("z")]},
    ])
    raw["schedule"]["major_frame"] = [{"partition": 0, "duration_ticks": 1}]
    assert load_config(raw).partitions[0].domain_size == 2048  # 1536 -> 2048


def test_legacy_partition_keys_rejected():
    raw = _base()
    raw["partitions"][0]["stack_size"] = "512B"
    with pytest.raises(ConfigError, match=r"processes\[\]"):
        load_config(raw)


def test_partition_needs_processes():
    raw = _base()
    raw["partitions"][0]["processes"] = []
    with pytest.raises(ConfigError, match="non-empty"):
        load_config(raw)


@pytest.mark.parametrize("bad", [-1, 32, "x", True])
def test_bad_priority(bad):
    raw = _base()
    raw["partitions"][0]["processes"][0]["priority"] = bad
    with pytest.raises(ConfigError, match="priority"):
        load_config(raw)


def test_duplicate_process_name_across_partitions():
    raw = _base()
    raw["partitions"][1]["processes"][0]["name"] = "a1"
    with pytest.raises(ConfigError, match="duplicate process name"):
        load_config(raw)


def test_stack_size_must_be_multiple_of_alignment():
    raw = _base()
    raw["partitions"][0]["processes"][0]["stack_size"] = "768B"
    with pytest.raises(ConfigError, match="multiple"):
        load_config(raw)


def test_too_many_processes():
    procs = [_proc(f"p{i}", "32B", "32B") for i in range(17)]
    raw = _base([{"id": 0, "name": "a", "mpu": {}, "processes": procs}])
    raw["schedule"]["major_frame"] = [{"partition": 0, "duration_ticks": 1}]
    with pytest.raises(ConfigError, match="MAXRTOS_MAX_PROCESSES"):
        load_config(raw)


def test_domains_must_fit_region():
    raw = _base()
    raw["memory"]["sram"]["size"] = "512B"
    raw["mpu"]["regions"][0].update(size="512B")
    with pytest.raises(ConfigError, match="512 bytes|region is only"):
        load_config(raw)


def _gen(raw):
    cfg = load_config(raw)
    return (
        generate_header(cfg, "m.json"),
        generate_source(cfg, "m.json"),
        generate_linker_script(cfg, "m.json"),
    )


def test_generated_output_multi_process_multi_partition():
    raw = _base([
        {"id": 0, "name": "a", "mpu": {}, "processes": [_proc("a1"), _proc("a2", "1K", "1K", 6)]},
        {"id": 1, "name": "b", "mpu": {}, "processes": [_proc("b1")]},
    ])
    h, c, ld = _gen(raw)

    assert "maxrtos_config_start" not in h and "maxrtos_config_start" not in c
    assert "PARTITION_STACK_SIZE_" not in h
    for name in ("a1", "a2", "b1"):
        assert f"extern uint8_t maxrtos_stack_{name}[" in h
        assert f"PROCESS_PRIORITY_{name}" in h
    assert "PARTITION_DOMAIN_SIZE_a ( 2048U )" in h

    # One MPU region per partition, based on the domain, not on a stack.
    assert c.count("maxrtos_mpu_set_partition_region(") == 2
    assert "( uint32_t ) __partition_a_domain_start,\n        2048U," in c
    assert "( uint32_t ) __partition_b_domain_start,\n        512U," in c
    assert "maxrtos_frame_init(" in c  # frame schedule now built in config_init

    assert ld.count("(NOLOAD)") >= 2
    assert ".partition_a_domain" in ld and "ALIGN(2048)" in ld
    assert "*(.partition_a_a2_stack)" in ld
    assert ld.index("*(.partition_a_a2_stack)") < ld.index("*(.partition_a_a1_stack)")


def test_examples_still_load():
    import json, pathlib
    ex = pathlib.Path(__file__).parent.parent / "examples"
    for f in ("example_module.json", "example_module_with_ports.json"):
        load_config(json.loads((ex / f).read_text()))


# ---- health monitor -------------------------------------------------------

def test_health_monitor_defaults_to_restart_for_every_raised_fault():
    p = load_config(_base()).partitions[0]
    assert set(p.health) == {"memory_access", "bus_error", "illegal_instruction", "divide_by_zero"}
    assert set(p.health.values()) == {"restart_process"}


def test_health_monitor_default_and_partition_override_resolve():
    raw = _base()
    raw["health_monitor"] = {"default": {"bus_error": "halt_partition"}}
    raw["partitions"][1]["health_monitor"] = {"memory_access": "halt_partition"}
    cfg = load_config(raw)
    a, b = cfg.partitions
    assert a.health["bus_error"] == "halt_partition"
    assert a.health["memory_access"] == "restart_process"
    assert b.health["bus_error"] == "halt_partition"      # inherited default
    assert b.health["memory_access"] == "halt_partition"  # override


@pytest.mark.parametrize("where", ["default", "partition"])
def test_health_monitor_rejects_ignore(where):
    raw = _base()
    if where == "default":
        raw["health_monitor"] = {"default": {"memory_access": "ignore"}}
    else:
        raw["partitions"][0]["health_monitor"] = {"memory_access": "ignore"}
    with pytest.raises(ConfigError, match="ignore"):
        load_config(raw)


def test_health_monitor_rejects_unknown_fault_and_action():
    raw = _base()
    raw["health_monitor"] = {"default": {"deadline_exceeded": "restart_process"}}
    with pytest.raises(ConfigError, match="not raised"):
        load_config(raw)
    raw["health_monitor"] = {"default": {"memory_access": "explode"}}
    with pytest.raises(ConfigError, match="action must be one of"):
        load_config(raw)
    raw["health_monitor"] = {"bogus": {}}
    with pytest.raises(ConfigError, match="only 'default'"):
        load_config(raw)


def test_generated_source_installs_health_monitor_before_mpu_setup():
    raw = _base()
    raw["partitions"][0]["health_monitor"] = {"memory_access": "halt_partition"}
    _, c, _ = _gen(raw)

    # 4 faults x 2 partitions.
    assert c.count("maxrtos_hm_set_policy(") == 8
    assert ("PARTITION_ID_a,\n        MAXRTOS_FAULT_MEMORY_ACCESS,\n"
            "        MAXRTOS_HM_ACTION_HALT_PARTITION") in c
    assert ("PARTITION_ID_b,\n        MAXRTOS_FAULT_MEMORY_ACCESS,\n"
            "        MAXRTOS_HM_ACTION_RESTART_PROCESS") in c
    assert "maxrtos_arch_set_health_monitor( &s_health_monitor );" in c
    assert "maxrtos_arch_fault_handlers_init();" in c
    # The partition table must be registered first: the fault handlers read it.
    assert c.index("maxrtos_arch_set_partition_table") < c.index("maxrtos_arch_set_health_monitor")
