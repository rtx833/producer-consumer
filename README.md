# Producer / Consumer over POSIX shared memory

Two independent command-line programs that stream data packets from one
process to another through a lock-free ring buffer in POSIX shared memory.

* **`producer`** builds packets of a payload size given on the command line
  (random bytes or a byte sequence), stamps each with a sequence number, a
  wall-clock timestamp and a checksum (CPU-accelerated CRC-32C, or CRC-32),
  and publishes it into the ring.
* **`consumer`** attaches to the ring, verifies every packet (structure,
  checksum, sequence continuity, timestamp sanity) and prints statistics once
  per second: totals, packets/s, bytes/s, end-to-end latency, ring fill level.

Both can be paused and resumed at any time with a key press or a signal.

The code is C++20, built with CMake, and depends only on the C++ standard
library and POSIX (`shm_open`, `mmap`, `sigwait`, `termios`). No third-party
libraries are used; the inter-process transport is implemented from scratch.

## Build and test

Requirements: a POSIX system, CMake ≥ 3.16, and a compiler with C++20 and
`<format>` support (GCC ≥ 13, or Clang ≥ 17 with libc++ ≥ 17 / libstdc++ ≥ 13).

The `build.sh` wrapper configures and builds with CMake:

```sh
./build.sh all                    # release build of both applications and the tests
./build.sh -d producer consumer   # debug build of the applications only
./build.sh --help                 # targets: producer | consumer | tests | all; -r/--release (default), -d/--debug
ctest --test-dir build/release --output-on-failure   # unit tests + end-to-end test
```

Release builds land in `build/release`, debug builds in `build/debug`. The
equivalent plain CMake invocation is:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j
```

CMake options: `-DPC_BUILD_TESTS=OFF` skips the tests, `-DPC_SANITIZE=ON`
builds with AddressSanitizer and UndefinedBehaviorSanitizer (the whole suite,
including the end-to-end test, passes under both).

Binaries: `build/release/producer/producer` and `build/release/consumer/consumer`.

## Usage

Run the two programs in two terminals; the order does not matter. The
consumer waits for a producer to appear, and keeps waiting for the next one
after a producer exits.

```sh
# terminal 1
./build/release/consumer/consumer

# terminal 2: 4 KiB payload per packet, as fast as possible
./build/release/producer/producer 4K
```

```
$ ./build/release/producer/producer 4K
[21:15:30] Keyboard: any key = pause/resume, q = quit
[21:15:30] Checksum: crc32c (hardware), payload generator: random
[21:15:30] Transport: shm:/pc-ring (64.00 MiB ring)
[21:15:30] Packet: 32 B header + 4096 B payload = 4128 B
[21:15:31] sent 519,324 pkt (2.00 GiB) | 519,323 pkt/s 2.00 GiB/s | ring   0% | RUNNING
...
[21:16:00] done: 16,344,717 packets, 62.84 GiB in 29.90 s (avg 546,657 pkt/s, 2.10 GiB/s)
```

```
$ ./build/release/consumer/consumer --once
[21:15:30] Keyboard: any key = pause/resume, q = quit
[21:15:30] Checksums: crc32 (software), crc32c (hardware)
[21:15:30] Pause policy 'block': packets queue up in the ring; the producer blocks when it is full; nothing is lost
[21:15:30] Waiting for a producer on shm:/pc-ring ...
[21:15:30] Connected to shm:/pc-ring (64.00 MiB ring, producer pid 76824)
[21:15:31] rx 517,447 pkt (1.99 GiB) | 517,445 pkt/s 1.99 GiB/s | latency avg 1.9 us max 695.7 us | ring   0% | RUNNING
[21:15:32] rx 1,064,459 pkt (4.09 GiB) | 547,011 pkt/s 2.10 GiB/s | latency avg 1.2 us max 123.6 us | ring   0% | RUNNING
...
[21:16:00] Producer finished the stream
[21:16:00] session summary: received 16,344,717 packets (62.84 GiB) in 29.97 s, avg 545,386 pkt/s 2.10 GiB/s
[21:16:00]   valid 16,344,717 | corrupted 0 | sequence gaps 0 (missing 0) | rewinds 0 | timestamp anomalies 0 | dropped while paused 0 (0 B)
```

### producer

```
Usage: producer [options] [payload-size]

