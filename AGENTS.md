# AGENTS.md — Guidance for AI coding agents working on INDI 3rd Party drivers

This file tells AI agents (Claude Code, Codex, Copilot, Cursor, etc.) how to work in this repository.
Human contributors should read [README.md](README.md) and [CONTRIBUTING.md](CONTRIBUTING.md); this file
summarises the parts that matter most and adds rules specific to agent-driven changes.

If anything here conflicts with an explicit instruction from the maintainer in the current session, follow the maintainer.

---

## 1. Project at a glance

This repository holds the INDI drivers that are not part of [INDI Core](https://github.com/indilib/indi): all
camera drivers, drivers that need a library or SDK INDI Core doesn't have, and drivers that follow their own
release schedule. The driver framework and base classes (`INDI::CCD`, `INDI::Telescope`, `INDI::Focuser`, …)
live in INDI Core; a change to them is a separate pull request to that repository.

| Path | Contents |
|---|---|
| `indi-<name>/` | One driver package per directory, each its own CMake project: sources, `CMakeLists.txt`, `config.h.cmake` and `indi_<name>.xml.cmake` (label, manufacturer, executable and **version**) |
| `lib<name>/` | Vendor SDK libraries that some drivers need, mostly binaries supplied by the manufacturer |
| `CMakeLists.txt` | Top level: a `WITH_<NAME>` option and an `add_subdirectory` per driver; `BUILD_LIBS` builds the libraries instead |
| `debian/`, `spec/` | Packaging, one directory per driver and library under `debian/` |
| `examples/` | Starting points for a new driver or client |
| `obsolete/` | Retired drivers. Don't revive them or copy from them. |
| `scripts/` | Build and install scripts used by CI |

Drivers run real observatories, often unattended overnight. A wrong command can slew a mount, run a focuser
into its stop, open a roof or switch power. Change carefully and conservatively.

---

## 2. Working rules

1. **Understand before changing.** Read the driver, its base class in `libs/indibase/` and a similar driver
   before editing.
2. **Keep changes minimal and focused.** One logical change per commit. Don't mix refactors or mass
   reformatting with behaviour changes.
3. **Take device commands from evidence, never from a guess.** Every command, reply format and unit must come
   from the manufacturer's protocol document or a log from a real device. Name that source in the pull request.
   A command that looks like a query may move the hardware: the PlaneWave EFA driver in INDI Core once sent a "read slew rate"
   command at connect that was in fact a move command, and every connection ran the focuser outward.
4. **Connecting, loading the configuration and polling must never move or power equipment.** Write nothing to
   the device when it already has the requested value.
5. **Properties are public API for every INDI client**, not only KStars. Don't remove or rename a property or
   element, change its type, or change the meaning of a standard property. Add new ones instead.
6. **Don't commit, push or open pull requests unless asked.** Never open a pull request for a new driver, or for
   a change to what a driver sends to its device, until a human has confirmed the hardware test in §5.3.
7. **Don't touch shared or running infrastructure**: a running `indiserver`, real devices, system packages, or
   the user's `~/.indi` configuration. For your own runs use another port (`indiserver -p <port>`) and a private
   configuration file (`INDICONFIG=<file>`).
8. **Leave vendor libraries alone.** The files under `lib*/` come from the manufacturers. Don't edit, rebuild or
   replace them as part of a driver change. An SDK update is its own change, and it states the vendor's version
   and where it came from.
9. **Report outcomes honestly.** If a build or test fails, or a step was skipped, say so and include the output.
   Never write "tested on hardware" unless a human told you so in this session, with the model and firmware.

---

## 3. Building

Build only the driver you work on, out of tree. Each `indi-<name>/` is a CMake project of its own:

```bash
cmake -S indi-eqmod -B <build-dir>/indi-eqmod -G Ninja -DCMAKE_BUILD_TYPE=Debug   # configure once
ninja -C <build-dir>/indi-eqmod -j"$JOBS"
```

- A driver needs INDI Core installed (`libindi-dev` or a local build) and, if it uses a vendor SDK, that
  `lib<name>/` built and installed first. Installing either changes the system: ask before you do it.
- Don't build inside the source directory, and reuse an existing build tree instead of creating new ones.
  Building all drivers at once needs every dependency of every driver; don't do it to check one driver.
- Always pass an explicit job count below the number of CPU cores.
- Drivers that include `CMakeCommon` build with `-Wall -Wextra -Werror`, so a new warning is a build failure.
  Don't turn `FIX_WARNINGS` off to get past one.
- Keep large build output off a RAM-backed `/tmp`.

---

## 4. Bump the driver version on any behaviour change

Any change to a driver that alters its behaviour must also bump that driver's version, in the same change.

Most drivers define the version once, in their own `CMakeLists.txt` (`VERSION_MAJOR` / `VERSION_MINOR`, or
prefixed names such as `TYR_VERSION_MINOR`). CMake passes it to `config.h`, which the driver's `setVersion()`
call uses, and to `indi_<name>.xml.cmake`. Bump it there. If a driver writes the version in more than one
place (a literal in `setVersion()`, a static `.xml` file), update all of them: the version the driver reports
and the version in its XML file must always match.

A behaviour change is anything a client or user can observe: a property added, removed or renamed, a changed
default or range, a change to what is sent to or read from the device, a change to what is saved in the
configuration, or a bug fix that changes what the driver does. Comments, formatting, renames of internal symbols
and refactoring with identical results do not need a bump.

---

## 5. Testing — REQUIRED for every change

"It compiles" is not validation, and an automated test is not a hardware test. Most changes need both kinds of
evidence, and the pull request must say which one each claim rests on.

### 5.1 What each change needs

| Change | Automated test | Real hardware |
|---|---|---|
| New driver | Required for logic that can run without the device (parsing, conversions, state handling) | **Required**, by a human, before the pull request is opened (§5.3) |
| Change to what a driver sends to or reads from its device (new command, new firmware or model, changed timing) | Regression or contract test against a fake device (§5.2) | **Required**, on that device family, by the submitter or the user who reported the problem |
| Bug fix | A regression test that **fails before the fix and passes after**. Run it both ways. | Required when the fix changes device communication |
| Driver logic with unchanged device communication (derived values, property handling, configuration) | Required | Not required; state "not tested on hardware" |
| Code shared by several drivers (e.g. `indi-toupbase`, the EQMod base classes) | Required where the logic can run without the device, and every driver that uses the code still builds | **Required**, on at least one device of each affected driver family where possible; name the ones not covered |
| Vendor SDK update under `lib*/` | Every driver that uses the library still builds | **Required**, on at least one device that uses that SDK |
| Docs, comments, build-only | Build succeeds | Not applicable |

If something really cannot be tested automatically, explain why and give the human concrete manual steps.

### 5.2 Automated tests

Tests live in the driver's own directory and are registered in its `CMakeLists.txt` inside
`if(INDI_BUILD_UNITTESTS)`. Existing examples to follow:

- `indi-tyr/tests/`: the built driver runs against a protocol emulator over simulated USB and TCP; CI runs these.
- `indi-maxdomeii/test_maxdomeii.cpp`: GoogleTest compiled together with the driver sources.
- `indi-scopelink/tests/`: protocol logic kept in a library that builds without libindi, tested against recorded wire data.

The most useful pattern is a **fake device** that answers like the real one and records every command it
receives. Where a driver is bound to a vendor SDK, keep the logic that doesn't need the SDK (parsing,
conversions, state handling) in code that can be tested without it.

