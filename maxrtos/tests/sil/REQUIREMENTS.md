# SIL requirements

The behaviours the SIL suites verify. Each row is verified by at least one
test case tagged with its ID (`SIL_CASE(..., "REQ-XXX-nnn", ...)`);
`tools/traceability.py` fails the build if a requirement here has no passing
test, or a test cites an ID that is not listed.

The `SIM-` rows qualify the virtual target itself: a SIL result is only as
trustworthy as the simulator beneath it.

## Start-up

| ID | Requirement |
|---|---|
| REQ-BOOT-001 | Every READY process created before start is registered with its partition and gets to run. |
| REQ-BOOT-002 | Processes that are not READY (suspended) are never run. |
| REQ-BOOT-003 | The major frame starts at tick zero with the first slot's partition, and that partition's memory is mapped before any of its code executes. |
| REQ-BOOT-004 | Starting with no runnable process stops safely and reports why. |

## Temporal partitioning

| ID | Requirement |
|---|---|
| REQ-TP-001 | Each partition executes for exactly its configured number of ticks per major frame. |
| REQ-TP-002 | The partition sequence is identical in every major frame: no jitter, no drift. |
| REQ-TP-003 | A partition that never yields cannot extend its slot or delay another partition. |

## Scheduling

| ID | Requirement |
|---|---|
| REQ-YLD-001 | Yield hands the CPU to another READY process of the same partition; equal-priority peers alternate and neither starves. |
| REQ-YLD-002 | Yield never crosses a partition boundary. |
| REQ-YLD-003 | A lone process can yield indefinitely without stalling. |

## Inter-partition communication (queuing ports)

| ID | Requirement |
|---|---|
| REQ-IPC-001 | A blocked receive returns exactly the message sent, whether the sender is in the same partition or another. |
| REQ-IPC-002 | A timed wait expires no earlier than its timeout, returns MAXRTOS_ERR_TIMEOUT, and leaves the port consistent. |
| REQ-IPC-003 | A stream through a bounded queue between partitions loses, duplicates and reorders nothing, with both sides blocking. |
| REQ-IPC-004 | A full queue rejects a non-blocking send; a blocked sender is admitted, in order, when a slot frees. |
| REQ-IPC-005 | The kernel never dereferences a pointer the caller could not access itself: kernel memory, another partition's memory, non-member ports and malformed ranges are rejected, and nothing is read, written or disturbed. |
| REQ-IPC-006 | A message whose size differs from the port's is rejected. |

## Spatial partitioning

| ID | Requirement |
|---|---|
| REQ-SP-001 | A process can use its own partition's memory. |
| REQ-SP-002 | A process cannot read or write another partition's memory. |
| REQ-SP-003 | A process cannot read or write kernel memory. |
| REQ-SP-004 | An IPC port's memory is accessible only to member partitions. |
| REQ-SP-005 | The MPU maps exactly the running partition's memory and the ports it is a member of, and follows every partition switch. |
| REQ-SP-006 | Processes of one partition share that partition's memory (partition-level isolation, by design). |
| REQ-SP-007 | Flash (code and constants) is read-only for every partition. |

## Periodic processes and deadlines

| ID | Requirement |
|---|---|
| REQ-DL-001 | A periodic process is released exactly every period, with no jitter or drift. |
| REQ-DL-002 | A process that completes each release within its time capacity is never reported as missing. |
| REQ-DL-003 | A process that overruns its time capacity is detected at its deadline and handled by its partition's policy: restarted and re-released, without disturbing anything else. |
| REQ-DL-004 | The deadline is wall-clock time, not a CPU budget: a process whose partition is not scheduled before its deadline misses it having used no CPU. |
| REQ-DL-005 | A miss under halt_partition stops that partition and only that partition. |
| REQ-DL-006 | A process blocked on IPC when its deadline expires is restarted and leaves the wait list. |
| REQ-DL-007 | While a process waits for its next release and nothing else is ready, the CPU idles instead of running another partition's code in its slot. |
| REQ-DL-008 | Each fault class keeps its own policy: a deadline miss can be configured independently of memory faults. |

## Privilege

| ID | Requirement |
|---|---|
| REQ-PRIV-001 | Application partitions are unprivileged by default: their processes cannot touch kernel memory. |
| REQ-PRIV-002 | A system partition runs privileged: its processes can use kernel memory that application partitions cannot. |
| REQ-PRIV-003 | Privilege belongs to the partition: all processes of a partition share it. |
| REQ-PRIV-004 | With the privileged default map disabled (deny by default), even a privileged process cannot touch memory no region maps, and the failure is contained like any other fault. |

## Fault containment

| ID | Requirement |
|---|---|
| REQ-FT-001 | RESTART_PROCESS: the faulting process is restarted at its entry point; every other process and the schedule are unaffected. |
| REQ-FT-002 | HALT_PARTITION: the partition, including healthy peers, never runs again; its slots idle; other partitions keep their exact timing. |
| REQ-FT-003 | Each partition applies its own policy. |
| REQ-FT-004 | Every fault class is handled according to its own policy. |
| REQ-FT-005 | Repeated faults (a fault storm) leave the system sound. |
| REQ-FT-006 | An asynchronous fault is charged to the partition that was running when it occurred. |
| REQ-FT-007 | A fault with no health monitor installed stops the CPU instead of continuing with unknown state. |
| REQ-FT-008 | Peers waiting on a faulted process are not stranded. |
| REQ-FT-009 | The kernel returns the configured action for every fault it is asked to recover, and repeated recovery never disturbs process identity or an unrelated partition. |

## Simulator qualification

| ID | Requirement |
|---|---|
| SIM-001 | The MPU model decides access as ARMv7-M PMSAv7 specifies (AP encodings, boundaries, overlap, PRIVDEFENA, disabled regions, illegal programming). |
| SIM-002 | Virtual/host address translation is exact. |
| SIM-003 | Virtual time is exact and runs are bit-identical; slicing a run does not change it. |
| SIM-004 | Start-up with nothing to run halts and is reported. |
