# Test Suite Roadmap - indi-celestronaux

This plan outlines the step-by-step development of the system test suite for the Celestron AUX driver.

## Phase 1: Basic Communication & Handshake
- [x] Verify TCP connection (Driver -> Simulator)
- [x] Verify Firmware Version retrieval (GET_VER)
- [x] Verify connection success state (Ok)

## Phase 2: Motion Control (Functional Testing)
- [x] **Slew (GOTO):** Verified motion initiation via HORIZONTAL_COORD.
- [x] **Abort:** Verified that motion stops immediately when an Abort command is issued.
- [ ] **Manual Motion:** Verify NSWE manual slewing at different rates (Issue 13).
- [ ] **Tracking Rates:** Verify that setting different tracking rates correctly updates the motor controller rates.
- [ ] **Slew Limits:** Verify that the driver respects software slew limits.

## Phase 3: Status, Telemetry & Encoders
- [x] **Position Polling:** Verified that encoder positions correspond to reported degrees.
- [x] **State Transitions:** Documented driver-specific state transitions (Idle after Slew).
- [x] **Sync:** Verified that the Sync command is accepted.
- [ ] **Pole Robustness:** Verify coordinate transformation stability at Dec +90.0.

## Phase 4a: Alignment Subsystem
- [x] **Initialization:** Set explicit Time and Location in tests to ensure Driver and Test agree on sky coordinates.
- [x] **1-Star Alignment:** Verified that `Sync` establishes a reliable mapping at a single point.
- [x] **GOTO Accuracy:** Verified that RA/Dec GOTO is accurate after alignment.
- [x] **Multi-Star Alignment:** Verified multi-point database management and coordinate transformations.
- [ ] **Multi-Star Nuances:** Verify point rejection and database updates with 3+ points.
- [x] **Model Isolation:** Implemented fresh driver/simulator instance per test.

## Phase 4b: Imperfections & Compensation
- [x] **Homing:** Verified automated return to zero/index position.
- [x] **Backlash Compensation:** Verified `APPROACH_CONSTANT_OFFSET` logic (GOTO completes with anti-backlash approach).
- [ ] **Approach Sequence:** Verify exact slew sequence (fast-offset then slow-target) as in auxdrv.
- [ ] **Periodic Error:** Simulate PE (if possible) and verify tracking stability.

## Phase 5: Stability & Robustness
- [x] **Long-duration Tracking:** Observed background tracking loop in simulator (Issue 14).
- [ ] **Predictive Tracking (2nd Order):** Verify period guide rate updates in response to drift.
- [x] **Reconnection:** Verified recovery from lost connection.
