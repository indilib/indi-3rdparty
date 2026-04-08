# Developer Guide: indi-celestronaux

This document outlines the development workflow and testing procedures for the `indi-celestronaux` driver.

## Project Structure

- `indi-celestronaux/`: Core driver source code.
- `cmake_modules/`: Shared CMake modules.
- `scripts/`: Helper scripts.
- `debian/indi-celestronaux/`: Debian packaging configuration.
- `build/`: Build directory (ignored by Git).
- `indi-celestronaux/tests/`: System test suite.

## Tools and Commands

Use the provided `Makefile` for common tasks:

- `make`: Compiles the driver.
- `make run`: Runs the driver under `indiserver`.
- `make run-sim`: Runs the telescope simulator (requires `python3-ephem`).
- `make deb`: Builds `.deb` packages.
- `make clean`: Cleans build artifacts.

## IDE / LSP Support

- `compile_commands.json` is linked in the root directory for LSP support (clangd).
- Code is formatted according to `.clang-format`.

## Testing

The project includes a system test suite located in `indi-celestronaux/tests/system/`.

### Running with Internal Simulator (Default)

To run the full suite using the internal Python simulator:

```bash
pytest indi-celestronaux/tests/system/test_basic.py
```

### Running with External Simulator

You can configure the test suite to use an external simulator binary (e.g., `caux-sim`) by setting the `INDI_SIM_EXEC` environment variable:

```bash
export INDI_SIM_EXEC=caux-sim
pytest indi-celestronaux/tests/system/test_basic.py
```

## Development Rules

- **Language:** All code comments, commit messages, and documentation must be in English.
- **Git:** Commit frequently with clear messages. Use feature branches for new features or bugfixes. Merge them into the main test branch to verify compatibility.
- **Stability:** Ensure tests pass before merging.
- **Golden Standard:** Treat the C++ driver and `nse_simulator.py` as authoritative implementations. **DO NOT** modify the C++ driver code during the test development stage. Document all findings in `ISSUES.md`.
