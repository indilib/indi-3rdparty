# INDI Debian Package Builder

This project provides scripts to build the latest version of the [INDI Library](https://github.com/indilib/indi) and selected 3rd-party drivers for Debian-based distributions.

It is designed to bridge the gap between rapid INDI development and the slower Debian/Ubuntu packaging cycle.

## Features

- **Three-Stage Build**: Orchestrates the build into:
    1.  **Stage 1**: INDI Core & Development headers.
    2.  **Stage 2**: Required 3rd-party libraries (auto-detected).
    3.  **Stage 3**: 3rd-party drivers and local packages.
- **Auto-detection**: Automatically finds the latest stable release from GitHub if version is set to `latest`.
- **Caching**: Reuses existing source clones in `build_area/` to save time and bandwidth.
- **Local Development**: Easily build packages from your local source directories while keeping them clean of build artifacts.

## Requirements

You will need the following tools installed:
```bash
sudo apt-get update
sudo apt-get install build-essential debhelper cmake git devscripts pkg-config
```

## Usage

1.  Edit `packages.conf` to select the versions and drivers you want to build.
2.  Run the build script:
    ```bash
    ./build.sh
    ```
    To force a rebuild of INDI Core:
    ```bash
    ./build.sh -f
    ```
    To build only specific drivers:
    ```bash
    ./build.sh -p gpsd,celestronaux
    ```
3.  Find your `.deb` packages in the `dist/` directory.

## Configuration

The `packages.conf` file allows you to:
- Set the GitHub tag/branch for INDI Core (defaults to `latest`).
- Set the version for the `indi-3rdparty` repository (defaults to `latest`).
- List specific 3rd-party drivers to build.
- Specify local paths for development packages.
