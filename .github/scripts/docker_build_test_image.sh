#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
IMAGE="${IMAGE:-entservices-appgateway-test-deps:local}"
DOCKERFILE="${ROOT_DIR}/.github/docker/Dockerfile.unit-tests"
APT_MIRROR="${APT_MIRROR:-}"
DOCKER_NETWORK_MODE="${DOCKER_NETWORK_MODE:-}"

calc_sha() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$@" | awk '{print $1}' | sha256sum | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$@" | awk '{print $1}' | shasum -a 256 | awk '{print $1}'
    else
        echo "[docker-build] ERROR: neither sha256sum nor shasum found" >&2
        exit 1
    fi
}

RUNNER_SHA="$(calc_sha \
    "${ROOT_DIR}/.github/docker/Dockerfile.unit-tests" \
    "${ROOT_DIR}/.github/docker/run-in-container.sh" \
    "${ROOT_DIR}/build_dependencies.sh")"

if ! command -v docker >/dev/null 2>&1; then
    echo "[docker-build] ERROR: docker is not installed or not in PATH" >&2
    exit 1
fi

echo "[docker-build] Building image: ${IMAGE}"

BUILD_ARGS=()
BUILD_ARGS+=(--build-arg "RUNNER_SHA=${RUNNER_SHA}")
if [[ -n "${APT_MIRROR}" ]]; then
    BUILD_ARGS+=(--build-arg "APT_MIRROR=${APT_MIRROR}")
    echo "[docker-build] Using apt mirror: ${APT_MIRROR}"
fi

if [[ -z "${DOCKER_NETWORK_MODE}" && "$(uname -s)" == "Linux" ]]; then
    DOCKER_NETWORK_MODE="host"
fi

if [[ -n "${DOCKER_NETWORK_MODE}" ]]; then
    BUILD_ARGS+=(--network "${DOCKER_NETWORK_MODE}")
    echo "[docker-build] Using docker build network mode: ${DOCKER_NETWORK_MODE}"
fi

docker build "${BUILD_ARGS[@]}" -t "${IMAGE}" -f "${DOCKERFILE}" "${ROOT_DIR}"

echo "[docker-build] Done"
