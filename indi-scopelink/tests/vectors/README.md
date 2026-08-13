# Conformance vectors

Most of the frames in `tests/test_scopelink.cpp` are hand built to the documented layouts. That proves
the C++ parsers agree with the specification as written down; it does not prove the specification matches
the firmware.

What proves that is a frame captured from a real controller. The recordings here are replayed through the
same `Device` the driver uses, and every value they decode to is checked against a file of expectations
that was worked out by hand from the bytes. The freeze frame layout has already caught the Windows driver
out once - the field order differs per hardware generation in ways a length check alone does not detect -
so this is the check that matters most.

## What is here

| File | Recorded from |
|---|---|
| `gen3-com20-session.wire` | Generation 3 unit `2B002E000D4330364E353020`: identification, capability read, clock set, two status frames and the fault store. Its infrared sensor works, so the temperatures in it are real readings. |
| `gen3-com20-eeprom.wire` | The same unit's EEPROM wear counters, with the open sequence that leads to them. |
| `gen2-com6-session.wire` | Generation 2 unit `09000E000C4330364E353020`, the same sequence one transaction shorter - identifier 0x0302 does not exist before generation 3. Its infrared sensor is dead, so it also records what a failed sensor looks like: 0xFFFF readings, the stored timeout, and an I2C error counter saturated at 0xFFFF. |
| `gen2-com6-eeprom.wire` | That unit's wear counters. |

The two generations together are the point. A single recording proves the parsers read one unit; a pair
whose status frames are 60 and 56 bytes and whose freeze frames are 48 and 34, with fields in different
places and four fewer in the shorter one, proves the driver takes the layout from the identification
block rather than from a length check both would pass.

Each has a `.expected` file beside it listing what the recording must decode to.

## Capturing

With the driver stopped and the controller connected:

```sh
scopelink-cli --port /dev/ttyACM0 --verbose info 2>capture.log
grep '\[wire\]' capture.log | sed 's/\[wire\] //'
```

Every request and every response is logged as hex, in both directions. `--verbose faults`, `--verbose
eeprom` and `--verbose parameters` capture the rest.

## Recording a vector

Name the file after the unit it came from and what it holds, so that a failing test says which unit and
which frame disagreed. The format is the capture itself:

```
# Free text, and a note of when and from what it was taken.
-> AA 55 12 00
<- AA 55 12 04 03 00 01 00
```

The replay answers a request with the response recorded for that same request. One request cannot match:
the clock set carries the moment of capture, so it is matched on its command byte alone. A request the
recording has no answer for fails the test rather than being guessed at - that means the driver now asks
something the recording predates, and the recording has to be taken again.

Beside it, a `.expected` file of `key = value` lines. **Work the values out from the bytes, not from what
the driver prints.** A vector whose expectations came from the code under test only proves the code has
not changed. Every key in the file has to be one the test reads; one that nothing reads fails the test,
which is what catches a mistyped key.

The keys the test understands are the `identification.*` and `capabilities.*` ones, which every recording
must carry, plus `status.*`, `faults.*` and `eeprom.*` for whichever of those the recording covers. See
`testRecordedSession` for the list.

## What is still missing

- **A motor in motion.** Every recorded status frame has both motors halted, so the moving and stalled
  status bytes are not recorded from hardware.
- **A fault store holding more than one code**, and one long enough that the length byte cannot describe
  it, which is the branch that reads at the expected length instead.