Generates packets (header + payload) and streams them to the consumer through
a shared-memory ring buffer. Any key toggles pause; 'q' or Ctrl-C quits.
Signals: SIGUSR1 pause, SIGUSR2 resume, SIGINT/SIGTERM stop.

Arguments:
  payload-size               Payload size in bytes (same as --payload-size)

Options:
  -h, --help                 Show this help and exit
  -s, --payload-size <bytes> Payload size per packet, e.g. 1024 or 4K
  -n, --shm-name <name>      POSIX shared memory object name (default: /pc-ring)
  -r, --ring-size <bytes>    Ring buffer capacity (rounded up to a power of two) (default: 64M)
  -g, --generator <kind>     Payload generator: random | sequential (default: random)
  -k, --checksum <algo>      Checksum: crc32c (CPU-accelerated where available) | crc32 (default: crc32c)
      --seed <n>             Seed for the random generator (default: 1)
      --rate <pps>           Packets per second, 0 = as fast as possible (default: 0)
  -c, --count <n>            Stop after this many packets, 0 = unlimited (default: 0)
      --report-interval <ms> Statistics report period in milliseconds (default: 1000)
  -w, --wait-for-consumer    Do not start until a consumer attaches
      --no-keyboard          Do not read keys from the terminal
```

### consumer

```
Usage: consumer [options]

Receives packets from the producer through a shared-memory ring buffer, verifies
their metadata and checksum and prints statistics once per report interval.
Any key toggles pause; 'q' or Ctrl-C quits.
Signals: SIGUSR1 pause, SIGUSR2 resume, SIGINT/SIGTERM stop.

Options:
  -h, --help                 Show this help and exit
  -n, --shm-name <name>      POSIX shared memory object name (default: /pc-ring)
  -p, --pause-policy <policy> What happens while paused: block (producer blocks, nothing lost) | discard (drop packets) (default: block)
      --report-interval <ms> Statistics report period in milliseconds (default: 1000)
      --log-defects <n>      Print details for at most n defective packets per session (default: 10)
  -1, --once                 Exit when the producer finishes instead of waiting for the next one
      --no-keyboard          Do not read keys from the terminal
```

### Pause, resume, stop

| Action | Keyboard (interactive terminal) | Signal |
|--------|----------------------------------|--------|
| Pause  | any key                          | `SIGUSR1` (`pkill -USR1 producer`) |
| Resume | any key                          | `SIGUSR2` (`pkill -USR2 consumer`) |
| Stop   | `q` or Ctrl-C                    | `SIGINT`, `SIGTERM`, `SIGHUP` |

Keys work when standard input is a terminal owned by the process' foreground
process group; the terminal is switched to non-canonical mode (no Enter
needed, no echo) and restored on exit. Otherwise, or with `--no-keyboard`,
only signals are used. Both mechanisms are always active together.

## Design

### Transport: shared memory ring buffer

The requirement was the most efficient way to pass packets between two
processes. Every kernel-mediated channel (pipes, FIFOs, UNIX sockets, message
queues) costs at least one system call and one copy through kernel memory per
packet. Shared memory is the only POSIX mechanism with neither: after set-up,
moving a packet is a plain `memcpy` into memory the other process already
maps, plus one atomic store to publish it. Both processes stay entirely in
user space on the hot path.

`pc::MirroredShmRegion` (`common/include/pc/shm_region.hpp`) maps a
`shm_open` object as one control page followed by the data area **mapped
twice, back to back**, in a reserved virtual range:

```
 virtual:  [ control page ][ data: 0 .. N-1 ][ data: 0 .. N-1 ]
 physical:                  \___ same pages ___/
