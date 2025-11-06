# OpenOGMA INDI Driver

This code provides the **INDI driver for the OpenOGMA Filter Wheel**, an astrophotography accessory manufactured by **OGMAVision**. It allows Linux, macOS, and other INDI-compatible systems (e.g., KStars/Ekos) to control the OpenOGMA filter wheel hardware over USB.

- **Device**: OpenOgma Filter Wheel
- **Connection**: USB CDC (Serial)
- **Baud**: 115200
- **INDI Interface**: Filter Wheel

## Features

- Auto protocol detection: **FRAMED → TEXT** (FRAMED preferred | TEXT for dvelopment).
- Robust framed RX with magic resync + CRC.
- Atomic state via `GET_STATE (0x1003)` (state/position/slots).
- Calibration flow:
  - Firmware auto-calibrates on power-up (USB plug).
  - Driver exposes a **Calibrate Now** button.
  - Driver queues user moves until wheel is IDLE & calibrated.
- Safe UX:
  - Reports **last known slot** while moving (hides “255/unknown”).
  - 1-based slots in UI, 0-based on the wire.
- Hot-unplug recovery:
  - Detects serial errors, **auto-reconnects**, re-handshakes,
    waits out firmware’s auto-calibration, then resumes.

---

## Quick Start

## Usage
1. **Connect your OpenOGMA Filter Wheel** via USB
2. **Start the INDI server**
3. **In your INDI client** (e.g., KStars/Ekos):
   - Add a Filter Wheel device
   - Select "OpenOGMA Filter Wheel"
   - Choose the correct serial port (usually `/dev/ttyACM0` or `/dev/ttyUSB0`)
   - Click Connect

## Basic Operations
- **Change Filter**: Set `FILTER_SLOT` to desired position (1-7)
- **Calibrate**: Set `FILTER_SLOT` to `0` to start calibration
- **Monitor Status**: Watch the device state (IDLE/MOVING/CALIBRATING/ERROR)
- **Filter Names**: Automatically sized to match your wheel's slot count

## Smart Property Management

The driver intelligently manages filter properties to match your specific wheel configuration:

- **Auto-Sizing**: After calibration, `FILTER_NAME` entries are automatically created to exactly match your wheel's slot count
- **No Confusion**: Moving from a 7-slot to 5-slot wheel automatically hides extra properties
- **Reasonable Bounds**: Safety limits (1-7 slots) prevent excessive property creation
- **Seamless Switching**: Hot-swapping different filter wheels "just works" without manual configuration

## INDI-Compliant Position Handling

The driver implements clean position normalization following INDI standards:

- **External Interface**: Uses 1-based positions (Filter 1, Filter 2, etc.) as expected by INDI clients
- **Internal Communication**: Converts to firmware's 0-based positions (0, 1, 2, etc.) automatically
- **Moving State**: Hides firmware's "255 while moving" value, keeping last known position visible in UI
- **Calibration**: Slot 0 triggers calibration, 1-N moves to specific filters
- **Error Handling**: Out-of-range positions are gracefully clamped to valid ranges

This ensures perfect compatibility with all INDI clients while handling firmware quirks transparently.

---

## How It Works

### Automatic Protocol Detection

The driver implements intelligent protocol detection that automatically identifies which communication protocol your OpenOGMA Filter Wheel supports:

1. **FRAMED Protocol** (Modern, Recommended)
   - Binary protocol with error detection
   - CRC checksums for data integrity
   - Most robust for production use

2. **TEXT Protocol** (Development/Debugging)
   - Human-readable commands
   - Easy to test manually
   - CRLF line endings (`\r\n`)

### Connection Flow

```
Driver Start → Serial Connection → Protocol Detection → Device Ready
     ↓               ↓                    ↓                 ↓
  Initialize      Open Port         Try FRAMED          Get Status
  Properties   → Set Baud Rate  → Try TEXT         → Start Polling
               → Handshake                             → Ready for Use
```

### Protocol Upgrade

The driver includes an intelligent **one-shot upgrade** mechanism that automatically attempts to upgrade the device to the best available protocol:

- **Automatic Detection**: During idle periods (every 5 minutes), the driver checks if a better protocol is available
- **FRAMED Priority**: If currently using TEXT, attempts to upgrade to FRAMED protocol
- **Silent Operation**: Upgrades happen transparently without user intervention
- **Fail-Safe**: If upgrade fails, driver continues with the current working protocol
- **No Interruption**: Upgrades only occur when the filter wheel is idle (not moving)

This ensures that devices with firmware updates or intermittent communication issues can automatically benefit from the most robust protocol available.

### Error Handling

