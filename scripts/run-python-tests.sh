#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../python"
python -m unittest discover -s tests -v
