# dmrng API

Declared in `include/dmrng.h`; all functions use DMOD dynamic API imports.

| API | Result |
|-----|--------|
| `dmrng_create(config)` | Context or NULL on invalid config, duplicate instance, allocation/clock failure |
| `dmrng_destroy(context)` | 0, negative port error, or -EBUSY while device files are open; NULL is a no-op |
| `dmrng_is_valid(context)` | Whether this is the current context with the correct magic |
| `dmrng_read(context, buffer, size)` | 0 only if the entire requested output was filled; negative errno otherwise |
| `dmrng_fill(buffer, size)` | Same read via the configured singleton; -ENODEV if none exists |

`dmrng_config_t` contains `instance` (only 0) and `timeout_ms` (1..1000,
default 100). NULL configuration selects both defaults. Zero-byte reads allow
a NULL buffer but still require a configured, valid instance. A NULL buffer
with a nonzero size is invalid. Arbitrary alignment and byte counts are
supported; unused bytes of a hardware word are discarded.

Call from task context only. All read calls are serialized. Destroy/create
must be externally serialized against direct context users. The singleton
API takes the registry lock so `fill` cannot race with destruction.

Output is cleared on a failed read. Seed/clock health flags and repeated
words return -EIO; missing data returns -ETIMEDOUT. Both latch the hardware
port into failure until destroy/recreate. Invalid arguments return -EINVAL;
unsupported/missing instances return -ENODEV. No output from a failed request
should ever be passed to a TLS entropy collector.

## TLS callback adapter

A TLS integration can call `dmrng_fill(output, requested_size)` and report
success plus the produced length only when it returns zero. Translate a
negative result to that TLS library's entropy-source error, and report zero
produced bytes. The TLS library owns its DRBG and entropy mixing policy;
this driver only supplies hardware samples, without implementing a DRBG.

## Device interface

A board INI creates `/dev/rng` and a numbered device (`dmrng0`). Read-only
opens are supported, including multiple readers. Writes and read/write opens
are rejected. Reads return a full byte count or a negative error through
DMDRVI 2.0; the VFS/SAL wrapper exposes failures using its usual conventions.
Offsets have no meaning for this stream. No device-specific ioctls exist.

The port API in `dmrng_port.h` is serialized by the core: `_acquire(instance)`,
`_release(instance)` and `_read_word(instance, timeout_ms, word)`.
