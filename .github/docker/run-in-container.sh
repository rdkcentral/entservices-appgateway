#!/usr/bin/env bash
set -euo pipefail

SRC_DIR="${SRC_DIR:-/workspace}"
WORK_DIR="${WORK_DIR:-/tmp/entservices-appgateway}"
DEPS_PREFIX="${DEPS_PREFIX:-/opt/entdeps/src/install/usr}"

if [[ ! -d "${SRC_DIR}" ]]; then
    echo "[docker-runner] ERROR: source directory not found: ${SRC_DIR}" >&2
    exit 1
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"

# Copy from mounted checkout to writable working dir.
cp -a "${SRC_DIR}/." "${WORK_DIR}/"
rm -rf "${WORK_DIR}/.git"

# Avoid stale host-generated CMake caches after path translation into /tmp.
rm -rf "${WORK_DIR}/build" "${WORK_DIR}/build-dev" "${WORK_DIR}/Tests/L0Tests/build-l0"

cd "${WORK_DIR}"

if [[ ! -x ./run_unit_tests.sh ]]; then
    echo "[docker-runner] ERROR: run_unit_tests.sh not found in copied workspace" >&2
    exit 1
fi

exec env AUTO_DOCKER_ON_MISSING_DEPS=OFF RUNNING_IN_DOCKER=1 \
    ./run_unit_tests.sh --prefix "${DEPS_PREFIX}" "$@"
