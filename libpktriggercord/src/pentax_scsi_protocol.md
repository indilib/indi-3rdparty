Pentax USB SCSI Communication Protocol
===============================

## 1 General

This is the documentation of the USB SCSI communication between computer and Pentax camera. Although several requests had been made, there is no official documents from Pentax. This is a summary from understanding of current implementations and analysis of Pentax camera/firmware.

To control the Pentax camera, the camera has to be set to Mass Storage (MSC) mode, rather than PTP mode.

There are 2 kind of commands referenced in this document, one is the low level `SCSI command`, another is the higher level `Command`, which is consist of several low level `SCSI commands`.

### 1.1 SCSI Command

Pentax camera is controlled via SCSI Pass-through interface of USB Mass Storage. Vendor specific `SCSI command` is used to communicate with Pentax camera.

`SCSI command` contains `8` bytes in the following format:
```Javascript
[F0 TT X2 X3 X4 X5 X6 X7]
```

 * The leading byte is the SCSI opcode, and always be `0xF0`, which means Vendor Specific.
 * `TT` is the type of this `SCSI command`, can be following values:

```Javascript
0x24:   Send Command
0x26:   Get Command Status
0x4F:   Send Arguments
0x49:   Read Result
```

 * `X2, X3, X4, X5` have different meaning for different `TT`.
 * `X6, X7` are always zero.

The `SCSI command` is limited to 8 bytes, a data buffer associated with the `SCSI command` can be used to transfer more data to/from camera. The associated data buffer is used in this protocol for both data transfer direction.

Such as, Send Arguments `SCSI command` will use the associated data buffer to send arguments to camera, and Read Result `SCSI command` will use the associated data buffer to receive the result from camera.

For the reason, there are 2 kind of SCSI operation, `scsi_write()` and `scsi_read()`.

Most `SCSI commands` will be called via `scsi_write()`, as they are simply passing data to the camera, however, some will be called via `scsi_read()` to receive data.

### 1.2 Command

The high level `Command` is more like a function, which has function name, arguments, and returned data. To achieve such task, several `SCSI Commands` are used in the following way:

 1. Computer sends the arguments by `SCSI Command - 0x4F` first, if there is any;
 2. Computer sends a command call by `SCSI Command - 0x24`;
 3. Computer keep checking the status by `SCSI Command - 0x26` until the status is ok;
 4. Computer receive the result by `SCSI Command - 0x49`, if needed.

The individual commands will be discussed later.

## 2 SCSI Commands Types

### 2.1 Send Arguments

Each `Command` can have arguments. If the command does have one or more arguments, the arguments will be send to camera separately, before sending the command.

This `SCSI command` will not be sent if there is no any argument.

The `TT` code for sending arguments is `0x4F`


The `SCSI command` format of sending arguments is:

```Javascript
[F0 4F X0 X1 Y0 Y1 00 00]
```

 * `X0 X1` is a Little-Endian 16-bit integer, `0xX1X0`, which is `the offset of the arguments`;
 * `Y0 Y1` is also a Little-Endian 16-bit integer, `0xY1Y0`, which is `the size of the arguments`.

Both of the `offset` and `size` are in bytes, and should be aligned to 4 (32-bit).

The maximum number of arguments are limited to `8`, which is `32 bytes`, so `X1` and `Y1` are always be `0x00`.

To explain the concept of `offset of the arguments`, we can imagine that there is an `arguments buffer` on the camera side, and for each time we send an argument(s), we need to specify where we want to put our arguments in the `arguments buffer`. The location is specified by the `offset`. Camera will simply follow our instruction and save the argument in the buffer at the given `offset`, and later the `SCSI command - 0x24` will trigger the camera to load arguments list from this buffer.

So, for example, if we're going to send the first argument, the offset should be `0x00`. After the first argument sent, if we decide to send more arguments, we will need to call `SCSI command - 0x4F` again. But as camera side didn't keep how many arguments we have sent, we need to specify the offset for those new arguments. This time, the offset should be `0x04`, as we have put our first argument in the buffer at offset `0x00`, and each argument should be 4 bytes(32-bit) aligned.

