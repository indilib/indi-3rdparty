/*
 * Copyright 2023 Suzhou Astroasis Vision Technology, Inc. All Rights Reserved.
 *
 * This is header file for Astroasis Oasis Filter Wheel .
 *
 * Note:
 * 1. OFWScan() should be called before any other APIs (except for
 *    OFWGetSDKVersion()) for at least one time because those APIs require
 *    filter wheel ID as their first parameter.
 * 2. OFWScan() can be called for multiple times at any time as long as the
 *    user wants to refresh filter wheels.
 * 3. OFWGetSDKVersion() can be called at anytime and any place.
 * 4. The buffer length for names (including product model, serial number,
 *    friendly name, Bluetooth name) should not be less than OFW_NAME_LEN. The
 *    buffer length for slot names should not be less than OFW_SLOT_NAME_LEN
 * 5. The list of API functions are as follows.
 *        OFWScan(int *number, int *ids);
 *        OFWOpen(int id);
 *        OFWClose(int id);
 *        OFWGetProductModel(int id, char *model);
 *        OFWGetVersion(int id, OFWVersion *version);
 *        OFWGetSerialNumber(int id, char *sn);
 *        OFWGetFriendlyName(int id, char *name);
 *        OFWSetFriendlyName(int id, char *name);
 *        OFWGetBluetoothName(int id, char *name);
 *        OFWSetBluetoothName(int id, char *name);
 *        OFWGetConfig(int id, OFWConfig *config);
 *        OFWSetConfig(int id, OFWConfig *config);
 *        OFWGetStatus(int id, OFWStatus *status);
 *        OFWFactoryReset(int id);
 *        OFWGetSlotNum(int id, int* num);
 *        OFWGetSlotName(int id, int slot, char* name);
 *        OFWSetSlotName(int id, int slot, char* name);
 *        OFWGetFocusOffset(int id, int num, int* offset);
 *        OFWSetFocusOffset(int id, int num, int* offset);
 *        OFWGetColor(int id, int num, int* color);
 *        OFWSetColor(int id, int num, int* color);
 *        OFWSetPosition(int id, int position);
 *        OFWCalibrate(int id);
 *        OFWGetCalibrateData(int id, AOCalibrateData* calibrate);
 *        OFWFirmwareUpgrade(int id, unsigned char *data, int len);
 *        OFWGetSDKVersion(char *version);
 *
 * Refer to SDK demo application for the details of the API usage.
 */

#ifndef __OASIS_FILTER_WHEEL_H__
#define __OASIS_FILTER_WHEEL_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WINDOWS
#define AOAPI		__declspec(dllexport)
#else
#define AOAPI
#endif

#define OFW_MAX_NUM					32		/* Maximum filter wheel numbers supported by this SDK */
#define OFW_VERSION_LEN				32		/* Buffer length for version strings */
#define OFW_NAME_LEN				32		/* Buffer length for name strings */
#define OFW_SLOT_NAME_LEN			16		/* Buffer length for slot name strings */

typedef enum _AOReturn {
	AO_SUCCESS = 0,					/* Success */
	AO_ERROR_INVALID_ID,			/* Device ID is invalid */
	AO_ERROR_INVALID_PARAMETER,		/* One or more parameters are invalid */
	AO_ERROR_INVALID_STATE,			/* Device is not in correct state for specific API call */
	AO_ERROR_BUFFER_TOO_SMALL,		/* Size of buffer is too small */
	AO_ERROR_COMMUNICATION,			/* Data communication error such as device has been removed from USB port */
	AO_ERROR_TIMEOUT,				/* Timeout occured */
	AO_ERROR_BUSY,					/* Device is being used by another application */
	AO_ERROR_NULL_POINTER,			/* Caller passes null-pointer parameter which is not expected */
	AO_ERROR_OUT_OF_RESOURCE,		/* Out of resouce such as lack of memory */
	AO_ERROR_NOT_IMPLEMENTED,		/* The interface is not currently supported */
	AO_ERROR_FAULT,					/* Significant fault which means the device may not work correctly and hard to recovery it */
	AO_ERROR_INVALID_SIZE,			/* Size is invalid */
	AO_ERROR_INVALID_VERSION,		/* Version is invalid */
	AO_ERROR_UNKNOWN = 0x40,		/* Any other errors */
} AOReturn;

/*
 * Used by OFWSetConfig() to indicate which field wants to be set
 */
#define MASK_MODE					0x00000001
#define MASK_AUTORUN				0x00000002
#define MASK_BLUETOOTH				0x00000004
#define MASK_ALL					0xFFFFFFFF

/*
 * Used by OFWGetStatus() to indicate the filter status
 */
