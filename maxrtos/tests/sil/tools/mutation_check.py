#!/usr/bin/env python3
"""Mutation smoke test for the SIL suite.

A test suite that cannot fail proves nothing. This script injects one
realistic defect at a time into the production kernel / architecture code,
rebuilds, runs the SIL suites, and checks that at least one of the suites
expected to catch that defect fails. The source tree is always restored.

Usage: mutation_check.py <build-dir> [--only <substring>]
The build dir must already be configured for maxrtos/tests.
"""

import argparse
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]  # .../maxrtos

# (name, file relative to maxrtos/, old text (must occur once), new text, suites expected to fail)
MUTATIONS = [
    ("timeouts never expire",
     "kernel/src/tick.c",
     "( void ) maxrtos_ipc_expire_timeouts( table, s_current_tick );",
     "",
     {"ipc_queue_port", "fault_containment"}),

    ("kernel accepts any buffer address from a process",
     "arch/cortex_m7/src/mpu_hw.c",
     "owns = ( end > address ) &&\n               ( address >= region.base_address ) &&\n               ( end <= ( region.base_address + region.size_bytes ) );",
     "owns = true;",
     {"ipc_queue_port"}),

    ("kernel accepts any port address from a process",
     "arch/cortex_m7/src/mpu_hw.c",
     "( port_region.base_address == address ) &&",
     "( true ) &&",
     {"ipc_queue_port"}),

    ("partition region grants no user access",
     "arch/cortex_m7/src/mpu_hw.c",
     "#define MAXRTOS_RASR_AP_READ_WRITE     ( UINT32_C( 3 ) )",
     "#define MAXRTOS_RASR_AP_READ_WRITE     ( UINT32_C( 1 ) )",
     {"spatial_partitioning", "ipc_queue_port", "fault_containment", "process_yield", "scheduler_startup"}),

    ("partition region is not reprogrammed on a partition switch",
     "arch/cortex_m7/src/mpu_hw.c",
     "if( ( uint32_t ) partition_id == s_active_partition_id )",
     "if( s_active_partition_id != MAXRTOS_INVALID_PARTITION_ID )",
     {"spatial_partitioning", "fault_containment"}),

    ("port regions are never enabled for members",
     "arch/cortex_m7/src/mpu_hw.c",
     "if( is_member )",
     "if( false )",
     {"spatial_partitioning", "ipc_queue_port"}),

    ("halt does not leave the faulting context",
     "arch/cortex_m7/src/fault_dispatch.c",
     "maxrtos_arch_idle_enter_discarding_current();",
     "",
     {"fault_containment"}),

    ("halt is not recorded in the partition table",
     "kernel/src/fault_recovery.c",
     "table->halted[ pcb->partition_id ] = true;",
     "",
     {"fault_containment"}),

    ("halted partition's slot keeps the previous partition running",
     "arch/cortex_m7/src/systick.c",
     "if( tick_status == MAXRTOS_ERR_PARTITION_HALTED )",
     "if( false )",
     {"fault_containment"}),

    ("restart does not reset the process context",
     "arch/cortex_m7/src/fault_dispatch.c",
     "status = maxrtos_arch_init_stack( faulting_pcb );",
     "status = MAXRTOS_OK;",
     {"fault_containment"}),

    ("a woken process goes to the waker's partition",
     "kernel/src/ipc_block.c",
     "pcb->partition_id,\n                id );",
     "0U,\n                id );",
     {"ipc_queue_port"}),

    ("blocked senders are never admitted",
     "kernel/src/queue_port.c",
     "if( maxrtos_ipc_wake_one(\n                    &port->waiting_senders,",
     "if( false && maxrtos_ipc_wake_one(\n                    &port->waiting_senders,",
     {"ipc_queue_port"}),

    ("a receiver is not handed the message directly",
     "kernel/src/queue_port.c",
     "( void ) memcpy(\n                    receiver_op.payload.queue_receive.out_message,\n                    message,\n                    message_size );",
     "",
     {"ipc_queue_port"}),

    ("scheduler start registers only the first process",
     "arch/cortex_m7/src/start_scheduler.c",
     "for( id = 0U; id < ( maxrtos_process_id_t ) MAXRTOS_MAX_PROCESSES; id++ )",
     "for( id = 0U; id < 1U; id++ )",
     {"scheduler_startup", "process_yield", "fault_containment"}),

    ("major frame off by one tick",
     "kernel/src/tick.c",
     "s_current_tick++;\n",
     "s_current_tick += ( s_current_tick == 4U ) ? 2U : 1U;\n",
     {"temporal_partitioning", "fault_containment", "process_yield"}),

    ("yield can select a process outside the caller's partition",
     "kernel/src/yield.c",
     "partition_id,\n            &next_id );",
     "( maxrtos_partition_id_t ) 1U,\n            &next_id );",
     {"process_yield", "temporal_partitioning"}),
]


def run(cmd, cwd=None):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)


def build(build_dir):
    result = run(["cmake", "--build", str(build_dir), "-j8"])
    return result.returncode == 0, result.stdout + result.stderr


def failed_suites(build_dir):
    """Names of SIL suites (ctest 'sil_*') that fail."""
    result = run(["ctest", "--test-dir", str(build_dir), "-L", "sil", "--timeout", "60"])
    failed = set()
    for line in result.stdout.splitlines():
        if "sil_" in line and ("Failed" in line or "Timeout" in line or "Exception" in line or "***" in line):
            for token in line.split():
                if token.startswith("sil_"):
                    failed.add(token[len("sil_"):])
    return failed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build_dir")
    parser.add_argument("--only", default="")
    args = parser.parse_args()
    build_dir = pathlib.Path(args.build_dir)

    ok, output = build(build_dir)
    if not ok:
        print(output)
        return 2

    baseline = failed_suites(build_dir)
    if baseline:
        print(f"baseline is not green, failing: {sorted(baseline)}")
        return 2

    survived = []
    selected = [m for m in MUTATIONS if args.only in m[0]]

    for name, rel, old, new, expected in selected:
        path = ROOT / rel
        original = path.read_text()

        if original.count(old) != 1:
            print(f"[ERROR   ] {name}: pattern occurs {original.count(old)}x in {rel}")
            survived.append(name)
            continue

        try:
            path.write_text(original.replace(old, new))
            built, output = build(build_dir)

            if not built:
                print(f"[BUILD   ] {name}: mutant does not compile\n{output[-400:]}")
                survived.append(name)
                continue

            caught = failed_suites(build_dir) & expected
        finally:
            path.write_text(original)

        if caught:
            print(f"[KILLED  ] {name}  <- {', '.join(sorted(caught))}")
        else:
            print(f"[SURVIVED] {name}  (expected one of {sorted(expected)})")
            survived.append(name)

    build(build_dir)  # leave the build matching the restored sources

    print(f"\n{len(selected) - len(survived)}/{len(selected)} mutants killed")
    return 1 if survived else 0


if __name__ == "__main__":
    sys.exit(main())