The reason Pentax is using `offset` design to pass the arguments is that, on some old Pentax cameras, the buffer to receive `SCSI command` associated data is very small (maybe just 4 bytes), so the arguments has to be passed in multiple times (such as one by one), so an `offset` is necessary to tell the camera where the arguments in current `SCSI command` should be put in the `arguments buffer`.

It's important to check the byte-order of the camera. Older Pentax cameras have Big-Endian system, K-3 or later Pentax cameras have a Little-Endian system. The arguments data buffer should be constructed in the same way of camera's.

### 2.2 Send Command

The `TT` code for sending command is `0x24`.

The `SCSI command` format of sending command is:

```Javascript
[F0 24 C0 C1 X0 X1 00 00]
```

 * `C0` is `Command Group`, `C1` is the command number in the group. The combination of `C0 C1` defines the `Command`.
 * `X0 X1` is a Little-Endian 16-bit integer, `0xX1X0`, which is the byte length of the previously sent arguments, and should be aligned to 4 (32-bit). For example, if one argument was sent previously, the `X0 X1` should be `04 00`. As the arguments number is quite small, `X1` will always be zero.
 * As discovered in K-S1 firmware, `X0 X1` are not used anymore.

### 2.3 Get Command Status

After the `SCSI command - 0x24` has been sent, we need to know what's the status of the execution. Is it still running? or completed successfully? or got any error? This `SCSI command` is to retrieve the current command execution status, and the length of the result may also be returned.

`TT` code is `0x26`. `SCSI command` format is:

```Javascript
[F0 26 00 00 00 00 00 00]
```

As the status data will be returned from camera, this `SCSI command` should be sent by `scsi_read()`, and the status data will be return via associated data buffer.

The status data which camera returned is an `8` bytes array, in following format.

```Javascript
[L0 L1 L2 L3 00 00 S0 S1]
```

 * `L0 L1 L2 L3` is a Little-Endian 32-bit integer, `0xL3L2L1L0`, which is the length of the returned data.
 * `S0` and `S1` are the status code.
   * (S0 == 0x01): Ready to read the result;
   * (S1 == 0x01): Completed Successfully;
   * (S1 == 0x81): Error: Command not available
   * (S1 == 0x82): Error: Command Rejected
   * (S1 == 0x83): Error: Illegal Parameter

### 2.4 Read Result

To retrieve the result data of the command we sent earlier, this `SCSI command` will be used, the `TT` code is `0x49`. The `SCSI command` will be send via `scsi_read()`, and the associated buffer will be the result data. The `SCSI command` format is:

```Javascript
[F0 49 X0 X1 L0 L1 00 00]
```

 * `X0 X1` is a Little-Endian 16-bit integer, `0xX1X0`, which is the offset of the result data, and should be aligned to 4 (32-bit).
 * `L0 L1` is a Little-Endian 16-bit integer, `0xL1L0`, which is the size of the result data we want to read, and should be aligned to 4 (32-bit).

Normally, we'll just leave `X0 X1` to zero, and `L0 L1` to the length of the whole content to be read.

When parsing the result data buffer, it's very important to check the byte-order of the camera, the returned result should be as same as camera's internal byte-order. Older Pentax cameras have Big-Endian system, K-3 or later Pentax cameras have a Little-Endian system, we need to parse the data in the same way of the camera.

## 3 Definition of Each Command

This is the list of `Commands` have been discovered by now. The `Command` will be defined as:

```
[C0 C1] Command Name (arg0, arg1, ...)
```

 * The arguments, `(arg0, arg1, ...)`, will be sent via `SCSI Command - 0x4F - Send Arguments`.
 * If the arguments is `()`, that means there is no arguments for the `command`, so no `SCSI Command - 0x4F` will be called.
 * `C0` is the `Command Group`, and `C1` is the `command number in the group`, the combination of `[C0 C1]` will be sent via `SCSI Command - 0x24 - Send Command`.


