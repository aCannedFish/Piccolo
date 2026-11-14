#!/bin/bash
set -euo pipefail

# Configure and build from the repository directory.
if [[ $# -ne 1 ]]; then
    echo "Usage: $0 {debug|release}" >&2
    exit 1
fi

case "$1" in
    debug|release)
        preset="macos-$1"
        ;;
    *)
        echo "Unsupported configuration: $1. Use debug or release." >&2
        exit 1
        ;;
esac

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$project_dir"

cmake --preset "$preset"
cp "build/$preset/compile_commands.json" compile_commands.json
cmake --build --preset "$preset" --parallel "$(sysctl -n hw.logicalcpu)"
