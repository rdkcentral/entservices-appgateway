#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="${ROOT_DIR}/.github/scripts"

show_help() {
    cat <<EOF
Usage: ./run_unit_tests.sh [run options]

Docker-first unit test entrypoint. On host it always delegates to Docker.
Any run options are forwarded to .github/scripts/run_unit_tests_core.sh inside the container.

Examples:
  ./run_unit_tests.sh --l0
  ./run_unit_tests.sh --l1
  ./run_unit_tests.sh --run AppGatewayCommonL1Test
EOF
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    show_help
    exit 0
fi

if [[ "${RUNNING_IN_DOCKER:-0}" == "1" ]]; then
    exec "${SCRIPTS_DIR}/run_unit_tests_core.sh" "$@"
fi

exec "${SCRIPTS_DIR}/run_unit_tests_docker.sh" "$@"
