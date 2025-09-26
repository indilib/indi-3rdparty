// SPDX-License-Identifier: AGPL-3.0-or-later

#include "config.h"
#include "indi_openogma.h"
#include "libindi/indidevapi.h"
#include <memory>
#include <any>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <unistd.h>
#include <termios.h>
#include <ctime>

static std::unique_ptr<openogma> driver(new openogma());

openogma::openogma() : FilterSlotNP(1), FilterNameTP(8), ConnectionSP(2)
{
    setVersion(CDRIVER_VERSION_MAJOR, CDRIVER_VERSION_MINOR);
    
    // Initialize adaptive polling
    pollMS = NORMAL_POLL_MS;
    commBackoff = false;
    
    // Initialize hot-plug resilience
    reconnectInProgress = false;
    reconnectNeeded = false;
    
    // Initialize USB recovery state
    recoveryState = RecoveryState::NONE;
    recoveryStartTime = 0;
    waitingForCalibration = false;
    consecutiveCommFailures = 0;
}

const char *openogma::getDefaultName()
{
    return "OpenOGMA Filter Wheel";
}

bool openogma::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Add serial connection with proper settings
    serialConnection = new Connection::Serial(this);
    // Set baud rate BEFORE connecting - adjust as needed for your firmware
    // Available rates: B_9600, B_19200, B_38400, B_57600, B_115200, B_230400
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    serialConnection->registerHandshake([this]() { return Handshake(); });
    registerConnection(serialConnection);

    // Filter slot number (1-based)
    FilterSlotNP[0].fill("FILTER_SLOT", "Filter Slot", "%2.0f", 0, 255, 0, 1);
    FilterSlotNP.fill(getDeviceName(), "FILTER_SLOT", "Filter Slot", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    defineProperty(FilterSlotNP);

    // Optional filter names vector (initially sized to common maximum, will be adjusted after calibration)
    FilterNameTP.resize(8);  // Start with 8 slots (common maximum), will auto-adjust
    for (int i = 0; i < 8; ++i)
    {
        FilterNameTP[i].fill(("FILTER_NAME_" + std::to_string(i+1)).c_str(), 
                            ("Filter " + std::to_string(i+1)).c_str(), 
                            ("Filter " + std::to_string(i+1)).c_str());
    }
    FilterNameTP.fill(getDeviceName(), "FILTER_NAME", "Filter Names", FILTER_TAB, IP_RW, 60, IPS_IDLE);
    defineProperty(FilterNameTP);

    // Calibration button
    CalibrationSP[0].fill("CALIBRATE", "Calibrate", ISS_OFF);
    CalibrationSP.fill(getDeviceName(), "CALIBRATION", "Calibration", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    defineProperty(CalibrationSP);

    setDriverInterface(FILTER_INTERFACE); // advertise as filter wheel
    SetTimer(1000);
    return true;
}

bool openogma::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    return true;
}

bool openogma::Handshake()
{
    LOG_INFO("Starting handshake with OpenOGMA Filter Wheel...");
    
    // Check if we have a valid file descriptor
    int fd = serialConnection->getPortFD();
    if (fd < 0)
    {
        LOG_ERROR("Invalid file descriptor for serial connection.");
        return false;
    }
    
    LOGF_INFO("Serial connection established on FD %d", fd);
    
    // Flush any stale input/output data before protocol detection
    LOG_INFO("Flushing serial buffers...");
    tcflush(fd, TCIOFLUSH);
    
    if (!detectProtocol())
    {
        LOG_ERROR("Failed to detect protocol. Device may not be responding or using unsupported protocol.");
        return false;
    }

    LOGF_INFO("Protocol detected: %s", 
        (proto == Proto::FRAMED) ? "FRAMED" :
        (proto == Proto::LEGACY) ? "LEGACY" :
        (proto == Proto::TEXT) ? "TEXT" : "UNKNOWN");

    // Initialize cached state
    FWState initialState;
    int initialPos, initialSlots;
    if (!cmdGetState(initialState, initialPos, initialSlots))
    {
        LOG_WARN("Could not get initial device state, but protocol detection succeeded.");
    }
    else
    {
        fwState = initialState;
        updateSlotBounds(initialSlots);  // Use helper to set initial bounds and filter names
        currentSlot = initialPos;
        
        LOGF_INFO("Initial state: slots=%d, current=%d, state=%d", 
                  totalSlots, currentSlot, static_cast<int>(fwState));
                  
        if (totalSlots > 0)
        {
            LOG_INFO("Filter wheel is calibrated and filter names have been auto-sized.");
        }
    }
    
    if (totalSlots <= 0) // uncalibrated
    {
        LOG_WARN("Wheel not calibrated. Filter names will auto-size after calibration completes.");
        // Keep initial 8-slot sizing until calibration provides actual count
    }

    LOG_INFO("Connected to OGMA Filter Wheel.");
    
    // Tell clients (Ekos Setup panel) in one friendly line
    // Use a format similar to other INDI devices that Ekos recognizes
    IDMessage(getDeviceName(),
              "OpenOGMA Filter Wheel connected. Protocol: %s, Slots: %d, Current: %d",
              (proto == Proto::FRAMED ? "FRAMED" :
               proto == Proto::LEGACY ? "LEGACY" :
               proto == Proto::TEXT   ? "TEXT"   : "UNKNOWN"),
              totalSlots,
              lastKnownSlot > 0 ? lastKnownSlot : currentSlot);
    
    return true;
}

