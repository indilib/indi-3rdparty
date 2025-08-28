# NexStar Telescope Simulator

This directory contains a Python-based simulator for Celestron NexStar telescopes. It allows you to control a virtual telescope using applications like Stellarium, which is useful for development, testing, and demonstration.

## Contents

*   `nse_simulator.py`:  The main script that sets up the network server, handles communication, and runs the simulation.
*   `nse_telescope.py`:  Defines the `NexStarScope` class, which implements the telescope's behavior and command handling.

## Features

*   Simulates a Celestron NexStar telescope.
*   Supports network control via a WiFly-like interface (port 2000).
*   Integrates with Stellarium (port 10001).
*   Provides a text-based user interface (TUI) for monitoring telescope status.
*   Handles various NexStar commands for telescope control, including goto, slew, and guiding.

## Usage

1.  Ensure you have Python 3 installed.
2.  Install the required dependencies: `pip install asyncio curses ephem`
3.  Run the simulator: `python nse_simulator.py` (for TUI) or `python nse_simulator.py t` (for terminal output only)
4.  Configure Stellarium to connect to the simulator on `localhost:10001`.

## Dependencies

*   Python 3
*   `asyncio`
*   `curses` (ncurses)
*   `ephem`

## Notes

*   The simulator emulates the behavior of a Celestron NexStar telescope.
*   The TUI provides a real-time view of the telescope's status.
*   Report any issues or contribute to the project on [GitHub (link to your repository)].