The driver includes comprehensive error handling and logging:

- **Connection Issues**: Detailed serial port diagnostics
- **Protocol Failures**: Individual protocol attempt logging
- **Communication Errors**: TTY error code translation
- **Device States**: Real-time status monitoring

---

## Communication Protocols

### 1. FRAMED Protocol (Recommended)

**Command Format (11 bytes):**
```
[Magic:0xA5][Length][CommandID(4)][Value(4)][CRC]
```

**Features:**
- Magic byte (`0xA5`) for synchronization
- Length field for variable-size responses
- XOR CRC checksum for error detection
- Auto-detection and response matching
- **Frame Synchronizer**: Intelligent 0xA5 hunter with:
  - CR/LF noise skipping for clean detection
  - 128-byte scan guard to prevent infinite searching
  - CRC failure debugging with hexdump (first 16 bytes)

### 2. TEXT Protocol (CRLF-terminated)

**Commands:**
| Command | Description | Example Response |
|---------|-------------|------------------|
| `CALIBRATE\r\n` | Start calibration | `OK` |
| `POS 3\r\n` | Move to position 3 | `OK` |
| `POS\r\n` | Get current position | `3` |
| `SLOTS\r\n` | Get total slots | `7` |
| `STATUS\r\n` | Get state code | `0` (IDLE) |

**Important:** The driver automatically adds `\r\n` (CRLF) to all text commands, regardless of input format.

### Command Reference

| Command ID | Name | Description | Value | Response |
|------------|------|-------------|-------|----------|
| `0x1001` | `FW_POSITION` | Move to position | 1-N (move), 0 (calibrate), -1 (get) | Current position |
| `0x1002` | `FW_SLOT` | Get slot count | 0 | Total slots (0 if uncalibrated) |
| `0x1003` | `FW_GET_STATE` | Get system state | 0 | Packed state data |

### State Codes

| Code | Name | Description |
|------|------|-------------|
| `0` | `IDLE` | Ready for commands |
| `1` | `CALIBRATING` | Running calibration |
| `2` | `MOVING` | Moving to position |
| `3` | `ERROR` | Hardware error |

---

## Configuration

### Serial Settings

The driver automatically configures:
- **Baud Rate**: 115,200 bps (default, expected by firmware)
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Flow Control**: None

### Timeouts

- **Initial Connection**: 3 seconds
- **Protocol Detection**: 2-3 seconds per protocol
- **Normal Operations**: 2 seconds
- **Device Stabilization**: 500ms after connection

---

## Troubleshooting

### Connection Issues

1. **Check Device Power**: Ensure the filter wheel is powered on
2. **Verify USB Connection**: Check cable and port
3. **Check Permissions**: Ensure user is in `dialout` group:
   ```bash
   sudo usermod -a -G dialout $USER
   # Log out and back in
   ```
4. **Identify Port**: Find the correct device:
   ```bash
   ls -la /dev/ttyACM* /dev/ttyUSB*
   ```

### Manual Testing

Test communication manually:
```bash
# Simple echo test
echo -e "SLOTS\r\n" > /dev/ttyACM0

# Interactive terminal
screen /dev/ttyACM0 115200
# Type: SLOTS (press Enter)
# Expected response: 8 (or your wheel's slot count)
```

### Debug Logging

Enable verbose logging:
```bash
# Start with verbose output
indiserver -v indi_openogma

# Or check system logs
journalctl -f -u indiserver
```

### Common Error Messages

| Error | Cause | Solution |
|-------|-------|----------|
| `TTY_TIME_OUT` | Device not responding | Check power, cables, baud rate |
| `Invalid file descriptor` | Port not accessible | Check permissions, port name |
| `All protocol detection attempts failed` | Communication failure | Manual test, check wiring |
| `Protocol detected: UNKNOWN` | Firmware issue | Contact manufacturer |
| Too many/few filter name entries | Switching between different wheels | Wait for calibration to auto-adjust properties |

---

## Development

### Code Structure

```
indi_openogma/
├── indi_openogma.h          # Driver class definition
├── indi_openogma.cpp        # Main implementation
├── indi_openogma.xml.cmake  # INDI device descriptor
├── CMakeLists.txt          # Build configuration
├── config.h.cmake         # Version information
└── README.md              # This file
```

**Key Methods:**
- `Handshake()`: Connection and protocol detection
- `detectProtocol()`: Auto-detection of communication protocol
- `sendFramed/sendText()`: Protocol-specific communication
- `TimerHit()`: Periodic status polling

---

## Integration with INDI 3rd Party Repository

This section provides guidance for maintainers who want to integrate the OpenOGMA driver into the official INDI 3rd Party repository.

