# Apollo security and reliability remediation plan

Status: source committed and verified; local deployment rolled back and remains incomplete
Version: 3
Date: 2026-08-17

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

- Fork HEAD before remediation: `13cd891`. The verified remediation commit is
  `81f7fb13f272c03b4021ef4c962647a137cfaf25` and is published only to
  `Marssvoodoo/Apollo`.
- The exact-commit Release executable is version `1.1.1.81f7fb1` with SHA-256
  `94F269F007476F2F4F934665FFBAEDBEC4FDD800A621CDAAC73E0D9018064F5F`.
- The local installation was rolled back after a failed elevated transaction.
  It remains the old `0.0.0.dirty` executable with SHA-256
  `22908D0F01D290928B5052D61EBC714A07C66160980C4F6AFA4DA43B5D75A5FF`,
  and `ApolloService` is stopped.
- The existing Windows Firewall rule allows all TCP and UDP ports, all profiles,
  and all remote addresses for `sunshine.exe`.
- The failed transaction's rollback preserved the intended private ACL on the
  live config, credentials, state, certificate, key, and logs: only SYSTEM,
  Administrators, and the Owner account have access.
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
   - Firewall access is restricted to exact Apollo ports and LocalSubnet. The
     Public and Private profiles are included because the active trusted
     Ethernet connection is currently classified as Public; changing the
     machine-wide network classification is outside this deployment.
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

## Local deployment attempt and rollback

The 2026-08-17 local deployment did not pass acceptance and is not represented
as deployed:

- Preflight proved there was no running Apollo process or active stream. The
  service had been stopped by the system-tray quit action at 00:13 local time.
- A non-elevated attempt was denied while creating the Program Files backup and
  changed no installed files.
- The UAC-elevated transaction created a complete protected rollback set at
  `C:\Program Files\Apollo\backups\security-remediation-20260817-011545`,
  including the prior executable, 139 web assets, seven config files, and an
  exported firewall policy.
- The elevated transaction then failed before runtime verification. Windows
  Sudo is forced to a new window on this host, so the child error text was not
  captured. The transaction restored the original executable, assets, config
  contents, and two broad firewall rules. It intentionally did not weaken the
  newly restricted config ACLs.
- Final live evidence after rollback: service `Stopped`; installed executable
  SHA-256 `22908D0F01D290928B5052D61EBC714A07C66160980C4F6AFA4DA43B5D75A5FF`;
  version `0.0.0.dirty`; 139 installed web assets; `wan_encryption_mode = 0`;
  obsolete `nv_preset`, `nv_multipass`, and `nv_rc` keys still present; firewall
  TCP and UDP rules still allow any port, profile, and remote address.
- No third deployment attempt was made after the two failed attempts. Local
  endpoint, listener, encrypted TV-client, and installed-hash acceptance remain
  blocked until an elevated run captures the failing child step.

## Progress log

- 2026-08-16: source, installed binary, service, listeners, firewall,
  credentials, upstream divergence, tests, and dependency state reverified.
- 2026-08-17: all planned source changes implemented. Frontend and native
  builds pass; the full 248-test suite has 0 failures; dependency audit is
  clean. Commit `81f7fb13f272c03b4021ef4c962647a137cfaf25` was pushed only
  to `Marssvoodoo/Apollo`; the upstream repository was untouched.
- 2026-08-17: exact-commit Release build and full suite reverified (241 passed,
  7 environment/expected skips, 0 failed). The local deployment transaction
  failed and rolled back; the protected rollback set and hardened config ACLs
  were verified, while all remaining live deployment acceptance stays open.