bool openogma::detectProtocol()
{
    LOG_INFO("Probing device protocol (order: FRAMED → LEGACY → TEXT)...");
    
    // Some devices need a moment to stabilize after connection
    usleep(500000); // 500ms delay
    
    // Enhanced wake-up for FRAMED protocol detection
    LOG_DEBUG("Sending wake-up sequences for binary protocols...");
    uint8_t wake_data[] = {0x00, 0xFF, 0x00, 0xFF, 0xA5, 0x24}; // Enhanced wake-up pattern
    writeExact(wake_data, sizeof(wake_data));
    usleep(200000); // 200ms delay
    
    // Try FRAMED protocol first (preferred - most robust with CRC validation)
    LOG_DEBUG("Trying FRAMED protocol (preferred binary with CRC)...");
    int32_t slotsProbe = 0;
    if (sendFramed(0x1002, 0, slotsProbe) && slotsProbe > 0)
    {
        LOG_INFO("Protocol selected: FRAMED (binary with CRC).");
        totalSlots = slotsProbe; // sane
        proto = Proto::FRAMED;
        return true;
    }
    LOG_DEBUG("FRAMED probe: no response (trying fallback protocols).");
    
    // Add delay between attempts
    usleep(300000); // 300ms delay
    
    // Try LEGACY protocol (binary fallback)
    LOG_DEBUG("Trying LEGACY protocol (binary fallback)...");
    int tmp = -99;
    int32_t outVal = 0;
    if (sendLegacy(0x1002, 0, outVal)) // FW_SLOT GET
    {
        LOG_INFO("Protocol selected: LEGACY (8-byte binary).");
        totalSlots = outVal;
        proto = Proto::LEGACY;
        return true;
    }
    LOG_DEBUG("LEGACY probe: no response (trying final fallback).");
    
    // Add delay before final fallback
    usleep(300000); // 300ms delay
    
    // Try TEXT protocol (final fallback - always works)
    LOG_DEBUG("Trying TEXT protocol (final fallback)...");
    if (sendText("SLOTS\n", tmp))
    {
        LOG_WARN("Protocol selected: TEXT (fallback - consider upgrading device firmware).");
        totalSlots = tmp;
        proto = Proto::TEXT;
        return true;
    }
    LOG_DEBUG("TEXT probe: no response.");
    
    LOG_ERROR("All protocol detection attempts failed.");
    return false;
}

bool openogma::tryProtocolUpgrade()
{
    // Only attempt upgrade during idle periods and not too frequently
    time_t now = time(nullptr);
    if (now - lastUpgradeAttempt < UPGRADE_ATTEMPT_INTERVAL)
        return false;
    
    lastUpgradeAttempt = now;
    
    // Only upgrade if we're currently using a lower-tier protocol
    if (proto == Proto::FRAMED) {
        return false; // Already using best protocol
    }
    
    // Cache current protocol state
    Proto originalProto = proto;
    
    // Try to upgrade to FRAMED if we're using LEGACY or TEXT
    if (proto == Proto::LEGACY || proto == Proto::TEXT) {
        LOG_DEBUG("Attempting one-shot upgrade to FRAMED protocol...");
        
        // Get serial file descriptor
        int fd = serialConnection->getPortFD();
        if (fd <= 0) return false;
        
        // Clear buffers for clean upgrade attempt
        tcflush(fd, TCIOFLUSH);
        usleep(100000); // 100ms settle time
        
        // Send enhanced wake-up pattern for FRAMED
        uint8_t wakeUp[] = {0x00, 0xFF, 0x00, 0xFF, 0xA5, 0x24};
        writeExact(wakeUp, sizeof(wakeUp));
        usleep(150000); // 150ms for device processing
        
        // Test FRAMED with SLOTS command (0x1002)
        int32_t slots = 0;
        if (sendFramed(0x1002, 0, slots) && slots > 0) {
            proto = Proto::FRAMED;
            LOG_INFO("Protocol successfully upgraded to FRAMED (binary with CRC).");
            return true;
        }
        
        // Restore original protocol on failure
        proto = originalProto;
        LOG_DEBUG("Protocol upgrade attempt failed, staying with current protocol.");
    }
    
    return false;
}

bool openogma::Connect()
{
    bool connected = INDI::DefaultDevice::Connect();
    
    if (connected)
    {
        // Send connection confirmation message that Ekos will display
        // Match the format that Ekos expects and shows in Setup panel
        IDMessage(getDeviceName(), "%s is online.", getDeviceName());
    }
    
    return connected;
}

bool openogma::Disconnect()
{
    // Clear any pending commands when disconnecting
    clearCommandQueue();
    
    // Reset adaptive polling state
    pollMS = NORMAL_POLL_MS;
    commBackoff = false;
    
    // Reset USB recovery state
    recoveryState = RecoveryState::NONE;
    recoveryStartTime = 0;
    waitingForCalibration = false;
    consecutiveCommFailures = 0;
    
    return INDI::DefaultDevice::Disconnect();
}