### Development Workflow

1. **Repository Setup**
   ```bash
   # Clone the INDI 3rd party repository
   git clone https://github.com/indilib/indi-3rdparty.git
   cd indi-3rdparty
   
   # Create a feature branch
   git checkout -b feature/openogma-driver
   ```

2. **Integration Steps**
   - Add driver source files to appropriate directory structure
   - Update CMakeLists.txt to include OpenOGMA build option
   - Add any required udev rules
   - Update documentation and changelogs

3. **Testing & Validation**
   - Verify clean builds in isolated environment
   - Test installation and functionality
   - Validate INDI property compliance

### Build and Testing

This section covers building the OpenOGMA driver as part of the INDI 3rd Party repository for development and testing purposes.

### Prerequisites

Before building, ensure you have the INDI 3rd Party repository set up:

```bash
# Clone the INDI 3rd party repository
git clone https://github.com/indilib/indi-3rdparty.git
cd indi-3rdparty

# Create a feature branch (if developing)
git checkout -b feature/openogma-driver
```

### Building for Development

For active development work, create a debug build with detailed logging and symbols:

```bash
# Create and enter build directory
mkdir -p ~/build/indi-3rdparty-debug
cd ~/build/indi-3rdparty-debug
rm -rf ./*

# Configure debug build with OpenOGMA enabled
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DWITH_OPENOGMA=ON \
      -DWITH_AVALONUD=OFF \
      -DWITH_GPSD=OFF \
      ~/indi-3rdparty

# Build only the OpenOGMA driver
cmake --build . --target indi_openogma -j"$(nproc)"

# Install locally for testing
sudo cmake --install . --component openogma
sudo ldconfig
```

### Building for Release Testing

Before submitting a pull request, thoroughly test the integration build with release settings:

```bash
# Clean build environment
mkdir -p ~/build/indi-3rdparty-release
cd ~/build/indi-3rdparty-release
rm -rf ./*

# Configure release build with OpenOGMA enabled
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DWITH_OPENOGMA=ON \
      -DWITH_AVALONUD=OFF \
      -DWITH_GPSD=OFF \
      ~/indi-3rdparty

# Build and install
cmake --build . --target indi_openogma -j"$(nproc)"
sudo cmake --install . --component openogma
sudo ldconfig
```

### Installation Verification

Confirm the driver was installed correctly:

```bash
# Check binary installation
ls -l /usr/local/bin/indi_openogma

# Check XML descriptor
ls -l /usr/share/indi/indi_openogma.xml

# Verify driver startup
indiserver -v indi_openogma
```
### Debugging

**With GDB:**
```bash
gdb --args indiserver -v indi_openogma
(gdb) set follow-fork-mode child
(gdb) run
```

**Debug Logging:**
The driver includes extensive debug logging. Look for:
- `Starting handshake with OpenOGMA Filter Wheel...`
- `Protocol detected: [FRAMED|TEXT]`
- `sendText: sending 'SLOTS\n' (7 bytes)`
- `readExact: attempting to read X bytes with Y second timeout`

### System Integration Testing

1. **udev Rules** (if applicable):
   ```bash
   sudo udevadm control --reload-rules
   sudo udevadm trigger
   ```

2. **Device Detection**:
   ```bash
   # Identify available serial ports
   ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
   ```

3. **INDI Property Testing**:
   ```bash
   # Set device port (adjust path as needed)
   indi_setprop "OpenOGMA Filter Wheel.DEVICE_PORT.PORT=/dev/ttyACM0"
   
   # Connect to device
   indi_setprop "OpenOGMA Filter Wheel.CONNECTION.CONNECT=On"
   
   # Verify connection status
   indi_getprop "OpenOGMA Filter Wheel.CONNECTION.*"
   ```

   Expected output:
   ```
   OpenOGMA Filter Wheel.CONNECTION.CONNECT=On
   OpenOGMA Filter Wheel.CONNECTION.DISCONNECT=Off
   ```

## Dependencies

### Required
- **INDI Core Library** (>= 1.9)
- **CMake** (>= 3.5)
- **GCC/G++** with C++11 support

## License

This driver is released under the **Affero GNU General Public License v3 (AGPLv3)**.

See the [LICENSE.txt](./LICENSE.txt) file for full terms.

---

## Support & Links

- **INDI Library**: [indilib.org](https://indilib.org/)
- **OGMAVision**: [getogma.com](https://getogma.com/)
- **Issues**: [GitHub Issues](https://github.com/OGMAVision/indi-openogma/issues)
- **INDI Forum**: [indilib.org/forum](https://indilib.org/forum/)

---