- Drive the driver as a client does (`ISNewText`, `ISNewSwitch`, `ISNewNumber`, or `indi_setprop` against a
  private `indiserver`): set the port, connect, load the configuration, send requests.
- Take the fake device's replies from a real device log or the protocol document, and say which in a comment.
- Assert on what reached the device (commands and their bytes) and on what the driver published, not only on
  return values. For a safety fix, assert on what must **not** be sent.
- Give a GoogleTest its own `main()`. `libindidriver` ships a `main()` of its own, and a test that links it
  without one sits in the driver event loop forever instead of running.
- Never touch the user's setup: set `INDICONFIG` to a temporary file, switch `DEVICE_AUTO_SEARCH` off so a failed
  connection doesn't scan real serial ports, and don't use the default port 7624.
- No real network access and no fixed long sleeps; tests must be deterministic.

Check that a new test can fail: break the code it covers (or revert the fix), see it fail, then restore.

```bash
cmake -S indi-tyr -B <build-dir>/indi-tyr -G Ninja -DCMAKE_BUILD_TYPE=Debug -DINDI_BUILD_UNITTESTS=ON
ninja -C <build-dir>/indi-tyr -j"$JOBS"
ctest --test-dir <build-dir>/indi-tyr --output-on-failure
```

A run passes only if `ctest` exits with code 0 and reports `100% tests passed`. Check its own exit code, not
that of a pipe.

Before blaming or fixing an unrelated failure, confirm it also fails on `HEAD` without your change, then report it.

### 5.3 Hardware tests