void openogma::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(1000);
        return;
    }
    
    // USB disconnect recovery takes priority over normal polling
    if (recoveryState != RecoveryState::NONE)
    {
        if (doRecovery())
        {
            // Recovery completed successfully
            LOG_DEBUG("USB recovery completed, resuming normal operation");
        }
        // Continue recovery on next timer cycle (either still in progress or failed and retrying)
        SetTimer(1000); // Use 1 second intervals during recovery
        return;
    }
    
    // Legacy hot-plug recovery (kept for compatibility, but USB recovery is preferred)
    if (reconnectNeeded && !reconnectInProgress)
    {
        reconnectNeeded = false;
        if (attemptReconnect())
        {
            // Reconnection successful, resume normal operation
            SetTimer(pollMS);
            return;
        }
        else
        {
            // Reconnection failed, continue with longer interval
            SetTimer(1000);
            return;
        }
    }

    FWState st; int pos=0, slots=0;
    bool commSuccess = cmdGetState(st, pos, slots);
    
    if (commSuccess)
    {
        // Communication successful - update state and reset backoff if needed
        consecutiveCommFailures = 0; // Reset failure counter on success
        
        if (commBackoff)
        {
            commBackoff = false;
            LOG_DEBUG("Communication recovered, returning to adaptive polling rate.");
        }
        
        fwState = st;
        updateSlotBounds(slots);  // Use helper to update bounds and properties
        
        // pos is 1-based here; -1 means "unknown / moving"
        if (pos > 0) {
            lastKnownSlot = pos;           // update cache only on valid, known pos
            currentSlot   = pos;           // keep your variable 1-based too
        }

        // If we were moving/calibrating, publish progress and completion
        if (fwState == FWState::IDLE)
        {
            // Clear in-flight flag when device returns to IDLE
            if (inFlight_)
            {
                LOG_DEBUG("Command completed (state=IDLE), clearing inFlight");
                inFlight_ = false;
            }
            // Check if we just completed firmware auto-calibration after USB reconnect
            if (waitingForCalibration && totalSlots > 0)
            {
                waitingForCalibration = false;
                LOG_INFO("Firmware auto-calibration complete after USB reconnect. Device ready.");
                IDMessage(getDeviceName(), "Calibration complete. Ready.");
            }
            
            // Update calibration switch state when returning to idle
            if (CalibrationSP.getState() == IPS_BUSY)
            {
                CalibrationSP[0].setState(ISS_OFF);
                CalibrationSP.setState(IPS_OK);
                CalibrationSP.apply();
                LOG_DEBUG("Calibration switch returned to OK state");
            }
            
            if (targetSlot > 0 && currentSlot == targetSlot)
                LOG_INFO("Move complete.");
            targetSlot = 0;
            
            // Process any queued commands when device returns to idle
            processQueuedCommands();
        }
        else if (fwState == FWState::CALIBRATING && waitingForCalibration)
        {
            // Update user on firmware calibration progress
            static time_t lastCalibrationUpdate = 0;
            time_t now = time(nullptr);
            if (now - lastCalibrationUpdate >= 5) // Update every 5 seconds
            {
                lastCalibrationUpdate = now;
                LOGF_DEBUG("Firmware auto-calibration in progress (slots detected: %d)", totalSlots);
            }
        }
        
        // Set adaptive polling rate based on device state and communication health
        int newPollMS;
        if (commBackoff)
        {
            newPollMS = BACKOFF_POLL_MS;  // Slow polling during comm issues
        }
        else if (fwState == FWState::MOVING || fwState == FWState::CALIBRATING)
        {
            newPollMS = FAST_POLL_MS;     // Fast polling during motion
        }
        else
        {
            newPollMS = NORMAL_POLL_MS;   // Normal polling when idle
        }
        
        // Log polling rate changes for debugging
        if (newPollMS != pollMS)
        {
            LOGF_DEBUG("Polling rate changed: %d→%d ms (%s)", pollMS, newPollMS,
                       commBackoff ? "backoff" :
                       (fwState == FWState::MOVING || fwState == FWState::CALIBRATING) ? "motion" : "idle");
        }
        pollMS = newPollMS;
        
        // Attempt protocol upgrade during idle periods (not moving or in backoff)
        if (!commBackoff && fwState != FWState::MOVING && fwState != FWState::CALIBRATING)
        {
            tryProtocolUpgrade(); // Silent attempt, no error handling needed
        }
        
        // Update the property only with a known value (hide "255 while moving")
        if (lastKnownSlot > 0) {
            FilterSlotNP[0].setValue(lastKnownSlot);
            FilterSlotNP.setState(IPS_OK);
            FilterSlotNP.apply();
        }
        
        // If moving/calibrating, don't overwrite with unknowns — just update state text/logs.
        // When st returns to IDLE and pos becomes valid again, the block above refreshes the UI.
    }
    else
    {
        // Communication failed - activate backoff if not already active
        if (!commBackoff)
        {
            commBackoff = true;
            pollMS = BACKOFF_POLL_MS;
            LOG_DEBUG("Communication timeout detected, reducing polling rate to ease link pressure.");
        }
        
        // Check for repeated communication failures (potential USB disconnect)
        consecutiveCommFailures++;
        
        if (consecutiveCommFailures >= 3) // After 3 consecutive failures
        {
            consecutiveCommFailures = 0; // Reset counter
            LOG_WARN("Multiple consecutive communication failures detected, triggering USB recovery");
            beginRecovery("repeated communication timeouts");
            return;
        }
        
        // Don't update properties on communication failure to avoid spurious alerts
        LOG_DEBUG("Skipping property update due to communication failure.");
    }

    // Use adaptive polling rate based on communication health
    SetTimer(pollMS);
}

bool openogma::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!isConnected() || strcmp(dev, getDeviceName())) return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);

    if (FilterSlotNP.isNameMatch(name))
    {
        int requested = static_cast<int>(values[0]);
        
        // Handle calibration request (slot 0)
        if (requested == 0)
        {
            enqueueCommand(CommandType::CALIBRATE);
            LOG_INFO("Calibration command queued.");
            FilterSlotNP.setState(IPS_BUSY);
            FilterSlotNP.apply();
            return true;
        }

        // Handle uncalibrated wheel - auto-calibrate when requesting any slot > 0
        if (totalSlots == 0 && requested > 0)
        {
            LOG_INFO("Calibration required—starting auto-calibration before move.");
            enqueueCommand(CommandType::CALIBRATE);
            // Queue the move command after calibration
            enqueueCommand(CommandType::MOVE_TO_SLOT, requested);
            LOGF_INFO("Will move to slot %d after calibration completes.", requested);
            FilterSlotNP.setState(IPS_BUSY);
            FilterSlotNP.apply();
            return true;
        }

        // Sticky bounds - clamp to valid range instead of rejecting
        int clamped = requested;
        if (requested < 1)
        {
            clamped = 1;
            LOGF_WARN("Slot %d out of range, clamping to minimum slot 1.", requested);
        }
        else if (totalSlots > 0 && requested > totalSlots)
        {
            clamped = totalSlots;
            LOGF_WARN("Slot %d out of range, clamping to maximum slot %d.", requested, totalSlots);
        }

        // Update the property to reflect the clamped value
        if (clamped != requested)
        {
            FilterSlotNP[0].setValue(clamped);
            FilterSlotNP.apply();
        }

        // Execute the (possibly clamped) move request
        enqueueCommand(CommandType::MOVE_TO_SLOT, clamped);
        LOGF_INFO("Move to slot %d command queued.", clamped);
        FilterSlotNP.setState(IPS_BUSY);
        FilterSlotNP.apply();
        return true;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool openogma::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (FilterNameTP.isNameMatch(name))
    {
        FilterNameTP.update(texts, names, n);
        FilterNameTP.setState(IPS_OK);
        FilterNameTP.apply();
        return true;
    }
    
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool openogma::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!isConnected() || strcmp(dev, getDeviceName())) 
        return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);

    if (CalibrationSP.isNameMatch(name))
    {
        CalibrationSP.update(states, names, n);
        
        // Check if the calibrate switch was turned on
        if (CalibrationSP[0].getState() == ISS_ON)
        {
            enqueueCommand(CommandType::CALIBRATE);
            LOG_INFO("Calibration command queued from calibration button.");
            CalibrationSP.setState(IPS_BUSY);
            CalibrationSP.apply();
            return true;
        }
        
        CalibrationSP.setState(IPS_OK);
        CalibrationSP.apply();
        return true;
    }
    
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

// -------------------------- Protocol --------------------------
uint8_t openogma::crcXor(const uint8_t *buf, size_t n)
{
    uint8_t c = 0;
    for (size_t i=0; i<n; ++i) c ^= buf[i];
    return c;
}

