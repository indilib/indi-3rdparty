# Atik EFW1 (Legacy) Serial Transport

This driver talks to the legacy Atik EFW1 (including the "EFW - USB v2"
revision) through the operating system serial device created for the wheel's
FTDI USB adapter. The wheel protocol remains the same framed byte protocol used
by the previous direct USB transport.

Newer Atik EFW2/EFW3 wheels are handled by `indi_atik_wheel` in `indi-atik`,
which uses the Atik SDK.

## Serial Port on Linux

The wheel's FTDI chip uses the Atik-specific product ID `0403:af01`, which the
kernel `ftdi_sio` driver does not list, so no `/dev/ttyUSB*` port is created by
default. `99-atik-efw1.rules` registers that exact VID:PID with `ftdi_sio` when
the wheel is plugged in. Generic FTDI adapters are not affected.

Without the rule installed, the same can be done by hand:

    sudo modprobe ftdi_sio
    echo 0403 af01 | sudo tee /sys/bus/usb-serial/drivers/ftdi_sio/new_id

## Modem Control Lines

The previous libusb implementation set the FTDI DTR and RTS lines high
(`SIO_SET_MODEM_CTRL`, value `0x0303`) and reset the USB device before sending
wheel commands. The serial implementation uses INDI's standard raw 9600 8N1
serial connection. Opening the port normally raises DTR and RTS, but no USB
reset is performed.
