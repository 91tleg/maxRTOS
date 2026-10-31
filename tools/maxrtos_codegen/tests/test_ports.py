"""Tests for queuing ports: sender and receiver sets, kernel-private storage."""

from __future__ import annotations

import copy

import pytest

from maxrtos_codegen.generators.c_config import (
    generate_header,
    generate_limits_header,
    generate_source,
)
from maxrtos_codegen.generators.linker import generate_linker_script
from maxrtos_codegen.schema import ConfigError, load_config


def _proc(name):
    return {"name": name, "stack_size": "512B", "stack_alignment": "512B", "priority": 5}


def _base():
    return {
        "module": {"name": "m"},
        "memory": {
            "flash": {"origin": "0x08000000", "size": "2M"},
            "dtcm": {"origin": "0x20000000", "size": "128K"},
            "sram": {"origin": "0x24000000", "size": "512K"},
        },
        "stack_region": "sram",
        "partitions": [
            {"id": 0, "name": "a", "mpu": {}, "processes": [_proc("a1")]},
            {"id": 1, "name": "b", "mpu": {}, "processes": [_proc("b1")]},
            {"id": 2, "name": "c", "mpu": {}, "processes": [_proc("c1")]},
        ],
        "schedule": {"major_frame": [
            {"partition": 0, "duration_ticks": 2},
            {"partition": 1, "duration_ticks": 2},
            {"partition": 2, "duration_ticks": 2},
        ]},
        "mpu": {"regions": [
            {"number": 0, "base": "0x08000000", "size": "2M", "access": "read_only", "executable": True},
            {"number": 2, "base": "0x20000000", "size": "128K", "access": "priv_read_write"},
            {"number": 3, "base": "0x24000000", "size": "512K", "access": "priv_read_write"},
        ]},
        "ports": [
            {"name": "cmd", "message_size": "4B", "capacity": 4,
             "senders": ["a"], "receivers": ["b", "c"]},
            {"name": "evt", "message_size": "16B", "capacity": 2,
             "senders": ["b", "c"], "receivers": ["a"]},
        ],
    }


def test_ports_keep_declaration_order_as_their_ids():
    cfg = load_config(_base())
    assert [p.name for p in cfg.ports] == ["cmd", "evt"]
    assert cfg.ports[0].sender_ids == [0]
    assert cfg.ports[0].receiver_ids == [1, 2]
    assert cfg.ports[1].sender_ids == [1, 2]
    assert cfg.ports[1].receiver_ids == [0]


def test_header_names_ports_by_id_and_exports_no_storage():
    h = generate_header(load_config(_base()), "m.json")
    assert "#define PORT_ID_cmd ( ( maxrtos_queue_port_id_t ) 0U )" in h
    assert "#define PORT_ID_evt ( ( maxrtos_queue_port_id_t ) 1U )" in h
    assert "PORT_MESSAGE_SIZE_evt ( 16U )" in h
    # The storage is the kernel's: nothing for an application to reach.
    assert "extern uint8_t maxrtos_port_" not in h
    assert "PORT_SIZE_" not in h


def test_source_creates_ports_with_their_own_sender_and_receiver_sets():
    c = generate_source(load_config(_base()), "m.json")
    assert "maxrtos_queue_port_pool_init();" in c
    # Exact-size storage, in the kernel's .bss and not in a linker section.
    assert "static uint8_t s_port_storage_cmd[ 16 ]" in c
    assert "static uint8_t s_port_storage_evt[ 32 ]" in c
    assert "section( \".port_" not in c
    assert "(1UL << 0U),\n            (1UL << 1U) | (1UL << 2U)," in c
    assert "(1UL << 1U) | (1UL << 2U),\n            (1UL << 0U)," in c
    # No MPU region is declared for a port.
    assert "add_port_region" not in c


def test_linker_script_places_no_port_sections():
    ld = generate_linker_script(load_config(_base()), "m.json")
    assert ".port_" not in ld


def test_limits_size_the_kernel_port_pool_to_the_model():
    cfg = load_config(_base())
    assert cfg.limits.max_queue_ports == 2
    assert "#define MAXRTOS_MAX_QUEUE_PORTS ( 2U )" in generate_limits_header(cfg, "m.json")


def test_no_ports_leaves_the_pool_at_the_kernel_default():
    raw = _base()
    del raw["ports"]
    cfg = load_config(raw)
    assert cfg.limits.max_queue_ports is None
    assert "MAX_QUEUE_PORTS" not in generate_limits_header(cfg, "m.json")


@pytest.mark.parametrize("key", ["size", "region", "members"])
def test_the_old_memory_region_keys_are_refused(key):
    raw = _base()
    raw["ports"][0][key] = "sram" if key == "region" else ["a"] if key == "members" else "1024B"
    with pytest.raises(ConfigError, match=f"ports\\[0\\]\\.{key} is no longer supported"):
        load_config(raw)


def test_unknown_port_key_is_refused():
    raw = _base()
    raw["ports"][0]["colour"] = "red"
    with pytest.raises(ConfigError, match="unknown key"):
        load_config(raw)


@pytest.mark.parametrize("key", ["senders", "receivers"])
def test_each_end_needs_at_least_one_known_partition(key):
    raw = _base()
    raw["ports"][0][key] = []
    with pytest.raises(ConfigError, match=f"{key} must be a non-empty list"):
        load_config(raw)

    raw = _base()
    raw["ports"][0][key] = ["nobody"]
    with pytest.raises(ConfigError, match="unknown partition"):
        load_config(raw)

    raw = _base()
    raw["ports"][0][key] = ["a", "a"]
    with pytest.raises(ConfigError, match="more than once"):
        load_config(raw)


