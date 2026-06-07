# Apollo Code Review & Fixes — 2026-06-07

Triggered by a host BSOD investigation that surfaced `Sunshine.exe` repeatedly
"crashing" with Windows exception `0x80000003` (STATUS_BREAKPOINT). A full
multi-agent review (4 reviewers: shutdown/lifetime, virtual-display/SudoVDA,
session-regressions, general stability) followed, then targeted fixes with an
adversarial lock-verification gate before building.

> **Note:** The host BSOD itself (`0xEF CRITICAL_PROCESS_DIED` + a history of
> hangs and `0x7A` inpage errors) was traced to **RAM instability** (DDR5-6400
> 2×48 GB above the AM5 stable spec, fix = run at 6000) — it is **unrelated to
> Apollo**. Apollo's `0x80000003` was a separate software issue, fixed below.

---

## Root cause of the `0x80000003` "crash"

It was **not a crash** — it was the force-shutdown watchdog. Chain of events
(confirmed in `config/sunshine.log.backup`):

1. `init_tray()` (`src/system_tray.cpp`) busy-waited `while (GetShellWindow() == nullptr) Sleep(1000)` **uninterruptibly**.
2. Right after a boot / service-start the shell isn't ready for tens of seconds, so the tray worker stayed stuck in that loop.
3. The main thread then blocked in `init_tray_threaded()` / `end_tray_threaded()`'s `join()` (the worker never checked the exit flag), so it could **not** observe the shutdown event.
4. The 10-second force-shutdown watchdog (the SIGINT/SIGTERM handler in `src/main.cpp`) fired and called `lifetime::debug_trap()` → `DebugBreak()` + `abort()` = the `0x80000003` reported as an application crash.

---

## Fixes shipped

### `ef2b856` — shutdown path (the crash)
- **`system_tray.cpp`** — the shell-wait loop now checks `tray_thread_should_exit` and polls every 250 ms, so it can no longer block the main thread's `join()` during shutdown.
- **`main.cpp`** — the watchdog now `std::_Exit(1)` (clean forced exit) instead of `lifetime::debug_trap()`, so a genuine hang no longer surfaces as a crash/WER report.
- **`main.cpp`** — the duplicate SIGINT/SIGTERM handlers (the SIGTERM one was a byte-identical copy added in a prior pass, with a double-`terminate()` hazard) are collapsed into one handler guarded by `std::atomic<bool>` so the shutdown body runs exactly once.

### `6fd8927` — stability + driver guard
- **`thread_safe.h`** — `event_t`/`queue_t::_continue` is now `std::atomic_bool` (`running()` read it without holding the lock); `mail_raw_t::cleanup()` now erases **all** expired entries instead of returning after the first (was a slow `id_to_post` leak).
- **`src_assets/windows/drivers/sudovda/install.bat`** — added a `/force` downgrade guard so an accidental re-run can't downgrade a newer installed SudoVDA driver into a permanent `STATUS_FAILED_DRIVER_ENTRY` / `0xC0000365`.

### Operational (not code)
- Removed **10 orphaned `DISPLAY\SMKD1CE` virtual monitors** (leaked from prior abnormal exits).
- Enabled **kernel crash dumps** (`CrashControl\CrashDumpEnabled = 7`) for future diagnosis.

---

## Attempted, then REVERTED — naive concurrency mutexes

A first attempt added a `std::recursive_mutex` to `proc_t` (execute/terminate/
running/pause/resume) and a mutex around the `update_tray_*` writers. A 3-agent
adversarial verification (deadlock / build / regression lenses) **caught that
this is unsafe**, so it was reverted:

1. **Won't compile + UB.** `proc_t` uses `KITTY_DEFAULT_CONSTR_MOVE_THROW` (defaulted move ops). A `std::recursive_mutex` *member* is non-movable → deletes the move ops → `proc::refresh()`'s `proc = std::move(...)` fails to compile. Worse, `terminate()` calls `refresh()` **while holding the lock**, which would move-assign over the very mutex being held (move-over-locked-mutex UB).
2. **Windows message-pump deadlock.** `tray_update()` issues a blocking cross-thread `SendMessage` to the tray worker. Holding *any* lock across it deadlocks with the worker's menu callback (`Force stop` → `terminate()` → `update_tray_stopped()`), which takes the same lock. The GUI message pump is a hidden lock-order participant the two-mutex invariant doesn't cover.
3. **Live-stream freeze.** Locking `running()` (polled every control-loop iteration) would block the whole control-stream loop for the duration of a concurrent `terminate()` (up to `exit_timeout` + a blocking undo-command `child.wait()`).

**Lesson:** the proc_t/tray concurrency cannot be fixed with a simple mutex
bolt-on. The correct fix needs a namespace-static, fine-grained proc lock with
all `update_tray_*` calls moved *outside* the lock, tray updates marshalled to
the worker via `PostMessage`/`SendMessageTimeout`, and `running()` state made
atomic. That is a dedicated, separately-verified effort.

---

## Deferred (next dedicated pass)

| Item | Why deferred |
|------|--------------|
| `proc_t` double-`terminate()` serialization | needs the namespace-static fine-grained lock design above + a real stream test |
| `update_tray_*` data race | needs a command-queue / `PostMessage` redesign (blocking `SendMessage` under a lock deadlocks) |
| Packet `channel_data` use-after-free (`stream.cpp`) | hot capture→encode→send path; needs session-lifetime work (drain queue / shared_ptr); live-session registry isn't reachable from the broadcast thread |
| SudoVDA driver-version drift | bundled `1.10.9` would downgrade the working `11.46.7`; `install.bat /force` guard added, full reconciliation pending |

The boot-time `Kernel-PnP/219 … WUDFRd … 0xC0000365` is a **benign** transient
cold-start race (the device is `OK` after boot) — no action.

---

## Build & install

```
cmake --build build --target sunshine -j 16        # MinGW/UCRT64 (C:\msys64\ucrt64\bin)
powershell -File build\install-local.ps1           # admin: stop service, swap binary, restart
```
