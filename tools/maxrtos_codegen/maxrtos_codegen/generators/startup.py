from __future__ import annotations

import pathlib
from jinja2 import Environment, FileSystemLoader, StrictUndefined

from ..schema import RtosConfig

_TEMPLATE_DIR = pathlib.Path(__file__).parent.parent / "templates"


def generate_startup(cfg: RtosConfig, config_path: str) -> str:
    env = Environment(
        loader=FileSystemLoader(str(_TEMPLATE_DIR)),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )
    template = env.get_template("startup.S.j2")
    return template.render(
        config_path=config_path,
        module_name=cfg.module_name,
    )