bool openogma::readExact(uint8_t *buf, size_t n, int timeout_ms)
{
    int got = 0;
    int total_timeout = timeout_ms / 1000; // Convert to seconds
    if (total_timeout < 2) total_timeout = 2; // Minimum 2 seconds for device response
    
    LOGF_DEBUG("readExact: attempting to read %zu bytes with %d second timeout", n, total_timeout);
    
    while (got < (int)n)
    {
        int nbytes_read = 0;
        int result = tty_read(serialConnection->getPortFD(), 
                             reinterpret_cast<char*>(buf + got), 
                             n - got, 
                             total_timeout, 
                             &nbytes_read);
        
        if (result != TTY_OK)
        {
            const char* error_name = "UNKNOWN";
            switch(result) {
                case TTY_OK: error_name = "TTY_OK"; break;
                case TTY_READ_ERROR: error_name = "TTY_READ_ERROR"; break;
                case TTY_WRITE_ERROR: error_name = "TTY_WRITE_ERROR"; break;
                case TTY_SELECT_ERROR: error_name = "TTY_SELECT_ERROR"; break;
                case TTY_TIME_OUT: error_name = "TTY_TIME_OUT"; break;
                case TTY_PORT_FAILURE: error_name = "TTY_PORT_FAILURE"; break;
                case TTY_PARAM_ERROR: error_name = "TTY_PARAM_ERROR"; break;
                case TTY_ERRNO: error_name = "TTY_ERRNO"; break;
                case TTY_OVERFLOW: error_name = "TTY_OVERFLOW"; break;
            }
            
            // Check for serious serial errors that indicate USB disconnect
            if (isSerialError(result))
            {
                LOGF_ERROR("readExact: serious serial error %s (%d) - may indicate USB disconnect", 
                           error_name, result);
                // Trigger USB recovery
                beginRecovery("read error during communication");
            }
            else
            {
                LOGF_ERROR("readExact: tty_read failed with %s (%d) after reading %d/%zu bytes", 
                           error_name, result, got, n);
            }
            return false;
        }
            
        got += nbytes_read;
        LOGF_DEBUG("readExact: read %d bytes, total %d/%zu", nbytes_read, got, n);
        
        if (nbytes_read == 0)
            usleep(1000); // small delay if no data
    }
    
    LOGF_DEBUG("readExact: successfully read %d bytes", got);
    return true;
}

bool openogma::writeExact(const uint8_t *buf, size_t n)
{
    LOGF_DEBUG("writeExact: attempting to write %zu bytes", n);
    
    int nbytes_written = 0;
    int result = tty_write(serialConnection->getPortFD(), 
                          reinterpret_cast<const char*>(buf), 
                          n, 
                          &nbytes_written);
    
    if (result != TTY_OK)
    {
        // Log detailed error information for debugging
        LOGF_DEBUG("writeExact: tty_write returned code %d, wrote %d/%zu bytes", 
                   result, nbytes_written, n);
        
        // Check for serious serial errors that indicate USB disconnect
        if (isSerialError(result))
        {
            LOGF_ERROR("writeExact: USB disconnect detected (error %d) - triggering recovery", result);
            // Trigger USB recovery
            beginRecovery("write error during communication");
        }
        else
        {
            LOGF_ERROR("writeExact: tty_write failed with code %d, wrote %d/%zu bytes", 
                       result, nbytes_written, n);
        }
        return false;
    }
    
    if (nbytes_written != (int)n)
    {
        LOGF_ERROR("writeExact: incomplete write, wrote %d/%zu bytes", nbytes_written, n);
        
        // Treat 0 bytes written as a potential USB disconnect (like error code -2)
        if (nbytes_written == 0)
        {
            LOG_WARN("writeExact: zero bytes written may indicate USB disconnect");
            beginRecovery("zero bytes written during communication");
        }
        
        return false;
    }
    
    LOGF_DEBUG("writeExact: successfully wrote %d bytes", nbytes_written);
    return true;
}

// Discard any ASCII/debug text and non-magic bytes until we see 0xA5 (frame magic).
// Returns true if 0xA5 was found before timeout/scan limit. The magic byte is consumed.
bool openogma::syncToMagic(int timeout_ms, int maxScanBytes)
{
    int scanned = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    uint8_t b = 0;

    while (std::chrono::steady_clock::now() < deadline && scanned < maxScanBytes)
    {
        int nbytes_read = 0;
        int result = tty_read(serialConnection->getPortFD(),
                              reinterpret_cast<char*>(&b),
                              1,
                              0, // non-blocking poll
                              &nbytes_read);

        if (result != TTY_OK)
        {
            if (isSerialError(result))
            {
                LOGF_ERROR("syncToMagic: serial error %d while scanning for magic", result);
                beginRecovery("serial error during sync");
                return false;
            }
            // Not an error we care about for sync; just wait a bit
            usleep(1000);
            continue;
        }

        if (nbytes_read <= 0)
        {
            // No data right now; yield briefly
            usleep(1000);
            continue;
        }

        scanned++;

        // Skip printable ASCII and common whitespace used by debug logs
        if ((b >= 0x20 && b <= 0x7E) || b == '\n' || b == '\r' || b == '\t')
            continue;

        if (b == 0xA5)
            return true; // Found frame start

        // Non-ASCII junk that isn't magic: keep scanning
    }

    LOGF_DEBUG("syncToMagic: aborted after scanning %d bytes without finding 0xA5", scanned);
    return false;
}