### 3.1 Command Group 0x00/0x01 - System

#### [00 00] Set Connect Mode (on_off)

`on_off`: 1: Connect, 0: Disconnect

#### [00 01] Get Status ()

This command will return first `16` or `28` bytes of the whole status data.

#### [00 04] Get Identity ()

The camera id consists of two 32-bit integers, the format is:

```Javascript
[A0 A1 A2 A3 B0 B1 B2 B3]
```

It will be returned in the same byte order of the camera, so for Little-Endian system, such as K-3, the id is `{0xA3A2A1A0, 0xB3B2B1B0}`.

#### [00 05] Connect ()

This command is used for old camera only.

#### [00 08] Get Full Status ()

This command will return the full status data, normally several hundreds bytes.

The status data includes camera settings, the range of some settings, battery status, etc. Although many fields are shared in different models, the offset of each fields may be different, and there might be new fields added in later model. It's very important to check whether the status data is correct or not for debugging or supporting new Pentax cameras.

The status data is actually a memory dump of the camera, so it's very important to check the byte order of the camera, the data should be parsed as the same way of camera.

#### [00 09] DSPTaskWUReq (mode)

`mode`: 1: Before our commands; 2: After our commands

#### [01 00] GetDSPInfo ()

#### [01 01] Get Firmware Version ()

This command returns 4 bytes in following format:

```
[X0 X1 X2 X3]
```

For Big-Endian cameras the firmware version is X0.X1.X2.X3 for Little-Endian cameras it is X3.X2.X1.X0

### 3.2 Command Group 0x02/0x03 - Image Buffer Related

#### [02 00] Get Buffer Status ()

This command requests the image buffer status, and the result should be read by `0x49`, Read Result, command. The result is 8 bytes data in following format:

```
[X0 X1 X2 X3 Y0 Y1 Y2 Y2]
```

`X0 X1 X2 X3` is an 32-bit integer, for each bit is corresponding a buffer status, `0` is empty, `1` is not empty.

#### [02 01] Select Buffer (buffer_number, buffer_type, buffer_resolution, 0)

For old camera, there is only 3 arguments:

```C
(buffer_number, buffer_type, buffer_resolution)
```

`buffer_type`

```C
typedef enum {
    PSLR_BUF_PEF,
    PSLR_BUF_DNG,
    PSLR_BUF_JPEG_MAX,
    PSLR_BUF_JPEG_MAX_M1,
    PSLR_BUF_JPEG_MAX_M2,
    PSLR_BUF_JPEG_MAX_M3,
    PSLR_BUF_PREVIEW = 8,
    PSLR_BUF_THUMBNAIL = 9 // 7 works also
} pslr_buffer_type;
```

#### [02 03] Delete Buffer (buffer_number)

This command deletes the specified buffer. Indexing starts from `0`.

### 3.3 Command Group 0x04/0x05 - Segment Info Related

The downloadable content of the selected image buffer are stored in chunks. The maximum number of chucks is four. Information about the chucks are stored in segments information blocks. The maximum number of blocks is ten.

#### [04 00] Get Buffer Segment Information ()

This command reads information about the currently selected segment information block. This command returns 16 bytes representing 4 32-bit integers: `a`, `b`, `addr`, `length`. `b` is the type of the block.

* `b==1` is the first information block. In this case `addr`, and `length` are 0.
* `b==4` defines the position in out output file where we should write the next chunk. `length` stores this byte position, `addr` is always 0 in this case.
* `b==3` gives us the position of the next chuck of the image in the internal buffer of the camera. `addr` defines the position, `length` is the number of bytes we can read.
* `b==2` is the last information block. In this case `addr`, and `length` are 0.

For instance a buffer stored in four chunks are represented the following segment information blocks: `b1, b4, b3, b4, b3, b4, b3, b4, b3, b2`. The `length` field of the `b4` blocks are always equals to the sum of the `length` fields of the `b3` blocks before.

