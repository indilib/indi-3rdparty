# Issues Registry - indi-celestronaux

This file tracks unresolved issues and observations discovered during the development and testing of the Celestron AUX driver.

---

## Part 1: Testing Suite Issues (TODO List)

These issues relate to the completeness and robustness of the system tests.

### 1. Homing Emulation in Simulator
*   **Description:** `test_homing` is currently skipped because the `caux-sim` binary does not emulate the physical index sensors required for the homing sequence.
*   **Impact:** Homing logic in the driver (Move to Index) remains unverified in the automated suite.
*   **Requirement:** Enhance the simulator to support index marker triggers or provide a mock sensor response.

### 2. CI/CD Integration
*   **Description:** The current test suite runs locally but is not yet integrated into a GitHub Actions or similar CI pipeline.
*   **Requirement:** Create a workflow that builds the driver, starts the simulator, and executes `pytest`.

### 3. HEX Log Analysis Robustness
*   **Description:** `test_predictive_tracking_altaz` relies on regex searching in `/tmp/nse_sim_cmds.log`. This is brittle if the simulator's log format changes.
*   **Requirement:** Implement a structured log parser or a real-time AUX bus sniffer within the test client.

---

## Part 2: Driver Issues and Observations

These are irregularities or potential bugs in the C++ driver that require separate investigation.

### 1. Connection Loss Detection Latency
*   **Observation:** The driver takes a significant amount of time (>60s) to detect that the TCP connection to the mount/simulator has been dropped.
*   **Replication:**
    1. Start `indiserver` with the driver and connect to `caux-sim`.
    2. Kill the `caux-sim` process.
    3. Monitor the `CONNECTION` property in an INDI client. It remains `Ok` (Green) for over a minute before transitioning to `Alert`.
*   **Diagnostic Tool:** `indi-celestronaux/tests/system/test_basic.py::test_reconnection` demonstrates this behavior.

### 2. Property State Jitter (Idle vs Ok)
*   **Observation:** The `EQUATORIAL_EOD_COORD` property often transitions to `Idle` instead of `Ok` immediately after a successful GOTO.
*   **Observation:** The `HORIZONTAL_COORD` property remains in `Idle` state even during active tracking or slewing.
*   **Impact:** Standard INDI clients may interpret `Idle` as a failure to reach the target or as an inactive state, leading to UI inconsistencies.
*   **Replication:** Use `debug_goto.py` (or run `test_1star_sync_accuracy`) and observe the state transitions in the console output.

### 3. Predictive Alt-Az Tracking Update Interval
*   **Observation:** According to NexStar documentation, Alt-Az tracking rates should be updated every 30 seconds. In testing, the update frequency seems inconsistent or highly dependent on system load.
*   **Impact:** Infrequent updates may lead to "staircase" tracking behavior, affecting long-exposure photography.
*   **Replication:** Run `test_predictive_tracking_altaz` and analyze the timestamps of `MC_SET_POS_GUIDERATE` commands in `/tmp/nse_sim_cmds.log`.

### 4. ON_COORD_SET Rule Enforcement
*   **Observation:** The `ON_COORD_SET` switch property (TRACK/SLEW/SYNC) does not strictly enforce the "OneOfMany" rule upon receiving updates. It is possible for multiple elements to appear `On` if the client sends multiple rapid updates.
*   **Impact:** Ambiguous driver state if both `SYNC` and `SLEW` are active.
*   **Location:** `celestronaux.cpp`, `ISNewSwitch()`.

### 5. MOUNT_TYPE Configuration Missing
*   **Observation:** The `MOUNT_TYPE` switch property is defined but commented out in `initProperties()`, preventing manual override of the mount's geometry (GEM vs Alt-Az).
*   **Impact:** The driver relies on device name string matching to guess the mount type, which is prone to error.
*   **Location:** `celestronaux.cpp`, line ~285.
