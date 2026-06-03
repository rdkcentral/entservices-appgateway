#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
IMAGE="${IMAGE:-entservices-appgateway-test-deps:local}"
REBUILD_IMAGE="false"
TTY_FLAG="-t"
RUN_AS_HOST_USER="${RUN_AS_HOST_USER:-OFF}"

if ! command -v docker >/dev/null 2>&1; then
    echo "[run-unit-tests-docker] ERROR: docker is not installed or not in PATH" >&2
    exit 1
fi

if [[ ! -t 1 ]]; then
    TTY_FLAG=""
fi

ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --rebuild-image)
            REBUILD_IMAGE="true"
            shift
            ;;
        --docker)
            # Already in the docker wrapper; drop this to avoid nested delegation.
            shift
            ;;
        --help|-h)
            cat <<EOF
Usage: ./.github/scripts/run_unit_tests_docker.sh [--rebuild-image] [run options]

Examples:
  ./.github/scripts/run_unit_tests_docker.sh --l0
  ./.github/scripts/run_unit_tests_docker.sh --l1
  ./.github/scripts/run_unit_tests_docker.sh --all
  ./.github/scripts/run_unit_tests_docker.sh --run appgateway_l0test
  ./.github/scripts/run_unit_tests_docker.sh --rebuild-image --l0

Notes:
  - Runs tests in a containerized Ubuntu environment to avoid host dependency installs.
  - Uses image: ${IMAGE}
    - Runs as root in-container by default; set RUN_AS_HOST_USER=ON on Linux to map uid/gid.
    - Forwarded options are passed to run_unit_tests_core.sh inside container.
EOF
            exit 0
            ;;
        *)
            ARGS+=("$1")
            shift
            ;;
    esac
done

if [[ "${REBUILD_IMAGE}" == "true" ]] || ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
    "${ROOT_DIR}/.github/scripts/docker_build_test_image.sh"
else
    echo "[run-unit-tests-docker] Using cached image: ${IMAGE}"
fi

USER_ARGS=()
if [[ "${RUN_AS_HOST_USER}" == "ON" && "$(uname -s)" == "Linux" ]]; then
    USER_ARGS=(--user "$(id -u):$(id -g)")
fi

echo "[run-unit-tests-docker] Running inside ${IMAGE}"

docker run --rm ${TTY_FLAG} \
    "${USER_ARGS[@]}" \
    -v "${ROOT_DIR}:/workspace:ro" \
    -e SRC_DIR=/workspace \
    -e DEPS_PREFIX=/opt/entdeps/src/install/usr \
    --entrypoint /bin/bash \
    "${IMAGE}" \
    -lc 'exec /workspace/.github/docker/run-in-container.sh "$@"' _ "${ARGS[@]}"