#### [04 01] Next Buffer Segment (0)

This command changes to the next segment information block. It is possible to cycle through the blocks.

### 3.4 Command Group 0x06 - Image Download/Upload

#### [06 00] Prepare Download (address, block)

This command seems to dump given memory area to a buffer, so it can be retrieved later.

#### [06 02] Get Download Data ()

This is a `scsi_read()` command, and the attached buffer will be the downloaded data.

#### [06 03] Upload?

#### [06 04] Upload?

### 3.5 Command Group 0x09 - Play

### 3.6 Command Group 0x0B - GUI

### 3.7 Command Group 0x10/0x11 - Action

#### [10 05] Shutter Release (press_status)

`press_status`: 1: Half pressed, 2: Full pressed.

#### [10 06] AE Lock ()
#### [10 07] Green ()
#### [10 08] AE Unlock ()
#### [10 0A] Connect (on_off)

`on_off`:   1: Connect, 0: Disconnect

#### [10 0C] Continuous Shooting ()
#### [10 0D] Bulb (on_off)

`on_off`:   1: Start, 0: Stop

#### [10 11] Dust Removal ()

### 3.8 Command Group 0x13 - Sequence

### 3.9 Command Group 0x14/15 - Auto Focus

### 3.10 Command Group 0x16/0x17 - Auto Exposure

### 3.11 Command Group 0x18/0x19 - Change Camera Settings

#### [18 01] Exposure Mode (1, exposure_mode)

```C
typedef enum {
    PSLR_EXPOSURE_MODE_P = 0 ,
    PSLR_EXPOSURE_MODE_GREEN = 1,
//    PSLR_EXPOSURE_MODE_HYP = 2,
//    PSLR_EXPOSURE_MODE_AUTO_PICT = 1,
//    PSLR_EXPOSURE_MODE_GREEN = 3, maybe 1 is AUTO_PICT
    PSLR_EXPOSURE_MODE_TV = 4,
    PSLR_EXPOSURE_MODE_AV = 5,
//    PSLR_EXPOSURE_MODE_TV_SHIFT = 6, //?
//    PSLR_EXPOSURE_MODE_AV_SHIFT = 7, //?
    PSLR_EXPOSURE_MODE_M = 8,
    PSLR_EXPOSURE_MODE_B = 9,
    PSLR_EXPOSURE_MODE_AV_OFFAUTO = 10,
    PSLR_EXPOSURE_MODE_M_OFFAUTO = 11,
    PSLR_EXPOSURE_MODE_B_OFFAUTO = 12,
    PSLR_EXPOSURE_MODE_TAV = 13, // ?
    PSLR_EXPOSURE_MODE_SV = 15,
    PSLR_EXPOSURE_MODE_X = 16, // ?   
    PSLR_EXPOSURE_MODE_MAX = 17
} pslr_exposure_mode_t;
```

#### [18 03] AE Metering Mode (metering_mode)

```C
typedef enum {
    PSLR_AE_METERING_MULTI,
    PSLR_AE_METERING_CENTER,
    PSLR_AE_METERING_SPOT,
    PSLR_AE_METERING_MAX
} pslr_ae_metering_t;
```

#### [18 04] Flash Mode (flash_mode)

```C
typedef enum {
    PSLR_FLASH_MODE_MANUAL = 0,
    PSLR_FLASH_MODE_MANUAL_REDEYE = 1,
    PSLR_FLASH_MODE_SLOW = 2,
    PSLR_FLASH_MODE_SLOW_REDEYE = 3,
    PSLR_FLASH_MODE_TRAILING_CURTAIN = 4,
    PSLR_FLASH_MODE_AUTO = 5,
    PSLR_FLASH_MODE_AUTO_REDEYE = 6,
    // 7 not used
    PSLR_FLASH_MODE_WIRELESS = 8,
    PSLR_FLASH_MODE_MAX = 9
} pslr_flash_mode_t;
```

#### [18 05] AF Mode (af_mode)

