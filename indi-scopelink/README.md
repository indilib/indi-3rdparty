# indi-scopelink

INDI driver for the Astrolabs ScopeLink telescope controller.

ScopeLink drives a focuser, a field rotator, a telescope front flap, a flat panel, two mirror fans, two
auxiliary power outputs and — from generation 3 — a powered USB hub, all over one USB connection. The
driver publishes the controller as a single INDI device carrying the focuser, rotator, dust cap and light
box interfaces, so KStars and Ekos can run autofocus, rotate the camera, open and close the flap and take
flat frames without any further configuration.

Which of those interfaces it claims is read out of the controller. From interface 1.1 the mapping between
motors and jobs is a setting the controller holds — which motor focuses, which rotates, and which ones the
front flap is made of — so the driver reads that assignment when it connects and offers the devices it
names. A controller with no motor assigned to the rotator has no rotator, and says so in the log rather
than publishing one that would refuse every command.

Hardware generations 2 to 4 are supported. What a given controller offers is discovered at connect time,
so a generation 2 unit simply has no USB hub or smart switch properties rather than having ones that
never read. Generation 3 is the production hardware; generations 2 and 3 are on the bench and the driver
is tested against real units of each.

**Generation 4 has been implemented but not yet met.** No such unit exists to test against, so its three
motors, its six port hub and its own fault numbering are built from the interface description and are
exercised against the simulator rather than against hardware. The same is true of everything the motor
assignment brings with it — the rotator, the multi-part front flap and the function commands that drive
them — although a generation 3 unit reflashed with interface 1.1 firmware exercises the assignment itself
on real hardware.

The driver also has a **simulation mode** — turn Simulation on before connecting and it answers itself,
as whichever generation is selected on the Options tab, without opening a port.

It can also **replace the controller's firmware**, from the Firmware tab or with `scopelink-cli flash`.
The controller carries a boot loader of its own that appears as the same serial port the firmware does,
so an update is an ordinary conversation on the port the driver is already using: nothing else has to be
installed, nothing has to be run as root, and no second device appears on the bus. Firmware is shipped as
one encrypted file per unit, named after that unit; the driver passes it through unopened and the boot
loader is what decrypts it, checks it and verifies what it programmed. Firmware updates are deliberately
not simulated — the one question worth asking of one is whether a real controller survives it.

## Contents

| Path | What it is |
|---|---|
| `libscopelink/` | The protocol core: framing, retries, identification, capabilities, status, configuration parameters, the motor assignment and the rules it has to satisfy, fault store, the firmware download, and the simulated controller both the driver's simulation mode and the pseudo terminal tool answer from. No INDI, no user interface, no dependencies. |
| `indi/` | The driver. Property definitions and the mapping onto INDI's interfaces. |
| `tools/` | `scopelink-cli` for support work and `scopelink-simulator`, which serves the simulated controller on a pseudo terminal. |
| `tests/` | Unit tests for the core, and the conformance vectors that keep it in step with the vendor's Windows driver. |
| `udev/` | Access rights for the controller. |

The core is kept separate from the driver deliberately. It has to agree, byte for byte, with a driver
written in a different language for a different platform, and that agreement is only testable if the
core can be built and exercised without libindi in the way. See `tests/vectors/README.md`.

### The generated parameter table

`libscopelink/src/parameters_generated.cpp` is generated, and says so at the top. It is what the firmware
itself says about each of its configuration parameters — the limits the controller will accept, the unit,
the display scale, the settings an enumerated parameter offers and the help text — turned into a table the
driver reads at load time. That description lives with the firmware and is not public, so the generator is
not in this tree either; the generated file is committed and compiled like any other source, and nothing
in the build reaches outside the tree or needs Python.

It is not edited by hand. Which of those parameters a given controller actually holds is a separate
question that the description says nothing about, and it is answered by `buildDidCatalogue()` in
`libscopelink/src/parameters.cpp`, from the capability set — which is hand written, and is where a change
for a new hardware generation goes.

The same function also narrows a parameter to the controller in front of it where the generated
description is wider than that controller. The motor selections are the case that matters: the table
describes a three motor firmware, so unnarrowed it would offer a Motor 3 that is not fitted and would
write 3 for "not used" on a board whose firmware spells that 2. The narrowing is why `Did` holds its
descriptor by value rather than pointing into the generated table.

## Documentation

User documentation, including the Ekos setup walkthrough, is on the INDI drivers documentation site:
<https://drivers.indilib.org/focusers/astrolabs/scopelink/scopelink>. The manual pages in `man/` cover the driver
and both command line tools.

Build and installation instructions are in `INSTALL.md`.

## Owner and maintainer

Bence Toth, Astrolabs Hungary Kft. — <bence.toth@astrolabs.hu>

## Licence

Copyright © 2026 Astrolabs Hungary Kft. GPL-2.0-or-later. Every source file carries the notice.
