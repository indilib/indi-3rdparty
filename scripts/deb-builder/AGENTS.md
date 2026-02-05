# Agent Instructions for INDI Debian Package Builder

This repository contains a specialized build system designed to bridge the gap between rapid INDI development and official Debian/Ubuntu packaging cycles. It automates the creation of high-quality Debian packages for the INDI library and its 3rd-party drivers.

## 1. Build, Lint, and Test Commands

### Main Build Process
- **Build All:** Run `./build.sh`. This is the primary entry point.
- **Forced Core Rebuild:** Run `./build.sh -f` or `./build.sh --force-core` to force a rebuild of INDI Core even if `libindi-dev` is already installed.
- **Select Packages:** Run `./build.sh -p driver1,driver2` to override the `OFFICIAL_3RDPARTY_DRIVERS` list.
- **Incremental Build:** The script uses `build_area/` as a cache. If repositories are already cloned, it will fetch updates instead of re-cloning.
- **Cleaning:** To force a fresh build, remove the `build_area/` and `dist/` directories:
  ```bash
  rm -rf build_area/ dist/
  ```

### Build Dependencies
The system requires the following packages on a Debian/Ubuntu system:
- `build-essential`, `debhelper`, `cmake`, `git`, `devscripts`, `pkg-config`.

### Linting
We use `shellcheck` for verifying script quality. Before committing changes to `build.sh`:
```bash
shellcheck build.sh
```
Ensure there are no errors (SCxxxx) or warnings. We aim for zero-warning builds.

### Testing and Verification
There are no automated unit tests for the build script itself. Verification is done by:
1. **Successful Execution:** The script should exit with status 0.
2. **Artifact Verification:** Check the `dist/` directory for the expected `.deb` packages:
   ```bash
   ls -l dist/*.deb
   ```
3. **Package Inspection:** Verify package metadata using `dpkg -I`:
   ```bash
   dpkg -I dist/indi-core_*.deb
   ```
4. **Single Driver Test:** To test a single driver build without re-running everything, you can temporarily comment out other drivers in `packages.conf`.
5. **Linting Verification**: Run `shellcheck build.sh` before any major changes to the build orchestration logic.

---

## 2. Code Style and Guidelines

### Bash Scripting Conventions
- **Error Handling:** 
  - Always use `set -e` at the top of scripts to exit on error.
  - Consider `set -u` to catch unset variables.
  - Use `set -o pipefail` to ensure pipeline failures are caught.
  - When a command failure is expected or handled, use `|| true` or `if command; then ...`.
- **Variable Expansion:** 
  - ALWAYS use double quotes for variable expansions: `"$VARIABLE"`. This prevents issues with spaces in paths.
  - Use curly braces for clarity when necessary: `"${VARIABLE}_suffix"`.
- **Conditionals:**
  - Prefer `[[ ... ]]` over `[ ... ]` for string and file tests as it is more robust in Bash.
  - Use `(( ... ))` for arithmetic comparisons.
- **Functions:**
  - Define functions using the `function name() { ... }` syntax for better visibility.
  - Use `local` keyword for all variables inside functions to avoid polluting the global scope.
  - Functions should return 0 for success and non-zero for failure.
- **Directory Navigation:**
  - Use `pushd` and `popd` for temporary directory changes to ensure you always return to the original location, even if a sub-command fails (though `set -e` usually handles this).
  - Prefer absolute paths for mission-critical operations; use `readlink -f` to resolve them.

### Naming Conventions
- **Variables:** Use `UPPER_CASE_SNAKE_CASE` for global configuration variables (e.g., `INDI_CORE_VERSION`). Use `lower_case_snake_case` for local variables inside functions.
- **Functions:** Use `lower_case_snake_case` with a descriptive verb prefix (e.g., `prepare_source`, `build_package`).
- **Files:** Use lowercase for scripts and configuration files, with `.sh` or `.conf` extensions.

### Formatting and Structure
- **Indentation:** Use 4 spaces for indentation. Do not use tabs.
- **Line Length:** Aim for a maximum of 100 characters per line. Use `\` for line continuation in long commands.
- **Comments:** 
  - Use `#` for single-line comments.
  - Document the purpose of every function with a brief comment above the definition.
  - Explain *why* complex logic (like regex or dependency parsing) is used.
- **Output:**
  - Use `echo` for general status messages.
  - Use `echo "Error: ..." >&2` for error messages.
  - Use consistent separators (like `====`) to delimit major build stages.
  - Progress is shown via a `tqdm`-style progress bar (percentage, ETA) by parsing CMake output from logs in `build_area/`.

