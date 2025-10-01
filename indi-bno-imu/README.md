# INDI BNO IMU Driver

This repository contains the INDI driver for BNO08X IMU sensors. This driver is designed to be built as a 3rd party INDI driver.

## Building and Installation

To build and install this driver, follow these steps:

1.  **Clone the repository:**

    ```bash
    git clone https://github.com/indilib/indi-3rdparty.git
    cd indi-3rdparty/indi-bno-imu
    ```

2.  **Create a build directory and run CMake:**

    ```bash
    mkdir build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr .
    ```

    If you have INDI installed in a custom location, you might need to specify `INDI_DIR`:

    ```bash
    cmake -DCMAKE_INSTALL_PREFIX=/usr -DINDI_DIR=/path/to/your/indi/install .
    ```

3.  **Build the driver:**

    ```bash
    make
    ```

4.  **Install the driver:**
    ```bash
    sudo make install
    ```

After installation, the `indi_bno08x_imu` driver should be available in your INDI system. You can then start the INDI server with this driver:

```bash
indiserver indi_bno08x_imu
```

## Usage

Once the driver is running, you can connect to it using any INDI client (e.g., KStars, PHD2). The driver will expose IMU properties such as orientation, acceleration, gyroscope, and magnetometer data, along with calibration controls.

## Driver Properties

The INDI BNO IMU driver provides several properties to configure the sensor and align it with your telescope's optics. Proper configuration is crucial for accurate astronomical coordinate calculations.

### IMU Frame

This property defines the native coordinate system of your IMU sensor. Different IMUs have different physical layouts for their internal axes. It is essential to select the correct frame to ensure the driver correctly interprets the IMU's orientation data.

- **ENU (East-North-Up)**: This is a common coordinate system where the X-axis points East, the Y-axis points North, and the Z-axis points Up.
- **NWU (North-West-Up)**: In this system, the X-axis points North, the Y-axis points West, and the Z-axis points Up.
- **Other systems**: The driver may support other coordinate systems depending on the specific IMU model.

**How to set it**:
Select the coordinate system that matches your IMU's datasheet or documentation. If you are unsure, you may need to experiment to find the correct setting. An incorrect setting will result in wildly inaccurate Altitude and Azimuth readings.

### Telescope Vector

This property defines the pointing direction of the telescope _relative to the IMU's own axes_. It tells the driver which of the IMU's axes is aligned with the telescope's optical axis. The vector is a set of three numbers (X, Y, Z).

**Examples**:

- `(1, 0, 0)`: The telescope points along the IMU's positive X-axis.
- `(0, 1, 0)`: The telescope points along the IMU's positive Y-axis.
- `(0, 0, 1)`: The telescope points along the IMU's positive Z-axis.
- `(-1, 0, 0)`: The telescope points along the IMU's negative X-axis.

**How to set it**:
Physically inspect how the IMU is mounted on your telescope. Determine which of the IMU's axes (often marked on the PCB) is parallel to the telescope's tube. Set the vector accordingly.

### Orientation Adjustments

This group of properties allows for fine-tuning the orientation calculations to match the physical reality of your setup.

- **Roll Multiplier**: Adjusts the roll calculation. Usually `1` or `-1`.
- **Pitch Multiplier**: Adjusts the pitch calculation. Usually `1` or `-1`.
- **Yaw Multiplier**: Adjusts the yaw calculation. Usually `1` or `-1`.

**Why are multipliers needed?**
Sometimes, the physical rotation of the telescope does not match the mathematical convention used by the driver. For example, you might observe that when you increase the telescope's altitude (pitching it up), the IMU's pitch angle _decreases_. This indicates a mismatch in the direction of rotation.

**Example**:
During our testing, we found that a positive rotation of the IMU around its pitch axis corresponded to a _negative_ change in the telescope's altitude. To correct this, we set the **Pitch Multiplier** to `-1`. This inverted the pitch reading from the IMU, bringing the software model in line with the physical setup.

**How to set them**:

1.  Start with all multipliers set to `1`.
2.  Point your telescope at a known altitude (e.g., the horizon, which is 0 degrees).
3.  Manually move the telescope up in altitude.
4.  Observe the Altitude reading in your INDI client.
    - If the Altitude increases as expected, the Pitch Multiplier is correct (`1`).
    - If the Altitude _decreases_, you need to set the **Pitch Multiplier** to `-1`.
5.  Repeat a similar process for Azimuth (yaw) and roll if necessary, though pitch is the most common adjustment needed for Alt/Az mounts.

## Dependencies

This driver requires the INDI Library (version 2.1.5 or higher). It also depends on the [libbno08x](https://github.com/knro/libbno08x) library for communicating with the BNO08X sensor via I2C. The implementation details for interacting with the BNO08X sensor are marked with `TODO` comments in the source code and need to be filled in based on the specific BNO08X library you intend to use.
