#!/usr/bin/env python3
"""Generate the standalone editor from its template and control catalog."""
import json
from pathlib import Path

BASE = Path(__file__).resolve().parent

def build():
    data = json.loads((BASE / "ctrlr/controls_final.json").read_text())
    names = [control["name"] for control in data["controls"]]
    if len(names) != len(set(names)):
        raise ValueError("Control names must be unique")
    template = (BASE / "src/editor.template.html").read_text()
    if template.count("__DATA__") != 1:
        raise ValueError("Expected one control catalog placeholder")
    output = template.replace("__DATA__", json.dumps(data, ensure_ascii=False, separators=(",", ":")))
    (BASE / "mkxl_editor.html").write_text(output)
    print(f"Generated mkxl_editor.html: {len(names)} controls")

if __name__ == "__main__":
    build()
