
This is a modified [tic-arduino](https://github.com/pololu/tic-arduino) library.
Two files: tic_internal.h and tic_names.c are copied from [tic-usb library](https://github.com/pololu/pololu-tic-software).

Goals:
- make it work on Linux,
- make as few changes as possible (to efficiently follow the origin).

Reasons to use tic-arduino over [tic-usb library](https://github.com/pololu/pololu-tic-software):
- it is more generic than tic-usb library, which supports only USB connection,
- it can be easily extended to support Serial, Bluetooth or I2C,
- it doesn't depend on Pololu [libusbp library](https://github.com/pololu/libusbp),
- it is light and comes without external dependencies (except regular libusb for USB connection).

Slightly modified Pololu created files are:
- LICENSE.txt,
- TicBase.h (renamed from Tic.h to to not create conflicts with pololu-tic-software library)
- TicBase.cpp (renamed from Tic.cpp to to not create conflicts with pololu-tic-software library)

Some content of tic_internal.h and tic_names.c from [tic-usb library](https://github.com/pololu/pololu-tic-software) is copied into:
- TicDefs.h
- TicDefs.cpp

All changes of code are published on the same license as the original library, which is MIT.

The latest intake was taken from the following versions:
- tic-arduino fa3f105bc0dd6085dc3af20f55deae8178f42610  (Wed Jul 28 15:11:57 2021 -0700)
- tic-usb 17e24ab81481ace02101c658945733de24a6ddea (Fri Jul 14 13:28:08 2023 -0700)