// Reads the header first, then read the rest based on the Length byte.
bool openogma::sendFramed(uint32_t cmd, int32_t val, int32_t &outVal)
{
    uint8_t tx[11];
    tx[0] = 0xA5;
    tx[1] = 0x08; // payload length only (8 bytes: cmd + value)
    std::memcpy(tx + 2, &cmd, 4);
    std::memcpy(tx + 6, &val, 4);
    tx[10] = crcXor(tx, 10);

    if (!writeExact(tx, sizeof(tx)))
        return false;

    usleep(50 * 1000); // 50 ms pause like ASCOM

    // Robust framed read with binary-first resync and CRC guard
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (std::chrono::steady_clock::now() < deadline)
    {
        // Sync to magic, discarding ASCII/debug noise
        if (!syncToMagic(500 /*ms*/, 512))
            continue; // try again until deadline

        uint8_t len = 0;
        if (!readExact(&len, 1, 500))
            continue; // resync again

        // Total frame size includes header (2) + payload (len) + crc (1)
        const size_t total = 2 + len + 1;
        if (total != 11 && total != 15)
        {
            LOGF_DEBUG("sendFramed: invalid length %u, resyncing", (unsigned)len);
            continue; // resync to next magic
        }

        std::vector<uint8_t> rx(total);
        rx[0] = 0xA5; rx[1] = len;
        if (!readExact(rx.data() + 2, total - 2, 800))
            continue; // resync to next frame

        // CRC check
        if (rx.back() != crcXor(rx.data(), total - 1))
        {
            // Log first few bytes for debugging then try to resync
            size_t dumpLen = std::min(total, static_cast<size_t>(16));
            std::string hexDump;
            for (size_t i = 0; i < dumpLen; i++)
            {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X ", rx[i]);
                hexDump += hex;
            }
            if (total > 16) hexDump += "...";
            LOGF_DEBUG("sendFramed: CRC mismatch (len=%u): %s — resyncing", (unsigned)len, hexDump.c_str());
            continue; // keep hunting for a good frame until deadline
        }

        // Good frame
        if (len == 0x08)
        {
            std::memcpy(&outVal, rx.data() + 6, 4);
        }
        else // len == 0x0C
        {
            uint8_t state = rx[6], pos = rx[7], slots = rx[8];
            outVal = (int32_t)((uint32_t)state | ((uint32_t)pos << 8) | ((uint32_t)slots << 16));
        }
        return true;
    }

    LOG_DEBUG("sendFramed: timed out waiting for valid framed response");
    return false;
}

bool openogma::sendLegacy(uint32_t cmd, int32_t val, int32_t &outVal)
{
    uint8_t tx[8];
    std::memcpy(tx, &cmd, 4);
    std::memcpy(tx+4, &val, 4);
    if (!writeExact(tx, 8)) return false;

    uint8_t rx[8];
    if (!readExact(rx, 8, 3000)) return false; // 3 second timeout
    std::memcpy(&outVal, rx+4, 4);
    return true;
}

// Send CRLF for text, and give it time to respond.
bool openogma::sendText(const std::string &line, int32_t &outVal)
{
    // Always send CRLF - strip any trailing CR/LF, then add CRLF
    std::string wire = line;
    while (!wire.empty() && (wire.back() == '\n' || wire.back() == '\r')) 
        wire.pop_back();
    wire += "\r\n";

    LOGF_DEBUG("sendText: sending '%s' (%zu bytes)", line.c_str(), wire.size());

    if (!writeExact(reinterpret_cast<const uint8_t*>(wire.data()), wire.size()))
        return false;

    char resp[256] = {0};
    int nbytes_read = 0;
    // allow up to 2 seconds to get a line
    int result = tty_read_section(serialConnection->getPortFD(), resp, '\n', 2, &nbytes_read);

    if (result != TTY_OK || nbytes_read == 0)
    {
        LOGF_DEBUG("sendText: no response or timeout, result=%d, nbytes_read=%d", result, nbytes_read);
        return false;
    }

    LOGF_DEBUG("sendText: received response '%s' (%d bytes)", resp, nbytes_read);
    outVal = std::atoi(resp);
    return true;
}

bool openogma::getState(uint8_t &state, uint8_t &pos, uint8_t &slots)
{
    // Retry FRAMED GET state operation since it's idempotent
    return withRetry(2, [&]() -> bool {
        int32_t out = 0;
        if (!sendFramed(0x1003, 0, out)) return false;
        uint32_t packed = static_cast<uint32_t>(out);
        state = packed & 0xFF;
        pos = (packed >> 8) & 0xFF;
        slots = (packed >> 16) & 0xFF;
        return true;
    });
}

bool openogma::cmdGetState(FWState &st, int &pos, int &slots)
{
    // Retry GET operations since they're idempotent and safe to repeat
    return withRetry(2, [&]() -> bool {
        if (proto == Proto::FRAMED)
        {
            uint8_t s=0,p0=0,sl=0;
            if (!getState(s,p0,sl)) return false;
            st = static_cast<FWState>(s);
            slots = sl;
            pos = fwPosToUi(p0, sl);   // Convert 0-based+255 to 1-based or -1
            return true;
        }
        else if (proto == Proto::LEGACY)
        {
            int32_t out=0;
            if (!sendLegacy(0x1001, -1, out)) return false; // GET current position
            int32_t out2=0;
            if (!sendLegacy(0x1002, 0, out2)) return false;  // GET slot count
            slots = out2;
            pos = fwPosToUi(out, slots);  // Convert firmware 0-based pos to UI 1-based
            // State approximation
            st = (out == 0xFF) ? FWState::MOVING : 
                 (out <= 0 && out != 0xFF) ? FWState::CALIBRATING : FWState::IDLE;
            return true;
        }
        else if (proto == Proto::TEXT)
        {
            int32_t v=0;
            if (!sendText("STATUS\n", v)) return false; // allow firmware to print e.g., 0..3
            st = static_cast<FWState>(v);
            if (!sendText("POS\n", v)) return false;
            int p0 = v;  // firmware's raw 0-based pos (or 255 while moving)
            if (!sendText("SLOTS\n", v)) return false;
            slots = v;
            pos = fwPosToUi(p0, slots);  // Convert firmware pos to UI 1-based
            return true;
        }
        return false;
    });
}

bool openogma::cmdGetSlots(int &slots)
{
    // Retry GET operations since they're idempotent
    return withRetry(2, [&]() -> bool {
        if (proto == Proto::FRAMED || proto == Proto::LEGACY)
        {
            int32_t out=0;
            bool ok = (proto==Proto::FRAMED)
                        ? sendFramed(0x1002, 0, out)
                        : sendLegacy(0x1002, 0, out);
            if (!ok) return false;
            slots = out;
            return true;
        }
        else
        {
            int32_t v=0;
            if (!sendText("SLOTS\n", v)) return false;
            slots = v;
            return true;
        }
    });
}

bool openogma::cmdGetPosition(int &pos)
{
    // Retry GET operations since they're idempotent
    return withRetry(2, [&]() -> bool {
        if (proto == Proto::FRAMED || proto == Proto::LEGACY)
        {
            int32_t out=0;
            bool ok = (proto==Proto::FRAMED)
                        ? sendFramed(0x1001, -1, out)  // GET
                        : sendLegacy(0x1001, -1, out);
            if (!ok) return false;
            pos = out;
            return true;
        }
        else
        {
            int32_t v=0;
            if (!sendText("POS\n", v)) return false;
            pos = v;
            return true;
        }
    });
}