```

A record that starts near the end of the data area continues seamlessly at
the beginning, so records never need to be split or copied through a staging
buffer. Combined with reserve/commit and peek/release APIs this makes the
transport **zero-copy on both sides**: the producer generates the payload
and computes the checksum directly in shared memory, and the consumer
validates the packet where it lies.

`pc::RingProducer` / `pc::RingConsumer` (`ring_buffer.hpp`) implement a
single-producer single-consumer queue of `[uint32 length][bytes]` records on
top of that mapping:

* `head` (bytes published) is written only by the producer, `tail` (bytes
  released) only by the consumer. Each lives on its own cache line to avoid
  false sharing, and each side caches the other's counter and re-reads it
  only when the ring looks full/empty.
* Publishing uses release semantics, reading uses acquire semantics; no
  locks, no system calls, no contention.
* Waiting (ring full or empty) uses a progressive backoff: spin with
  `pause`, then `sched_yield`, then sleep up to 1 ms. This keeps latency in
  the microseconds while both sides are busy and CPU usage near zero while
  one side is idle or paused.
* Waits are bounded by a timeout and a cooperative `std::stop_token`, so both
  loops keep reporting and react to stop/pause requests even when blocked.

Using shared memory instead of a stream also makes the pause behaviour
well-defined: the ring **is** the buffer between the processes (see below).

### Packet format

Every record in the ring is a fixed 32-byte header followed by the payload:

| Offset | Size | Field          | Content |
|-------:|-----:|----------------|---------|
| 0      | 4    | `magic`        | `"PCKT"` |
| 4      | 1    | `version`      | 2 |
| 5      | 1    | `checksum_kind`| 1 = CRC-32 (IEEE), 2 = CRC-32C (Castagnoli) |
| 6      | 2    | `header_size`  | 32 |
| 8      | 4    | `payload_size` | bytes of payload that follow the header |
| 12     | 4    | `checksum`     | CRC over the whole packet with this field zeroed, algorithm per `checksum_kind` |
| 16     | 8    | `sequence`     | 0, 1, 2, ... per producer run |
| 24     | 8    | `timestamp_ns` | `CLOCK_REALTIME` nanoseconds at creation |

The consumer checks all of it: magic, version and header size (structure),
`payload_size` against the record length carried by the ring (size), the
CRC (integrity), the sequence number against the expected one (gaps and
rewinds are counted separately, with the number of missing packets), and the
timestamp (not in the future beyond 1 s, not earlier than the previous
packet). Packets failing a structural or checksum check are counted as
corrupted; the remaining checks are anomalies and do not affect the count of
received packets. The timestamp also yields the end-to-end latency reported
each interval.

Two checksum algorithms are available (`--checksum` on the producer; the
consumer reads the choice from each header through `ChecksumRegistry`):

* **CRC-32C** (default) uses the CPU's CRC instructions when they are present
  at run time (SSE4.2 on x86, the CRC extension on ARMv8) and falls back to a
  table otherwise. Both paths give identical results, which the unit test
  verifies against a bit-by-bit reference. The hardware path runs at about
  6 GiB/s per core on the benchmark machine.
* **CRC-32** (IEEE) is the classic polynomial, computed with the slice-by-8
  table technique at about 1.6 GiB/s per core.

The checksum is the dominant per-byte cost on both sides, not the transport;
see Measurements.

### Lifecycle

* The producer owns the shared memory object: it creates it (replacing a
  stale one left by a crash), marks the stream finished when it exits and
  unlinks the name. A consumer that is still attached keeps its mapping and
  drains the remaining packets before noticing the end of stream.
* The consumer attaches to an existing, initialised ring whose producer
  process is alive, and otherwise polls every 100 ms. When the producer
  finishes or dies, it drains the ring, prints a session summary and goes
  back to waiting, so a producer can be restarted at will (`--once` exits
  instead).
* Only one producer and one consumer may use a ring (it is an SPSC queue).
  A second producer or consumer for the same name is refused with a clear
  error while the first is alive; slots left by a crashed process are
  reclaimed automatically (liveness is checked with `kill(pid, 0)`).
* `--wait-for-consumer` makes the producer hold off until a consumer is
  attached; it is used by the end-to-end test and for benchmarking.

### Pause semantics

**Producer.** Pausing stops packet creation at a packet boundary. Sequence
numbers continue where they left off, so the consumer sees a gap-free stream;
timestamps reflect the real creation time.

**Consumer.** The behaviour while paused is selectable with `--pause-policy`
(`consumer/pause_policy.hpp`):

* **`block`** (default): the consumer stops reading. Packets accumulate in
  the ring; when it is full the producer's reserve call blocks (it keeps
  printing its statistics with `ring 100%`). After resume the consumer drains
  the backlog and continues. **No packet is ever lost**; the pause is pure
  back-pressure. The latency figures right after a resume show how long the
  backlog waited.
* **`discard`**: the consumer keeps reading and throws every packet away
  while paused, so the producer never blocks. Discarded packets and bytes are
  counted (`dropped while paused`). The sequence baseline is re-established
  on resume, so the intentional gap is not reported as a defect.

`block` was chosen as the default because the task is a reliable transfer:
with a bounded ring the memory cost of a pause is fixed, and the producer
being able to observe back-pressure is a feature rather than a problem.
`discard` exists for the real-time use case where fresh data matters more
than complete data.

### Statistics

Both programs print one line per report interval (default 1 s; the rates use
the real elapsed time, not the nominal period). The consumer shows the total
packet and byte count, packets/s and bytes/s for the last interval, average
and maximum latency over the interval, ring fill level, and the running
counts of corrupted packets, missing packets, sequence rewinds, timestamp
anomalies and packets dropped while paused. The first few defective packets
of a session are also printed individually. A summary is printed at the end
of every session.

## Architecture

The code is organised as a small library shared by both programs plus one
directory per application. Each application's `main.cpp` is the composition
root: it parses options, instantiates concrete classes and hands them to the
application object through interfaces.

```
common/include/pc/       shared library "pc_common"
  checksum.hpp           IChecksum, Crc32, Crc32c (hardware/software), ChecksumRegistry
  packet.hpp             PacketHeader, seal_packet / read_header / packet_checksum
  shm_region.hpp         MirroredShmRegion: POSIX shm object with a mirrored mapping
  ring_buffer.hpp        RingControl, RingProducer, RingConsumer (SPSC queue)
  transport.hpp          IPacketSink, IPacketSource (what the apps depend on)
  shm_transport.hpp      ShmPacketSink, ShmPacketSource (the shm implementation)
  control.hpp            RunControl (stop/pause state), IControlSource
  signal_source.hpp      SignalControlSource (sigwait thread)
  keyboard_source.hpp    RawTerminal, KeyboardControlSource (poll/read thread)
  backoff.hpp            Backoff (spin / yield / sleep)
  stats.hpp              RateWindow, RateSample, PeriodicTimer
  cli.hpp                CommandLine (declarative option parser), UsageError
  byte_size.hpp          parsing and formatting helpers
