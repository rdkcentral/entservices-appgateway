#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

MODE="all"
BUILD_TYPE="Debug"
USE_THUNDER_R4="ON"
RUN_ONLY=""
USER_SET_PREFIX="false"
USER_SET_CMAKE_PREFIX="false"

PREFIX="${PREFIX:-${ROOT_DIR}/install/usr}"
CMAKE_PREFIX_PATH_INPUT="${CMAKE_PREFIX_PATH:-${PREFIX}}"
L0_BUILD_DIR="${ROOT_DIR}/build/unit/l0"
L1_BUILD_DIR="${ROOT_DIR}/build/unit/l1"
FORWARD_ARGS=()

detect_jobs() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
        return
    fi

    if command -v getconf >/dev/null 2>&1; then
        getconf _NPROCESSORS_ONLN
        return
    fi

    if command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.ncpu
        return
    fi

    echo 1
}

JOBS="${JOBS:-$(detect_jobs)}"

REQUIRED_HEADERS=(
    "core/core.h"
    "plugins/plugins.h"
    "plugins/JSONRPC.h"
)

print_help() {
    cat <<EOF
Usage: ./run_unit_tests_core.sh [options]

Options:
  --l0                     Run only L0 unit tests
  --l1                     Run only L1 unit tests
  --all                    Run both L0 and L1 unit tests (default)
  --run <name>             Run only one test binary name
                           Values: appgateway_l0test, appgatewaycommon_l0test,
                                   appnotifications_l0test,
                                   AppGatewayL1Test, AppGatewayCommonL1Test,
                                   AppNotificationsL1Test
  --prefix <path>          Install prefix for headers/libs (default: ./install/usr)
  --cmake-prefix <path>    CMAKE_PREFIX_PATH value (default: --prefix)
  --build-type <type>      CMake build type (default: Debug)
    --jobs <n>               Parallel build jobs (default: auto-detected)
    --thunder-r4 <ON|OFF>    Toggle USE_THUNDER_R4 for L1 (default: ON)
    --help                   Show this help

Examples:
    ./run_unit_tests_core.sh --l0
    ./run_unit_tests_core.sh --l1 --prefix /opt/wpe/install/usr
    ./run_unit_tests_core.sh --run AppGatewayCommonL1Test

Notes:
    - The build requires Thunder/WPEFramework headers, including:
            core/core.h, plugins/plugins.h, plugins/JSONRPC.h
    - Intended for use inside the test container.
    - If deps are missing, pass a valid --prefix.
EOF
}

log() {
    echo "[run-unit-tests] $*"
}

die() {
    echo "[run-unit-tests] ERROR: $*" >&2
    exit 1
}

have_required_headers() {
    local prefix="$1"
    local header

    for header in "${REQUIRED_HEADERS[@]}"; do
        if [[ -f "${prefix}/include/${header}" || -f "${prefix}/include/WPEFramework/${header}" ]]; then
            continue
        fi
        return 1
    done

    return 0
}

collect_prefix_candidates() {
    local candidates=()
    local token

    candidates+=("${PREFIX}")
    candidates+=("${ROOT_DIR}/install/usr")

    IFS=':' read -r -a _cmake_tokens <<< "${CMAKE_PREFIX_PATH_INPUT}"
    for token in "${_cmake_tokens[@]}"; do
        [[ -n "${token}" ]] && candidates+=("${token}")
    done

    candidates+=(
        "/usr"
        "/usr/local"
        "/opt/wpe/install/usr"
        "/opt/thunder/install/usr"
        "/opt/WPEFramework/install/usr"
    )

    printf '%s\n' "${candidates[@]}" | awk 'NF && !seen[$0]++'
}

resolve_prefix() {
    local candidate

    if have_required_headers "${PREFIX}"; then
        return 0
    fi

    if [[ "${USER_SET_PREFIX}" == "true" ]]; then
        return 1
    fi

    while IFS= read -r candidate; do
        if have_required_headers "${candidate}"; then
            log "Auto-detected dependency prefix: ${candidate}"
            PREFIX="${candidate}"
            if [[ "${USER_SET_CMAKE_PREFIX}" != "true" ]]; then
                CMAKE_PREFIX_PATH_INPUT="${candidate}"
            fi
            return 0
        fi
    done < <(collect_prefix_candidates)

    return 1
}

