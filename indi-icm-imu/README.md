# INDI ICM-20948 IMU Driver

This repository contains the INDI driver for ICM-20948 IMU sensors. This driver is designed to be built as a 3rd party INDI driver.

## Overview

The ICM-20948 is a 9-axis motion tracking device that combines a 3-axis gyroscope, 3-axis accelerometer, and 3-axis magnetometer. Unlike the BNO08x series which provides sensor fusion and orientation data, the ICM-20948 provides **raw sensor data only**.

**Important Note:** This driver does NOT provide orientation/quaternion data. It only provides raw accelerometer, gyroscope, and magnetometer readings. If you need orientation/attitude data, you will need to implement sensor fusion algorithms at the application level or use a sensor that includes built-in sensor fusion (like the BNO08x).

## Features

- Raw 3-axis accelerometer data (m/s²)
- Raw 3-axis gyroscope data (degrees/s)
- Raw 3-axis magnetometer data (µT)
- Temperature readings (°C)
- Comprehensive calibration system with offset compensation
- I2C communication support
- Persistent calibration storage

## Hardware Requirements

- ICM-20948 IMU sensor
- I2C bus connection (typically `/dev/i2c-1` on Raspberry Pi)
- Compatible Linux system with I2C support

## Building and Installation

### Prerequisites

