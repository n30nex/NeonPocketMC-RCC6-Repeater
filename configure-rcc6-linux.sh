#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CACHE=${XDG_CACHE_HOME:-"$HOME/.cache"}/neonpocketmc-rcc6-repeater/configurator

if ! command -v python3 >/dev/null 2>&1; then
  echo "Python 3 is required. Install python3 and python3-venv, then run this file again." >&2
  exit 1
fi

if [ ! -x "$CACHE/bin/python" ]; then
  echo "First run: preparing the small USB helper..."
  python3 -m venv "$CACHE"
  "$CACHE/bin/python" -m pip install --disable-pip-version-check pyserial==3.5
fi

exec "$CACHE/bin/python" "$ROOT/scripts/configure_rcc6.py" "$@"
