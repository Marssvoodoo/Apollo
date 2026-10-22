# Apollo regression review

Version: 1
Status: code review fixes pushed, installed and verified; hardware acceptance remains

## Contract

Review the custom source delta from upstream merge `a40b179` through `c86d674`.
Fix demonstrated regressions with narrow changes, preserve existing pairing,
applications and display settings, commit and push only to Marssvoodoo/Apollo,
then install and verify the tested host if local permissions permit.
Artemis, the display driver, unrelated dirty files, network policy and encoder
feature development are outside this review.

## Observed starting state

- Local and fork master: `c86d674db6e5ee955021c1bf550a817f76f13999`.
- Installed executable SHA256:
  `22908D0F01D290928B5052D61EBC714A07C66160980C4F6AFA4DA43B5D75A5FF`.
  This matches the old executable recorded in the August rollback.
- ApolloService is running. The current shell is not elevated.
- Existing changes: .gitignore, untracked .ijfw, review diffs/notes/results.
  Preserve and exclude them from this change.
- Baseline existing native binary: 248 tests, 239 passed, 7 skipped, 2 failed.
  Both failures read missing `src/confighttp.cpp` from the test fixture directory.
- Evidence: `L:/Coding/_scratch/apollo-review-20261021/`.

## Review and verification tasks

- [x] Reproduce the invalid SetDisplayConfig flag combination with SDC_VALIDATE
  only (no display changes). Remove the incompatible path-order modifier.
- [x] Exercise password persistence failures through real loopback HTTPS handlers;
  require HTTP 500 and retain the prior usable login/session.
- [x] Verify exact cookie-name parsing and confidential request-header redaction.
- [x] Include the missing config fixture and run native regression/build checks.
- [x] Inject at least four realistic regressions in scratch copies; require failures.
- [x] Inspect final diff, scan staged changes, commit and push to the fork.
- [x] Package binary/assets, preserve rollback, install when authorized access is
  available, verify process/listeners/endpoints/hashes. Never interrupt an active
  stream. If permissions prevent installation, retain the tested candidate and
  report the exact incomplete boundary.

## Acceptance and rollback

No false success for failed password writes; old credentials and paired clients
survive failures and successful password changes. Cookie lookup uses exact names.
Debug logs omit authorization, cookies and CSRF values regardless of header case.
Windows accepts the restoration parameters in validation mode. Native tests run
from their build directory. Production native and web builds succeed.

Back up the installed executable and assets before any swap; preserve live
configuration and all credentials. Restore the backed-up files and original
service state if runtime verification fails. Real TV streaming and physical
monitor/HDR restoration remain hardware acceptance unless exercised explicitly.

## Primary API reference

Microsoft's SetDisplayConfig flag table permits SDC_ALLOW_PATH_ORDER_CHANGES only
with SDC_TOPOLOGY_SUPPLIED, whereas Apollo restores full supplied path/mode arrays:
https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setdisplayconfig

## Findings fixed

1. Display restoration combined SDC_ALLOW_PATH_ORDER_CHANGES with full supplied
   path/mode restoration, an invalid Windows API combination. The real Windows
   validation request returned 87 before the fix and succeeds after it. The
   regression uses SDC_VALIDATE so it never changes the display configuration.
2. Initial account setup and password changes ignored both persistence return
   codes, reported success and invalidated sessions after failed writes. Return
   HTTP 500 on failure and preserve the usable session; successful changes still
   invalidate the old session and preserve paired devices.
3. Authentication matched `auth=` anywhere in a cookie header, including inside
   another cookie name or value. Match whole cookie names and retain values
   containing equals signs.
4. Debug request logs exposed cookies, CSRF tokens and mixed-case Authorization
   headers. Redact those headers in both web and GameStream request loggers.
5. The native test build omitted confighttp.cpp from its copied source fixtures,
   so two configuration checks failed when launched from the build directory.

## Observed verification

- New regression tests before the fix: six tests, five fail for the documented
  defects; the successful password-change baseline passes (`red-tests.json`).
- After changes: 254 native tests, 247 pass, seven existing hardware/expected
  skips, zero failures (`green-tests.json`). Includes actual loopback HTTPS
  requests for failed and successful saves, login, CSRF and session invalidation.
