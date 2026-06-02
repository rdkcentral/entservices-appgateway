#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

# Include top-level scripts, test helpers, and docker helper scripts.
mapfile -t scripts < <(
    {
        git ls-files '*.sh'
    } | awk 'NF && !seen[$0]++' | sort
)

if [[ ${#scripts[@]} -eq 0 ]]; then
    echo "[lint-shell] No shell scripts found."
    exit 0
fi

echo "[lint-shell] Running bash syntax checks"
for script in "${scripts[@]}"; do
    [[ -f "${script}" ]] || continue
    bash -n "${script}"
done

echo "[lint-shell] bash -n passed for ${#scripts[@]} scripts"

if command -v shellcheck >/dev/null 2>&1; then
    echo "[lint-shell] Running shellcheck"
    shellcheck "${scripts[@]}"
    echo "[lint-shell] shellcheck passed"
else
    echo "[lint-shell] shellcheck not found; install it to enable semantic shell linting"
fi
