// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <indifilterwheel.h>
#include "libindi/indicom.h"
#include "libindi/connectionplugins/connectionserial.h"
#include "libindi/indipropertytext.h"
#include "libindi/indipropertynumber.h"
#include "libindi/indipropertyswitch.h"
#include <cstdint>
#include <vector>
#include <string>
#include <termios.h>
#include <queue>

enum class FWState : uint8_t { IDLE=0, CALIBRATING=1, MOVING=2, ERROR=3, UNKNOWN=255 };

// Command types for the queue
enum class CommandType { MOVE_TO_SLOT, CALIBRATE };

struct QueuedCommand {
    CommandType type;
    int target_slot;  // Used for MOVE_TO_SLOT, ignored for CALIBRATE

    QueuedCommand(CommandType t, int slot = 0) : type(t), target_slot(slot) {}
};

class openogma : public INDI::FilterWheel
{
public:
    openogma();
    virtual ~openogma() = default;

    const char *getDefaultName() override;

protected:
    // INDI lifecycle
    bool initProperties() override;
    bool updateProperties() override;
    void TimerHit() override;
    bool Handshake();

    // FilterWheel virtual methods
    bool SelectFilter(int position) override;
    int QueryFilter() override;

    // Property handlers
    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

private:
    // ---- Retry helper for transient USB/serial issues ----
    template <typename Fn>
    bool withRetry(int attempts, Fn&& op) {
        while (attempts--) {
            if (op()) return true;
            if (attempts > 0) {
                usleep(50000); // 50ms delay between retries
            }
        }
        return false;
    }

    // ---- Transport helpers (serial) ----
    bool detectProtocol(); // framed -> legacy -> text
    bool tryProtocolUpgrade(); // Attempt one-shot upgrade to better protocol when idle
    bool sendFramed(uint32_t cmd, int32_t val, int32_t &outVal);
    bool sendLegacy(uint32_t cmd, int32_t val, int32_t &outVal);
    bool sendText(const std::string &line, int32_t &outVal);
    bool getState(uint8_t &state, uint8_t &pos, uint8_t &slots);

    // ---- Protocol helpers ----
    uint8_t crcXor(const uint8_t *buf, size_t n);
    bool readExact(uint8_t *buf, size_t n, int timeout_ms);
    bool writeExact(const uint8_t *buf, size_t n);
    // Discard ASCII/debug noise until 0xA5 is seen; consumes the magic byte.
    bool syncToMagic(int timeout_ms, int maxScanBytes = 512);

    // ---- High-level ops ----
    bool cmdGetSlots(int &slots);
    bool cmdGetPosition(int &pos);
    bool cmdSetPosition(int target);       // target 1..N; use 0 to calibrate
    bool cmdCalibrate();
    bool cmdGetState(FWState &st, int &pos, int &slots);

    // ---- Command queue and state gating ----
    void enqueueCommand(CommandType type, int target_slot = 0);
    void processQueuedCommands();
    bool isDeviceBusy() const { return fwState != FWState::IDLE; }
    void clearCommandQueue();

    // ---- Sticky bounds and auto-refresh ----
    int clampSlot(int slot) const;
    void updateSlotBounds(int newTotalSlots);
    void updateFilterNames(int newTotalSlots);  // Manage filter name properties dynamically

    // ---- Hot-plug / stale FD resilience ----
    bool attemptReconnect();
    bool isSerialError(int tty_result) const;
    void preserveConnectionSettings();
    void restoreConnectionSettings();

    // ---- USB disconnect recovery ----
    void beginRecovery(const char* reason);
    bool doRecovery();
    void drainCommandQueue();

    // ---- State ----
    INDI::PropertySwitch CalibrationSP {1};

    int totalSlots = 0;
    int currentSlot = 0;  // 1-based for INDI consistency
    int targetSlot = 0;
    FWState fwState = FWState::ERROR;

    // Position normalization helpers and state
    // Map firmware pos (0-based; 0xFF = unknown) -> UI (1-based; -1 = unknown)
    inline int fwPosToUi(int fwPos0, int slots) {
        if (fwPos0 < 0 || fwPos0 == 0xFF || slots <= 0) return -1; // unknown
        if (fwPos0 >= slots) return -1; // out of range => unknown
        return fwPos0 + 1;
    }
    // Map UI (1-based) -> firmware (0-based)
    inline int uiPosToFw(int uiPos1) {
        if (uiPos1 <= 0) return -1;               // caller handles 0 as "calibrate"
        return uiPos1 - 1;
    }

    // Remember last good position to avoid showing garbage while moving
    int lastKnownSlot = 0; // 1-based; 0 means none yet

    enum class Proto { UNKNOWN, FRAMED, LEGACY, TEXT } proto = Proto::UNKNOWN;

    // Tiny command scheduler
    bool inFlight_ = false; // True after we dispatch a command; cleared when we observe IDLE again

    // Command queue for preventing overlapping moves
    std::queue<QueuedCommand> commandQueue;
    static const size_t MAX_QUEUE_SIZE = 2;  // Keep queue small to prevent delays

    // Adaptive polling for marginal communication links
    int pollMS = 200;              // Current polling interval (ms)
    static const int FAST_POLL_MS = 150;       // Fast polling during motion
    static const int NORMAL_POLL_MS = 200;     // Normal polling rate
    static const int BACKOFF_POLL_MS = 350;    // Slower rate during comm issues
    bool commBackoff = false;      // True when we're in backoff mode

    // Hot-plug resilience state
    bool reconnectInProgress = false;     // Prevent recursive reconnect attempts
    bool reconnectNeeded = false;         // Flag serious errors for reconnect attempt
    std::vector<std::string> savedFilterNames;  // Preserve filter names

    // USB disconnect recovery state
    enum class RecoveryState { NONE, IN_PROGRESS, WAIT_CALIBRATION } recoveryState = RecoveryState::NONE;
    time_t recoveryStartTime = 0;         // When recovery began
    static constexpr int RECOVERY_TIMEOUT_SEC = 60;  // Give up after 60s
    bool waitingForCalibration = false;   // True when firmware is auto-calibrating after reconnect
    int consecutiveCommFailures = 0;      // Track repeated communication failures
    bool suppressStartupSlotApply = false; // Swallow first FILTER_SLOT echo after connect

    // Protocol upgrade tracking
    time_t lastUpgradeAttempt = 0;
    static constexpr time_t UPGRADE_ATTEMPT_INTERVAL = 300; // Try upgrade every 5 minutes when idle

    // serial plugin
    Connection::Serial *serialConnection {nullptr};
};
