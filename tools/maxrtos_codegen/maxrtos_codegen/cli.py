from __future__ import annotations

import argparse
import json
import pathlib
import sys

from .generators.c_config import (
    generate_header,
    generate_limits_header,
    generate_source,
)
from .generators.linker import generate_linker_script
from .schema import ConfigError, load_config


def main(argv: list[str] | None = None) -> int:
    """
    Generate MAXRTOS configuration artifacts.

    The input JSON configuration is validated before any output files are
    created. Generated files include the linker script and the C
    configuration header/source. Startup code is owned by the application.

    Args:
        argv: Optional command-line arguments. If None, arguments are read
            from sys.argv.

    Returns:
        0 on successful generation, 1 if the configuration cannot be
        parsed or validated.
    """
    parser = argparse.ArgumentParser(
        prog="maxrtos-codegen",
        description=(
            "Generate MPU/partition/frame-schedule C glue and a linker "
            "script for a maxrtos ARINC-653-style partitioned application "
            "from a JSON config."
        ),
    )
    parser.add_argument(
        "config",
        type=pathlib.Path,
        help="path to config JSON",
    )
    parser.add_argument(
        "--out-dir",
        type=pathlib.Path,
        default=pathlib.Path("generated"),
        help="output directory (default: ./generated)",
    )

    parser.add_argument(
        "--emit-limits",
        action="store_true",
        help=(
            "also write maxrtos_limits.h, the smallest kernel table sizes "
            "that fit this config. The build must then force-include it into "
            "the kernel and the architecture layer as well as the "
            "application (default: not written, so the kernel defaults apply)"
        ),
    )

    args = parser.parse_args(argv)

    try:
        raw = json.loads(args.config.read_text())
        cfg = load_config(raw)
    except (json.JSONDecodeError, ConfigError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    args.out_dir.mkdir(parents=True, exist_ok=True)
    config_path_str = str(args.config)

    # Generate all artifacts from the same validated configuration so
    # that the linker script and C configuration remain
    # consistent with one another.
    (args.out_dir / "link.ld").write_text(
        generate_linker_script(cfg, config_path_str)
    )
    (args.out_dir / "maxrtos_config.h").write_text(
        generate_header(cfg, config_path_str)
    )
    (args.out_dir / "maxrtos_config.c").write_text(
        generate_source(cfg, config_path_str)
    )

    if args.emit_limits:
        (args.out_dir / "maxrtos_limits.h").write_text(
            generate_limits_header(cfg, config_path_str)
        )

    print(f"generated: {args.out_dir}/link.ld")
    print(f"generated: {args.out_dir}/maxrtos_config.h")
    print(f"generated: {args.out_dir}/maxrtos_config.c")
    if args.emit_limits:
        print(f"generated: {args.out_dir}/maxrtos_limits.h")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
