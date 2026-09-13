#!/usr/bin/env bash
# Configure and build the project with CMake.
#
#   ./build.sh [-r|--release] [-d|--debug] <target>...
#
# Release builds go to build/release, debug builds to build/debug.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

usage() {
    cat <<'EOF'
Usage: ./build.sh [options] <target>...

Targets:
  producer    build the producer executable
  consumer    build the consumer executable
  tests       build the unit and integration tests (plus producer and consumer,
              which the integration test needs)
  all         build everything

Options:
  -r, --release   Release build, optimised (default)   -> build/release
  -d, --debug     Debug build with assertions           -> build/debug
  -h, --help      show this help

Examples:
  ./build.sh all                         # release build of everything
  ./build.sh -d producer consumer        # debug build of both applications
  ./build.sh tests && ctest --test-dir build/release --output-on-failure
EOF
}

build_type="Release"
targets=()

if [ $# -eq 0 ]; then
    usage
    exit 1
fi

while [ $# -gt 0 ]; do
    case "$1" in
        -r|--release) build_type="Release" ;;
        -d|--debug)   build_type="Debug" ;;
        -h|--help)    usage; exit 0 ;;
        producer|consumer|all) targets+=("$1") ;;
        tests)        targets+=("pc_tests") ;;
        -*)           echo "build.sh: unknown option '$1'" >&2; usage >&2; exit 1 ;;
        *)            echo "build.sh: unknown target '$1' (expected: producer, consumer, tests, all)" >&2
                      usage >&2; exit 1 ;;
    esac
    shift
done

if [ ${#targets[@]} -eq 0 ]; then
    echo "build.sh: no target given" >&2
    usage >&2
    exit 1
fi

build_dir="build/$(tr '[:upper:]' '[:lower:]' <<<"$build_type")"
jobs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "== Configuring ($build_type) in $build_dir"
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE="$build_type" -DPC_BUILD_TESTS=ON

echo "== Building: ${targets[*]}"
cmake --build "$build_dir" -j"$jobs" --target "${targets[@]}"

echo "== Done ($build_type)"
for target in "${targets[@]}"; do
    case "$target" in
        producer|all) echo "   producer: $build_dir/producer/producer" ;;
    esac
    case "$target" in
        consumer|all) echo "   consumer: $build_dir/consumer/consumer" ;;
    esac
    case "$target" in
        pc_tests|all) echo "   tests:    ctest --test-dir $build_dir --output-on-failure" ;;
    esac
done