bool openogma::cmdSetPosition(int target)
{
    // target is UI 1-based, convert to firmware 0-based
    int fwTarget = uiPosToFw(target);
    if (fwTarget < 0) return false; // Invalid target
    
    // Retry SET operations since they're ACK-based and position is tracked via polling
    return withRetry(2, [&]() -> bool {
        int32_t out=0;
        bool ok = false;
        if (proto == Proto::FRAMED)
            ok = sendFramed(0x1001, fwTarget, out);
        else if (proto == Proto::LEGACY)
            ok = sendLegacy(0x1001, fwTarget, out);
        else // TEXT
            ok = sendText(std::string("POS ") + std::to_string(fwTarget) + "\n", out);

        // Treat response as ACK (0 = success or current pos), motion observed via state polling
        return ok;
    });
}

bool openogma::cmdCalibrate()
{
    LOG_INFO("Sending calibration command to firmware...");
    
    // Retry calibration command since it's ACK-based and safe to repeat
    bool result = withRetry(2, [&]() -> bool {
        int32_t out=0;
        bool ok = false;
        if (proto == Proto::FRAMED || proto == Proto::LEGACY)
        {
            // Correct calibration: CMD_POSITION (0x1001) with value -1 triggers full calibration
            LOGF_INFO("Sending binary calibration: protocol=%s, cmd=0x1001 (CMD_POSITION), value=-1 (calibrate)", 
                      (proto == Proto::FRAMED) ? "FRAMED" : "LEGACY");
            ok = (proto == Proto::FRAMED) 
                ? sendFramed(0x1001, -1, out)  // -1 triggers calibration
                : sendLegacy(0x1001, -1, out);
            LOGF_INFO("Binary calibration result: ok=%s, response=%d", ok ? "true" : "false", out);
        }
        else // TEXT
        {
            LOG_INFO("Sending text calibration: CALIBRATE");
            ok = sendText("CALIBRATE\n", out);
            LOGF_INFO("Text calibration result: ok=%s, response=%d", ok ? "true" : "false", out);
        }
        return ok;
    });
    
    LOGF_INFO("Calibration command %s", result ? "succeeded" : "failed");
    return result;
}

void openogma::enqueueCommand(CommandType type, int target_slot)
{
    // CALIBRATE has priority: clear pending moves
    if (type == CommandType::CALIBRATE && !commandQueue.empty())
    {
        drainCommandQueue(); // keep at most last move, but calibration overrides moves
        while (!commandQueue.empty()) commandQueue.pop();
    }

    // If device is idle or unknown and nothing is in-flight, execute immediately
    const bool readyNow = (fwState == FWState::IDLE || fwState == FWState::UNKNOWN) && !inFlight_;
    if (readyNow)
    {
        LOGF_DEBUG("Device idle, executing command immediately: %s", 
                   (type == CommandType::MOVE_TO_SLOT) ? "MOVE" : "CALIBRATE");
        
        if (type == CommandType::MOVE_TO_SLOT)
        {
            targetSlot = target_slot;
            // If we don't know slots yet, defer by queuing instead of immediate execute
            if (totalSlots <= 0)
            {
                LOG_DEBUG("Unknown slot count; deferring immediate MOVE and queueing it");
                commandQueue.push(QueuedCommand(CommandType::MOVE_TO_SLOT, target_slot));
            }
            else
            {
                inFlight_ = true;
                cmdSetPosition(target_slot);
            }
        }
        else if (type == CommandType::CALIBRATE)
        {
            targetSlot = 0;
            waitingForCalibration = true; // Gate moves until calibration provides slots
            inFlight_ = true;
            cmdCalibrate();
        }
        return;
    }
    
    // Device is busy, check queue space
    if (commandQueue.size() >= MAX_QUEUE_SIZE)
    {
        LOG_WARN("Command queue full, rejecting new command to prevent delays. Please wait for current operation to complete.");
        return;
    }
    
    // Enqueue the command
    commandQueue.push(QueuedCommand(type, target_slot));
    LOGF_INFO("Device busy (%s), command queued (%zu/%zu): %s%s", 
              (fwState == FWState::MOVING) ? "moving" : 
              (fwState == FWState::CALIBRATING) ? "calibrating" : "busy",
              commandQueue.size(), MAX_QUEUE_SIZE,
              (type == CommandType::MOVE_TO_SLOT) ? "MOVE to slot " : "CALIBRATE",
              (type == CommandType::MOVE_TO_SLOT) ? std::to_string(target_slot).c_str() : "");
}

void openogma::processQueuedCommands()
{
    // Don't process commands during recovery or if waiting for firmware calibration
    if (recoveryState != RecoveryState::NONE || waitingForCalibration)
    {
        if (!commandQueue.empty())
        {
            LOGF_DEBUG("Deferring command processing: recovery=%d, waitingCalibration=%s (queue size: %zu)", 
                       static_cast<int>(recoveryState), 
                       waitingForCalibration ? "true" : "false", 
                       commandQueue.size());
        }
        return;
    }
    
    // Process only when device is idle or unknown, and nothing is in flight
    if (((fwState != FWState::IDLE) && (fwState != FWState::UNKNOWN)) || commandQueue.empty() || inFlight_)
    {
        if ((fwState != FWState::IDLE && fwState != FWState::UNKNOWN) && !commandQueue.empty())
        {
            LOGF_DEBUG("Device busy (state=%d), will process %zu queued command(s) once idle", 
                       static_cast<int>(fwState), commandQueue.size());
        }
        return;
    }

    // Peek at the next command, but only pop when we actually dispatch it
    const QueuedCommand &cmd = commandQueue.front();

    // Decide if command can run now
    bool canRun = false;
    if (cmd.type == CommandType::CALIBRATE)
    {
        // Always allow calibration when idle, even if slot count is unknown (0)
        canRun = true;
    }
    else if (cmd.type == CommandType::MOVE_TO_SLOT)
    {
        // Only move when we have a valid slot count
        canRun = (totalSlots > 0);
        if (!canRun)
        {
            LOG_DEBUG("Deferring MOVE command until slot count becomes known (waiting for calibration)");
            return; // keep it queued; will retry after calibration updates slot count
        }
    }

    if (!canRun)
        return;

    // Dispatch
    LOGF_INFO("Processing queued command: %s%s", 
              (cmd.type == CommandType::MOVE_TO_SLOT) ? "MOVE to slot " : "CALIBRATE",
              (cmd.type == CommandType::MOVE_TO_SLOT) ? std::to_string(cmd.target_slot).c_str() : "");

    if (cmd.type == CommandType::MOVE_TO_SLOT)
    {
        targetSlot = cmd.target_slot;
        inFlight_ = true;
        cmdSetPosition(cmd.target_slot);
    }
    else // CALIBRATE
    {
        targetSlot = 0;
        waitingForCalibration = true;
        inFlight_ = true;
        cmdCalibrate();
    }

    // Remove the command after dispatching
    commandQueue.pop();
}