def test_a_partition_may_be_both_sender_and_receiver():
    raw = _base()
    raw["ports"][0]["senders"] = ["a", "b"]
    raw["ports"][0]["receivers"] = ["b"]
    cfg = load_config(raw)
    assert cfg.ports[0].sender_ids == [0, 1]
    assert cfg.ports[0].receiver_ids == [1]


def test_too_many_ports_for_the_kernel_pool_are_refused():
    raw = _base()
    raw["ports"] = [
        {"name": f"p{i}", "message_size": "4B", "capacity": 1,
         "senders": ["a"], "receivers": ["b"]}
        for i in range(9)
    ]
    with pytest.raises(ConfigError, match="MAXRTOS_MAX_QUEUE_PORTS"):
        load_config(raw)


def test_a_duplicate_port_name_is_refused():
    raw = _base()
    raw["ports"][1]["name"] = "cmd"
    with pytest.raises(ConfigError, match="duplicate port name"):
        load_config(raw)


@pytest.mark.parametrize("access", ["read_write", "read_only"])
def test_an_unprivileged_region_over_kernel_memory_is_refused(access):
    # dtcm holds the kernel's data, ports included. Let partitions at it and
    # they could corrupt the port state.
    raw = _base()
    raw["mpu"]["regions"].append(
        {"number": 4, "base": "0x20000000", "size": "4K", "access": access}
    )
    with pytest.raises(ConfigError, match="holds the kernel's data"):
        load_config(raw)


def test_an_unprivileged_region_elsewhere_is_fine():
    raw = copy.deepcopy(_base())
    raw["mpu"]["regions"].append(
        {"number": 4, "base": "0x40000000", "size": "1M", "access": "read_write"}
    )
    load_config(raw)


# ---- sampling ports --------------------------------------------------------


def _with_sampling():
    raw = _base()
    raw["sampling_ports"] = [
        {"name": "alt", "message_size": "8B", "writers": ["a"], "readers": ["b", "c"]},
        {"name": "mode", "message_size": "1B", "writers": ["b", "c"], "readers": ["a"]},
    ]
    return raw


def test_sampling_ports_keep_declaration_order_as_their_ids():
    cfg = load_config(_with_sampling())
    assert [p.name for p in cfg.sampling_ports] == ["alt", "mode"]
    assert cfg.sampling_ports[0].writer_ids == [0]
    assert cfg.sampling_ports[0].reader_ids == [1, 2]
    assert cfg.sampling_ports[1].writer_ids == [1, 2]
    assert cfg.sampling_ports[1].reader_ids == [0]


def test_sampling_header_names_ports_by_id_and_exports_no_storage():
    h = generate_header(load_config(_with_sampling()), "m.json")
    assert "#define SAMPLING_PORT_ID_alt ( ( maxrtos_sampling_port_id_t ) 0U )" in h
    assert "#define SAMPLING_PORT_ID_mode ( ( maxrtos_sampling_port_id_t ) 1U )" in h
    assert "SAMPLING_PORT_MESSAGE_SIZE_alt ( 8U )" in h
    # The storage is the kernel's: nothing for an application to reach.
    assert "sampling_storage" not in h


def test_sampling_source_creates_ports_with_their_own_writer_and_reader_sets():
    c = generate_source(load_config(_with_sampling()), "m.json")
    assert "maxrtos_sampling_port_pool_init();" in c
    assert "static uint8_t s_sampling_storage_alt[ 8 ]" in c
    assert "static uint8_t s_sampling_storage_mode[ 1 ]" in c
    assert "(1UL << 0U),\n            (1UL << 1U) | (1UL << 2U)," in c
    assert "(1UL << 1U) | (1UL << 2U),\n            (1UL << 0U)," in c


def test_limits_size_the_sampling_pool_to_the_model():
    cfg = load_config(_with_sampling())
    assert cfg.limits.max_sampling_ports == 2
    assert "#define MAXRTOS_MAX_SAMPLING_PORTS ( 2U )" in generate_limits_header(cfg, "m.json")
    raw = _base()
    assert load_config(raw).limits.max_sampling_ports is None


@pytest.mark.parametrize("key", ["writers", "readers"])
def test_each_sampling_end_needs_at_least_one_known_partition(key):
    raw = _with_sampling()
    raw["sampling_ports"][0][key] = []
    with pytest.raises(ConfigError, match=f"{key} must be a non-empty list"):
        load_config(raw)

    raw = _with_sampling()
    raw["sampling_ports"][0][key] = ["nobody"]
    with pytest.raises(ConfigError, match="unknown partition"):
        load_config(raw)

    raw = _with_sampling()
    raw["sampling_ports"][0][key] = ["a", "a"]
    with pytest.raises(ConfigError, match="more than once"):
        load_config(raw)


def test_a_sampling_port_takes_no_queue_keys():
    for key, value in (("capacity", 4), ("senders", ["a"]), ("size", "1K")):
        raw = _with_sampling()
        raw["sampling_ports"][0][key] = value
        with pytest.raises(ConfigError, match="unknown key"):
            load_config(raw)


def test_too_many_sampling_ports_are_refused():
    raw = _base()
    raw["sampling_ports"] = [
        {"name": f"s{i}", "message_size": "4B", "writers": ["a"], "readers": ["b"]}
        for i in range(9)
    ]
    with pytest.raises(ConfigError, match="MAXRTOS_MAX_SAMPLING_PORTS"):
        load_config(raw)


def test_a_duplicate_sampling_name_is_refused():
    raw = _with_sampling()
    raw["sampling_ports"][1]["name"] = "alt"
    with pytest.raises(ConfigError, match="duplicate sampling port name"):
        load_config(raw)
