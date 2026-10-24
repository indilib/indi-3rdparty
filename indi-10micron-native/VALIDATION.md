# Validation and review limits — October 24, 2026

## Source provenance

Input: 10micron-v019-upstream-source.tar.gz uploaded from the Pi. Only current
driver sources/tests were selected. config-restore-backup and earlier candidate
manifests/patches/historical docs were not republished. SOURCE-HASHES.json at the
package root records input/archive hashes and retained code identity.

The configuration restore correction is present: a saved Active Dome member is
retained, while only the simulator default is cleared if no saved member exists.
tests/indi_endpoint_config_test.py covers endpoint and enclosure restore/save.
No physical motion implementation was changed during packaging.

## Prior reported validation

| Environment | Evidence |
|---|---|
| INDI 2.2.1, Linux x86_64 | Archived development report: 18 short tests plus accepted-flip timeout passed |
| INDI 2.2.5, Linux x86_64 | Archived development report: 18 short tests plus accepted-flip timeout passed |
| Reference Pi, installed INDI 2.2.1 | User supplied 19/19 CTest pass, including timeout ~182.43 seconds; isolated staging completed |
| Real GM1000HPS / v0.19 | User telemetry confirmed ordinary Unpark, repeat settled Unpark, native Park; closed-enclosure refusal and successful reopening/unpark recovery |
| Saved Active Dome/policy | User reported restoration after restart; supplied code includes dedicated regression |

These are development records/user reports, not fresh re-execution of every
historical hardware condition by the publication environment. The read-only
diagnostic executable is not a substitute for motion-driver commissioning.

## Publication checks

Current environment has g++ and Python but no CMake/pkg-config/INDI development
libraries. Protocol/state binaries and loopback TCP suites can be compiled here;
the actual INDI executable cannot be freshly built here. See package HOST-TESTS.txt
for the results of this packaging run. Full publication CMake build, all 19 CTest
cases and isolated install staging must be repeated on the Pi before marking ready.

Changes from the uploaded build file: driver build defaults ON; test default OFF;
project name/version; GNUInstallDirs for registry and binary installation. The
root upstream hooks are guarded against changed anchors and destination overlays.
These build-only changes do not alter motion callbacks or protocol source.

## Publication Pi build completed

User supplied the publication run from
/home/astroberry/10micron-publication-validation-20261024-110729:
19/19 CTest tests passed in 210.43 seconds, including indi-flip-timeout at
182.42 seconds, then both executables and XML registries installed successfully
into the private stage directory. No production binary/settings were replaced.
This supersedes the earlier pending-publication-build requirement above.
The subsequent legacy flip guide is documentation only; no source changed.

## Unresolved acceptance/commissioning scope

- Other real 10Micron models, operating systems and enclosure drivers.
- Live enclosure-loss/reconnect safety hold under controlled conditions.
- Broader stock Ekos flip interaction; native budget/orchestration patches are separate.
- Serial transport, pulse guiding/manual axes and other unsupported capabilities.
- Manufacturer protocol licensing: no PDF redistribution or endorsement claimed.
- Source contribution license/attribution and whether maintainers prefer placement
  in INDI core alongside the existing LX200 driver, rather than indi-3rdparty.

The new driver's standard properties deliberately differ from a private physical
roof sensor contract. Tests and history do not prove that all interlocks or
hardware conditions are safe. Public review is intended to broaden that evidence.
