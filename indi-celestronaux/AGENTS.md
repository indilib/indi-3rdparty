# Agent Guide: indi-celestronaux

This document provides essential information for agentic coding assistants working on the `indi-celestronaux` repository.

## Modification Scope
- **Strict Isolation:** Any modifications to files outside the `indi-celestronaux/` directory (e.g., root `README.md`, common CMake modules, other drivers) **require explicit approval** from the user.
- **Driver Integrity:** **DO NOT** modify the C++ driver code (`celestronaux.cpp/h`, `auxproto.cpp/h`, etc.) during the test development stage. It is considered working production code. If issues are discovered, document them clearly in `ISSUES.md`.
- **Troubleshooting Hierarchy:** When a test fails, assume the cause of the problem in the following order:
    1.  Test code (logic, timeouts, property names).
    2.  Simulator code (protocol implementation, timing).
    3.  Driver code (C++ implementation).
    Do not jump to conclusions about driver bugs without exhausting test and simulator causes.

## Build, Run, and Test Commands

The project uses CMake but provides a `Makefile` in the root directory for common development tasks.

### Build and Install
- **Compile:** `make` (runs cmake and make in the `build/` directory)
- **Clean:** `make clean`
- **Build Debian Packages:** `make deb` (requires `devscripts`, `debhelper`, etc.)

### Running the Driver
- **Run under indiserver:** `make run`
- **Run with Simulator:** `make run-sim`
  - *Note:* Requires `python3-ephem`. Connect via INDI panel to `127.0.0.1:2000`.
  - The simulator is located in `indi-celestronaux/simulator/nse_simulator.py`.

### Testing
- **Run all tests:** `cd build && ctest -V`
- **Run a single test:** `cd build && ctest -R <test_name_regex> -V`
- **System Tests:** Located in `tests/system/`. Run using `pytest`.
  - Detailed functional descriptions are available in `indi-celestronaux/tests/system/TEST_DESCRIPTION.md`.
  - Ensure `indiserver` and `python3-ephem` are installed.
  - The tests use a simulator (`nse_simulator.py` or `caux-sim`).
  - **Parity with auxdrv:** The test suite aims for functional parity with the experimental `auxdrv` test suite, including robustness at celestial poles and anti-backlash approach verification.
- **Enable Unit Tests:** `cmake -DINDI_BUILD_UNITTESTS=ON ..` (if not already enabled)

## Code Style and Conventions

Strictly adhere to the following conventions to maintain consistency with the existing codebase.

### Formatting
- **Tool:** `clang-format` using the provided `.clang-format` file.
- **Style:** Allman (braces on new lines).
- **Indentation:** 4 spaces (no tabs).
- **Column Limit:** 120 characters.

### Naming Conventions
- **Classes:** `PascalCase` (e.g., `CelestronAUX`, `AUXCommand`).
- **Methods:**
  - **INDI API Overrides:** `PascalCase` (e.g., `ISNewNumber`, `ISGetProperties`).
  - **Internal Methods:** `camelCase` (e.g., `slewTo`, `trackByRate`, `sendAUXCommand`).
  - **Telescope Overrides:** `PascalCase` (e.g., `Handshake`, `Sync`, `Abort`).
- **Member Variables:** `m_` prefix followed by `PascalCase` (e.g., `m_AxisStatus`, `m_SkyCurrentRADE`).
- **Local Variables:** `camelCase` (e.g., `steps`, `rate`, `enabled`).
- **Enums:** `PascalCase` (e.g., `ScopeStatus_t`, `AxisStatus`).
- **Enum Members:** `UPPER_CASE` (e.g., `IDLE`, `SLEWING_FAST`) or `PascalCase` for property indices.
- **Constants:** `UPPER_CASE` (e.g., `STEPS_PER_REVOLUTION`).
- **INDI Properties:** `PascalCase` with type suffix (e.g., `FirmwareTP`, `EncoderNP`, `HomeSP`).

### Imports and Includes
- **Order:**
  1. System headers (e.g., `<vector>`, `<termios.h>`)
  2. INDI headers (e.g., `<inditelescope.h>`, `<indicom.h>`)
  3. Local headers (e.g., `"auxproto.h"`, `"celestronaux.h"`)
- **Format:** Use `<...>` for system/INDI and `"..."` for local files.

### Error Handling and Logging
- **Logging:** Use INDI logging macros:
  - `LOG_ERROR("message")`
  - `LOG_DEBUG("message")`
  - `LOGF_INFO("format %d", value)`
- **Return Values:** Methods should generally return `bool` (true for success, false for failure) unless a specific INDI type is required (e.g., `IPState`).

### Documentation
- Use Doxygen-style comments for public and protected methods in header files.
- Focus on the "why" in comments, especially for complex protocol logic.

## Project Structure
- `indi-celestronaux/`: Core driver source code.
- `indi-celestronaux/simulator/`: Python-based NexStar simulator for testing.
- `cmake_modules/`: Shared CMake find modules.
- `scripts/`: Utility scripts for environment setup and SDK syncing.
- `debian/`: Debian packaging configuration.
- `indi-celestronaux/Makefile`: Makefile for common development tasks.
- `CMakeLists.txt`: Root-level CMake configuration.

## Technical Details