You need to install the [libicm20948](https://github.com/knro/libicm20948) library first:

```bash
cd /path/to/libicm20948
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install
```

### Building the Driver

1. **Clone the repository (if not already done):**

   ```bash
   git clone https://github.com/indilib/indi-3rdparty.git
   cd indi-3rdparty/indi-icm-imu
   ```

2. **Create a build directory and run CMake:**

   ```bash
   mkdir build
   cd build
   cmake -DCMAKE_INSTALL_PREFIX=/usr ..
   ```

   If you have INDI installed in a custom location, you might need to specify `INDI_DIR`:

   ```bash
   cmake -DCMAKE_INSTALL_PREFIX=/usr -DINDI_DIR=/path/to/your/indi/install ..
   ```

3. **Build the driver:**

   ```bash
   make
   ```

4. **Install the driver:**

   ```bash
   sudo make install
   ```

After installation, the `indi_icm20948_imu` driver should be available in your INDI system.

## Usage

### Starting the Driver

You can start the INDI server with this driver:

```bash
indiserver indi_icm20948_imu
```

### Connecting to the Driver

Once the driver is running, you can connect to it using any INDI client (e.g., KStars, PHD2). The driver will expose IMU properties for raw sensor data and calibration controls.

## Driver Properties

The INDI ICM-20948 IMU driver provides several properties to configure the sensor and align it with your telescope's optics. Proper configuration and calibration are crucial for accurate measurements.

### Available Properties

#### Main Tab
- **Connection**: Connect/disconnect from the sensor
- **Driver Info**: Device information including chip ID and status
- **Polling Period**: Update rate for sensor readings (milliseconds)
- **Debug Logging**: Enable verbose logging for troubleshooting

#### Sensors Tab
- **Acceleration**: Raw accelerometer data (X, Y, Z in m/s²)
- **Gyroscope**: Raw gyroscope data (X, Y, Z in degrees/s)
- **Magnetometer**: Raw magnetometer data (X, Y, Z in µT)

#### Calibration Tab
- **Calibration Status**: Displays calibration state for each sensor
- **Calibration Controls**: Start, save, load, and reset calibration
- **Accel Offsets**: Accelerometer offset values (X, Y, Z in m/s²)
- **Gyro Offsets**: Gyroscope offset values (X, Y, Z in degrees/s)
- **Mag Offsets**: Magnetometer offset values (X, Y, Z in µT)

### IMU Frame

This property defines the native coordinate system of your IMU sensor. The ICM-20948 typically uses:

- **NWU (North-West-Up)**: X-axis points North, Y-axis points West, Z-axis points Up

Select the coordinate system that matches your IMU's datasheet or physical mounting orientation. An incorrect setting will result in incorrect altitude and azimuth calculations.

### Telescope Vector

This property defines the pointing direction of the telescope _relative to the IMU's own axes_. It tells the driver which of the IMU's axes is aligned with the telescope's optical axis.

**Examples**:
- `(1, 0, 0)`: The telescope points along the IMU's positive X-axis
- `(0, 1, 0)`: The telescope points along the IMU's positive Y-axis
- `(0, 0, 1)`: The telescope points along the IMU's positive Z-axis
- `(-1, 0, 0)`: The telescope points along the IMU's negative X-axis

**How to set it**: Physically inspect how the IMU is mounted on your telescope. Determine which of the IMU's axes (often marked on the PCB) is parallel to the telescope's tube and set the vector accordingly.

### Orientation Adjustments

- **Roll Multiplier**: Adjusts the roll calculation (usually `1` or `-1`)
- **Pitch Multiplier**: Adjusts the pitch calculation (usually `1` or `-1`)
- **Yaw Multiplier**: Adjusts the yaw calculation (usually `1` or `-1`)

These multipliers correct for any mismatches between the physical rotation of the telescope and the mathematical convention used by the driver.

## Calibration Procedure

The ICM-20948 requires manual calibration to compensate for sensor offsets and environmental factors. The driver implements a comprehensive three-stage calibration process:

### Step 1: Gyroscope Calibration (Zero-Rate Offset)

1. Click **Start Calibration** in the Calibration Controls
2. Place the device on a **stable, stationary surface**
3. Keep the device **perfectly still** for approximately 10 seconds
4. The driver will collect 100 samples and calculate the gyroscope zero-rate offset
5. Status: "Gyroscope calibration complete"

### Step 2: Accelerometer Calibration (Zero-G Offset)

1. After gyroscope calibration completes, the driver automatically proceeds
2. Place the device **flat on a level surface** (Z-axis pointing up)
3. Keep the device **still** for approximately 10 seconds
4. The driver calculates offsets assuming Z-axis = 1g (9.80665 m/s²)
5. Status: "Accelerometer calibration complete"

### Step 3: Magnetometer Calibration (Hard Iron Offset)

1. After accelerometer calibration completes, the driver automatically proceeds
2. **Slowly rotate the device** in figure-8 patterns
3. Continue rotating for approximately 20 seconds (200 samples)
4. Cover all orientations to map the magnetic field sphere
5. The driver calculates the center of the min/max sphere as the hard iron offset
6. Status: "Magnetometer calibration complete"

### Saving Calibration Data

1. After all three calibrations complete, click **Save Calibration** in the Calibration Controls
2. The offsets are saved to the INDI configuration file
3. Calibration data persists across sessions and will be automatically loaded on reconnection

### Manual Offset Adjustment

You can manually adjust the offset values in the Calibration Tab:
- **Accel Offsets**: Fine-tune accelerometer offsets
- **Gyro Offsets**: Fine-tune gyroscope drift compensation
- **Mag Offsets**: Fine-tune magnetometer hard iron offsets

### Resetting Calibration

Click **Reset Calibration** to clear all offsets and return to factory defaults.

## Calibration Notes

### Gyroscope
- Gyroscope drift is temperature-dependent; recalibrate after temperature changes
- For best results, calibrate after the sensor has warmed up (5-10 minutes of operation)

### Accelerometer
- The simple calibration assumes perfect leveling; for better accuracy, consider 6-position calibration
- Mounting vibrations can affect accuracy; ensure rigid mounting

### Magnetometer
- Hard iron offsets come from nearby ferromagnetic materials (permanent magnets, motors, etc.)
- Soft iron distortion (from ferrous materials) is not compensated in this simple calibration
- Keep the IMU away from strong magnetic fields during normal operation
- Recalibrate when the magnetic environment changes (different location, added equipment)

## Troubleshooting

### Sensor Not Detected

1. Check I2C connections
2. Verify I2C is enabled: `sudo raspi-config` → Interface Options → I2C
3. Check if device is visible: `i2cdetect -y 1` (should show 0x68 or 0x69)
4. Check permissions: Add user to `i2c` group: `sudo usermod -a -G i2c $USER`

### Erratic Readings

1. Ensure proper calibration has been performed
2. Check for loose connections or vibrations
3. Verify adequate power supply (ICM-20948 can be sensitive to power fluctuations)
4. Keep away from magnetic interference sources

### Build Errors

1. Ensure libicm20948 is properly installed
2. Verify INDI library version (2.1.5 or higher)
3. Check CMake finds both INDI and ICM20948: `cmake .. -DCMAKE_PREFIX_PATH=/usr`

## Dependencies

- INDI Library (version 2.1.5 or higher)
- [libicm20948](https://github.com/knro/libicm20948) - ICM-20948 sensor library
- I2C development libraries (`libi2c-dev` on Debian/Ubuntu)

## Technical Details

### Sensor Specifications

- **Gyroscope**: ±250, ±500, ±1000, ±2000 dps (default: ±250 dps)
- **Accelerometer**: ±2g, ±4g, ±8g, ±16g (default: ±2g)
- **Magnetometer**: ±4900 µT (AK09916, 0.15 µT/LSB)
- **Temperature**: -40°C to +85°C
- **Interface**: I2C (400 kHz) or SPI (up to 7 MHz)
- **I2C Addresses**: 0x68 (AD0=0) or 0x69 (AD0=1)

### Data Output

All sensor data is provided in standard units:
- Acceleration: meters per second squared (m/s²)
- Angular velocity: degrees per second (°/s)
- Magnetic field: microTesla (µT)
- Temperature: degrees Celsius (°C)

### Limitations

- **No sensor fusion**: Unlike BNO08x, this driver does not provide orientation/quaternion data
- **No built-in calibration**: Calibration must be performed manually through the driver
- **I2C only**: Current implementation supports I2C only (SPI support could be added)
- **Simple offset calibration**: Does not implement scale factor or soft iron compensation

## License

This driver is licensed under the GNU Lesser General Public License v2.1 or later. See the source files for full license text.

## Credits

- Driver implementation: Jasem Mutlaq
- Based on the INDI IMU interface
- Uses the libicm20948 library for sensor communication

## Contributing

Contributions are welcome! Please submit pull requests or open issues on the INDI 3rd party repository.

## See Also

- [INDI Library](https://github.com/indilib/indi)
- [libicm20948](https://github.com/knro/libicm20948)
- [ICM-20948 Datasheet](https://invensense.tdk.com/products/motion-tracking/9-axis/icm-20948/)
