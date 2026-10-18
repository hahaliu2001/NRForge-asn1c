#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ $# -lt 1 ]]; then
  printf '%s\n' 'Usage: run.sh BUILD_EVIDENCE_DIR --schema-root FROZEN_SCHEMA_ROOT --work NEW_WORK_DIR'
  exit 2
fi
evidence_dir="$1"
shift
exec python3 "$script_dir/../ngap-transfer-sdk/resource_guard.py" --evidence "$evidence_dir" -- python3 "$script_dir/produce.py" "$@"