### Configuration (`packages.conf`)
- Keep the configuration file strictly declarative. It should contain variable assignments and array definitions only.
- Use Bash arrays for lists: `OFFICIAL_3RDPARTY_DRIVERS=("driver1" "driver2")`.
- Provide sensible defaults (e.g., `"latest"`) to minimize user effort.
- **Auto-detection**: Using `"latest"` for version variables triggers auto-detection of the most recent Git tag via `git ls-remote`.
- **Forced Rebuild**: Setting `FORCE_CORE_REBUILD=true` bypasses the installation check for Stage 1.
- **Driver Names**: Drivers can be listed with or without the `indi-` prefix. The script automatically checks for both.
- **Local Packages**: Use the format `"PackageName:LocalPath"`. Relative paths are resolved against the project root.

### Error Handling Strategy
- The script should be "fail-fast". If `git clone` or `debuild` fails, the whole process should stop.
- For multi-stage builds, the script must clearly communicate to the user what needs to be installed manually, as `sudo` operations are intentionally avoided within the build script itself.
- Validate that the `packages.conf` file exists before sourcing it.
- Use `set -e` to catch errors early and `set -o pipefail` for pipeline robustness.

---

## 3. Project Architecture

### Build Area Management
- `build_area/` is the scratch space.
- Official sources are cloned into subdirectories within `build_area/`.
- Local packages are copied into `build_area/local-<name>` to ensure the original source remains pristine and `debian/` modifications don't leak back.
- **Mono-repo Strategy**: The `indi-3rdparty` repository is treated as a mono-repo. Drivers and libraries within it are built by creating a temporary build directory that symlinks the repo's root contents (like `cmake_modules/`) and links the specific package's `debian/` metadata. This ensures CMake finds all necessary common components.
- **Symlinking Logic**: To build a specific driver from the mono-repo, we:
    1. Create a `tp-build-<name>` directory.
    2. Symlink everything from the mono-repo root *except* the `debian` folder.
    3. Symlink the specific driver's metadata folder to `debian`.
    4. Run `debuild` inside this synthesized environment.

### Dependency Management
- The script identifies "local" dependencies (libraries within the same 3rd-party repo) by checking the `Build-Depends` in `debian/control` against the list of available directories in `debian/`.
- **Version-Aware Build Trigger**: The script compares the version of installed packages with the detected upstream version. A build is triggered only if the installed version is older or missing.
- **Three-Stage Build Flow**:
    1.  **Stage 1**: Builds `indi-core`. Stops if `libindi-dev` is not installed.
    2.  **Stage 2**: Builds required 3rd-party libraries. Stops if any required libraries are not installed.
    3.  **Stage 3**: Builds selected drivers and local packages.
- If a required package is not installed (`dpkg-query`), the build pauses to allow for user installation. This is a design choice to avoid requiring `sudo` inside the build script.

## 4. Maintenance Guidelines
- When adding new functionality, ensure it respects the existing caching logic.
- Periodically verify that the `get_latest_tag` logic still works with the GitHub `ls-remote` output format.
- Avoid adding external dependencies to the build script itself; rely on standard Bash tools (`awk`, `sed`, `grep`, `git`).
- **Version Auto-detection**: The `get_latest_tag` function sorts versions using `git ls-remote --sort='v:refname'`. This is reliable for most INDI repositories but should be monitored if naming conventions change.
- **Source Preparation**: The `prepare_source` function performs a clean checkout and clean-up (`git clean -df`). This ensures that manual changes in the `build_area` don't persist between builds.
- **Git Commit Messages**: Follow the project's style: "Action: Short description" (e.g., "Refactor: improve dependency detection").

---

## 5. Troubleshooting
- **Missing Build Dependencies**: If `debuild` fails, it's usually due to missing system-level development packages. Install them using `sudo apt install <package-name>` as reported by the build log.
- **Timeout on Large Repos**: The `indi-3rdparty` repository is massive. If running in a restricted environment, ensure the network timeout is sufficient for the initial clone.
- **CMake Failures**: If CMake cannot find a library, verify that Stage 2 was successful and that you installed the resulting `.deb` files from the `dist/` directory.
- **Gitea/GitHub connectivity**: Ensure your environment has access to the official INDI repositories.

---

## 6. Summary of Project Conventions
- **Root Directory**: All scripts and configs must reside in the root.
- **Output**: All packages go to `dist/`.
- **Cache**: All clones and builds happen in `build_area/`.
- **Verification**: No build should leave the local source tree modified (hence the copying/symlinking).

## 7. Future Enhancements
- **CI/CD Integration**: Consider adding a GitHub Action to verify builds on every push to main.
- **Improved Dependency Resolution**: Enhance the `get_local_deps` function to handle complex version constraints in `Build-Depends`.
- **Pre-built Cache**: Option to use pre-built binary blobs for extremely large libraries to speed up the developer cycle.