An agent cannot perform these. Its job is to write the test plan, ask the human to run it, and record exactly
what they report. A fake device, a simulator or "it follows the protocol document" does not replace this for a
new driver or a change in device communication.

A hardware test covers at least:

1. Connect, disconnect and reconnect; then connect with the device unplugged or powered off (a clean error, no hang or crash).
2. Every property the driver defines: set it, read it back, and compare with the device itself (its display,
   its own software, or the physical position).
3. Every function that moves or powers something: start, completion, abort, and behaviour at the limits.
4. Save the configuration, restart the driver and load it: values are restored and nothing moves or switches on.
5. Unplug the device during a session: the driver reports the error and doesn't crash.
6. One real session in a client for that device class, e.g. in KStars/Ekos: an autofocus run for a focuser; a
   goto, sync, park and guiding for a mount; a capture sequence for a camera or a filter wheel.

The pull request must state:

- The device model and **firmware version**, and the connection used (USB, serial, TCP).
- The host (OS, architecture) and the INDI commit the test was built from.
- What was exercised and what was observed, per feature. A driver debug log is welcome.
- What was **not** tested, e.g. "not tested under a real sky" or "only the USB connection".

### 5.4 Final report

List the changed files, the exact test commands with their results, what was verified on hardware and by whom,
and anything skipped or unverified.

---

## 6. Submitting a new driver

### 6.1 First check that a new driver is needed

**Don't write a second driver for a device that an existing driver already supports or could support.** Two
drivers for the same hardware confuse users choosing one in their client, split bug fixes, documentation and
saved configurations, and double the maintenance. A new driver for an already supported device is refused
unless there is a very good reason.

Before writing any code, look for an existing driver:

- Search the `indi-*/` directories and their `*.xml.cmake` files here, `drivers.xml` and `drivers/` in
  [INDI Core](https://github.com/indilib/indi), and <https://drivers.indilib.org>, by manufacturer, model
  **and protocol or SDK**. Many devices are supported under another name: a lot of mounts speak LX200 or OnStep,
  a lot of focusers speak the MoonLite protocol, and many camera brands are rebadged models that share one SDK.
- Check the open pull requests in both repositories for the same device.

Then take the first option that works:

1. **Use the existing driver as it is.** If only the name is missing, add a `<device>` entry to that driver's
   XML file that points to the same executable.
2. **Fix or extend the existing driver**: a new model or firmware, a missing feature, a bug.
3. **Derive from it or share its code**, as `indi-toupbase` does for the camera brands built on the same SDK,
   when the device needs its own behaviour on top of a common protocol or SDK.
4. **Write a new driver** only when none of the above can work.

Good reasons for a new driver: the device speaks a different, incompatible protocol; supporting it in the
existing driver would break that driver's current users; or the maintainer agreed beforehand that the existing
driver should be replaced. These are **not** reasons: the existing driver has a bug or lacks a feature (fix or
add it), its code is unfamiliar or in a style you don't like, you want different property names, or generating
a driver from scratch is quicker than reading the existing one.

If you are asked to create a driver and find one that already covers the device, stop and tell the user which
driver it is and what it would take to extend it. Go on with a new driver only if they give a reason that fits
the list above, and suggest they agree it with the maintainers first (a GitHub issue or the INDI forum), before
the work is done. The pull request must then name the existing drivers that were considered and say why each
could not be used or extended.

### 6.2 What a new driver must come with

A new driver is complete only when all of these exist:

1. **The right repository.** A driver belongs here if it is a camera driver, needs a library or SDK that INDI
   Core doesn't have, or must stay independent of the INDI Core release schedule. Any other driver goes to
   [INDI Core](https://github.com/indilib/indi).
2. **The right base class and standard properties.** Derive from the `INDI::` class for the device type and
   implement its interface, so every client can use the device without driver-specific knowledge. Use
   custom properties only for what the standard ones don't cover.
3. **Its own package**: a directory `indi-<name>/` with `CMakeLists.txt`, `config.h.cmake` and
   `indi_<name>.xml.cmake` (label, manufacturer, executable, and the version taken from CMake so it matches
   `setVersion`); a `WITH_<NAME>` option and `add_subdirectory` in the top-level `CMakeLists.txt`; and the
   packaging under `debian/indi-<name>/`, as the existing drivers have. Add new build dependencies to
   `scripts/requisites-install.sh`.
4. **Its vendor library, if it needs one**, in its own `lib<name>/` directory with the vendor's licence, which
   must allow redistribution. State the SDK version and where it came from.
5. **A licence header** on every new file, with a licence compatible with INDI (see CONTRIBUTING.md).
6. **Automated tests** for the logic that can run without the device (§5.2).
7. **A hardware test by a human on the real device** (§5.3), reported in the pull request. A driver that
   was never connected to its device is not accepted, however complete it looks.
8. **User documentation, submitted to the drivers documentation repository** (§7), with its pull request linked.

---

## 7. Driver documentation (drivers.indilib.org)

User documentation for all drivers lives in a separate repository,
[indilib/drivers-docs](https://github.com/indilib/drivers-docs), which is published at <https://drivers.indilib.org>.
Don't add user documentation (a `doc/` directory, screenshots) to the driver's directory in this repository; a
short README with build notes for developers is fine.

**A documentation pull request there is required for:**

- Every new driver.
- Every driver change that makes its page wrong or incomplete: a property added, removed, renamed or moved
  to another tab; a changed default, range or meaning; a new connection type; a new supported model or
  firmware; changed setup steps; changed behaviour the user must know about. Update the `version` in the
  page's `.yaml` too.

A fix with nothing new for the user to see or do needs no documentation change. If a changed driver has no
page yet, say so in your report; a page is welcome but only required for new drivers.

Each driver has a directory `src/content/docs/<category>/<manufacturer>/<driver>/` (lowercase, hyphenated) with:

- `<driver>.md`: the page, with front matter (`title`, `categories`, `description`, `thumbnail`).
- `<driver>.yaml`: metadata (`driver_name`, `manufacturer_name`, `categories`, `executable`, `version`, `author`, …).
- `<driver>.webp`: a 300x300 thumbnail. More images go in `images/`, as `.webp`.

The repository's README is authoritative for the layout, the category list and the image rules, and its CI rejects
a directory that misses one of the three files.

The page must cover what the device is and which models and firmware are supported, the features, how to
connect, each tab and its properties as the driver really shows them, and known issues. Use the property
labels of the running driver. Screenshots must come from the running driver, not from mock-ups or generated
images; if you cannot take them, ask the human to.

Link the two pull requests to each other. If you cannot open the documentation pull request yourself, hand the
files to the human and say plainly that it is still owed.

---

## 8. Code style and conventions

- **Formatting is checked in CI** with astyle on the lines each pull request changes (vendor files under `lib*/`
  excepted). The options are in README.md ("Code Style"). Check your changes before reporting:
  `scripts/check-codestyle.sh <file>` for uncommitted changes, or `scripts/check-codestyle.sh <file> origin/master`
  for everything on your branch. The check looks at whole functions, so format the functions you touch and
  nothing else. The astyle version is pinned in `.astyle-version`; the script refuses another version, and a
  format made with another version may fail in CI.
- **Match the surrounding code**: naming, comment density, logging (`LOG_INFO`, `LOGF_DEBUG`, …) and idioms.
- **New code uses the property classes** (`INDI::PropertyNumber`, `INDI::PropertySwitch`, `INDI::PropertyText`,
  …), not the legacy `IUFill*` functions and raw property structs.
- **Save what the user configures** in `saveConfigItems()`. Don't save values the device stores itself and
  reports at connect; sending them back on every configuration load can have side effects.
- **Hardware safety**: code that moves or powers equipment must handle failure, abort and timeout paths
  explicitly, check every reply, and never become less safe by default.

---

## 9. Pull requests (only when asked)

- Subject: short and specific, with the driver name first for a driver change (`indi-eqmod: …`).
- Description: what was wrong or missing and why, the changes, the source of any device command, the driver
  version, and a **Testing** section as in §5.3 that separates automated tests, hardware tests and what was not tested.
- Link the documentation pull request (§7), or state why none is needed.
- For a new driver: the existing drivers that were considered and why none could be used or extended (§6.1).

---

## 10. Checklist before reporting a task as complete

- [ ] The change is minimal, focused, and follows existing patterns.
- [ ] It builds with no warnings and `scripts/check-codestyle.sh` is clean for the changed files.
- [ ] Tests were added or updated, **run**, and pass; each new test was seen to fail without the change.
- [ ] The driver version is bumped if behaviour changed, and the driver and its XML file report the same one.
- [ ] No property was removed or renamed, and nothing moves or powers equipment on connect or configuration load.
- [ ] Hardware testing is reported as done by a human (model, firmware, what was observed), or clearly as not done.
- [ ] For a new driver: no existing driver covers the device, or the reason for a second one is stated (§6.1).
- [ ] For a new driver, or a change the user can see: the drivers-docs change is prepared or linked.
- [ ] The final report lists the changed files, the commands run with their results, and anything unverified.