void openogma::clearCommandQueue()
{
    if (!commandQueue.empty())
    {
        LOGF_INFO("Clearing command queue (%zu pending commands)", commandQueue.size());
        while (!commandQueue.empty())
            commandQueue.pop();
    }
}

int openogma::clampSlot(int slot) const
{
    if (totalSlots <= 0) return slot;  // Can't clamp if we don't know bounds
    
    if (slot < 1) return 1;
    if (slot > totalSlots) return totalSlots;
    return slot;
}

void openogma::updateSlotBounds(int newTotalSlots)
{
    if (newTotalSlots != totalSlots && newTotalSlots > 0)
    {
        int oldSlots = totalSlots;
        totalSlots = newTotalSlots;
        
        LOGF_INFO("Slot bounds updated: %d → %d slots", oldSlots, totalSlots);
        
        // Update filter names to match new slot count
        updateFilterNames(newTotalSlots);
    }
}

void openogma::updateFilterNames(int newTotalSlots)
{
    // Safety check: reasonable bounds for filter wheels (1-16 slots typical)
    if (newTotalSlots <= 0 || newTotalSlots > 16)
    {
        LOGF_WARN("Unusual slot count (%d), capping filter names to reasonable range", newTotalSlots);
        newTotalSlots = std::max(1, std::min(newTotalSlots, 16));
    }
    
    if (newTotalSlots == static_cast<int>(FilterNameTP.size()))
        return; // No change needed
    
    int oldSize = static_cast<int>(FilterNameTP.size());
    
    // Preserve existing filter names before resizing
    std::vector<std::string> preservedNames;
    for (int i = 0; i < std::min(oldSize, newTotalSlots); ++i)
    {
        preservedNames.push_back(FilterNameTP[i].getText());
    }
    
    // Resize the property to exactly match the new slot count
    FilterNameTP.resize(newTotalSlots);
    
    // Restore preserved names and initialize new slots
    for (int i = 0; i < newTotalSlots; ++i)
    {
        std::string name, label;
        
        if (i < static_cast<int>(preservedNames.size()) && !preservedNames[i].empty())
        {
            // Restore existing name
            name = preservedNames[i];
            label = name;
        }
        else
        {
            // Initialize new slot with default name
            name = "Filter " + std::to_string(i + 1);
            label = name;
        }
        
        FilterNameTP[i].fill(("FILTER_NAME_" + std::to_string(i + 1)).c_str(), 
                            name.c_str(), 
                            label.c_str());
    }
    
    // Apply the updated property
    FilterNameTP.apply();
    
    LOGF_INFO("Filter names updated: %d → %d entries (preserved %d existing names)", 
              oldSize, newTotalSlots, static_cast<int>(preservedNames.size()));
}

bool openogma::isSerialError(int tty_result) const
{
    // Detect serial errors that indicate potential USB disconnect/reconnect needed
    // Error code -2 specifically indicates USB disconnect (invalid file descriptor)
    // TTY_WRITE_ERROR (-2) is the common code when USB is yanked
    return (tty_result == TTY_PORT_FAILURE || 
            tty_result == TTY_ERRNO ||
            tty_result == TTY_SELECT_ERROR ||
            tty_result == TTY_WRITE_ERROR ||  // This is often -2 for USB disconnect
            tty_result == -2);  // Direct error code for USB disconnect
}

void openogma::preserveConnectionSettings()
{
    // Save filter names (these are our device properties)
    savedFilterNames.clear();
    for (size_t i = 0; i < FilterNameTP.size(); i++)
    {
        savedFilterNames.push_back(FilterNameTP[i].getText());
    }
    
    LOGF_DEBUG("Preserved filter names: %zu filters", savedFilterNames.size());
}

void openogma::restoreConnectionSettings()
{
    // Restore filter names if we have them and sizes match
    if (!savedFilterNames.empty() && savedFilterNames.size() == FilterNameTP.size())
    {
        for (size_t i = 0; i < savedFilterNames.size(); i++)
        {
            FilterNameTP[i].setText(savedFilterNames[i].c_str());
        }
        FilterNameTP.apply();
        LOGF_DEBUG("Restored filter names: %zu filters", savedFilterNames.size());
    }
}

bool openogma::attemptReconnect()
{
    if (reconnectInProgress)
    {
        LOG_DEBUG("Reconnect already in progress, skipping duplicate attempt.");
        return false;
    }
    
    reconnectInProgress = true;
    LOG_WARN("Serial communication failure detected, attempting automatic reconnection...");
    
    // Preserve current settings before disconnect
    preserveConnectionSettings();
    
    // Clear any pending commands to avoid confusion
    clearCommandQueue();
    
    // Reset communication state
    commBackoff = false;
    pollMS = NORMAL_POLL_MS;
    
    // Attempt graceful disconnect and reconnect
    bool success = false;
    try
    {
        LOG_INFO("Disconnecting from device...");
        if (Disconnect())
        {
            usleep(1000000); // 1 second delay to allow device to reset
            
            LOG_INFO("Attempting to reconnect...");
            
            if (Connect())
            {
                // Restore user settings after successful reconnection
                restoreConnectionSettings();
                LOG_INFO("Reconnection successful! Device communication restored.");
                success = true;
            }
            else
            {
                LOG_ERROR("Reconnection failed during Connect() phase.");
            }
        }
        else
        {
            LOG_ERROR("Reconnection failed during Disconnect() phase.");
        }
    }
    catch (...)
    {
        LOG_ERROR("Exception during reconnection attempt.");
    }
    
    reconnectInProgress = false;
    
    if (!success)
    {
        LOG_ERROR("Automatic reconnection failed. Manual intervention may be required.");
        // Set device to alert state
        FilterSlotNP.setState(IPS_ALERT);
        FilterSlotNP.apply();
    }
    
    return success;
}