```C
typedef enum {
    PSLR_AF_MODE_MF,
    PSLR_AF_MODE_AF_S,
    PSLR_AF_MODE_AF_C,
    PSLR_AF_MODE_AF_A,
    PSLR_AF_MODE_MAX
} pslr_af_mode_t;
```

#### [18 06] AF Point Selection (selection)

```C
typedef enum {
    PSLR_AF_POINT_SEL_AUTO_5,
    PSLR_AF_POINT_SEL_SELECT,
    PSLR_AF_POINT_SEL_SPOT,
    PSLR_AF_POINT_SEL_AUTO_11, // maybe not for all cameras
    PSLR_AF_POINT_SEL_MAX
} pslr_af_point_sel_t;
```

#### [18 07] AF Point (point)

```C
#define PSLR_AF_POINT_TOP_LEFT   0x1
#define PSLR_AF_POINT_TOP_MID    0x2
#define PSLR_AF_POINT_TOP_RIGHT  0x4
#define PSLR_AF_POINT_FAR_LEFT   0x8
#define PSLR_AF_POINT_MID_LEFT   0x10
#define PSLR_AF_POINT_MID_MID    0x20
#define PSLR_AF_POINT_MID_RIGHT  0x40
#define PSLR_AF_POINT_FAR_RIGHT  0x80
#define PSLR_AF_POINT_BOT_LEFT   0x100
#define PSLR_AF_POINT_BOT_MID    0x200
#define PSLR_AF_POINT_BOT_RIGHT  0x400
```

#### [18 10] White Balance (white_balance_mode)

```C
typedef enum {
    PSLR_WHITE_BALANCE_MODE_AUTO,
    PSLR_WHITE_BALANCE_MODE_DAYLIGHT,
    PSLR_WHITE_BALANCE_MODE_SHADE,
    PSLR_WHITE_BALANCE_MODE_CLOUDY,
    PSLR_WHITE_BALANCE_MODE_FLUORESCENT_DAYLIGHT_COLOR,
    PSLR_WHITE_BALANCE_MODE_FLUORESCENT_DAYLIGHT_WHITE,
    PSLR_WHITE_BALANCE_MODE_FLUORESCENT_COOL_WHITE,
    PSLR_WHITE_BALANCE_MODE_TUNGSTEN,
    PSLR_WHITE_BALANCE_MODE_FLASH,
    PSLR_WHITE_BALANCE_MODE_MANUAL,
    PSLR_WHITE_BALANCE_MODE_MANUAL_2,
    PSLR_WHITE_BALANCE_MODE_MANUAL_3,
    PSLR_WHITE_BALANCE_MODE_KELVIN_1,
    PSLR_WHITE_BALANCE_MODE_KELVIN_2,
    PSLR_WHITE_BALANCE_MODE_KELVIN_3,
    PSLR_WHITE_BALANCE_MODE_FLUORESCENT_WARM_WHITE,
    PSLR_WHITE_BALANCE_MODE_CTE,
    PSLR_WHITE_BALANCE_MODE_MULTI_AUTO,
    PSLR_WHITE_BALANCE_MODE_MAX
} pslr_white_balance_mode_t;
```

#### [18 11] White Balance Adjustment (white_balance_mode, tint, temperature)
#### [18 12] Image Format (1, format)

```C
typedef enum {
    PSLR_IMAGE_FORMAT_JPEG,
    PSLR_IMAGE_FORMAT_RAW,
    PSLR_IMAGE_FORMAT_RAW_PLUS,
    PSLR_IMAGE_FORMAT_MAX
} pslr_image_format_t;
```

#### [18 13] JPEG Stars (1, star)
#### [18 14] JPEG Resolution (1, resolution)

`resolution`:   Number in megapixel

#### [18 15] ISO (value, auto_min, auto_max)
#### [18 16] Shutter Speed (nom, denom)
#### [18 17] Aperture (nom, denom, 0)
#### [18 18] Exposure Compensation (nom, denom)
#### [18 1A] Flash Exposure Compensation (nom, denom)
#### [18 1B] JPEG Image Tone (tone)

