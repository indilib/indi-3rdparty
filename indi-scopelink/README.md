# indi-scopelink

INDI driver for the Astrolabs ScopeLink telescope controller.

ScopeLink drives a focuser, a telescope front flap, a flat panel, two mirror fans, two auxiliary power
outputs and — on generation 3 — a powered USB hub, all over one USB connection. The driver publishes the
controller as a single INDI device carrying the focuser, dust cap and light box interfaces, so KStars and
Ekos can run autofocus, open and close the flap and take flat frames without any further configuration.

Hardware generations 2 and 3 are supported. What a given controller offers is discovered at connect time,
so a generation 2 unit simply has no USB hub or smart switch properties rather than having ones that
never read. Generation 3 is the production hardware; both generations are on the bench and the driver is
tested against each of them.

The driver also has a **simulation mode** — turn Simulation on before connecting and it answers itself,
as whichever generation is selected on the Options tab, without opening a port.

## Contents

| Path | What it is |
|---|---|
| `libscopelink/` | The protocol core: framing, retries, identification, capabilities, status, configuration parameters, fault store, and the simulated controller both the driver's simulation mode and the pseudo terminal tool answer from. No INDI, no user interface, no dependencies. |
| `indi/` | The driver. Property definitions and the mapping onto INDI's interfaces. |
| `tools/` | `scopelink-cli` for support work and `scopelink-simulator`, which serves the simulated controller on a pseudo terminal. |
| `tests/` | Unit tests for the core, and the conformance vectors that keep it in step with the vendor's Windows driver. |
| `udev/` | Access rights for the controller. |

The core is kept separate from the driver deliberately. It has to agree, byte for byte, with a driver
written in a different language for a different platform, and that agreement is only testable if the
core can be built and exercised without libindi in the way. See `tests/vectors/README.md`.

## Documentation

User documentation, including the Ekos setup walkthrough, is on the INDI drivers documentation site:
<https://drivers.indilib.org/focusers/astrolabs/scopelink/scopelink>. The manual pages in `man/` cover the driver
and both command line tools.

Build and installation instructions are in `INSTALL.md`.

## Owner and maintainer

Bence Toth, Astrolabs Hungary Kft. — <bence.toth@astrolabs.hu>

## Licence

Copyright © 2026 Astrolabs Hungary Kft. GPL-2.0-or-later. Every source file carries the notice.
