# Code Review Fixes — 2026-06-03

Fresh adversarial pass over the fork's custom delta vs upstream (`a40b179..HEAD`),
cross-checked against `SECURITY_CHANGELOG.md` and `CODE_REVIEW_FIXES_2026-05-01.md`
so nothing already-fixed was touched. The prior 22-commit audit train holds up
well; these are **regressions/gaps inside the defensive code the audit added**.

## Fixed

### HIGH

**1. Per-IP login lockout was dead code** — `src/confighttp.cpp`
The prune loop keyed off `lockout_until`, which stays at the steady_clock epoch
for any entry below `MAX_LOGIN_ATTEMPTS`. On any host up >5 min, every
sub-threshold attempt entry was erased on the next request, so the counter never
reached 5 and the 5-attempt lockout never engaged — the web-UI password endpoint
was open to unlimited online brute force.
*Fix:* added a `last_attempt` timestamp to the attempt record (new
`login_attempt_t` struct) and prune on idle-since-last-activity, not on
`lockout_until`. The "don't extend lockout on every failure" property is kept.

**2. Display restore was not crash-safe — black-screen path** — `src/main.cpp`
The `display_topology_snapshot` RAII guard restores physical displays on normal
exit, exception unwind, and SIGINT (which calls `proc.terminate()`). But **SIGTERM
never called `terminate()`**, and the 10s force-shutdown watchdog funnels through
`lifetime::debug_trap()` → `abort()`, which does **not** run static destructors —
so an abnormal/forced shutdown mid-stream could leave physical monitors
deactivated (black screen, needs Win+P / registry recovery).
*Fix:* (a) added `proc::proc.terminate()` to the SIGTERM handler to match SIGINT;
(b) added an explicit `VDISPLAY::topology_snapshot_slot().reset()` in both
force-shutdown watchdog tasks (belt-and-suspenders before the hard abort).
*Honest scope:* raw `abort()` from a crypto RNG failure and `TerminateProcess`/
SIGKILL still cannot run the restore — the previous changelog's claim that the
static dtor "runs on `std::abort`" is incorrect and should not be relied on.

**3. Adaptive streaming suite shipped ON by default** — `src/config.cpp`
`adaptive_bitrate`, `adaptive_fec`, `frame_pacing`, and `thermal_protection`
defaulted `true`. `frame_pacing` injects up to `max_pacing_buffer_ms` (4 ms) of
`sleep_for` on the video **send** thread per frame, and the NVENC bitrate
reconfigure path is intentionally hard-refused — so the suite added latency /
silent resolution step-downs by default while the docs treated it as inert.
*Fix:* defaulted the four adaptive toggles **off** (opt-in via `sunshine.conf` —
they are **not** exposed in the web UI yet, only `pin_required` is).
`smart_reconnect` stays on (separate gated feature).

### MEDIUM

**4. Audit-log line injection** — `src/nvhttp.cpp`
The "tamper-evident" skip-PIN audit log wrote client-controlled `client.name` and
`client.uniqueID` unsanitized; a `\n` in the device name could forge/overwrite
AUTO_PAIR records. *Fix:* added `audit_sanitize()` (replaces control chars incl.
CR/LF) applied to both fields.

**5. Unaligned reads of ENet control payloads** — `src/stream.cpp`
`IDX_LOSS_STATS` and `IDX_INVALIDATE_REF_FRAMES` reinterpret-cast the unaligned
payload to `int32_t*`/`int64_t*` — UB on x86/x64, faults on ARM — the exact issue
the same commit fixed for `IDX_INPUT_DATA` via `memcpy`. *Fix:* `std::memcpy` the
values out (matching the sibling handler).

### LOW

- **getOTP missing CSRF** — `src/confighttp.cpp`: `/api/otp` is a state-changing
  POST but its guard was the only one missing `verifyCsrf`. *Fix:* added it.
