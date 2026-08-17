# Apollo security and reliability remediation plan

Status: source verified; commit and production deployment pending
Version: 2
Date: 2026-08-16

## Objective

Make the installed Apollo host safe for normal LAN TV streaming by closing the
confirmed client-certificate authentication bypass, removing identified
lifetime and persistence hazards, making configuration behavior truthful, and
shipping a tested, attributable binary with hardened runtime configuration.

## Non-goals

- No Artemis Android client changes.
- No protocol-incompatible redesign of GameStream pairing.
- No blind merge of the 14 upstream commits; only reviewed compatible changes.
- No implementation of live NVENC reconfiguration without an encoder-thread
  command path and hardware validation. Until then the incomplete controls fail
  closed instead of pretending to work.

## Current-state map and constraints

- Fork HEAD before remediation: `13cd891`; installed executable has the same
  SHA-256 as `build/sunshine.exe`.
- Apollo runs as `LocalSystem` and listens on all interfaces.
- The existing Windows Firewall rule allows all TCP and UDP ports, all profiles,
  and all remote addresses for `sunshine.exe`.
- The combined credentials/pairing state file is readable by ordinary users and
  contains a legacy v1 password hash.
- The working tree contains unrelated user changes and untracked review files;
  they must remain untouched and excluded from commits.
- The known Windows sandbox/toolchain ACL failure may prevent a fresh test build.
  Stop after two equivalent failed attempts and retain exact evidence.

## Milestones and acceptance

1. Authentication and local-secret hardening
   - Unknown-issuer client certificates fail closed.
   - A live untrusted-certificate request no longer reaches protected endpoints.
   - Successful legacy login upgrades credentials to PBKDF2 v3.
   - Private state/key writes are atomic and restricted to SYSTEM and
     Administrators on Windows.

2. Streaming/session correctness
   - Audio and video queued packets retain safe session ownership and stopped
     sessions are not transmitted.
   - Suspended sessions use their configured reconnect deadline before generic
     ping expiry and cannot exceed `max_suspended_sessions`.

3. Lifecycle and display recovery
   - Process mutations are serialized without placing a movable mutex in
     `proc_t`; non-mutating polling never blocks the stream loop behind teardown.
   - Tray state is mutated only on the tray worker and session-0 services skip
     tray initialization without a timeout error.
   - The pre-stream topology is persisted before display mutation and recovered
     on the next start after an ungraceful process death.

4. Truthful controls, dependencies, and identity
   - Unwired adaptive-bitrate and thermal settings are rejected and forced off;
     no fake encoder acknowledgement or telemetry is emitted.
   - `npm audit --omit=dev` reports no known production dependency advisories, or
     any remaining build-only advisory is explicitly documented with reachability.
   - The installed executable exposes a non-`0.0.0.dirty` version and exact commit.

5. Deployment hardening
   - LAN and WAN video encryption are mandatory in the live configuration.
   - Firewall access is restricted to exact Apollo ports, Private profile, and
     LocalSubnet.
   - Service restarts cleanly; HTTPS, pairing protection, listeners, logs,
     binary hash/version, state ACL, and a normal client connection are checked.

## Data migration and compatibility

- Credential migration occurs only after a correct legacy password is verified;
  it preserves pairing data in the same JSON object and uses one locked atomic
  read-modify-write transaction.
- The original configuration, state file, installed binary, and firewall rule
  definitions are backed up before deployment.
- Mandatory encryption requires a client supporting encrypted GameStream video;
  current Artemis clients are expected to support it. Roll back only the two
  encryption settings if the verified TV client cannot negotiate encryption.

## Security and failure modes

- Certificate verification, state parsing, migration, and ACL application fail
  closed and log a sanitized error.
- State writes publish a fully flushed temporary file and never expose a
  permissive temporary ACL.
- Queue ownership may extend a stopped session briefly, but consumers discard
  stopped-session packets and the bounded queue releases ownership promptly.
- Persisted display recovery data is size/version checked before use and removed
  after successful restoration.

## Deployment, rollback, and recovery

- Build and test before service changes.
- Back up `sunshine.exe`, `sunshine.conf`, `sunshine_state.json`, and exported
  firewall rules with a timestamped deployment receipt under `tools/results/`.
- Stop `ApolloService`, install the reviewed binary, harden configuration and
  firewall rules, then start and verify.
- Rollback restores the backed-up executable/config/state/firewall policy and
  restarts the service. Never delete the backups during this task.

## Decision log

- 2026-08-16: retain only the upstream certificate fix rather than merging all
  upstream commits into the customized fork.
- 2026-08-16: use shared packet ownership instead of queue draining based on raw
  pointers; this directly closes the lifetime gap with bounded overhead.
- 2026-08-16: marshal tray updates to its worker and serialize process mutations
  with namespace-static state; do not add a mutex member to movable `proc_t`.
- 2026-08-16: force incomplete encoder controls off rather than enabling the
  previously unreachable NVENC reconfiguration body without hardware proof.
- 2026-08-17: remove persistent fixed-PIN pairing rather than attempt to make a
  network-reachable `0000` bypass safe with warnings or UPnP checks.
- 2026-08-17: force smart reconnect off. A 32-bit connection token and source
  IP do not prove possession of the suspended session's AES key; re-enable only
  after a fresh-nonce challenge succeeds before peer replacement.
- 2026-08-17: disable GameStream TLS resumption so every connection reaches the
  application pairing/certificate verification path.
- 2026-08-17: use the configuration parser inventory as the web save allowlist,
  with tests proving that every UI control is writable and every allowed key is
  consumed.

## Source completion record

| Area | Verified result |
|------|-----------------|
| Certificates and TLS | Unknown issuer rejected; exact trusted certificate accepted; TLS >=1.2 with tickets/cache disabled |
| Pairing and setup | Fixed-PIN branch/UI removed; per-IP/global expiring pending-session bounds; first admin setup host-only |
| Credentials and state | PBKDF2 v3 verified-login migration; locked atomic state transactions; hardened key/state ACLs |
| Packet lifetime | Audio/video queued packets retain channel owners; stopped-session packets are dropped |
| Process and tray | Lifecycle serialization, atomic active-app state, worker-owned tray command queue, stale-task invalidation |
| Display recovery | Bounded private topology snapshot written before mutation and restored on startup after crash |
| Configuration | Complete parser-backed allowlist; invalid keys/newlines reject entire save; write failures return 500 |
| Incomplete controls | Adaptive bitrate, thermal reconfiguration, and smart reconnect forced off with explicit warnings/docs |
| Web supply chain | `npm audit --json`: 219 dependencies, 0 vulnerabilities at every severity |
| Build and tests | UCRT64 native build passed; Vite built 143 modules; 248 tests, 241 passed, 7 expected/environment skips, 0 failed |

The production service, installed binary hash/version, firewall, live
configuration, endpoint behavior, and TV client remain deployment acceptance
items. They are not represented as complete by this source record.

## Progress log

- 2026-08-16: source, installed binary, service, listeners, firewall,
  credentials, upstream divergence, tests, and dependency state reverified.
- 2026-08-17: all planned source changes implemented. Frontend and native
  builds pass; the full 248-test suite has 0 failures; dependency audit is
  clean. Commit/push and live deployment acceptance remain.
