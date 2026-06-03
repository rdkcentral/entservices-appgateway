# entservices-appgateway
This repo contains the firebolt gateway implementation as a thunder plugin. This deprecates the ripple gateway in RDK Apps Managers Framework.

## Containerized Unit Test Workflow

Unit tests are docker-first by default to avoid host dependency setup and improve portability across Linux and macOS.

### Default one-shot entrypoint

```bash
./run_unit_tests.sh --l0
```

This delegates to Docker automatically on host machines.

### Build dependency image

```bash
./.github/scripts/docker_build_test_image.sh
```

This builds a local image named `entservices-appgateway-test-deps:local` (override with `IMAGE=...`).

If your Docker daemon has DNS restrictions, you can override network/mirror settings:

- `DOCKER_NETWORK_MODE=host ./.github/scripts/docker_build_test_image.sh` (Linux)
- `APT_MIRROR=http://<your-mirror>/ubuntu ./.github/scripts/docker_build_test_image.sh`

### Run unit tests in container

```bash
./.github/scripts/run_unit_tests_docker.sh --l0
```
Forward any existing `run_unit_tests.sh` options through this wrapper:

- `./.github/scripts/run_unit_tests_docker.sh --l1`
- `./.github/scripts/run_unit_tests_docker.sh --all`
- `./.github/scripts/run_unit_tests_docker.sh --run appgateway_l0test`
- `./.github/scripts/run_unit_tests_docker.sh --rebuild-image --l0`

Notes:

- The wrapper executes the workspace script `.github/docker/run-in-container.sh` inside the container (mounted read-only), so script updates are picked up without rebuilding the image.
- The wrapper only builds the image when needed: if the image already exists, it reuses the cached image and does not run a build.
- Linux: wrapper runs as root in-container by default for dependency readability; set `RUN_AS_HOST_USER=ON` to map uid/gid when needed.
- macOS: wrapper skips uid/gid mapping for Docker Desktop compatibility.
- Source is mounted read-only and copied to a writable temp directory inside the container before test execution.
- If you see apt resolution errors during image build, use `DOCKER_NETWORK_MODE` and/or `APT_MIRROR` overrides above.
