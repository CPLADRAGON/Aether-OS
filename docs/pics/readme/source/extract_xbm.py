"""Extract selected 24×24 XBM arrays from firmware display_manager.cpp.

The README host renderer includes the generated header so its icon pixels
come from the exact same firmware arrays rather than hand-drawn substitutes.
"""

from pathlib import Path
import re

REPO = Path(__file__).resolve().parents[4]
SOURCE = REPO / "firmware" / "src" / "display_manager.cpp"
OUTPUT = Path(__file__).resolve().parent / "generated_icons.h"

NAMES = [
    "xbm_measure_lg",
    "xbm_weather_sun_lg",
    "xbm_weather_lg",
    "xbm_room_lg",
    "xbm_interval_lg",
]

text = SOURCE.read_text(encoding="utf-8")
blocks = []
for name in NAMES:
    match = re.search(
        rf"static const uint8_t\s+{re.escape(name)}\[\]\s+PROGMEM\s*=\s*\{{(.*?)\}};",
        text,
        flags=re.S,
    )
    if not match:
        raise SystemExit(f"Could not find {name}")
    values = re.findall(r"0x[0-9a-fA-F]+", match.group(1))
    if len(values) != 72:
        raise SystemExit(f"{name}: expected 72 bytes, found {len(values)}")
    formatted = ", ".join(values)
    blocks.append(f"static const uint8_t {name}[] = {{ {formatted} }};")

OUTPUT.write_text(
    "/* Generated from firmware/src/display_manager.cpp; do not hand-edit. */\n"
    "#pragma once\n#include <stdint.h>\n\n"
    + "\n".join(blocks)
    + "\n",
    encoding="utf-8",
)
