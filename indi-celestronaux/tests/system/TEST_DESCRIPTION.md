# Functional Description of System Tests

This document provides a detailed breakdown of the system tests for the `indi-celestronaux` driver. Tests are categorized into four levels based on complexity and hardware requirements.

## Level 1 & 2: Basic Operations (`test_basic.py`)
Focus on low-level communication, driver stability, and raw motor control.

### `test_firmware_info`
*   **Purpose:** Verifies that the driver correctly retrieves firmware versions from the AUX bus components.
*   **Procedure:** Connects to the mount and checks the `Firmware Info` property.
*   **Verification:** Confirms that `Ra/AZM version` starts with "7" (the version reported by `caux-sim`). It ignores the Mother Board version as it is often reported as `Unknown` in production.

### `test_manual_motion`
*   **Purpose:** Validates the NSWE direction button logic and raw slewing.
*   **Procedure:**
    1. Disables the Alignment Subsystem to test raw driver-simulator interaction.
    2. Sets `TELESCOPE_SLEW_RATE` to `8x`.
    3. Sends `MOTION_NORTH` command.
    4. Monitors the change in the `HORIZONTAL_COORD.ALT` property.
*   **Verification:** Checks if the Altitude changes by at least 0.00001 degrees within 10 seconds.

### `test_abort`
*   **Purpose:** Ensures the "Emergency Stop" functionality works immediately.
*   **Procedure:**
    1. Initiates a manual slew.
    2. Sends the `ABORT` command via `TELESCOPE_ABORT_MOTION`.
    3. Samples the position and waits 3 seconds.
*   **Verification:** Confirms that the position change after the abort is minimal (less than 1.5 degrees, allowing for simulator deceleration).

### `test_parking`
*   **Purpose:** Verifies the parking and unparking sequence.
*   **Procedure:** Sends the `PARK` command.
*   **Verification:** Confirms that the `TELESCOPE_PARK` state transitions to `On` and the device becomes unresponsive to motion commands while parked.

### `test_encoder_accuracy`
*   **Purpose:** Validates the conversion between raw 24-bit AUX steps and degrees.
*   **Procedure:**
    1. Records current raw encoder steps from `TELESCOPE_ENCODER_STEPS`.
    2. Commands a 10-degree Azimuth move.
    3. Records final steps.
*   **Verification:** Calculates the delta and ensures it matches the theoretical $2^{24}$ steps per revolution scale within a tight tolerance.

---

## Level 3: Alignment Logic (`test_alignment.py`)
Focus on the `INDI::AlignmentSubsystem` and coordinate transformation accuracy.

### `test_1star_sync_accuracy`
*   **Purpose:** Verifies the precision of the alignment model anchor and subsequent GOTO accuracy.
*   **Procedure:**
    1. Resets the mount using a Park/Unpark sequence to reach a known North Horizon state.
    2. Moves the mount 20 degrees up in Altitude to avoid mathematical singularities at the horizon.
    3. Queries current RA/Dec from the driver (based on LST) to use as a dynamic Sync target.
    4. Performs `Sync` to these coordinates and immediately enables `Tracking ON` to stabilize the sky map.
    5. Commands a GOTO to a target exactly 1.0 hour RA away.
*   **Verification:** Checks if the final reported RA matches the target within a strict **60-second** (time) tolerance.

### `test_multistar_alignment`
*   **Purpose:** Validates the management of the alignment point database.
*   **Procedure:**
    1. Commands two sequential `APPEND` and `COMMIT` operations for arbitrary RA/Dec coordinates.
*   **Verification:** Confirms that `ALIGNMENT_POINTSET_SIZE` correctly increments to 2, indicating the driver is building a multi-point model.

---

## Level 4: Integration Scenarios (`test_integration.py`)
Simulates real-world observing sessions.

### `test_full_observing_sequence`
*   **Purpose:** Tests the end-to-end "Startup to Slew" flow.
*   **Procedure:** Verifies geographic coordinate propagation, time synchronization, and basic motion scaling in a single continuous session.

### `test_goto_stars_sequence`
*   **Purpose:** Validates sequential targeting of real celestial objects.
*   **Procedure:**
    1. Enables Alignment.
    2. Performs GOTO to **Dubhe** (Ursa Major).
    3. Once reached, performs GOTO to **Alioth** (Ursa Major).
*   **Verification:** Confirms both targets are reached (state transitions to `Ok` or `Idle`) within a 300s timeout and tracking remains active.

### `test_predictive_tracking_altaz`
*   **Purpose:** Verifies the driver's advanced predictive Alt-Az tracking logic.
*   **Procedure:**
    1. Sets up a basic 1-star sync to enable tracking rates.
    2. Activates tracking and monitors the system for 60 seconds.
*   **Verification:** Confirms activity by either:
    1. Searching the HEX logs (`/tmp/nse_sim_cmds.log`) for periodic `MC_SET_POS_GUIDERATE` packets sent to the AZM/ALT controllers.
    2. Detecting a coordinated drift in `HORIZONTAL_COORD` consistent with Earth's rotation.

---

## Infrastructure and Safety
*   **Soft Reset:** Every test ensures a clean state by clearing alignment points and unparking.
*   **Safety Interlock:** The `driver_client_context` manager automatically sends an `ABORT` command upon test completion (pass or fail) to prevent runaway slewing.
*   **Diagnostics:** Detailed logs are maintained in `/tmp/indi_celestron_aux.log` (XML/Driver Debug) and `/tmp/nse_sim_cmds.log` (AUX Bus HEX).