- **WiFi preemptive clamp could latch forever** — `src/bitrate_controller.h`:
  `_wifi_preemptive_active` only cleared at quality ≥3, so a link stable at a
  mediocre tier stayed clamped to 60% indefinitely. *Fix:* also recover when
  quality stops dropping (stable/improving), letting the loss-driven path take over.
- **`wifi_quality_signaling` was a no-op** — `src/stream.cpp`: the IDX_WIFI_QUALITY
  handler ignored the config toggle. *Fix:* return early when disabled.
- **Stale `hash_version` comment** — `src/config.h`: updated to note v3 =
  PBKDF2-HMAC-SHA256 600k.

## Deliberately NOT changed (residuals — owner decision)

- **`max_suspended_sessions` not enforced** — `src/stream.cpp` suspend branch. The
  knob is parsed/validated but the disconnect handler suspends without a cap.
  Enforcing it means counting suspended sessions inside a concurrency-sensitive
  handler; left for a build-verified change. Low risk in single-user streaming.
- **`queue_t::raise()` overflow policy** (`clear()` → `pop_front()`) —
  `src/thread_safe.h`. `pop_front` (drop-oldest, bounded) is a defensible policy;
  reverting a shared concurrency primitive blind to all consumers is riskier than
  the finding. Flagged for the owner to confirm intent.
- **Private-key temp-file permissions** (`src/httpcommon.cpp`/`file_handler.cpp`).
  The chmod-after-write targets the wrong temp name on POSIX, but on this fork's
  Windows deployment `fs::permissions` owner-only is largely a no-op and the
  atomic-write path was just hardened — not worth a risky unverified change.

## Verification

- Symbols confirmed (`config::stream.wifi_quality_signaling`,
  `VDISPLAY::topology_snapshot_slot()`, `proc::proc.terminate()` all resolve).
- Incremental MinGW/UCRT64 build of the `sunshine` target links clean.

---

## Optimization pass (same day, second review)

A second review with a performance / build / robustness lens (the hot path was
already well-optimized by the prior 16 perf fixes — these are the remaining wins):

1. **Strip the Release binary** — `cmake/targets/windows.cmake`. The build
   shipped ~58 MB with full DWARF + a symbol table mapping the auth/crypto code;
   added `-s` for `$<CONFIG:Release>`.
2. **Safe-by-default struct defaults** — `src/config.h` + `src/bitrate_controller.h`.
   The in-class `stream_t`/`config_t` defaults still declared the adaptive suite
   `= true`; the 06-03 fix only changed the global aggregate initializer. Flipped
   the in-class defaults to `false` so the type can't silently re-enable adaptive
   from any other construction path.
3. **Per-frame lock fast-path** — `src/bitrate_controller.h`. `get_pacing_buffer_us`,
   `get_thermal_resolution`, `get_thermal_fps`, and `record_frame_interval` took
   the `recursive_mutex` every frame even with the feature off. Added a pre-lock
   read of the init-once `_cfg` flag (race-free: `_cfg` is written only in
   `init()` before the video thread starts) so disabled features skip the lock.
4. **`-march=x86-64-v2` floor** — `cmake/compile_definitions/common.cmake`
   (x86-64 GCC/Clang only). Lets GCC vectorize the FEC/Reed-Solomon path with
   SSE4.2/POPCNT. Not `-march=native` — binary stays portable.
5. **Doc correction** — the adaptive suite is opt-in via `sunshine.conf`, not the
   web UI (only `pin_required` is surfaced in the UI). Wording fixed above.
6. **Adaptive min/max cross-validation** — `src/config.cpp`. Independent range
   checks couldn't catch an inverted `min_bitrate > max_bitrate` (or FEC) pair;
   normalize with `std::swap` after parsing.

Not done (would need a build-verified, larger change): surfacing the ~20 adaptive
knobs in the Vue web UI, and wiring the NVENC reconfigure queue so the thermal /
adaptive-bitrate control surface actually reaches the encoder (both deferred).

Verified: incremental MinGW build of `sunshine` links clean after this pass.