- UCRT64 Release executable builds (`green-build-2.log`). The first compilation
  identified a missing Boost predicate include, which was added before success.
- Vite production build succeeds (`web-build.log`, 143 modules).
- Five independently compiled scratch mutations are caught (`mutations/results.json`):
  false password-save success, failed-save session invalidation, cookie suffix
  matching, secret-header logging, invalid display flags. No mutation touched
  the working source. An initial mutation of only one redundant failure check
  remained safe because reload still failed; the actual regression removes both
  failure checks and is caught. Scratch compile include paths were corrected
  from -I to -iquote to avoid shadowing the Windows system process.h.
- The npm advisory endpoint rejected the installed lockfile tree with HTTP 400
  ("Invalid package tree"); no clean dependency-audit verdict is claimed and no
  dependency changes were made. Evidence: `npm-audit-bounded.json`.
- User clarified that no visible failure has occurred: this is preventive review.

Real TV reconnect, controllers and monitor/HDR restoration after a stream remain
hardware acceptance items, not conclusions from the native suite.

## Installation and browser follow-up

- Native source `32590c1806e27b442b96e31227b86fedfb9fc4a0` was pushed to fork
  master and rebuilt as `1.1.1.32590c1`. Installed SHA256:
  `90f9a8a15b04e55986cf56bad2892ae35f11e4bd637e94df900d8813c9ad0e57`.
- The hidden elevated transaction succeeded. New process 84668 started at
  2026-10-21 22:16:35 CDT. Binary and all 112 materialized assets matched the
  candidate. Existing configuration/pairing file contents retained their hashes.
  Rollback: `C:/Program Files/Apollo/backups/regression-review-20261021`.
- Nineteen installed-runtime checks pass (`runtime-checks.json`): login and its
  bundled asset bytes, unauthenticated protected API requests, login redirect,
  untrusted GameStream client certificate rejection, and actual redaction in
  both web and GameStream request logs.
- The separate real browser check found a missing Font Awesome stylesheet despite
  Vite exiting successfully. The newer shared-web dependency no longer supplies
  Font Awesome transitively, but every page still references its package CSS.
  Added explicit `@fortawesome/fontawesome-free` version 7.0.1, recovered from
  the prior installed stylesheet. The exact npm registry tarball was unpacked
  without running package scripts; direct dependency is now in package.json.
- The production build now checks all eight HTML entry points and every linked
  stylesheet, script, preload, icon and CSS font asset. Before repair the check
  fails on the missing package URL. After repair it validates 24 referenced
  assets, including the required font faces. Four scratch mutations (missing
  stylesheet, script, font, page) all fail (`web-mutations/results.json`).
- Final web commit `ba2a164294fd0185fa3ba863b69109b51874a7b5` is pushed and
  installed. The verified native source and executable are unchanged. All 116
  manifested files (executable plus 115 assets) match the final package.
  `web-deployment.json` records the guarded asset-only activation; the original
  native process remains running. Configuration content hashes remain unchanged.
- Final real browser check: login form visible and enabled, Font Awesome 7 Free
  face loaded, zero JavaScript errors, zero failed requests and zero HTTP errors.
  Evidence: `browser-final.json`, `installed-login-final.png`. Nineteen runtime
  checks still pass; the packaged-asset checker also passes against the live
  installation itself. No actual user credentials were submitted or changed.
- Self-contained final archive:
  `L:/Coding/_scratch/apollo-review-20261021/Apollo-32590c1-ba2a164-Windows-local.zip`,
  SHA256 `b250d9d4de21fa563c6542d1d91e6a93b73343c1932e60fd50d36c0a430bd55f`.
  CMake's shader junction is materialized inside the archive. The earlier failed
  candidate and all scratch reproductions remain available as evidence.

## Remaining limits

- No physical Artemis/TV connection, real controller input, streamed audio/video,
  or full display/HDR teardown cycle was exercised in this preventive review.
- The npm dependency advisory service rejected the existing tree; this is an
  audit coverage gap, not a clean bill of health. Existing known disabled encoder
  reconfiguration/reconnect features were not enabled.
- Pre-existing .gitignore changes and unrelated untracked review/harness files
  were preserved. No driver, firewall, encoder tuning, app definition or live
  credential changes were made. Sol performed all work directly.