void openogma::beginRecovery(const char* reason)
{
    // Only start recovery if not already in progress
    if (recoveryState != RecoveryState::NONE)
    {
        LOGF_DEBUG("Recovery already in progress (%s), ignoring new trigger: %s", 
                   (recoveryState == RecoveryState::IN_PROGRESS) ? "connecting" : "waiting for calibration", 
                   reason);
        return;
    }
    
    recoveryState = RecoveryState::IN_PROGRESS;
    recoveryStartTime = time(nullptr);
    waitingForCalibration = false;
    
    LOGF_WARN("USB disconnect recovery started: %s", reason);
    
    // Tell user what's happening
    IDMessage(getDeviceName(), "USB unplug detected or serial error. Recovering...");
    
    // Preserve settings and current operation
    preserveConnectionSettings();
    
    // Preserve current move if device was busy
    if (targetSlot > 0 && (fwState == FWState::MOVING || fwState == FWState::CALIBRATING))
    {
        LOGF_INFO("Preserving interrupted move to slot %d for retry after recovery", targetSlot);
        // Clear existing queue and add the interrupted move
        clearCommandQueue();
        commandQueue.push(QueuedCommand(CommandType::MOVE_TO_SLOT, targetSlot));
    }
    else
    {
        // Just drain queue normally (keep last move)
        drainCommandQueue();
    }
    
    // Reset communication state
    commBackoff = false;
    pollMS = NORMAL_POLL_MS;
    
    // Set properties to alert state
    FilterSlotNP.setState(IPS_ALERT);
    FilterSlotNP.apply();
}

bool openogma::doRecovery()
{
    time_t now = time(nullptr);
    
    // Check for timeout
    if (now - recoveryStartTime > RECOVERY_TIMEOUT_SEC)
    {
        LOG_ERROR("USB recovery timeout after 60 seconds. Manual intervention required.");
        IDMessage(getDeviceName(), "Recovery failed - timeout. Please check USB connection.");
        
        // Reset recovery state and mark device as disconnected
        recoveryState = RecoveryState::NONE;
        FilterSlotNP.setState(IPS_ALERT);
        FilterSlotNP.apply();
        return false;
    }
    
    if (recoveryState == RecoveryState::IN_PROGRESS)
    {
        // Attempt reconnection
        LOG_INFO("Attempting USB reconnection...");
        
        try
        {
            // Force disconnect and reconnect
            if (serialConnection)
            {
                serialConnection->Disconnect();
                usleep(1000000); // 1 second delay
                
                if (serialConnection->Connect())
                {
                    // Handshake will detect protocol and get initial state
                    if (Handshake())
                    {
                        LOG_INFO("USB reconnection successful. Checking device state...");
                        IDMessage(getDeviceName(), "Reconnected. Wheel is calibrating...");
                        
                        // Restore user settings
                        restoreConnectionSettings();
                        
                        // Check if firmware is auto-calibrating
                        FWState st; int pos, slots;
                        if (cmdGetState(st, pos, slots))
                        {
                            if (st == FWState::CALIBRATING)
                            {
                                // Firmware is auto-calibrating, wait for it to complete
                                recoveryState = RecoveryState::WAIT_CALIBRATION;
                                waitingForCalibration = true;
                                LOG_INFO("Firmware auto-calibration detected. Waiting for completion...");
                                return true; // Continue recovery in wait state
                            }
                            else if (st == FWState::IDLE && slots > 0)
                            {
                                // Device is ready immediately
                                recoveryState = RecoveryState::NONE;
                                waitingForCalibration = false;
                                LOG_INFO("Device ready immediately after reconnect.");
                                IDMessage(getDeviceName(), "Recovery complete. Ready.");
                                
                                FilterSlotNP.setState(IPS_OK);
                                FilterSlotNP.apply();
                                return true;
                            }
                        }
                        
                        // Default to waiting for calibration
                        recoveryState = RecoveryState::WAIT_CALIBRATION;
                        waitingForCalibration = true;
                        return true;
                    }
                }
            }
        }
        catch (...)
        {
            LOG_ERROR("Exception during USB recovery attempt");
        }
        
        // Reconnection failed, will retry on next timer cycle
        LOGF_DEBUG("USB recovery attempt failed, will retry (elapsed: %ld/%d seconds)", 
                   now - recoveryStartTime, RECOVERY_TIMEOUT_SEC);
        return false;
    }
    else if (recoveryState == RecoveryState::WAIT_CALIBRATION)
    {
        // Check if calibration is complete
        FWState st; int pos, slots;
        if (cmdGetState(st, pos, slots))
        {
            if (st == FWState::IDLE && slots > 0)
            {
                // Calibration complete!
                recoveryState = RecoveryState::NONE;
                waitingForCalibration = false;
                
                LOG_INFO("Firmware auto-calibration complete. Device ready.");
                IDMessage(getDeviceName(), "Calibration complete. Ready.");
                
                // Update device state
                fwState = st;
                updateSlotBounds(slots);
                
                FilterSlotNP.setState(IPS_OK);
                FilterSlotNP.apply();
                
                // Process any queued commands that were preserved during recovery
                if (!commandQueue.empty())
                {
                    LOGF_INFO("Recovery complete, processing %zu queued commands", commandQueue.size());
                }
                
                return true;
            }
            else if (st == FWState::CALIBRATING)
            {
                // Still calibrating, keep waiting
                LOGF_DEBUG("Firmware still calibrating (elapsed: %ld/%d seconds)", 
                           now - recoveryStartTime, RECOVERY_TIMEOUT_SEC);
                return false; // Continue waiting
            }
        }
        
        // Communication failed during calibration wait
        LOG_WARN("Communication lost during calibration wait, restarting recovery");
        recoveryState = RecoveryState::IN_PROGRESS;
        return false;
    }
    
    return false;
}

void openogma::drainCommandQueue()
{
    if (commandQueue.empty()) return;
    
    // Keep only the last move command, drop everything else
    std::unique_ptr<QueuedCommand> lastMove;
    
    while (!commandQueue.empty())
    {
        QueuedCommand cmd = commandQueue.front();
        commandQueue.pop();
        
        if (cmd.type == CommandType::MOVE_TO_SLOT)
        {
            lastMove = std::make_unique<QueuedCommand>(cmd.type, cmd.target_slot);
        }
    }
    
    // Re-queue only the last move if we had one
    if (lastMove)
    {
        commandQueue.push(*lastMove);
        LOGF_INFO("Command queue drained, kept last move to slot %d", lastMove->target_slot);
    }
    else
    {
        LOG_INFO("Command queue drained, no commands preserved");
    }
}