#define STATUS_IDLE					0
#define STATUS_MOVING				1
#define STATUS_CALIBRATING			2
#define STATUS_BENCHMARKING			3

typedef struct _OFWVersion
{
	unsigned int protocal;			/* Version of protocal over USB and Bluetooth communication */
	unsigned int hardware;			/* Device hardware version */
	unsigned int firmware;			/* Device firmware version */
	char built[24];					/* Null-terminated string which indicates firmware building time */
} OFWVersion;

typedef struct _OFWConfig {
	unsigned int mask;				/* Used by OFWSetConfig() to indicates which field wants to be set */
	int mode;						/* Mode of the filter wheel operation */
	int autorun;					/* Automatic switch to the target slot when power on */
	int bluetoothOn;				/* 0 - Turn off Bluetooth, others - Turn on Bluetooth */
} OFWConfig;

typedef struct _OFWStatus {
	int temperature;				/* Internal (on board) temperature in 0.01 degree unit */
	int filterStatus;				/* Current motor position */
	int filterPosition;				/* Current motor position, zero - unknown position */
} OFWStatus;

typedef struct _OFWCalibrateData {
	int low[4];						/* Calibration low value */
	int high[4];					/* Calibration high value */
} OFWCalibrateData;

/*
 * API: OFWScan
 *
 * Description
 *     Scan for connected filter wheels.
 *
 * Parameters
 *     number:
 *         Pointer to an int value to hold the number of connected filter wheels.
 *     ids:
 *         Pointer to an int array to hold IDs of connected filter wheels.
 *         The length of the 'ids' array should be equal to or larger than
 *         OFW_MAX_NUM, and it's the caller's responsibility to allocate memory
 *         for "ids" before the API is called.
 *         The valid IDs are stored in first "number" of elements in "ids" array.
 *
 * Return value
 *     AO_SUCCESS:
 *         The function succeeds. This is the only value the API returns.
 *
 * Remarks
 *     This function should be called before any other APIs except for
 *     OFWGetSDKVersion().
 *     Each filter wheel has a unique ID. If one filter wheel is removed from
 *     USB port and then plugged in again, it will be considered as different
 *     filter wheel and will be assigned with different ID during next scan.
 *     If one filter wheel remains connected during two or more scan operations,
 *     it will always have the same ID to indicate it's the same filter wheel.
 *     That is, the caller can call OFWScan() at any time and the IDs of
 *     connected filter wheels returned by previous OFWScan() will still be valid.
 */
AOAPI AOReturn OFWScan(int *number, int *ids);
AOAPI AOReturn OFWOpen(int id);
AOAPI AOReturn OFWClose(int id);
AOAPI AOReturn OFWGetProductModel(int id, char *model);
AOAPI AOReturn OFWGetVersion(int id, OFWVersion *version);
AOAPI AOReturn OFWGetSerialNumber(int id, char *sn);
AOAPI AOReturn OFWGetFriendlyName(int id, char *name);
AOAPI AOReturn OFWSetFriendlyName(int id, char *name);
AOAPI AOReturn OFWGetBluetoothName(int id, char *name);
AOAPI AOReturn OFWSetBluetoothName(int id, char *name);
AOAPI AOReturn OFWGetConfig(int id, OFWConfig *config);
AOAPI AOReturn OFWSetConfig(int id, OFWConfig *config);
AOAPI AOReturn OFWGetStatus(int id, OFWStatus *status);
AOAPI AOReturn OFWFactoryReset(int id);

AOAPI AOReturn OFWGetSlotNum(int id, int *num);
AOAPI AOReturn OFWGetSlotName(int id, int slot, char *name);
AOAPI AOReturn OFWSetSlotName(int id, int slot, char *name);
AOAPI AOReturn OFWGetFocusOffset(int id, int num, int *offset);
AOAPI AOReturn OFWSetFocusOffset(int id, int num, int *offset);
AOAPI AOReturn OFWGetColor(int id, int num, int* color);
AOAPI AOReturn OFWSetColor(int id, int num, int* color);
AOAPI AOReturn OFWSetPosition(int id, int position);
AOAPI AOReturn OFWCalibrate(int id, int mode);
AOAPI AOReturn OFWGetCalibrateData(int id, OFWCalibrateData *calibrate);

AOAPI AOReturn OFWUpgrade(int id);
AOAPI AOReturn OFWFirmwareUpgrade(int id, unsigned char *data, int len);
AOAPI AOReturn OFWGetSDKVersion(char *version);

#ifdef __cplusplus
}
#endif

#endif	/* __OASIS_FILTER_WHEEL_H__ */