producer/
  payload_generator.hpp  IPayloadGenerator, RandomPayloadGenerator, SequentialPayloadGenerator
  packet_builder.hpp     PacketBuilder (payload + header + checksum, in place)
  rate_limiter.hpp       RateLimiter (--rate)
  producer_reporter.hpp  IProducerReporter, ConsoleProducerReporter
  producer_app.hpp       ProducerApp (the main loop)
  producer_options.hpp   command-line -> ProducerOptions
  main.cpp               composition root
consumer/
  packet_validator.hpp   IPacketValidator, PacketValidator, ValidationResult
  pause_policy.hpp       IPausePolicy, BlockingPausePolicy, DiscardingPausePolicy
  consumer_stats.hpp     ConsumerStats, ConsumerSnapshot
  consumer_reporter.hpp  IConsumerReporter, ConsoleConsumerReporter
  consumer_app.hpp       ConsumerApp (the main loop)
  consumer_options.hpp   command-line -> ConsumerOptions
  main.cpp               composition root
tests/                   unit tests (CRC, ring buffer, validator, CLI) and integration.sh
```

## Measurements

Release build, both processes on one machine: Intel Core i7-11850H (8 cores /
16 threads, 2.5 GHz, 24 MiB L3), 32 GiB RAM, Linux 6.14, GCC 13.3. 30 s runs,
64 MiB ring, random payload; reproducible with `scripts/bench.sh`. Throughput
counts header + payload bytes.

### CRC-32C, hardware (`--checksum crc32c`, the default)

| Payload | Packets/s | Throughput | Latency avg / max | Packets in 30 s |
|--------:|----------:|-----------:|------------------:|----------------:|
| 16 B    | 4,398,000 | 201 MiB/s  | 33 µs / 6.0 ms    | 131,501,693 |
| 64 B    | 3,775,000 | 346 MiB/s  | 2.3 µs / 712 µs   | 112,874,913 |
| 256 B   | 3,097,000 | 851 MiB/s  | 0.9 µs / 1.0 ms   | 92,586,235 |
| 1 KiB   | 1,308,000 | 1.29 GiB/s | 0.8 µs / 1.1 ms   | 39,095,090 |
| 4 KiB   |   547,000 | 2.10 GiB/s | 1.2 µs / 753 µs   | 16,344,717 |
| 16 KiB  |   156,000 | 2.39 GiB/s | 6.0 µs / 768 µs   | 4,672,948 |
| 64 KiB  |    41,400 | 2.53 GiB/s | 59 µs / 856 µs    | 1,236,757 |
| 256 KiB |    10,500 | 2.57 GiB/s | 89 µs / 774 µs    | 315,238 |
| 512 KiB |     5,280 | 2.58 GiB/s | 130 µs / 970 µs   | 157,898 |
| 1 MiB   |     2,570 | 2.51 GiB/s | 243 µs / 1.2 ms   | 76,979 |
| 2 MiB   |     1,280 | 2.50 GiB/s | 473 µs / 1.5 ms   | 38,318 |
| 4 MiB   |       633 | 2.48 GiB/s | 940 µs / 2.0 ms   | 18,955 |

### CRC-32, software slice-by-8 (`--checksum crc32`)

| Payload | Packets/s | Throughput | Latency avg / max | Packets in 30 s |
|--------:|----------:|-----------:|------------------:|----------------:|
| 16 B    | 3,415,000 | 156 MiB/s  | 7.3 µs / 1.9 ms   | 102,124,249 |
| 64 B    | 3,690,000 | 338 MiB/s  | 22 µs / 6.0 ms    | 110,323,450 |
| 256 B   | 2,238,000 | 615 MiB/s  | 0.9 µs / 625 µs   | 66,900,226 |
| 1 KiB   |   851,000 | 857 MiB/s  | 1.6 µs / 848 µs   | 25,450,617 |
| 4 KiB   |   262,000 | 1.01 GiB/s | 3.2 µs / 1.1 ms   | 7,827,080 |
| 16 KiB  |    74,500 | 1.14 GiB/s | 11 µs / 3.0 ms    | 2,226,111 |
| 64 KiB  |    19,500 | 1.19 GiB/s | 86 µs / 824 µs    | 583,577 |
| 256 KiB |     4,920 | 1.20 GiB/s | 198 µs / 2.0 ms   | 147,119 |
| 512 KiB |     2,460 | 1.20 GiB/s | 347 µs / 1.0 ms   | 73,645 |
| 1 MiB   |     1,210 | 1.18 GiB/s | 684 µs / 3.3 ms   | 36,139 |
| 2 MiB   |       595 | 1.16 GiB/s | 1.4 ms / 4.4 ms   | 17,813 |
| 4 MiB   |       295 | 1.15 GiB/s | 2.7 ms / 5.9 ms   | 8,830 |

Every run ended with all packets valid: 0 corrupted, 0 missing, 0 rewinds,
0 timestamp anomalies. Latency is measured from the producer's timestamp to
the moment the consumer finished validating the packet, so for large packets
it includes generating, hashing and verifying the payload.

Small packets are bound by per-packet overhead (about 200 ns across both
processes), so the checksum makes little difference there. Large packets are
bound by per-byte work: with the software CRC-32 (1.6 GiB/s per core) the
stream plateaus at about 1.2 GiB/s; the CRC-32C instruction (6 GiB/s per
core) lifts that to about 2.5 GiB/s, where the producer becomes bound by
generating random payload bytes (the cheaper sequential generator reaches
3.5 GiB/s). The ring stays nearly empty (`ring 0%`) because the consumer
keeps up. Ring size has little effect; pinning both processes to the same
CPU core roughly halves the throughput.

## Limitations and assumptions

* Both processes must run on the same host (that is what shared memory is);
  the packet layout uses native byte order and alignment on purpose.
* Exactly one producer and one consumer per ring name (SPSC). Multiple
  streams can run side by side with different `--shm-name` values.
* The ring capacity is rounded up to a power of two and must hold at least
  one packet; the largest packet is limited to the ring size minus 4 bytes.
* Timestamps use `CLOCK_REALTIME`; a clock step on the host shows up as a
  timestamp anomaly, which is reported but is not an error.
* If a process is killed with `SIGKILL` while reading the keyboard, the
  terminal stays in non-canonical mode until `reset` is run (orderly stops,
  including Ctrl-C, restore it).
* Developed and tested on Linux/x86-64. The code only uses POSIX interfaces
  (`shm_open`, `mmap` with `MAP_FIXED` into a reserved range, `sigwait`,
  `termios`, `poll`) and should build on other POSIX systems, but has not
  been run there; in particular the ARMv8 hardware CRC path compiles from
  the same source but was not exercised on ARM hardware.
