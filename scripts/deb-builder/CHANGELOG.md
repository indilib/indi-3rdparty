# Changelog

All notable changes to this project will be documented in this file.

## [0.1.4] - 2026-01-19

### Improved
- `setup_worktree.sh`: Refactored to use **Git Sparse Checkout** and **Blob Filtering**. This follows industry best practices for monorepos (like those at Google/Microsoft), providing a full Git experience with a fraction of the disk space.

## [0.1.3] - 2026-01-19

### Fixed
- Version-Aware Builds: Improved comparison logic to use driver-specific versions from `debian/changelog` instead of the global `indi-3rdparty` repository version.
- Library Versioning: Extended version checking to include local library dependencies (e.g., `libplayerone`), ensuring drivers are rebuilt if their underlying libraries are updated.
- Bug Fix: Corrected missing `get_installed_version` function definition and removed invalid `local` variable usage.

### Added
- Progress bar enhancement: Integrated real-time CMake percentage parsing, package name display, and tqdm-style ETA calculation.

## [0.1.1] - 2026-01-19

### Fixed
- Progress indicator `grep` error: Added `-a` flag to treat build logs as text (fixing "binary file matches" errors).
- Terminal display: Improved line clearing using escape sequences (`\e[K`) to ensure the progress bar stays on a single line.

### Added
- Progress bar enhancement: Integrated real-time CMake percentage parsing and tqdm-style ETA calculation.

## [0.1.0] - 2026-01-19

### Added
- Initial project structure with Git initialization.
- `build.sh`: Main orchestration script for building INDI Debian packages.
- `packages.conf`: Declarative configuration file for selecting drivers and versions.
- **Auto-detection**: Support for `latest` keyword to automatically find the most recent Git tags for INDI Core and 3rd-party repositories.
- **Three-Stage Build**: Orchestrated build process separating INDI Core, required libraries, and drivers/local packages.
- **Improved Mono-repo Handling**: Robust handling of the `indi-3rdparty` repository structure, allowing individual driver builds with correct CMake context via temporary synthesized build environments.
- **Forced Core Rebuild**: Added optional `FORCE_CORE_REBUILD` setting in `packages.conf` and a CLI switch (`-f` or `--force-core`) to allow rebuilding INDI Core even if `libindi-dev` is already installed.
- **Package List Override**: Added a CLI switch (`-p` or `--packages`) to manually specify which 3rd-party drivers to build, overriding the configuration file.
- **Graceful Dependency Reporting**: Added automatic detection of missing build-dependencies using `dpkg-checkbuilddeps`, with clear instructions on how to install them via `apt-get`.
- **Improved Logging**: Quiet builds with output redirected to log files, shown only on error.
- **Source Caching**: Incremental build support by reusing and updating clones in `build_area/`.
- **Optional Prefix**: Simplified configuration allowing driver names without the `indi-` prefix.
- **Local Development**: Support for building packages from local source directories.
- `AGENTS.md`: Comprehensive 145-line technical documentation and coding standards for the project.
- `README.md`: Basic usage and setup instructions.
- `.gitignore`: Configured to exclude build artifacts and temporary files.