print_prefix_hints_and_die() {
    local candidate

    echo "[run-unit-tests] Checked include roots:" >&2
    while IFS= read -r candidate; do
        echo "  - ${candidate}/include" >&2
        echo "  - ${candidate}/include/WPEFramework" >&2
    done < <(collect_prefix_candidates)

    die "Missing Thunder/WPEFramework headers (${REQUIRED_HEADERS[*]}). Run ./build_dependencies.sh or rerun with --prefix <install/usr>."
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --l0)
            MODE="l0"
            FORWARD_ARGS+=("$1")
            shift
            ;;
        --l1)
            MODE="l1"
            FORWARD_ARGS+=("$1")
            shift
            ;;
        --all)
            MODE="all"
            FORWARD_ARGS+=("$1")
            shift
            ;;
        --run)
            [[ $# -ge 2 ]] || die "--run requires a value"
            RUN_ONLY="$2"
            FORWARD_ARGS+=("$1" "$2")
            shift 2
            ;;
        --prefix)
            [[ $# -ge 2 ]] || die "--prefix requires a value"
            PREFIX="$2"
            USER_SET_PREFIX="true"
            if [[ "${USER_SET_CMAKE_PREFIX}" != "true" ]]; then
                CMAKE_PREFIX_PATH_INPUT="$2"
            fi
            FORWARD_ARGS+=("$1" "$2")
            shift 2
            ;;
        --cmake-prefix)
            [[ $# -ge 2 ]] || die "--cmake-prefix requires a value"
            CMAKE_PREFIX_PATH_INPUT="$2"
            USER_SET_CMAKE_PREFIX="true"
            FORWARD_ARGS+=("$1" "$2")
            shift 2
            ;;
        --build-type)
            [[ $# -ge 2 ]] || die "--build-type requires a value"
            BUILD_TYPE="$2"
            FORWARD_ARGS+=("$1" "$2")
            shift 2
            ;;
        --jobs)
            [[ $# -ge 2 ]] || die "--jobs requires a value"
            JOBS="$2"
            FORWARD_ARGS+=("$1" "$2")
            shift 2
            ;;
        --thunder-r4)
            [[ $# -ge 2 ]] || die "--thunder-r4 requires ON or OFF"
            USE_THUNDER_R4="$2"
            FORWARD_ARGS+=("$1" "$2")
            shift 2
            ;;
        --help|-h)
            print_help
            exit 0
            ;;
        *)
            die "Unknown option: $1"
            ;;
    esac
done

if [[ "${USE_THUNDER_R4}" != "ON" && "${USE_THUNDER_R4}" != "OFF" ]]; then
    die "--thunder-r4 must be ON or OFF"
fi

if ! resolve_prefix; then
    print_prefix_hints_and_die
fi

run_binary_if_selected() {
    local bin_path="$1"
    local bin_name
    bin_name="$(basename "${bin_path}")"

    if [[ -n "${RUN_ONLY}" && "${RUN_ONLY}" != "${bin_name}" ]]; then
        return
    fi

    if [[ ! -x "${bin_path}" ]]; then
        log "Skipping missing binary: ${bin_name}"
        return
    fi

    local runtime_lib_path=""
    local candidate
    local ns_lower=""

    if [[ -n "${NAMESPACE:-}" ]]; then
        ns_lower="$(echo "${NAMESPACE}" | tr '[:upper:]' '[:lower:]')"
    fi

    for candidate in \
        "${PREFIX}/lib" \
        "${PREFIX}/lib/plugins" \
        "${PREFIX}/lib/wpeframework" \
        "${PREFIX}/lib/wpeframework/plugins" \
        "${PREFIX}/lib/thunder" \
        "${PREFIX}/lib/thunder/plugins" \
        "${PREFIX}/lib/${ns_lower}" \
        "${PREFIX}/lib/${ns_lower}/plugins"; do
        if [[ -n "${candidate}" && -d "${candidate}" ]]; then
            if [[ -z "${runtime_lib_path}" ]]; then
                runtime_lib_path="${candidate}"
            else
                runtime_lib_path="${runtime_lib_path}:${candidate}"
            fi
        fi
    done

    if [[ -n "${LD_LIBRARY_PATH:-}" ]]; then
        if [[ -n "${runtime_lib_path}" ]]; then
            runtime_lib_path="${runtime_lib_path}:${LD_LIBRARY_PATH}"
        else
            runtime_lib_path="${LD_LIBRARY_PATH}"
        fi
    fi

    log "Running ${bin_name}"
    if [[ -n "${runtime_lib_path}" ]]; then
        if [[ "Darwin" == "$(uname -s)" ]]; then
            local dyld_runtime_path="${runtime_lib_path}"
            if [[ -n "${DYLD_LIBRARY_PATH:-}" ]]; then
                dyld_runtime_path="${dyld_runtime_path}:${DYLD_LIBRARY_PATH}"
            fi
            DYLD_LIBRARY_PATH="${dyld_runtime_path}" LD_LIBRARY_PATH="${runtime_lib_path}" "${bin_path}"
        else
            LD_LIBRARY_PATH="${runtime_lib_path}" "${bin_path}"
        fi
    else
        "${bin_path}"
    fi
}

configure_l0() {
    log "Configuring L0 tests in ${L0_BUILD_DIR}"
    cmake -S "${ROOT_DIR}/Tests/L0Tests" -B "${L0_BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DPREFIX="${PREFIX}" \
        -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH_INPUT}"
}

build_l0() {
    log "Building L0 tests"
    cmake --build "${L0_BUILD_DIR}" -j "${JOBS}" --target \
        appgateway_l0test appgatewaycommon_l0test appnotifications_l0test
}

run_l0() {
    configure_l0
    build_l0

    run_binary_if_selected "${L0_BUILD_DIR}/appgateway_l0test"
    run_binary_if_selected "${L0_BUILD_DIR}/appgatewaycommon_l0test"
    run_binary_if_selected "${L0_BUILD_DIR}/appnotifications_l0test"
}

configure_l1() {
    log "Configuring L1 tests in ${L1_BUILD_DIR}"

    # L1 standalone configure needs write_config() from FindConfigGenerator.cmake.
    # In our dependency layout, this module is installed under <prefix>/../tools/cmake.
    local l1_cmake_module_path="${PREFIX}/../tools/cmake;${PREFIX}/include/WPEFramework/Modules"

    cmake -S "${ROOT_DIR}/Tests/L1Tests" -B "${L1_BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
        -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH_INPUT}" \
        -DCMAKE_MODULE_PATH="${l1_cmake_module_path}" \
        -DDISABLE_SECURITY_TOKEN=ON \
        -DUSE_THUNDER_R4="${USE_THUNDER_R4}" \
        -DPLUGIN_APPGATEWAY=ON \
        -DPLUGIN_APPGATEWAYCOMMON=ON \
        -DPLUGIN_APPNOTIFICATIONS=ON
}

build_l1() {
    log "Building L1 tests"
    cmake --build "${L1_BUILD_DIR}" -j "${JOBS}" --target \
        AppGatewayL1Test AppGatewayCommonL1Test AppNotificationsL1Test
}

run_l1() {
    configure_l1
    build_l1

    run_binary_if_selected "${L1_BUILD_DIR}/AppGatewayL1Test"
    run_binary_if_selected "${L1_BUILD_DIR}/AppGatewayCommonL1Test"
    run_binary_if_selected "${L1_BUILD_DIR}/AppNotificationsL1Test"
}

log "Mode: ${MODE}"
log "Prefix: ${PREFIX}"
log "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH_INPUT}"
log "Build type: ${BUILD_TYPE}"
log "Jobs: ${JOBS}"

case "${MODE}" in
    l0)
        run_l0
        ;;
    l1)
        run_l1
        ;;
    all)
        run_l0
        run_l1
        ;;
    *)
        die "Invalid mode: ${MODE}"
        ;;
esac

log "Done"