### Celestron AUX Protocol
- **Encoders:** 24-bit integers representing fractional revolutions. `2^24` steps = 360 degrees.
- **Communication:** Supports both Serial (9600/19200 baud) and TCP (port 2000).
- **Targets:** AZM (0x10), ALT (0x11), FOCUS (0x12), GPS (0xb0), etc.
- **Commands:** Defined in `auxproto.h` (e.g., `MC_GET_POSITION`, `MC_GOTO_FAST`).

### PID Control and Adaptive Tuning
- The driver uses a `PID` class for axis control.
- `AdaptivePIDTuner` provides real-time adjustment of PID gains based on a reference model.

### INDI Properties
- Properties are defined in `celestronaux.h` and initialized in `initProperties()`.
- Use `PascalCase` for property names.
- Common suffixes: `SP` (Switch), `NP` (Number), `TP` (Text), `BP` (BLOB).
- Property states are managed using `IPState` (e.g., `IPS_IDLE`, `IPS_BUSY`, `IPS_OK`, `IPS_ALERT`).

## Common Tasks for Agents

### Adding a New Property
1. Define the property in `celestronaux.h` (e.g., `INDI::PropertyNumber MyNewNP {1};`).
2. Initialize it in `initProperties()` in `celestronaux.cpp`.
3. Define its labels and attributes.
4. Handle updates in `ISNewNumber`, `ISNewSwitch`, etc.
5. Update `ISGetProperties` to register the property.

### Implementing a New AUX Command
1. Add the command code to the `AUXCommands` enum in `auxproto.h`.
2. Implement a helper method in `CelestronAUX` to send the command using `sendAUXCommand`.
3. Handle the response in `processResponse` if necessary.

### Debugging Protocol Issues
- Use `LOG_DEBUG` to trace AUX commands.
- The `hex_dump` method is available for raw buffer inspection.
- Check `m_IsRTSCTS` and `m_isHandController` flags for connection-specific logic.
- Use the simulator to reproduce issues: `make run-sim` and then connect with `indiserver`.
- The `AUXCommand` class handles checksum calculation and packet parsing.
- **Simulator (caux-sim):** Use `--log-file /tmp/nse_sim_cmds.log --log-categories 31` to enable traffic logging required by system tests. Commands appear as `3b...` in hex logs.
- **Alt-Az Tracking:** For predictive tracking to work, `ON_COORD_SET` must be `TRACK` and `APPROACH_DIRECTION` should be `APPROACH_TRACKING_VECTOR`. `TRACK_SIDEREAL` is only for EQ mounts. Basic tests should disable `ALIGNMENT_SUBSYSTEM_ACTIVE` to test raw driver behavior.
- **Simulator (caux-sim) Geometry:** The simulator starts at 0,0 (North on horizon). Basic tests rely on this and avoid unnecessary sync points.
- **Sync Deviation:** A consistent deviation of ~0.15h in RA sync tests is currently expected when using `caux-sim`, likely due to LST calculation or Epoch (JNow vs J2000) differences between the driver and simulator.
- **Connection Stabilization:** When connecting to `caux-sim`, the driver might take a few seconds to initialize all properties. Always wait for `CONNECTION` state to be `Ok` with a sufficient timeout (10s+).

### Updating Firmware Information
- Firmware versions are queried during `Handshake()`.
- If adding support for a new module, update `m_MainBoardVersion`, etc., and the `FirmwareTP` property.

## Documentation and Git Requirements
- **Incremental Commits:** Commit after each successfully completed step, phase, or feature. Maintain small, complete increments to enable easy backtracking.
- **Detailed Messages:** Commit messages should clearly state the "why" and "what" of the changes.
- **Changelog and Docs:** Update `indi-celestronaux/CHANGELOG.md` and other documentation (e.g., `ISSUES.md`, `TEST_PLAN.md`) at every commit where necessary.
- **Timestamped Entries:** Use `YYYY-MM-DD HH:MM` timestamps in `CHANGELOG.md` to help track history and resolve merge conflicts.
- **English Only:** All on-disk texts (comments, documentation, commit messages, logs) MUST be in English, even if the conversation with the user is in another language.
- **Driver Integrity:** **DO NOT** modify the C++ driver code (`celestronaux.cpp/h`, `auxproto.cpp/h`, etc.) during the test development stage. It is considered working code. If issues are discovered, document them clearly in `ISSUES.md` to be addressed in a later stage.
- **Issue Tracking:** Record all discovered driver bugs, unexpected behaviors, or observations in `indi-celestronaux/ISSUES.md`.

## Development Workflow
1. **Branching Strategy:** 
   - Develop each well-defined feature or bugfix in a separate branch.
   - Merge feature branches into a single shared test branch to verify compatibility and resolve merge conflicts.
2. **Environment:** Ensure `libindi-dev` and other prerequisites are installed.
3. **Simulator:** Use the simulator for testing protocol changes without hardware.
   - Run `make run-sim` in one terminal.
   - Run `make run` in another terminal.
   - Connect using an INDI client (e.g., KStars/Ekos) to `localhost`.
4. **Verification:** Verify changes by running `make` and checking for compiler warnings.
5. **Testing:** Run existing tests using `ctest` or `pytest` before submitting changes.
6. **Formatting:** Run `clang-format` on modified files.

## Licensing
- All new files must include the GPLv2+ license header.
- Refer to existing files for the exact header format.
