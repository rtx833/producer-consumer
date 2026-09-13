#!/usr/bin/env bash
# End-to-end test: runs a real producer and consumer through shared memory
# with the consumer paused via signals, and checks that back-pressure occurs
# (the ring fills up and the producer blocks) and that every packet still
# arrives intact and in order after the resume (block policy = no loss).
set -euo pipefail

PRODUCER=${PRODUCER:?path to producer binary}
CONSUMER=${CONSUMER:?path to consumer binary}
NAME="/pc-it-$$"
COUNT=200000
PAYLOAD=1000
WORK=$(mktemp -d)
CONSUMER_PID=""; PRODUCER_PID=""
trap 'kill $CONSUMER_PID $PRODUCER_PID 2>/dev/null || true; rm -rf "$WORK"' EXIT

fail() { echo "FAIL: $*"; exit 1; }

# No `timeout` wrappers: the pause signals must reach the consumer itself;
# CTest enforces the overall time limit.
"$CONSUMER" --shm-name "$NAME" --once --no-keyboard --report-interval 100 \
    > "$WORK/consumer.log" 2>&1 < /dev/null &
CONSUMER_PID=$!
sleep 0.2
kill -USR1 "$CONSUMER_PID" || fail "could not pause the consumer"

# The consumer is paused before any data flows. An 8 MiB ring with 200 MB
# of traffic forces the producer to fill the ring and block until the resume.
"$PRODUCER" "$PAYLOAD" --shm-name "$NAME" --ring-size 8M --count "$COUNT" \
    --wait-for-consumer --no-keyboard --report-interval 100 \
    > "$WORK/producer.log" 2>&1 < /dev/null &
PRODUCER_PID=$!
sleep 1.0
kill -USR2 "$CONSUMER_PID" || fail "could not resume the consumer"

wait "$PRODUCER_PID" || fail "producer exited with status $?"
wait "$CONSUMER_PID" || fail "consumer exited with status $?"
PRODUCER_PID=""; CONSUMER_PID=""

echo "--- producer.log"; cat "$WORK/producer.log"
echo "--- consumer.log"; cat "$WORK/consumer.log"

grep -q "PAUSED" "$WORK/consumer.log" || fail "consumer never paused"
grep -q "RESUMED" "$WORK/consumer.log" || fail "consumer never resumed"
grep -q "ring 100%" "$WORK/producer.log" || fail "producer never blocked on a full ring"
grep -q "done: $(printf "%'d" "$COUNT") packets" "$WORK/producer.log" \
    || fail "producer did not send $COUNT packets"
grep -q "received $(printf "%'d" "$COUNT") packets" "$WORK/consumer.log" \
    || fail "consumer did not receive $COUNT packets"
grep -q "valid $(printf "%'d" "$COUNT") | corrupted 0 | sequence gaps 0 (missing 0) | rewinds 0" "$WORK/consumer.log" \
    || fail "consumer reported defects"
grep -q "Producer finished the stream" "$WORK/consumer.log" || fail "end of stream not detected"

# The shared memory name must be gone after the producer exits.
[ -e "/dev/shm${NAME}" ] && fail "shared memory object not unlinked"
echo "PASS"
