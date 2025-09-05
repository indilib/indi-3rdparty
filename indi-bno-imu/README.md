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

## Dependencies

This driver requires the INDI Library (version 2.1.5 or higher). It also depends on the [libbno08x](https://github.com/knro/libbno08x) library for communicating with the BNO08X sensor via I2C. The implementation details for interacting with the BNO08X sensor are marked with `TODO` comments in the source code and need to be filled in based on the specific BNO08X library you intend to use.