```C
typedef enum {
    PSLR_JPEG_IMAGE_TONE_NONE = -1,  
    PSLR_JPEG_IMAGE_TONE_NATURAL,
    PSLR_JPEG_IMAGE_TONE_BRIGHT,
    PSLR_JPEG_IMAGE_TONE_PORTRAIT,
    PSLR_JPEG_IMAGE_TONE_LANDSCAPE,
    PSLR_JPEG_IMAGE_TONE_VIBRANT,
    PSLR_JPEG_IMAGE_TONE_MONOCHROME,
    PSLR_JPEG_IMAGE_TONE_MUTED,
    PSLR_JPEG_IMAGE_TONE_REVERSAL_FILM,
    PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS,
    PSLR_JPEG_IMAGE_TONE_RADIANT,
    PSLR_JPEG_IMAGE_TONE_CROSS_PROCESSING,
    PSLR_JPEG_IMAGE_TONE_FLAT,
    PSLR_JPEG_IMAGE_TONE_AUTO,
    PSLR_JPEG_IMAGE_TONE_MAX
} pslr_jpeg_image_tone_t;
```

#### [18 1C] Drive Mode (drive_mode)

```C
typedef enum {
    PSLR_DRIVE_MODE_SINGLE,
    PSLR_DRIVE_MODE_CONTINUOUS_HI,
    PSLR_DRIVE_MODE_SELF_TIMER_12,
    PSLR_DRIVE_MODE_SELF_TIMER_2,
    PSLR_DRIVE_MODE_REMOTE,
    PSLR_DRIVE_MODE_REMOTE_3,
    PSLR_DRIVE_MODE_CONTINUOUS_LO,
    PSLR_DRIVE_MODE_MAX
} pslr_drive_mode_t;
```

#### [18 1F] Raw Format (1, format)

```C
typedef enum {
    PSLR_RAW_FORMAT_PEF,
    PSLR_RAW_FORMAT_DNG,
    PSLR_RAW_FORMAT_MAX
} pslr_raw_format_t;
```

#### [18 20] JPEG Saturation (0, saturation)
#### [18 21] JPEG Sharpness (0, sharpness)
#### [18 22] JPEG Contrast (0, contrast)
#### [18 23] Color Space (color_space)

```C
typedef enum {
    PSLR_COLOR_SPACE_SRGB,
    PSLR_COLOR_SPACE_ADOBERGB,
    PSLR_COLOR_SPACE_MAX
} pslr_color_space_t;
```

#### [18 25] JPEG Hue (0, hue)

### 3.12 Command Group 0x1A/0x1B - Accessory

### 3.13 Command Group 0x1D - Shack Reduction

### 3.14 Command Group 0x20/0x21 - Settings

#### [20 06] Read DateTime ()

This command returns 24 bytes representing 6 32-bit numbers. The numbers represents year, month, day, hour, minute, and sec.

#### [20 08] Write Setting (offset, value)

This command modifies one setting at the selected offset. Value is a four-byte number (Big-Endian for older, Little-Endian for newer cameras) between 0 and 255.

#### [20 09] Read Setting (offset)

This command returns a 4-byte number (Big-Endian for older, Little-Endian for newer cameras). The represented value is between 0 and 255.

### 3.15 Command Group 0x23 - Adjustment

#### [23 00] WriteAdjData (value)
#### [23 04] SetAdjModeFlag (mode, value)
#### [23 05] GetAdjModeFlag (mode)
#### [23 06] SetAdjData (mode, arg0, arg1, arg2, arg3)

When turn on the debug mode, the `arg0` and `arg1` should be `0x01`;
When turn off the debug mode, `arg{0-3}` should all be zero.

#### [23 07] GetAdjData (mode)

`mode` is 3 when turn on/off the debug mode.

#### [23 11] GetAFTestData ()

### 3.16 Command Group 0x25 - CPU Adjustment
