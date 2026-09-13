#!/usr/bin/env bash
# Throughput benchmark: runs producer + consumer for a fixed time per
# configuration and prints a Markdown table.
#
#   scripts/bench.sh [build-dir] [seconds]
#
# Rows: payload sizes with default settings, then ring sizes, payload
# generators, checksum algorithms and CPU pinning for a 4 KiB payload.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

BUILD=${1:-build/release}
SECONDS_PER_RUN=${2:-4}
PRODUCER="$BUILD/producer/producer"
CONSUMER="$BUILD/consumer/consumer"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

[ -x "$PRODUCER" ] && [ -x "$CONSUMER" ] || { echo "binaries not found in $BUILD (run ./build.sh all)" >&2; exit 1; }

# run <label> <payload> <ring> <generator> <producer-prefix> <consumer-prefix> [checksum]
run() {
    local label=$1 payload=$2 ring=$3 generator=$4 pprefix=$5 cprefix=$6 checksum=${7:-crc32c}
    local name="/pc-bench-$$-${RANDOM}"
    $cprefix "$CONSUMER" --shm-name "$name" --once --no-keyboard --report-interval 1000 \
        < /dev/null > "$WORK/c.log" 2>&1 &
    local cpid=$!
    sleep 0.2
    $pprefix "$PRODUCER" "$payload" --shm-name "$name" --ring-size "$ring" --generator "$generator" \
        --checksum "$checksum" --wait-for-consumer --no-keyboard --report-interval 1000 < /dev/null > "$WORK/p.log" 2>&1 &
    local ppid=$!
    sleep "$SECONDS_PER_RUN"
    kill -INT "$ppid"; wait "$ppid"; wait "$cpid"

    # "done: N packets, X in T s (avg P pkt/s, B/s)"
    local pps bps lat_avg lat_max valid corrupted
    pps=$(sed -n 's/.*(avg \([0-9,]*\) pkt\/s.*/\1/p' "$WORK/p.log")
    bps=$(sed -n 's/.*pkt\/s, \(.*\)\/s)$/\1/p' "$WORK/p.log")
    # latency: average of the per-second "avg" values, and the largest "max"
    lat_avg=$(grep -o 'latency avg [0-9.]* [a-z]*' "$WORK/c.log" | awk '
        { v=$3; u=$4; if (u=="ns") v/=1000; else if (u=="ms") v*=1000; else if (u=="s") v*=1e6; s+=v; n++ }
        END { if (n) printf "%.1f us", s/n; else print "n/a" }')
    lat_max=$(grep -o 'max [0-9.]* [a-z]*' "$WORK/c.log" | awk '
        { v=$2; u=$3; if (u=="ns") v/=1000; else if (u=="ms") v*=1000; else if (u=="s") v*=1e6; if (v>m) m=v }
        END { if (m>=1000) printf "%.2f ms", m/1000; else printf "%.0f us", m }')
    valid=$(sed -n 's/.*valid \([0-9,]*\) | corrupted \([0-9,]*\).*/\1/p' "$WORK/c.log")
    corrupted=$(sed -n 's/.*valid \([0-9,]*\) | corrupted \([0-9,]*\).*/\2/p' "$WORK/c.log")
    printf '| %-28s | %-8s | %-6s | %-10s | %-8s | %12s | %11s | %9s | %9s | %12s | %s |\n' \
        "$label" "$payload" "$ring" "$generator" "$checksum" "$pps" "$bps" "$lat_avg" "$lat_max" "$valid" "$corrupted"
}

echo "Machine: $(lscpu 2>/dev/null | sed -n 's/^Model name: *//p' | head -1), $(nproc) CPUs, $(uname -sr)"
echo "Run time per row: ${SECONDS_PER_RUN}s. Throughput counts header + payload bytes."
echo
echo "| Configuration                | Payload  | Ring   | Generator  | Checksum |    Packets/s |  Throughput | Lat. avg  | Lat. max  |  Valid pkts  | Corrupted |"
echo "|------------------------------|----------|--------|------------|----------|-------------:|------------:|----------:|----------:|-------------:|----------:|"

for p in 16 64 256 1K 4K 16K 64K 256K 1M; do
    run "payload sweep" "$p" 64M random "" ""
done
for r in 1M 4M 16M 256M; do
    run "ring size" 4K "$r" random "" ""
done
run "generator" 4K 64M sequential "" ""
run "generator" 64 64M sequential "" ""
run "checksum" 4K 64M random "" "" crc32
run "checksum" 1M 64M random "" "" crc32
if command -v taskset > /dev/null && [ "$(nproc)" -ge 4 ]; then
    run "pinned: separate cores" 4K 64M random "taskset -c 2" "taskset -c 3"
    run "pinned: separate cores" 64 64M random "taskset -c 2" "taskset -c 3"
    run "pinned: same core" 4K 64M random "taskset -c 2" "taskset -c 2"
fi
