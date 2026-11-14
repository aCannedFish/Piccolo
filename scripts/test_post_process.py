#!/usr/bin/env python3
"""Run the rendering smoke test with FXAA disabled and enabled, and check Vulkan logs."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def run_variants(executable, config_path):
    """Create temporary configs that share the installed assets without modifying them."""
    config = {}
    for line in config_path.read_text().splitlines():
        name, separator, value = line.partition("=")
        if separator:
            config[name] = value
    root = (config_path.parent / config["BinaryRootFolder"]).resolve()
    rendering_path = root / config["GlobalRenderingRes"]
    rendering = json.loads(rendering_path.read_text())

    with tempfile.TemporaryDirectory(prefix="piccolo-post-process-") as directory:
        temporary = Path(directory)
        for fxaa_enabled in (False, True):
            rendering["enable_fxaa"] = fxaa_enabled
            override = temporary / "rendering.json"
            override.write_text(json.dumps(rendering, indent=2))
            config["BinaryRootFolder"] = str(root)
            config["GlobalRenderingRes"] = str(override)
            ini = temporary / "PiccoloEditor.ini"
            ini.write_text("\n".join(f"{name}={value}" for name, value in config.items()) + "\n")

            result = subprocess.run([str(executable), str(ini)], capture_output=True, text=True, timeout=120)
            output = result.stdout + result.stderr
            errors = ("Validation Error", "[error]", "Assertion failed")
            if result.returncode != 0 or any(error in output for error in errors):
                raise RuntimeError(f"Post-process smoke failed with FXAA={fxaa_enabled}:\n{output}")
            if "Passed 12 post-process combinations" not in output:
                raise RuntimeError(f"Post-process smoke did not report completion:\n{output}")
            print(f"FXAA={fxaa_enabled}: 12 combinations passed with no Vulkan validation errors.")


def main():
    """Accept the compiled smoke executable and an existing editor INI file."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("config", type=Path)
    args = parser.parse_args()
    run_variants(args.executable.resolve(), args.config.resolve())


if __name__ == "__main__":
    main()
