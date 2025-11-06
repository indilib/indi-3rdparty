///
/// @file libflipro.h
///
/// @brief Finger Lakes Instrumentation Camera API
///
/// The library uses the standard way of creating macros which make exporting
/// from a DLL simpler. All files within this DLL are compiled with the LIBFLIPRO_EXPORTS
/// symbol defined on the command line. This symbol should not be defined on any project
/// that uses this DLL. This way any other project whose source files include this file see 
/// LIBFLIPRO_API functions as being imported from a DLL, whereas this DLL sees symbols
/// defined with this macro as being exported.
///

#ifdef __cplusplus
extern "C" { 
#endif

#ifndef _LIBFLIPRO_H_
/// @cond DO_NOT_DOCUMENT
#define _LIBFLIPRO_H_
/// @endcond

#ifdef __linux__
#include <wchar.h>
#include <stdarg.h>
#endif
#include "stdbool.h"
#include "stdint.h"

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the LIBFLIPRO_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// LIBFLIPRO_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
/// @cond DO_NOT_DOCUMENT
#if defined(_WIN32) || defined(_WINDOWS)
#ifdef LIBFLIPRO_EXPORTS
#define LIBFLIPRO_API __declspec(dllexport) int32_t 
#define LIBFLIPRO_API_DATA __declspec(dllexport)
#define LIBFLIPRO_VOID __declspec(dllexport) void
#else
#ifdef _USRDLL
#define LIBFLIPRO_API __declspec(dllimport) int32_t 
#define LIBFLIPRO_API_DATA __declspec(dllimport)
#define LIBFLIPRO_VOID __declspec(dllimport) void
#else
#define LIBFLIPRO_API int32_t
#define LIBFLIPRO_API_DATA
#define LIBFLIPRO_VOID void
#endif

#endif
#endif

#ifdef __linux__
#define LIBFLIPRO_API int32_t
#define LIBFLIPRO_API_DATA
#define LIBFLIPRO_VOID void
#endif
/// @endcond

////////////////////////////////////////////////////////////////////////
// Typedefs, Defines, Macros
////////////////////////////////////////////////////////////////////////
// Version information
/// @cond DO_NOT_DOCUMENT
#define FPRO_API_VERSION_MAJOR (2)
#define FPRO_API_VERSION_MINOR (1)    // Should change if must change the Camera side as well
#define FPRO_API_VERSION_BUILD (4)    // Minor changes not requiring Camera change
/// @endcond

//
// Some Helpful #defines
///
/// @brief Convert image size in  Bytes to Pixels
///
/// The frame size is 1.5 * width * height (12 bit pixels)
/// This macro only works if the pixel size is 12 bits.
#define FPRO_IMAGE_FRAMEBYTES_TO_PIXELS(__framebytes)  (((__framebytes) << 1) / 3)
///
/// @brief Convert Pixels to Image size in Bytes
///
/// This macro only works if the pixel size is 12 bits.
#define FPRO_IMAGE_PIXELS_TO_FRAMEBYTES(__pixels)  (((__pixels) & 0x1) ? ((((__pixels) * 3 ) >> 1) + 1) : (((__pixels) * 3 ) >> 1))
///
/// @brief Convert image dimensions in Pixels to Image size in Bytes
///
/// This macro only works if the pixel size is 12 bits.
#define FPRO_IMAGE_DIMENSIONS_TO_FRAMEBYTES(__width,__height)  FPRO_IMAGE_PIXELS_TO_FRAMEBYTES((__width) * (__height))
///
/// @brief Maximum number of pre/post frame Reference rows supported by the API.
///
/// This number should be treated as a constant.
#define FPRO_REFERENCE_ROW_MAX  (4094)
///
/// @brief Height of Thumbnail image in pixels.
#define FPRO_THUMBNAIL_ROWS  (512)
///
/// @brief Width of Thumbnail image in pixels.
#define FPRO_THUMBNAIL_COLUMNS  (512)

///
/// @brief Known Device Types
/// @enum FPRODEVICETYPE
///
/// These constants are returned for the Device Capabilities enumeration 
/// FPROCAPS::CAP_DEVICE_TYPE in the member.
/// See the user manual for a description of the capabilities for 
/// your device.
///
/// @var FPRODEVICETYPE::FPRO_CAM_DEVICE_TYPE_GSENSE400
/// @brief Enum value = 0x01000400
/// @var FPRODEVICETYPE::FPRO_CAM_DEVICE_TYPE_GSENSE2020
/// @brief Enum value = 0x01002020
/// @var FPRODEVICETYPE::FPRO_CAM_DEVICE_TYPE_GSENSE4040
/// @brief Enum value = 0x01004040
/// @var FPRODEVICETYPE::FPRO_CAM_DEVICE_TYPE_GSENSE6060
/// @brief Enum value = 0x01006060
/// @var FPRODEVICETYPE::FPRO_CAM_DEVICE_TYPE_DC230_42
/// @brief Enum value = 0x03023042
/// @var FPRODEVICETYPE::FPRO_CAM_DEVICE_TYPE_DC230_84
/// @brief Enum value = 0x03023084
/// @var FPRODEVICETYPE::FPRO_CAM_DEVICE_TYPE_SONYIMX183
/// @brief Enum value = 0x04000183
/// @var FPRODEVICETYPE::FPRO_CAM_DEVICE_TYPE_FTM
/// @brief Enum value = 0x04000F1F
/// @var FPRODEVICETYPE::FPRO_CAM_DEVICE_TYPE_LSGEN
/// @brief Enum value = 0x850F0000
/// 

#ifdef __cplusplus
enum class FPRODEVICETYPE : unsigned int
#else
typedef enum
#endif
{
	FPRO_CAM_DEVICE_TYPE_GSENSE400 = 0x01000400,
	FPRO_CAM_DEVICE_TYPE_GSENSE2020 = 0x01002020,
	FPRO_CAM_DEVICE_TYPE_GSENSE4040 = 0x01004040,
	FPRO_CAM_DEVICE_TYPE_GSENSE6060 = 0x01006060,
	FPRO_CAM_DEVICE_TYPE_DC230_42 = 0x03023042,
	FPRO_CAM_DEVICE_TYPE_DC230_84 = 0x03023084,
	FPRO_CAM_DEVICE_TYPE_DC4320 = 0x03004320,
	FPRO_CAM_DEVICE_TYPE_SONYIMX183 = 0x04000183,
	FPRO_CAM_DEVICE_TYPE_FTM = 0x04000F1F,
	FPRO_CAM_DEVICE_TYPE_BIOLINE_420 = 0x05000420,
	FPRO_CAM_DEVICE_TYPE_BIOLINE_455 = 0x05000455,
	FPRO_CAM_DEVICE_TYPE_BIOLINE_492 = 0x05000492,
	FPRO_CAM_DEVICE_TYPE_BIOLINE_530 = 0x05000530,
	FPRO_CAM_DEVICE_TYPE_BIOLINE_533 = 0x05000533,
	FPRO_CAM_DEVICE_TYPE_BIOLINE_541 = 0x05000541,
	FPRO_CAM_DEVICE_TYPE_BIOLINE_571 = 0x05000571,
	FPRO_CAM_DEVICE_TYPE_LSGEN = 0x850F0000,
#ifdef __cplusplus
};
#else
} FPRODEVICETYPE;
#endif

///
/// @brief Maximum String Length
///
/// The maximum number of characters (not bytes) allowed in USB strings throughout
/// the API.
///
#define FPRO_USB_STRING_MAXLEN  (256)

///
/// @brief Maximum path length for low level OS device path
///
/// The maximum number of characters (not bytes) allowed for device path
/// strings throughout the API.
///
#define FPRO_DEVICE_MAX_PATH_LENGTH  (1024)

///
/// @brief Supported Connection Types
/// @enum FPROCONNECTION
///
/// This enumeration is used as part of the FPRODEVICEINFO structure to
/// return the physical connection type to the camera.
///
/// @var FPROCONNECTION::FPRO_CONNECTION_USB 
/// @brief Camera is connected with a USB link.
///
/// @var FPROCONNECTION::FPRO_CONNECTION_FIBRE 
/// @brief Camera is connected with a Fibre Optic link.
///
/// @var FPROCONNECTION::FPRO_CONNECTION_ENET 
/// @brief Camera is connected with an ethernet link.
///
#ifdef __cplusplus
enum class FPROCONNECTION
#else
typedef enum
#endif
{
	FPRO_CONNECTION_USB,
	FPRO_CONNECTION_FIBRE,
	FPRO_CONNECTION_ENET
#ifdef __cplusplus
};
#else
} FPROCONNECTION;
#endif



///
/// @brief Known USB Connection Speeds
/// @enum FPROUSBSPEED
///
/// This enumeration is used as part of the FPRODEVICEINFO structure to
/// return the detected USB connection speed.  FLI Cameras require a
/// FPRO_USB_SUPERSPEED USB connection in order to transfer image data reliably.
///
/// @var FPROUSBSPEED::FPRO_USB_FULLSPEED 
/// @brief Full Speed Connection
///
/// @var FPROUSBSPEED::FPRO_USB_HIGHSPEED 
/// @brief High Speed Connection
///
/// @var FPROUSBSPEED::FPRO_USB_SUPERSPEED 
/// @brief Super Speed Connection
///
#ifdef __cplusplus
enum class FPROUSBSPEED
#else
typedef enum
#endif
{
	FPRO_USB_FULLSPEED,
	FPRO_USB_HIGHSPEED,
	FPRO_USB_SUPERSPEED
#ifdef __cplusplus
};
#else
} FPROUSBSPEED;
#endif


///
/// @typedef ip_info_t FPROIPINFO
/// @brief IP Connection Information
///
/// This structure contains detailed information on the physical IP Network
/// connection. See  #FPRODEVICEINFO for additional information.
/// 
/// @var FPROIPINFO::uiIPAddress 
/// @brief The IP Address of the camera.  The 32 bit number is partitioned in 8 bit
///        quantities corresponding to the dot ('.') notation of an IPV4 address.  The
///        most significant byte is the first number (A) in the string dot notation
///        A.B.C.D
/// @var FPROIPINFO::uiSpeed 
/// @brief If it can be determined on the host, the speed of the connection in Mega-bits per second.
///        Zero (0) means it could not be determined.
/// 
typedef struct ip_info_t
{
	uint32_t uiIPAddress;
	uint32_t uiSpeed;
} FPROIPINFO;

///
/// @typedef fibre_info_t FPROFIBREINFO
/// @brief Fibre Connection Information
///
/// This structure contains detailed information on the physical Fibre
/// connection. See  #FPRODEVICEINFO for additional information.
/// 
/// @var FPROFIBREINFO::uiChannelStatus 
/// @brief The Channel status of the card.  Used internally by the API.
/// @var FPROFIBREINFO::uiVersion 
/// @brief The version information from the PCIE Fibre card.
/// 
typedef struct fibre_info_t
{
	uint32_t uiChannelStatus;
	uint64_t uiVersion;
} FPROFIBREINFO;

///
/// @typedef con_info_t FPROCONINFO
/// @brief Connection Information
///
/// This structure contains detailed information on how the camera is physically 
/// connected to the host.  It is used as part of the #FPRODEVICEINFO structure
/// returned by the #FPROCam_GetCameraList function.  See #FPRODEVICEINFO and 
/// #FPROCam_GetCameraList for additional information.
///
/// @var FPROCONINFO::eConnType 
/// @brief The physical connection type.  If the connection type is FPRO_CONNECTION_USB, then the
///        uiVendorId, uiProdId, and eUSBSpeed fields are also filled in.  Otherwise those fields
///        are not used and their contents is undefined.
/// @var FPROCONINFO::uiVendorId 
/// @brief The USB vendor ID.  This field is applicable only when the eConnType is #FPRO_CONNECTION_USB.
/// @var FPROCONINFO::uiProdId 
/// @brief The USB Product ID.  This field is applicable only when the eConnType is #FPRO_CONNECTION_USB.
/// @var FPROCONINFO::attr 
/// @brief The attributes of the connection, This eUSBSpeed field is applicable only when the eConnType is 
///        #FPRO_CONNECTION_USB.  Likewise, the ipInfo field is only applicable when the eConnType is
///        #FPRO_CONNECTION_ENET, and fibreInfo is only applicable when the eConnType is #FPRO_CONNECTION_FIBRE.
/// <br><br>NOTE: When connected through USB, FLI Cameras require a FPRO_USB_SUPERSPEED USB connection 
///           in order to transfer image data reliably.
typedef struct con_info_t
{
	FPROCONNECTION eConnType;
	uint32_t       uiVendorId;
	uint32_t       uiProdId;
	union
	{
		FPROUSBSPEED  eUSBSpeed;
		FPROFIBREINFO fibreInfo;
		FPROIPINFO    ipInfo;
	} attr;

} FPROCONINFO;

///
/// @typedef host_if_info FPROHOSTINFO
/// @brief Host driver and hardware information
///
/// This structure contains detailed information for any host drivers or hardware
/// specific to the FLI cameras (e.g. the PCIE Fibre card).  This structure is used
/// by the #FPROCam_GetHostInterfaceInfo API and is typically used for debug purposes.
///
/// @var FPROHOSTINFO::uiDriverVersion 
/// @brief The driver version.
/// @var FPROHOSTINFO::uiHWVersion 
/// @brief The hardware version information if available.
/// @var FPROHOSTINFO::cFibreSerialNum 
/// @brief The serial number of the Fibre portion of the PCIE card.
/// @var FPROHOSTINFO::cPcieSerialNum 
/// @brief The serial number of the PCIE portion of the PCIE card.
typedef struct host_if_info
{
	uint32_t uiDriverVersion;
	uint64_t uiHWVersion;
	wchar_t  cFibreSerialNum[FPRO_USB_STRING_MAXLEN];
	wchar_t  cPcieSerialNum[FPRO_USB_STRING_MAXLEN];
} FPROHOSTINFO;

///
/// @typedef device_info_t FPRODEVICEINFO
/// @brief Device Information
///
/// This is used as the Camera Device enumeration structure.  It is
/// returned by the #FPROCam_GetCameraList function and contains the
/// list of detected cameras.  To open a connection to a specific camera,
/// a single FPRODEVICEINFO structure is passed to the #FPROCam_Open function.
///
/// @var FPRODEVICEINFO::cFriendlyName 
/// @brief Human readable friendly name of the USB device.
///        This string along with the cSerialNo field provide a unique name
///        for your device suitable for a user interface.
/// @var FPRODEVICEINFO::cSerialNo 
/// @brief The manufacturing serial number of the device.
/// @var FPRODEVICEINFO::cDevicePath
/// @brief The OS device Path.  Used internally by the API for opening requisite file 
///        descriptors to connect to the device.
/// @var FPRODEVICEINFO::conInfo
/// @brief Details of the physical connection. 
typedef struct device_info_t
{
	wchar_t        cFriendlyName[FPRO_USB_STRING_MAXLEN];
	wchar_t        cSerialNo[FPRO_USB_STRING_MAXLEN];
	wchar_t        cDevicePath[FPRO_DEVICE_MAX_PATH_LENGTH];
	FPROCONINFO    conInfo;
} FPRODEVICEINFO;


///
/// @brief Version String Lengths
///
/// Maximum length characters (not bytes) of version strings.
#define FPRO_VERSION_STRING_MAXLEN  (32)
///
/// @typedef device_version_info_t FPRODEVICEVERS
/// @brief Device Version Information
///
/// Contains the various version numbers supplied by the device.
/// See #FPROCam_GetDeviceVersion.
///
/// @var FPRODEVICEVERS::cFirmwareVersion
/// @brief The version of firmware on the internal device processor.
/// @var FPRODEVICEVERS::cFPGAVersion
/// @brief The version of firmware on the internal FPGA device.
/// @var FPRODEVICEVERS::cControllerVersion
/// @brief The version of firmware on the internal sensor controller device.
/// @var FPRODEVICEVERS::cHostHardwareVersion
/// @brief The version of firmware on the host interface card if any.  For example,
///        it returns the hardware version of the Host PCIE card for Fibre connections.
///        For USB connections, there is no host side interface card so '0' is returned.
typedef struct device_version_info_t
{
	wchar_t cFirmwareVersion[FPRO_VERSION_STRING_MAXLEN];
	wchar_t cFPGAVersion[FPRO_VERSION_STRING_MAXLEN];
	wchar_t cControllerVersion[FPRO_VERSION_STRING_MAXLEN];
	wchar_t cHostHardwareVersion[FPRO_VERSION_STRING_MAXLEN];
} FPRODEVICEVERS;

///
/// @enum FPROTESTIMAGETYPE
/// @brief Test Image Types
///
/// Some cameras have the ability to generate test image data.  This enumeration 
/// is used to tell the camera how you would like the test image data to be
/// formatted.  Not all cameras support all test image types.  Consult you camera
/// documentation for more details on which test images are supported for your device.
///
/// @var FPROTESTIMAGETYPE::FLI_TESTIMAGE_TYPE_ROW
/// @brief Row order format.
/// <br>The first 'width' number of pixels will be 0, the second 'width'
///     number of pixels will be 1... etc.
/// @var FPROTESTIMAGETYPE::FLI_TESTIMAGE_TYPE_COL
/// @brief Column order format.
/// <br>The first pixel of the first row will be 0, the second pixel will be 1...
///     the n'th pixel of the row will n.  The first pixel of the second row
///     will be 0 again, followed by 1, etc...
/// @var FPROTESTIMAGETYPE::FLI_TESTIMAGE_IMX183_VERTICAL
/// @brief IMX183 Sensor Vertical Test Pattern.
/// @var FPROTESTIMAGETYPE::FLI_TESTIMAGE_IMX183_HORIZONTAL
/// @brief IMX183 Sensor Horizontal Test Pattern.
/// @var FPROTESTIMAGETYPE::FLI_TESTIMAGE_IMX183_ALL_LOW
/// @brief IMX183 Sensor All Low Test Pattern.
/// <br>All pixels are 0x000.
/// @var FPROTESTIMAGETYPE::FLI_TESTIMAGE_IMX183_ALL_HIGH
/// @brief IMX183 Sensor All High Test Pattern.
/// <br>All pixels are 0xFFF.
/// @var FPROTESTIMAGETYPE::FLI_TESTIMAGE_IMX183_LOW_HIGH
/// @brief IMX183 Sensor Repeating Low High Test Pattern.
/// <br>All pixels are 0x555.
/// @var FPROTESTIMAGETYPE::FLI_TESTIMAGE_IMX183_HIGH_LOW
/// @brief IMX183 Sensor Repeating High Low Test Pattern.
/// <br>All pixels are 0xAAA.
///
#ifdef __cplusplus
enum class FPROTESTIMAGETYPE
#else
typedef enum
#endif
{
	FLI_TESTIMAGE_TYPE_ROW,
	FLI_TESTIMAGE_TYPE_COL,
	FLI_TESTIMAGE_IMX183_VERTICAL,
	FLI_TESTIMAGE_IMX183_HORIZONTAL,
	FLI_TESTIMAGE_IMX183_ALL_LOW,
	FLI_TESTIMAGE_IMX183_ALL_HIGH,
	FLI_TESTIMAGE_IMX183_LOW_HIGH,
	FLI_TESTIMAGE_IMX183_HIGH_LOW
#ifdef __cplusplus
};
#else
} FPROTESTIMAGETYPE;
#endif


///
/// @enum FPROEXTTRIGTYPE
/// @brief External Trigger Types
///
/// This enumeration defines the types of external triggers available.
/// There is a single external trigger line available to the camera.  This
/// enumeration governs how this signal behaves.  This enumeration is used with
/// the #FPROCtrl_GetExternalTriggerEnable and #FPROCtrl_SetExternalTriggerEnable API's.
///
/// @var FPROEXTTRIGTYPE::FLI_EXT_TRIGGER_RISING_EDGE
/// @brief Trigger Exposure on Rising Edge
/// <br>For this setting, when the external trigger line goes from low to high, it 
///     triggers the exposure to begin on the camera.  The exposure will complete based
///     on the exposure time set with the FPROCtrl_SetExposure API.
/// @var FPROEXTTRIGTYPE::FLI_EXT_TRIGGER_FALLING_EDGE
/// @brief Trigger Exposure on Falling Edge
/// <br>For this setting, when the external trigger line goes from high to low, it 
///     triggers the exposure to begin on the camera.  The exposure will complete based
///     on the exposure time set with the FPROCtrl_SetExposure API.
/// @var FPROEXTTRIGTYPE::FLI_EXT_TRIGGER_EXPOSE_ACTIVE_HIGH
/// @brief Exposure Active High
/// <br>For this setting, the exposure is active the entire time the external trigger signal is high.  
///     The exposure will complete when the external trigger line goes low or when the exposure time
///     has reached the value set with the FPROCtrl_SetExposure API (whichever occurs first).  
///     That is, in this case the value used in the FPROCtrl_SetExposure API acts as a maximum exposure time. 
/// @var FPROEXTTRIGTYPE::FLI_EXT_TRIGGER_EXPOSE_ACTIVE_LOW
/// @brief Exposure Active High
/// <br>For this setting, the exposure is active the entire time the external trigger signal is low.  
///     The exposure will complete when the external trigger line goes high or when the exposure time
///     has reached the value set with the FPROCtrl_SetExposure API (whichever occurs first).  
///     That is, in this case the value used in the FPROCtrl_SetExposure API acts as a maximum exposure time. 
///
#ifdef __cplusplus
enum class FPROEXTTRIGTYPE
#else
typedef enum
#endif
{
	FLI_EXT_TRIGGER_FALLING_EDGE,
	FLI_EXT_TRIGGER_RISING_EDGE,
	FLI_EXT_TRIGGER_EXPOSE_ACTIVE_LOW,
	FLI_EXT_TRIGGER_EXPOSE_ACTIVE_HIGH
#ifdef __cplusplus
};
#else
} FPROEXTTRIGTYPE;
#endif

///
/// @typedef ext_trigger_info_t FPROEXTTRIGINFO
/// @brief External Trigger Setup Details
///
/// This structure is used to set up the External Trigger capability on the camera.
/// See #FPROEXTTRIGTYPE for more information.
/// <br>
/// Note that the bSingleFramePerTrigger function is not avaliable on older cameras.  This 
/// function was introduced in the camera firmware versions 0x2A.  In addition, in API versions
/// prior to 1.12.32, the API enforced an Image Count of 1 when enabling the external trigger.
/// <br>
///
/// @var FPROEXTTRIGINFO::eTriggerType 
/// @brief The trigger behavior type.
/// @var FPROEXTTRIGINFO::bSingleFramePerTrigger 
/// @brief Default behavior of the external trigger gets uiFrameCount images.  Setting this ensures
///        only a single frame per trigger.  see #FPROCtrl_SetExternalTriggerEnable() for the uiFrameCount parameter.
/// @var FPROEXTTRIGINFO::bEnable 
/// @brief True= enable the external trigger.  False= disable the external trigger.
typedef struct ext_trigger_info_t
{
	FPROEXTTRIGTYPE eTriggerType;
	bool            bSingleFramePerTrigger;
	bool            bEnable;
} FPROEXTTRIGINFO;

///
/// @typedef FPRODBGLEVEL
/// @brief Convenience (unsigned int) typedef for debug levels.
///
/// The API provides a debug interface.  This sets the level of debug information
/// that can be logged by your application.
///
/// @var FPRODBGLEVEL::FPRO_DEBUG_NONE 
/// @brief All debug disabled
/// @var FPRODBGLEVEL::FPRO_DEBUG_ERROR 
/// @brief Only ERROR level debug is output
/// @var FPRODBGLEVEL::FPRO_DEBUG_WARNING 
/// @brief WARNING and ERROR debug is output
/// @var FPRODBGLEVEL::FPRO_DEBUG_INFO 
/// @brief INFO, WARNING, and ERROR debug is output
/// @var FPRODBGLEVEL::FPRO_DEBUG_REGRW 
/// @brief REGWR, INFO, WARNING, and ERROR debug is output
/// @var FPRODBGLEVEL::FPRO_DEBUG_DEBUG 
/// @brief DEBUG, REGRW, INFO, WARNING, and ERROR debug is output
/// @var FPRODBGLEVEL::FPRO_DEBUG_TRACE 
/// @brief TRACE, DEBUG, REGRW, INFO, WARNING, and ERROR debug is output


#define FPRO_DEBUG_NONE     (0x00000000)
#define FPRO_DEBUG_ERROR    (0x00000001)
#define FPRO_DEBUG_WARNING  (0x00000002)
#define FPRO_DEBUG_INFO     (0x00000004)
#define FPRO_DEBUG_REGRW    (0x00000008)
#define FPRO_DEBUG_DEBUG    (0x00000010)
#define FPRO_DEBUG_TRACE    (0x00000020)
typedef unsigned int        FPRODBGLEVEL;



///
/// @enum FPROGPSSTATE
/// @brief GPS Connection State
///
/// This enumeration defines the possible states of an optional GPS receiver attached
/// to the camera.  The GPS data is contained in the Meta Data that prepends every image.
/// The format for the fields in the Meta Data is shown below:
/// <br>
/// <br><B>Timestamp</B><br>
/// @code
///	Year - 2016(31:26), Month(25:22), Days(21:17), Hours(16:12), Minutes(11:6), Seconds(5:0)
/// @endcode
/// <br><B>Longitude</B><br>
/// @code
///  East / West(31), 600000 * DDD + 10000 * MM.MMMM(31:0)
/// @endcode
/// Where bit 31 is 1 for East and 0 for West
/// <br>
/// <br><B>Latitude</B><br>
/// @code
/// North / South(31), 600000 * DD + 10000 * MM.MMMM(31:0)
/// @endcode
/// Where bit 31 is 1 for North and 0 for South
/// <br>
/// @var FPROGPSSTATE::FPRO_GPS_NOT_DETECTED 
/// @brief GPS unit has not been detected by the camera.
/// @var FPROGPSSTATE::FPRO_GPS_DETECTED_NO_SAT_LOCK 
/// @brief GPS unit has been detected by the camera but the satellite lock has not been made.
/// @var FPROGPSSTATE::FPRO_GPS_DETECTED_AND_SAT_LOCK 
/// @brief GPS unit has been detected by the camera and the satellite lock has been made.  This is the only
///        value that will provide accurate results in the Meta Data.
/// @var FPROGPSSTATE::FPRO_GPS_DETECTED_SAT_LOCK_TIME_ERROR
/// @brief GPS unit has been detected by the camera and the satellite lock has been made.  The camera
///        has lost the precision time signal from the GPS unit.  As a result, the time stamp in the meta data
///        for an image could be incorrect by as much as 1 second.  This is typically the result of using
///        the external illumination signal on the camera (the physical lines are shared).  Make sure the
///        the external illumination signal is off using the #FPROCtrl_SetIlluminationOn() API.  This could
///        also be caused by a GPS cable problem.
#ifdef __cplusplus
enum class FPROGPSSTATE
#else
typedef enum
#endif
{
	FPRO_GPS_NOT_DETECTED = 0,
	FPRO_GPS_DETECTED_NO_SAT_LOCK,
	FPRO_GPS_DETECTED_AND_SAT_LOCK,
	FPRO_GPS_DETECTED_SAT_LOCK_TIME_ERROR

#ifdef __cplusplus
};
#else
} FPROGPSSTATE;
#endif

///
/// @enum FPROGPSOPT
/// @brief GPS Options
///
/// This enumeration defines the possible options that may be set up for the
/// GPS unit.  Note that not all cameras support this feature. Consult your camera
/// documentation for details.
/// <br>
/// <br>
/// @var FPROGPSOPT::FPRO_GPSOPT_WAAS_EGNOS_ENABLE 
/// @brief Enable the WAAS and EGNOS augmentation feature.
/// @var FPROGPSOPT::FPRO_GPSOPT_GLONASS_ENABLE 
/// @brief Enable GLONASS (Globalnaya Navigazionnaya Sputnikovaya Sistema, or Global Navigation Satellite System) 
/// capability.
#ifdef __cplusplus
enum class FPROGPSOPT
#else
typedef enum
#endif
{
	FPRO_GPSOPT_WAAS_EGNOS_ENABLE = 0x01,
	FPRO_GPSOPT_GLONASS_ENABLE = 0x02

#ifdef __cplusplus
};
#else
} FPROGPSOPT;
#endif


///
/// @enum FPROSENSREADCFG
/// @brief Sensor Read Out Configuration
///
/// Some camera models support different physical imaging sensor read out configurations.
/// This enumeration allows setting and retrieving the sensor read out configuration through the
/// #FPROSensor_SetReadoutConfiguration() and #FPROSensor_GetReadoutConfiguration().  Consult your
/// camera user documentation for availability of this feature for your camera model.
/// <br>
/// For the Cobalt cameras that support this feature, you may select one of the channels or all four
/// of the channels.  Selecting 2 or 3 channels is not allowed.
/// <br>
/// @var FPROSENSREADCFG::FPRO_SENSREAD_CB_TOPLEFT
/// @brief Read data using the top left channel of the sensor.
/// @var FPROSENSREADCFG::FPRO_SENSREAD_CB_TOPRIGHT
/// @brief Read data using the top right channel of the sensor.
/// @var FPROSENSREADCFG::FPRO_SENSREAD_CB_BOTTOMLEFT
/// @brief Read data using the bottom left channel of the sensor.
/// @var FPROSENSREADCFG::FPRO_SENSREAD_CB_BOTTOMRIGHT
/// @brief Read data using the bottom left channel of the sensor.
/// @var FPROSENSREADCFG::FPRO_SENSREAD_CB_ALL
/// @brief Read data using all 4 sensor channels.
#ifdef __cplusplus
enum class FPROSENSREADCFG
#else
typedef enum
#endif
{
	FPRO_SENSREAD_CB_BOTTOMLEFT = 0x01,
	FPRO_SENSREAD_CB_BOTTOMRIGHT = 0x02,
	FPRO_SENSREAD_CB_TOPLEFT = 0x04,
	FPRO_SENSREAD_CB_TOPRIGHT = 0x08,
	FPRO_SENSREAD_CB_ALL = 0x0F
#ifdef __cplusplus
};
#else
} FPROSENSREADCFG;
#endif

//
/// @brief Sensor Mode Name Length
///
/// Max allowed name length for Camera Modes. See #FPROSENSMODE
///
#define FPRO_SENSOR_MODE_NAME_LENGTH  (32)

///
/// @typedef sensor_mode_t FPROSENSMODE
/// @brief Sensor Modes
///
/// The FLI Camera devices support the concept of 'Modes'.  A mode is a collection
/// of settings for the camera.  As this structure illustrates, the mode has a
/// name and an index.  The name can be used primarily for a user interface so that
/// a user can see a friendly and descriptive name for the mode.  The index is used by
/// the API to set a particular mode on the camera.  See #FPROSensor_SetMode, #FPROSensor_GetMode,
/// and #FPROSensor_GetModeCount.
///
/// @var FPROSENSMODE::uiModeIndex 
/// @brief The corresponding index of the mode name.
/// @var FPROSENSMODE::wcModeName 
/// @brief A descriptive human readable name for the mode suitable for
///        a user interface.
typedef struct sensor_mode_t
{
	uint32_t uiModeIndex;
	wchar_t  wcModeName[FPRO_SENSOR_MODE_NAME_LENGTH];
} FPROSENSMODE;


///
/// @brief Gain Scale Factor
///
/// All gain table values (see #FPROGAINTABLE) returned by the API are scaled 
/// by the factor #FPRO_GAIN_SCALE_FACTOR.
#define FPRO_GAIN_SCALE_FACTOR (1000)

///
/// @enum  FPROGAINTABLE
/// @brief Gain Tables
///
/// The camera makes available specific gain values for the image sensor.
/// Each set of values is stored in a table and this enum allows you to pick
/// the desired gain table to get using the function #FPROSensor_GetGainTable.
/// The values in the table can be used as part of a user interface allowing users
/// to select a specific gain setting. The settings are retrieved and set by index
/// in the gain table using #FPROSensor_GetGainIndex and #FPROSensor_SetGainIndex.
/// <br><br> Note that all gain table values returned by the API are scaled by the factor #FPRO_GAIN_SCALE_FACTOR.
///
/// @var FPROGAINTABLE::FPRO_GAIN_TABLE_LOW_CHANNEL 
/// @brief Low Gain Channel used for Low Gain images in HDR modes.
/// @var FPROGAINTABLE::FPRO_GAIN_TABLE_HIGH_CHANNEL 
/// @brief High Gain Channel used for LDR modes
/// <br>NOTE: Different cameras support different gain settings.  See #FPROCAPS
/// for obtaiing the size of each of these gain tables.
#ifdef __cplusplus
enum class FPROGAINTABLE
#else
typedef enum
#endif
{
	FPRO_GAIN_TABLE_LOW_CHANNEL,
	FPRO_GAIN_TABLE_HIGH_CHANNEL,

	FPRO_GAIN_TABLE_CHANNEL_NUM,
#ifdef __cplusplus
};
#else
} FPROGAINTABLE;
#endif

///
/// @typedef gain_value_t FPROGAINVALUE
/// @brief Gain Value
///
/// The function #FPROSensor_GetGainTable returns a list of FPROGAINVALUE 
/// items.  The uiDeviceIndex must be used to set the desired gain on the camera
/// using the #FPROSensor_SetGainIndex function.
///
/// @var FPROGAINVALUE::uiValue 
/// @brief The actual gain value.
/// @var FPROGAINVALUE::uiDeviceIndex 
/// @brief The device index to use to set the gain value on the camera.
typedef struct gain_value_t
{
	uint32_t uiValue;
	uint32_t uiDeviceIndex;
} FPROGAINVALUE;

///
/// @enum  FPROBLACKADJUSTCHAN
/// @brief Black Adjust Channels
///
/// Depending on the camera model, multiple channels may be supported with respect to Black Level and Black Sun adjustment.
/// This enumeration lists the appropriate channels supported by the API.  They are meant for use with the 
/// #FPROSensor_GetBlackLevelAdjust, and #FPROSensor_GetBlackSunAdjust
/// API calls to specify the channel for the adjustment.
/// <br><br>
///
/// @var FPROBLACKADJUSTCHAN::FPRO_BLACK_ADJUST_CHAN_LDR 
/// @brief Specifies the LDR Black adjust channel.
/// @var FPROBLACKADJUSTCHAN::FPRO_BLACK_ADJUST_CHAN_HDR 
/// @brief Specifies the HDR Black adjust channel.
/// <br>NOTE: Not supported on all devices. See your specific device documentation for details.
#ifdef __cplusplus
enum class FPROBLACKADJUSTCHAN
#else
typedef enum
#endif
{
	FPRO_BLACK_ADJUST_CHAN_LDR,
	FPRO_BLACK_ADJUST_CHAN_HDR,
#ifdef __cplusplus
};
#else
} FPROBLACKADJUSTCHAN;
#endif

/// @typedef FPROCAPS
/// @brief Camera Capabilities
///
/// Different camera models offer different sets of capabilities based on the
/// imaging sensor and other hardware attributes.  The values returned for these can 
/// be used by an application to configure settings and user interfaces based
/// on the specific camera model that is connected.  The uiDeviceType capability is a 
/// specific device type, one of #FPRODEVICETYPE, that allows further checking
/// by an application as it can cover specific functionality for each model.  
/// That is, the uiDeviceType number maps to a specific camera model.  As such,
/// you can use this number along with the user documentation provided for the
/// associated camera model to determine specific functionality not covered here.
/// See the #FPROSensor_GetCapabilityList() for additional information.
/// <br>
/// <br>
/// Image Scan Inversion: The FPROCAP_IMAGE_INVERTABLE capability indicates whether or not
/// the image sensor supports an inverted readout scan of the pixels. The 32 bit value
/// returned for this capability is partitioned into two 16 bit quantities.  The least
/// significant bit of the upper 16 bits (0x00010000) is the horizontal inversion
/// capability (1 == inverted).  The least significant bit of the lower 16 bits (0x00000001)
/// is the vertical inversion capability.
/// <br>
/// <br>
/// Frame Reference Rows: The CAP_FRAME_REFERENCE_ROWS fields in the capabilities
/// enumeration is the number of physical pre/post frame imaging sensor cells available for
/// the camera model. The 32 bit value returned for this capability is partitioned into two 
/// 16 bit numbers.  The upper 16 bits contains the pre-reference rows, the lower 16 bits
/// contains the post-rows pixels.
///
/// <br>
/// <br>
/// Row Reference Pixels: The FPROCAP_ROW_REFERENCE_PIXELS are reference pixels that may occur 
/// prior (pre) or after (post) each row in an image.  Each camera model handles row reference 
/// pixels differently.  Consult your camera model documentation for additional information.  
/// See See #FPROFrame_SetDummyPixelEnable.  The 32 bit value returned for this capability is 
/// partitioned into two 16 bit numbers.  The upper 16 bits contains the pre-reference pixels,
/// the lower 16 bits contains the post-reference pixels.
/// <br>
/// <br>
/// FPROCAP_MERGE_REFERENCE_FRAMES_SUPPORTED: When calling the #FPROFrame_GetVideoFrameUnpacked()
/// API to get unpacked image data, some cameras support image correction through the Merge
/// Reference Frame API's (See #FPRO_REFFRAMES and #FPROAlgo_SetHardwareMergeReferenceFiles).  Use this
/// capability setting to see if your camera will support the image correction.
#ifdef __cplusplus
enum class FPROCAPS : unsigned int
#else
typedef enum
#endif
{
	FPROCAP_DEVICE_TYPE= 0,                      ///< General device type- see documentation
	FPROCAP_META_DATA_SIZE,                      ///< Number of bytes used for the pre-frame image meta data
	FPROCAP_MAX_PIXEL_WIDTH,                     ///< Max allowed image width in pixels
	FPROCAP_MAX_PIXEL_HEIGHT,                    ///< Max allowed image height in pixels
	FPROCAP_PIXEL_BIT_DEPTHS,                    ///< Bit 'b' is set if pixel b+1 depth allowed (bit 0 (lsb)= pixel depth 1)
	FPROCAP_BINNING_TABLE_SIZE,                  ///< 0= 1:1 binning only
	FPROCAP_BLACK_LEVEL_MAX,                     ///< Max Value Allowed (see #FPROSensor_SetBlackLevelAdjust())
	FPROCAP_BLACK_SUN_MAX,                       ///< Max Value Allowed (see #FPROSensor_SetBlackSunAdjust())
	FPROCAP_LOW_GAIN_TABLE_SIZE,                 ///< Number of Gain Values (Low Gain channel for low gain frame in HDR Modes)
	FPROCAP_HIGH_GAIN_TABLE_SIZE,                ///< Number Of Gain Values (High Gain channel for LDR and HDR Modes)
	FPROCAP_ROW_SCAN_TIME,                       ///< Row Scan Time in nano secs (LDR)
	FPROCAP_ROW_REFERENCE_PIXELS,                ///< Number of Pre and Post Row Dummy Pixels when enabled. 
	FPROCAP_FRAME_REFERENCE_ROWS,                ///< Number of Pre and Post Frame Reference rows available
	FPROCAP_IMAGE_INVERTABLE,                    ///< False= Normal scan direction only, True= Inverse Scan Available
	FPROCAP_NV_STORAGE_AVAILABLE,                ///< Number of bytes used for the pre-frame image meta data
	FPROCAP_MERGE_REFERENCE_FRAMES_SUPPORTED,    ///< Whether merge reference frames are supported: 0= not supported, otherwise supported
	FPROCAP_ROI_SUPPORT,                         ///< Region of Interest Support: Row support is in upper 16 bits, column support in lower 16 bits.  
	                                             ///< See the #FPROCAPROI enumeration for details. Use the FPROCAP_ROI_xxx macros to inspect.

	FPROCAP_NUM                              ///< Number of supported capabilities

#ifdef __cplusplus
};
#else
} FPROCAPS;
#endif


/// @typedef FPROCAPROI
/// @brief Region of Interest Support
///
/// Different camera models offer different levels of support for specifying a Region of
/// Interest (ROI) for the image frame.  When supported by the camera device, this can be used to specify
/// an image region smaller than the full frame to effectively increase the the frame rate of for those images.
/// This enumeration an  the FPROCAP_ROI_xxx macros are intended to operate on the #FPROCAP_ROI_SUPPORT capability 
/// of the #FPROCAPS array. See the macros #FPROCAP_ROI_BYCOL_ISCAM_SUPPORTED, #FPROCAP_ROI_BYCOL_ISAPI_SUPPORTED, 
/// #FPROCAP_ROI_BYROW_ISCAM_SUPPORTED, and #FPROCAP_ROI_BYROW_ISAPI_SUPPORTED for additional information.
/// Also see the #FPROSensor_GetCapabilityList() for additional information.
/// <br>
/// <br>
/// The FPROCAPROI enumeration defines the specific support flags for ROI.  Support can be by row, by column, or both.
/// By Row support means that you can specify an arbitrary hight and row offset in the #FPROFrame_SetImageArea() API.
/// By Column support means that you can specify an arbitrary width and column offset in the #FPROFrame_SetImageArea() API.
/// <br>
/// <br>
/// If the API_ONLY flag is set for the given dimension, the support if only available through software processing in the API.
/// While this can simplify your application, the performance of this feature is based on the performance of your computer platform.
/// In the case of API only support, the API will request the smallest frame it can get from the camera (e.g. always full rows if 'by column'
/// is not supported) and extract the region of interest for the caller.  This involves parsing the raw image data and memory copying 
/// to the users buffer.
///  
#ifdef __cplusplus
enum class FPROCAPROI : unsigned int
#else
typedef enum
#endif
{
	FPROCAP_ROI_NONE =             0,                  ///< No ROI supported
	FPROCAP_ROI_BYROW =            (0x00010000),       ///< By Row ROI is supported by the camera
	FPROCAP_ROI_BYROW_API_ONLY =   (0x00020000),       ///< By Row ROI is supported only by the API
	FPROCAP_ROI_BYCOL  =           (0x00000001),       ///< By Column ROI is supported by the camera.
	FPROCAP_ROI_BYCOL_API_ONLY =   (0x00000002)        ///< By Column ROI is supported only by the API.
#ifdef __cplusplus
};


/// @brief Macro to determine if ROI By Column is supported natively by the camera.  See #FPROCAPROI.
#define FPROCAP_ROI_BYCOL_ISCAM_SUPPORTED(__cap)   ((__cap & 0xFFFF) == (uint32_t)FPROCAPROI::FPROCAP_ROI_BYCOL)
/// @brief Macro to determine if ROI By Column is supported only by the API.  See #FPROCAPROI.
#define FPROCAP_ROI_BYCOL_ISAPI_SUPPORTED(__cap)   ((__cap & 0xFFFF) == (uint32_t)FPROCAPROI::FPROCAP_ROI_BYCOL_API_ONLY)
/// @brief Macro to determine if ROI By Row is supported natively by the camera.  See #FPROCAPROI.
#define FPROCAP_ROI_BYROW_ISCAM_SUPPORTED(__cap)   ((__cap & 0xFFFF0000) == (uint32_t)FPROCAPROI::FPROCAP_ROI_BYROW)
/// @brief Macro to determine if ROI By Row is supported only by the API.  See #FPROCAPROI.
#define FPROCAP_ROI_BYROW_ISAPI_SUPPORTED(__cap)   ((__cap &  0xFFFF0000) == (uint32_t)FPROCAPROI::FPROCAP_ROI_BYROW_API_ONLY)
#else
} FPROCAPROI;
/// @brief Macro to determine if ROI By Column is supported natively by the camera.  See #FPROCAPROI.
#define FPROCAP_ROI_BYCOL_ISCAM_SUPPORTED(__cap)   ((__cap & 0xFFFF) == (uint32_t)FPROCAP_ROI_BYCOL)
/// @brief Macro to determine if ROI By Column is supported only by the API.  See #FPROCAPROI.
#define FPROCAP_ROI_BYCOL_ISAPI_SUPPORTED(__cap)   ((__cap & 0xFFFF) == (uint32_t)FPROCAP_ROI_BYCOL_API_ONLY)
/// @brief Macro to determine if ROI By Row is supported natively by the camera.  See #FPROCAPROI.
#define FPROCAP_ROI_BYROW_ISCAM_SUPPORTED(__cap)   ((__cap & 0xFFFF0000) == (uint32_t)FPROCAP_ROI_BYROW)
/// @brief Macro to determine if ROI By Row is supported only by the API.  See #FPROCAPROI.
#define FPROCAP_ROI_BYROW_ISAPI_SUPPORTED(__cap)   ((__cap &  0xFFFF0000) == (uint32_t)FPROCAP_ROI_BYROW_API_ONLY)
#endif




/// @enum  FPROHDR
/// @brief HDR Mode setting
///
/// When enabled, puts the camera in a High Dynamic Range (HDR) mode of operation, whereby images 
/// can be produced that have enhanced detail discernment in dark areas of the image, without 
/// compromising brighter areas of the image. As this enumeration describesm there are two separate 
/// settings for an HDR mode:
///     1.) FPRO_HDR_CAMERA: Using internal algorithm, the camera returns a 'combined' resultant
///         image to the application. 
///     2.) FPRO_HDR_INTERLEAVED: The camera returns both the �dark� and �bright� planes of 
///         the same image to the application. It is up to the application to use its own HDR 
///         algorithms to combine the interleaved images to a final resultant image.
///<br>
///<br>
/// Not all camera models support the FPRO_HDR_CAMERA setting.  Use the #FPROSensor_GetHDREnable() API
/// to retrieve the actual value.  For Kepler Cameras, the HDR setting is typically 
/// enabled/disabled by setting the appropriate mode using the #FPROSensor_SetMode() API.  
///
/// @var FPROHDR::FPRO_HDR_DISABLED 
/// @brief HDR mode is disabled
/// @var FPROHDR::FPRO_HDR_CAMERA 
/// @brief HDR Mode is enabled and the camera will combine the image planes.
/// @var FPROHDR::FPRO_HDR_INTERLEAVED 
/// @brief HDR Mode is enabled and the application must combine the image planes.
///
#ifdef __cplusplus
enum class FPROHDR
#else
typedef enum
#endif
{
	FPRO_HDR_DISABLED = 0,
	FPRO_HDR_CAMERA,
	FPRO_HDR_INTERLEAVED,
#ifdef __cplusplus
};
#else
} FPROHDR;
#endif



// Auxiliary I/O
/// @enum  FPROAUXIO
/// @brief Auxiliary I/O Pins
///
/// The camera makes available auxiliary I/O pins for customer defined use.  This enum simply 
/// assigns a name for each of the pins to be used in the FPROAuxIO_xxx() set of API calls.
/// <br><br> Note that different camera models can support different Aux I/O pins.  Consult your 
/// specific camera documentation for supported pins and physical pin outs.
///
/// @var FPROAUXIO::FPRO_AUXIO_1 
/// @brief Name for AUX I/O Pin 1
/// @var FPROAUXIO::FPRO_AUXIO_2 
/// @brief Name for AUX I/O Pin 2
/// @var FPROAUXIO::FPRO_AUXIO_3 
/// @brief Name for AUX I/O Pin 3
/// @var FPROAUXIO::FPRO_AUXIO_4 
/// @brief Name for AUX I/O Pin 4
///
#ifdef __cplusplus
enum class FPROAUXIO
#else
typedef enum
#endif
{
	FPRO_AUXIO_1= 0x01,
	FPRO_AUXIO_2= 0x02,
	FPRO_AUXIO_3= 0x04,
	FPRO_AUXIO_4= 0x08,
#ifdef __cplusplus
};
#else
} FPROAUXIO;
#endif

/// @enum  FPROAUXIO_DIR
/// @brief Auxiliary I/O Pin Direction
///
/// The camera makes available auxiliary I/O pins for customer defined use.  The pins can be defined
/// as inputs or outputs.  This enum is to be used with the FPROAuxIO_xxx() set of API calls to set the
/// direction of a given AUX I/O pin.  See #FPROAUXIO for more information.
/// <br><br> Note that different camera models can support different Aux I/O pins.  Consult your 
/// specific camera documentation for supported pins and physical pin outs.
///
/// @var FPROAUXIO_DIR::FPRO_AUXIO_DIR_IN 
/// @brief Set AUX I/O pin as an input with respect to the camera.
/// @var FPROAUXIO_DIR::FPRO_AUXIO_DIR_OUT 
/// @brief Set AUX I/O pin as an output with respect to the camera.
#ifdef __cplusplus
enum class FPROAUXIO_DIR
#else
typedef enum
#endif
{
	FPRO_AUXIO_DIR_IN= 0,
	FPRO_AUXIO_DIR_OUT,
#ifdef __cplusplus
};
#else
} FPROAUXIO_DIR;
#endif

/// @enum  FPROAUXIO_STATE
/// @brief Auxiliary Output State 
///
/// The camera makes available auxiliary I/O pins for customer defined use.  The pins can be defined
/// as inputs or outputs.  For pins defined as outputs, this enum is to be used with the FPROAuxIO_xxx()
/// set of API calls to set the state of that pin.  See #FPROAUXIO for more information.
/// <br><br> Note that different camera models can support different Aux I/O pins.  Consult your 
/// specific camera documentation for supported pins and physical pin outs.
///
/// @var FPROAUXIO_STATE::FPRO_AUXIO_STATE_LOW 
/// @brief Pin is in the low state.
/// @var FPROAUXIO_STATE::FPRO_AUXIO_STATE_HIGH 
/// @brief Pin is in the high state.
#ifdef __cplusplus
enum class FPROAUXIO_STATE
#else
typedef enum
#endif
{
	FPRO_AUXIO_STATE_LOW,
	FPRO_AUXIO_STATE_HIGH,
#ifdef __cplusplus
};
#else
} FPROAUXIO_STATE;
#endif


/// @enum  FPROAUXIO_EXPACTIVETYPE
/// @brief Exposure Active Auxiliary Output
///
/// The camera makes available an auxiliary output pin that signals when an exposure is active.  This
/// enum defines the the set of signal types that may be configured for the output.  Consult your
/// specific camera documentation for the timing details of each of these signal types.
/// <br><br> Note that different camera models can support different Aux I/O pins.  Consult your 
/// specific camera documentation for supported pins and physical pin outs.
///
/// @var FPROAUXIO_EXPACTIVETYPE::FPRO_AUXIO_EXPTYPE_EXPOSURE_ACTIVE 
/// @brief Exposure Active- If supported, consult your camera documentation for timing details.
/// @var FPROAUXIO_EXPACTIVETYPE::FPRO_AUXIO_EXPTYPE_GLOBAL_EXPOSURE_ACTIVE 
/// @brief Global Exposure Active- If supported, consult your camera documentation for timing details.
/// @var FPROAUXIO_EXPACTIVETYPE::FPRO_AUXIO_EXPTYPE_FIRST_ROW_SYNC 
/// @brief First Row Sync- If supported, consult your camera documentation for timing details.
#ifdef __cplusplus
enum class FPROAUXIO_EXPACTIVETYPE
#else
typedef enum
#endif
{
	FPRO_AUXIO_EXPTYPE_EXPOSURE_ACTIVE= 0,
	FPRO_AUXIO_EXPTYPE_GLOBAL_EXPOSURE_ACTIVE,
	FPRO_AUXIO_EXPTYPE_FIRST_ROW_SYNC,
	FPRO_AUXIO_EXPTYPE_RESERVED,

	FPRO_AUXIO_EXPTYPE_NOTSET
#ifdef __cplusplus
};
#else
} FPROAUXIO_EXPACTIVETYPE;
#endif


/// @typedef FPROSTREAMERSTATUS
/// @brief Streamer Status
///
/// The FLI Camera devices support the ability to stream images to disk.
/// The FPROFrame_Streamxxx() API's are used to enable, start, and stop the streaming process.
/// In addition, #FPROFrame_StreamGetStatistics() is provided to retrieve the current stream
/// statistics.  The status is part of the #FPROSTREAMSTATS statistics returned by the
/// #FPROFrame_StreamGetStatistics() API.  Note that this status is with respect to  images
/// arriving from the camera.  Multiple frames can be received and queued to be written to disk.
/// As such, in order to correctly determine when all images have been received and written
/// to the disk, you need to check the #FPROSTREAMSTATS uiDiskFramesWritten field and make sure it
/// matches the number of images you requested.  If you stop the stream before all frames are
/// written to the disk, any frames not fully written will be lost.
///
/// @var FPROSTREAMERSTATUS::FPRO_STREAMER_STOPPED 
/// @brief Streaming Stopped.  This is the default state.  It also enters this state when the
/// requested number of images have been streamed or #FPROFrame_StreamStop() is called.
/// @var FPROSTREAMERSTATUS::FPRO_STREAMER_STREAMING 
/// @brief Streaming is running.  This state is entered when streaming is started via the FPROFrame_StreamStart() API.
/// It remains in this state until #FPROFrame_StreamStop() is called, the requested number of images have been streamed, or
/// an error has caused streaming to stop.
/// @var FPROSTREAMERSTATUS::FPRO_STREAMER_STOPPED_ERROR 
/// @brief If streaming has stopped due to an error, the status will be less than 0.  Consult the log file for error messages.
#ifdef __cplusplus
enum class FPROSTREAMERSTATUS
#else
typedef enum
#endif
{
	FPRO_STREAMER_STOPPED_ERROR = -1,
	FPRO_STREAMER_STOPPED= 0,
	FPRO_STREAMER_STREAMING,

#ifdef __cplusplus
};
#else
} FPROSTREAMERSTATUS;
#endif


///
/// @typedef fpro_stream_stats_t FPROSTREAMSTATS
/// @brief Streamer Statistics
///
/// The FLI Camera devices support the ability to stream images to disk.
/// The FPROFrame_Streamxxx() API's are used to enable, start, and stop the streaming process.
/// In addition, #FPROFrame_StreamGetStatistics() is provided to retrieve the current stream
/// statistics in this structure.  The statistics are reset each time #FPROFrame_StreamStart() 
/// is called.
///
/// @var FPROSTREAMSTATS::uiNumFramesReceived 
/// @brief The number of frames received from the camera.
/// @var FPROSTREAMSTATS::uiTotalBytesReceived 
/// @brief The total number of bytes received from the camera.
/// @var FPROSTREAMSTATS::uiDiskFramesWritten 
/// @brief The total number of frames written to disk.
/// @var FPROSTREAMSTATS::dblDiskAvgMBPerSec 
/// @brief The average disk write rate in MBytes/sec on a per frame basis
/// @var FPROSTREAMSTATS::dblDiskPeakMBPerSec 
/// @brief The peak write rate in MBytes/sec; the fastest a given frame was written.
/// @var FPROSTREAMSTATS::dblOverallFramesPerSec
/// @brief The overall frames per second received by the streamer.  Note this depends on actual
/// frame rate from the camera (e.g. exposure time, etc.).  It is calculated simply by counting the 
/// number of frames received a dividing by the time delta from when the streaming was started.
/// @var FPROSTREAMSTATS::dblOverallMBPerSec
/// @brief The overall MB per second received by the streamer.  Note this depends on actual
/// frame rate from the camera (e.g. exposure time, etc.). Calculated similarly to dblOverallFramesPerSec.
/// @var FPROSTREAMSTATS::iStatus 
/// @brief The status of the streamer. See #FPROSTREAMERSTATUS.
/// @var FPROSTREAMSTATS::uiReserved 
/// @brief Reserved for internal use.
typedef struct fpro_stream_stats_t
{
	uint32_t            uiNumFramesReceived;
	uint64_t            uiTotalBytesReceived;
	uint64_t            uiDiskFramesWritten;
	double              dblDiskAvgMBPerSec;
	double              dblDiskPeakMBPerSec;
	double              dblOverallFramesPerSec;
	double              dblOverallMBPerSec;
	FPROSTREAMERSTATUS  iStatus;
	uint32_t            uiReserved;
} FPROSTREAMSTATS;

///
/// @typedef fpro_stream_preview_info_t FPROPREVIEW
/// @brief Streamer Statistics For Preview Images
/// When obtaining a preview image while streaming, this structure
/// will contain the stream statistics for the image returned.
/// See #FPROFrame_StreamGetPreviewImageEx().
///
/// @var FPROPREVIEW::uiFrameNumber
/// @brief The frame number returned in the preview.
/// @var FPROPREVIEW::streamStats
/// @brief The stream statistics.  See #FPROSTREAMSTATS.
///
typedef struct fpro_stream_preview_info_t
{
	uint32_t        uiFrameNumber;
	FPROSTREAMSTATS streamStats;
} FPROPREVIEW;

/// @enum  FPRO_FRAME_TYPE
/// @brief Image Frame Type 
///
/// The camera has the ability to produce different frame types.  The default
/// frame type is FPRO_FRAMETYPE_NORMAL.  Consult you camera documentation for
/// the details of each frame type and availability on a given camera model.
/// <br>
/// <br>
/// See #FPROFrame_SetFrameType() and #FPROFrame_GetFrameType().
/// <br>
///
/// @var FPRO_FRAME_TYPE::FPRO_FRAMETYPE_NORMAL 
/// @brief Normal Frame (default).
/// @var FPRO_FRAME_TYPE::FPRO_FRAMETYPE_DARK 
/// @brief Dark Frame.
/// @var FPRO_FRAME_TYPE::FPRO_FRAMETYPE_BIAS 
/// @brief Bias Frame.
/// @var FPRO_FRAME_TYPE::FPRO_FRAMETYPE_LIGHTFLASH 
/// @brief Light Flash Frame.
/// @var FPRO_FRAME_TYPE::FPRO_FRAMETYPE_DARKFLASH 
/// @brief Dark Flash Frame.
#ifdef __cplusplus
enum class FPRO_FRAME_TYPE
#else
typedef enum
#endif
{
	FPRO_FRAMETYPE_NORMAL= 0,
	FPRO_FRAMETYPE_DARK,
	FPRO_FRAMETYPE_BIAS,
	FPRO_FRAMETYPE_LIGHTFLASH,
	FPRO_FRAMETYPE_DARKFLASH,
#ifdef __cplusplus
};
#else
} FPRO_FRAME_TYPE;
#endif

/// @enum  FPROCMS
/// @brief Correlated Multiple Samples (Samples Per Pixel)
///
/// Some camera models are capable of taking multiple sensor samples per pixel.  Based
/// on imaging modes this can effect the amount of image data sent by the camera for
/// a frame of data.  Consult your specific camera documentation for details on how this
/// feature is supported and its effect on image data.  The values are used in the 
/// #FPROSensor_GetSamplesPerPixel() and #FPROSensor_SetSamplesPerPixel() API calls.
///
/// @var FPROCMS::FPROCMS_1 
/// @brief Single Sample Per Pixel.  This is the default for all cameras.
/// @var FPROCMS::FPROCMS_2 
/// @brief Two sensor samples per pixel are read out.
/// @var FPROCMS::FPROCMS_4 
/// @brief Four sensor samples per pixel are read out.
#ifdef __cplusplus
enum class FPROCMS
#else
typedef enum
#endif
{
	FPROCMS_1 = 0,
	FPROCMS_2,
	FPROCMS_4,
#ifdef __cplusplus
};
#else
} FPROCMS;
#endif


///
/// @typedef ref_frames_t FPRO_REFFRAMES
/// @brief Reference Frames for Hardware Image Merging
///
/// Version 2 and later of the PCIE Fibre interface allows for the image
/// merging process to be done in hardware on the host side PCIE Fibre interface card.
/// This structure is used to transfer the reference frames used in the processing.  See 
/// #FPROAlgo_SetHardwareMergeReferenceFrames and #FPROAlgo_SetHardwareMergeReferenceFiles for
/// additional information.
/// <br>
/// <br>
/// The format of the additive frames is a fixed point number with the lower 3 bits
/// being the decimal.  The 16 bit quantity must be in Little Endian byte order.
/// <br>
/// The aditive frames are also referred to as Dark Signal Non-Uniformity (DSNU) frames.
/// <br>
/// <br>
/// The format of the multiply frames is a fixed point number with the lower 10 bits being the
/// decimal.  For example, a vaue of 1.0 = 0x0400.  These values must also be stored in 
/// Little Endian byte order.
/// <br>
/// The multiplicative frames are also referred to as Photo Response Non-Uniformity (PRNU) frames.
/// <br>
/// 
///
/// @var FPRO_REFFRAMES::uiWidth 
/// @brief Width of the frames in pixels.
/// @var FPRO_REFFRAMES::uiHeight 
/// @brief Height of the frames in pixels.
/// @var FPRO_REFFRAMES::pAdditiveLowGain 
/// @brief Low Gain Additive Reference Frame.
/// @var FPRO_REFFRAMES::pAdditiveHighGain 
/// @brief High Gain Additive Reference Frame.
/// @var FPRO_REFFRAMES::pMultiplicativeLowGain 
/// @brief Low Gain Multiply Reference Frame.
/// @var FPRO_REFFRAMES::pMultiplicativeHighGain 
/// @brief High Gain Multiply Reference Frame.
typedef struct ref_frames_t
{
	uint32_t uiWidth;
	uint32_t uiHeight;

	int16_t* pAdditiveLowGain;
	int16_t* pAdditiveHighGain;
	uint16_t* pMultiplicativeLowGain;
	uint16_t* pMultiplicativeHighGain;
} FPRO_REFFRAMES;


///
/// @typedef FPRO_IMAGE_FORMAT
/// @brief Output Frame Formats for image merging and conversion
///
/// This enum is used by the merging algorithms and conversion functions
/// to specify the resultant image format of the operation.
/// @var FPRO_IMAGE_FORMAT::IFORMAT_RCD 
/// @brief FLI native RCD Frame.
/// @var FPRO_IMAGE_FORMAT::IFORMAT_TIFF 
/// @brief TIFF Formatted image.
/// @var FPRO_IMAGE_FORMAT::IFORMAT_FITS 
/// @brief FITS formatted image.
#ifdef __cplusplus
enum class FPRO_IMAGE_FORMAT
#else
typedef enum
#endif
{
	IFORMAT_NONE = 0,
	IFORMAT_RCD = 0,
	IFORMAT_TIFF,
	IFORMAT_FITS
#ifdef __cplusplus
};
#else
} FPRO_IMAGE_FORMAT;
#endif


///
/// @typedef FPRO_PIXEL_FORMAT
/// @brief SUpported Pixel Formats across the various cameras.
///
/// This enum defines the supported Pixel Formats supported by the various
/// FLI cameras.  Note that not all cameras support all the formats. A given
/// camera model will only support a small subset of these formats.  In order
/// to obtain the list of supported Pixel Formats for a specific camera, you 
/// can use the #FPROFrame_GetSupportedPixelFormats() API.  Further, see #FPROFrame_GetPixelFormat()
/// and #FPROFrame_SetPixelFormat() for additional information. 
/// @var FPRO_PIXEL_FORMAT::PFORMAT_MONO8 
/// @brief Gray scale 8 bits per pixel
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER8_GRBG 
/// @brief 8 bits per pixel. Data in Bayer pattern format with first pixel being green (on a red line).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER8_RGGB 
/// @brief 8 bits per pixel. Data in Bayer pattern format with first pixel being red.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER8_GBRG 
/// @brief 8 bits per pixel. Data in Bayer pattern format with first pixel being green (on a blue line).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER8_BGGR 
/// @brief 8 bits per pixel. Data in Bayer pattern format with first pixel being bllue.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_MONO10_PACKED_MSFIRST 
/// @brief Gray scale 10 bits per pixel. Most significant bit is first (beg-endian like).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER10_GRBG_PACKED_MSFIRST 
/// @brief 10 bits per pixel. Data in Bayer pattern format with first pixel being green  (on a red line).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER10_RGGB_PACKED_MSFIRST 
/// @brief 10 bits per pixel. Data in Bayer pattern format with first pixel being red.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER10_GBRG_PACKED_MSFIRST 
/// @brief 10 bits per pixel. Data in Bayer pattern format with first pixel being green  (on a blue line).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER10_BGGR_PACKED_MSFIRST 
/// @brief 10 bits per pixel. Data in Bayer pattern format with first pixel being blue.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_MONO12_PACKED 
/// @brief Gray scale 12 bits per pixel so 2 pixels are spread over 3 bytes.  
/// The first byte will contain the 8 most significant bits of the first pixel and  
/// the 4 least significant bits of the second byte contain the 4 least significant bits of the first pixel.  
/// The most significant 4 bits of the second byte contain the least significant 4 bits of the second pixel, 
/// and the 3rd byte will contain the most significant 8 bits of the second pixel.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER12_GRBG_PACKED 
/// @brief 12 bits per pixel. Data in Bayer pattern format with first pixel being green (on a red line).
///   See PFORMAT_MONO12_PACKED for packing information.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER12_RGGB_PACKED 
/// @brief 12 bits per pixel. Data in Bayer pattern format with first pixel being red.  See PFORMAT_MONO12_PACKED for packing information.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER12_GBRG_PACKED 
/// @brief 12 bits per pixel. Data in Bayer pattern format with first pixel being green (on a blue line).
///   See PFORMAT_MONO12_PACKED for packing information.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER12_BGGR_PACKED 
/// @brief 12 bits per pixel. Data in Bayer pattern format with first pixel being blue.  See PFORMAT_MONO12_PACKED for packing information.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_MONO12_PACKED_MSFIRST 
/// @brief Gray scale 12 bits per pixel so 2 pixels are spread over 3 bytes.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER12_GRBG_PACKED_MSFIRST 
/// @brief 12 bits per pixel. Data in Bayer pattern format with first pixel being green (on a red line).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER12_RGGB_PACKED_MSFIRST 
/// @brief 12 bits per pixel. Data in Bayer pattern format with first pixel being red.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER12_GBRG_PACKED_MSFIRST 
/// @brief 12 bits per pixel. Data in Bayer pattern format with first pixel being green (on a blue line).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER12_BGGR_PACKED_MSFIRST 
/// @brief 12 bits per pixel. Data in Bayer pattern format with first pixel being blue.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_MONO16 
/// @brief Gray scale 16 bits per pixel.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_MONO16_MSFIRST 
/// @brief Gray scale 16 bits per pixel.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_YUV422 
/// @brief Color, 16 bits per pixel, with a coding pattern of U0, Y0, V0, Y1, U2, Y2, V2, Y3.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER16_GRBG 
/// @brief 16 bits per pixel. Data in Bayer pattern format with first pixel being green (on a red line).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER16_RGGB 
/// @brief 16 bits per pixel. Data in Bayer pattern format with first pixel being red.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER16_GBRG 
/// @brief 16 bits per pixel. Data in Bayer pattern format with first pixel being green (on a blue line).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BAYER16_BGGR 
/// @brief 16 bits per pixel. Data in Bayer pattern format with first pixel being blue.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_RGB24 
/// @brief Color, 8 bits per color, 24 bits per pixel (RGB). 
/// @var FPRO_PIXEL_FORMAT::PFORMAT_RGB24_NON_DIB 
/// @brief Color, 24 bits per pixel.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BGR24 
/// @brief Color, 8 bits per color, 24 bits per pixel (BGR).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_RGBA 
/// @brief Color, 32 bits per pixel with alpha channel (RGBA).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_BGRA 
/// @brief Color, 32 bits per pixel with alpha channel (BGRA).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_ARGB 
/// @brief Color, 32 bits per pixel with alpha channel (ARGB).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_ABGR 
/// @brief Color, 32 bits per pixel with alpha channel (ABGR).
/// @var FPRO_PIXEL_FORMAT::PFORMAT_RGB48 
/// @brief Color, 48 bits per pixel
/// @var FPRO_PIXEL_FORMAT::PFORMAT_RGB48_DIB 
/// @brief Color, 48 bits per pixel
/// @var FPRO_PIXEL_FORMAT::PFORMAT_STOKES4_12 
/// @brief 48 bits per pixel, 12 bits for each of 4 Stokes channels.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_POLAR4_12 
/// @brief 48 bits per pixel � made up of a 12 bit weighted polar channel value, repeated 4 times.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_POLAR_RAW4_12 
/// @brief 48 bits per pixel; 4 12-bit values for each of the polar channels � 0, 45, 90, and 135 degrees.
/// @var FPRO_PIXEL_FORMAT::PFORMAT_HSV4_12 
/// @brief 48 bits per pixel; 12 bits for each of degree and angle of polarization, and 12 bits pixel value repeated twice.
/// 
#ifdef __cplusplus
enum class FPRO_PIXEL_FORMAT
#else
typedef enum
#endif
{
	PFORMAT_MONO8,
	PFORMAT_BAYER8_GRBG,
	PFORMAT_BAYER8_RGGB,
	PFORMAT_BAYER8_GBRG,
	PFORMAT_BAYER8_BGGR,

	PFORMAT_MONO10_PACKED_MSFIRST,
	PFORMAT_BAYER10_GRBG_PACKED_MSFIRST,
	PFORMAT_BAYER10_RGGB_PACKED_MSFIRST,
	PFORMAT_BAYER10_GBRG_PACKED_MSFIRST,
	PFORMAT_BAYER10_BGGR_PACKED_MSFIRST,

	PFORMAT_MONO12_KEPLER,
	PFORMAT_MONO12_PACKED,
	PFORMAT_BAYER12_GRBG_PACKED,
	PFORMAT_BAYER12_RGGB_PACKED,
	PFORMAT_BAYER12_GBRG_PACKED,
	PFORMAT_BAYER12_BGGR_PACKED,
	PFORMAT_MONO12_PACKED_MSFIRST,
	PFORMAT_BAYER12_GRBG_PACKED_MSFIRST,
	PFORMAT_BAYER12_RGGB_PACKED_MSFIRST,
	PFORMAT_BAYER12_GBRG_PACKED_MSFIRST,
	PFORMAT_BAYER12_BGGR_PACKED_MSFIRST,

	PFORMAT_MONO16_KEPLER,
	PFORMAT_MONO16,
	PFORMAT_MONO16_MSFIRST,
	PFORMAT_YUV422,
	PFORMAT_BAYER16_GRBG,
	PFORMAT_BAYER16_RGGB,
	PFORMAT_BAYER16_GBRG,
	PFORMAT_BAYER16_BGGR,

	PFORMAT_RGB24,
	PFORMAT_RGB24_NON_DIB,
	PFORMAT_BGR24,

	PFORMAT_RGBA,
	PFORMAT_BGRA,
	PFORMAT_ARGB,
	PFORMAT_ABGR,

	PFORMAT_RGB48,
	PFORMAT_RGB48_DIB,
	PFORMAT_STOKES4_12,
	PFORMAT_POLAR4_12,
	PFORMAT_POLAR_RAW4_12,
	PFORMAT_HSV4_12,

	PFORMAT_UNKNOWN
#ifdef __cplusplus
};
#else
} FPRO_PIXEL_FORMAT;
#endif


///
/// @typedef FPRO_CONV
/// @brief Conversion info structure supplied to conversion functions.
///
/// This enum is used by the conversion functions
/// to specify the resultant image format of the operation.  It also supplies 
/// merge reference frames to use in the case a image merge needs to occur.  If these
/// are not supplied, identity frames are used based on the gain settings in the meta
/// data of the given rcd file to convert.   See #FPROAlgo_SetHardwareMergeReferenceFiles and
/// #FPROFrame_ConvertFile for more information.
/// @var FPRO_CONV::eFormat 
/// @brief File format for the converted frame.
/// @var FPRO_CONV::pDSNUFile 
/// @brief DSNU Reference file (may be NULL).
/// @var FPRO_CONV::pPRNUFile 
/// @brief PRNU Reference file (may be NULL).
typedef struct conv_info_t
{
	FPRO_IMAGE_FORMAT eFormat;
	wchar_t* pDSNUFile;
	wchar_t* pPRNUFile;
} FPRO_CONV;

///
/// @typedef FPRO_HWMERGEFRAMES
/// @brief Enables for Hardware Image Merging
///
/// This enum is used by the hardware merging algorithms and 
/// specifies which image planes from the camera to merge.  See #FPRO_HWMERGEENABLE.
/// Normally you would merge both the low and high gain frames to get the
/// best resultant merged image.  This allows you to obtain either the 
/// low or high gain images as well.
/// @var FPRO_HWMERGEFRAMES::HWMERGE_FRAME_BOTH 
/// @brief Normal merge, both low and high gain planes are corrected and merged.
/// @var FPRO_HWMERGEFRAMES::HWMERGE_FRAME_LOWONLY 
/// @brief Only the corrected low gain pixels will be sent through to the API.  The high gain pixels will be ignored.
/// @var FPRO_HWMERGEENABLE::HWMERGE_FRAME_HIGHONLY 
/// @brief Only the corrected high gain pixels will be sent through to the API.  The low gain pixels will be ignored.
#ifdef __cplusplus
enum class FPRO_HWMERGEFRAMES
#else
typedef enum
#endif
{
	HWMERGE_FRAME_BOTH = 0,
	HWMERGE_FRAME_LOWONLY,
	HWMERGE_FRAME_HIGHONLY,

#ifdef __cplusplus
};
#else
} FPRO_HWMERGEFRAMES;
#endif


///
/// @typedef hw_merge_enables_t FPRO_HWMERGEENABLE
/// @brief Enables for Hardware Image Merging
///
/// Version 2 and later of the PCIE Fibre interface card allows for the image
/// merging process to be done in hardware directly on the card.
/// This structure is used to enable the different merging options.
/// See #FPROAlgo_SetHardwareMergeEnables() and #FPRO_REFFRAMES for more information.
/// <br>
/// <br>
/// In addition, the same merge algorithm used on the PCIE card is available in the API for use
/// on USB connections, Fibre connections with older hardware, and even with version 2 PCIE hardware.
/// For use in the API, all of the same hardware merge API's are used to set up the reference frames,
/// thresholds, and enables (as with this structure).  In the API's emulation, the bMergeEnable and eMergeFrames 
/// fields in this structure are ignored.  You tell the API to merge or return the desired frames through the #FPROUNPACKEDIMAGES 
/// structure using the #FPROFrame_GetVideoFrameUnpacked() call.  See the structure definition #FPROUNPACKEDIMAGES 
/// for more information on unpacking and merging. 
/// <br>
/// <br>
/// If unpacked and unmerged data is desired the #FPROFrame_GetVideoFrame() call is used.
/// <br>
/// 
/// @var FPRO_HWMERGEENABLE::bMergeEnable 
/// @brief True if merging enabled.  This must be true for the other enables to have any effect.  False turns merging off
/// and the unprocessed frame data is passed through to the host directly from the camera.
/// @var FPRO_HWMERGEENABLE::eMergeFormat 
/// @brief The image file format for the merged image.  The Actual PCIE card only supports RCD and TIFF.  When the 
/// API is used for merging through this mechanism, FITS is also supported.
/// @var FPRO_HWMERGEENABLE::eMergeFrames 
/// @brief Specifies the frames to merge.
typedef struct hw_merge_enables_t
{
	bool bMergeEnable;
	FPRO_IMAGE_FORMAT eMergeFormat;
	FPRO_HWMERGEFRAMES eMergeFrames;
} FPRO_HWMERGEENABLE;


///
/// @typedef unpacked_images_t FPROUNPACKEDIMAGES
/// @brief Unpacked Image Buffers
///
/// The raw data returned by the cameras is of varying formats, bit depths, and interleaving based on the 
/// internal sensor used in the camera.  In order to make use of the data for analysis or display, the images
/// must be unpacked to a form more suitable for such purposes.  This structure is used by the API to allow the application to 
/// request the frames to be automatically unpacked.  The specific usage of these pointers is described in the
/// function documentation in which they are used.  For example, see #FPROFrame_GetVideoFrameUnpacked() for a description
/// of how this structure is used for that particular call.
///
/// @var FPROUNPACKEDIMAGES::uiMetaDataSize 
/// @brief The Size of the pMetaData buffer in bytes.
/// @var FPROUNPACKEDIMAGES::bMetaDataRequest 
/// @brief The Meta Data request Flag.  Set to 'true' to unpack meta data.
///
/// @var FPROUNPACKEDIMAGES::pLowMetaData 
/// @brief This meta data reflects the image data in the pLowImage buffer.
/// @var FPROUNPACKEDIMAGES::pLowImage 
/// @brief The Low Image Buffer.
/// @var FPROUNPACKEDIMAGES::uiLowImageSize 
/// @brief The Size of the pLowImage image in pixels.
/// @var FPROUNPACKEDIMAGES::uiLowBufferSize 
/// @brief The Size of the pLowImage buffer in bytes.  This may be different than 
///       uiLowImageSize * sizeof(uint16_t) when a merge format other than RCD is chosen. 
/// @var FPROUNPACKEDIMAGES::bLowImageRequest 
/// @brief The Low Image request Flag.  Set to 'true' to unpack the low gain image plane.
///
/// @var FPROUNPACKEDIMAGES::pHighMetaData 
/// @brief This meta data reflects the image data in the pHighImage buffer.
/// @var FPROUNPACKEDIMAGES::pHighImage 
/// @brief The High Image Buffer.
/// @var FPROUNPACKEDIMAGES::uiHighImageSize 
/// @brief The Size of the pHighImage image in pixels.
/// @var FPROUNPACKEDIMAGES::uiHighBufferSize 
/// @brief The Size of the pHighImage buffer in bytes.  This may be different than 
///       uiHighImageSize * sizeof(uint16_t) when a merge format other than RCD is chosen. 
/// @var FPROUNPACKEDIMAGES::bHighImageRequest 
/// @brief The High Image request Flag.  Set to 'true' to unpack the high gain image plane.
///
/// @var FPROUNPACKEDIMAGES::pMergedMetaData 
/// @brief This meta data reflects the image data in the pMergedImage buffer.
/// @var FPROUNPACKEDIMAGES::pMergedImage 
/// @brief The Merged Image Buffer.
/// @var FPROUNPACKEDIMAGES::uiMergedImageSize 
/// @brief The Size of the pMergedImage image in pixels.
/// @var FPROUNPACKEDIMAGES::uiMergedBufferSize 
/// @brief The Size of the pMergedImage buffer in bytes.  This will be different than 
///       uiMergedImageSize * sizeof(uint16_t) when a merge format other than RCD is chosen. 
/// @var FPROUNPACKEDIMAGES::bMergedImageRequest 
/// @brief The Merged Image request Flag.  Set to 'true' to merge the low and high gain image planes.
/// @var FPROUNPACKEDIMAGES::eMergeFormat 
/// @brief On entry to the #FPROFrame_GetVideoFrameUnpacked() API, it is the requested format for
///        the unpacked/merged image.  since not all outpu formats may be supported for a given source image, the
///        API may change this value to produce a merged/converted format.  In the event the requested format
///        is not supported, the API will produce a TIFF.
typedef struct unpacked_images_t
{
	uint32_t uiMetaDataSize;
	bool     bMetaDataRequest;

	uint8_t  *pLowMetaData;
	uint16_t *pLowImage;
	uint64_t uiLowImageSize;
	uint64_t uiLowBufferSize;
	bool     bLowImageRequest;

	uint8_t  *pHighMetaData;
	uint16_t *pHighImage;
	uint64_t uiHighImageSize;
	uint64_t uiHighBufferSize;
	bool     bHighImageRequest;

	uint8_t  *pMergedMetaData;
	uint16_t *pMergedImage;
	uint64_t uiMergedImageSize;
	uint64_t uiMergedBufferSize;
	bool     bMergedImageRequest;

	FPRO_IMAGE_FORMAT eMergeFormat;


} FPROUNPACKEDIMAGES;

///
/// @typedef int_point_t FPROPOINT
/// @brief Point Coordinates
///
/// A simple structure to define a point in space.  Used by other structures in the API 
/// such as #FPROPLANESTATS to specify the location of the dimmest and brightest
/// pixels in an image plane.
///
/// @var FPROPOINT::X 
/// @brief The x coordinate.
/// @var FPROPOINT::Y 
/// @brief The y coordinate.
typedef struct int_point_t
{
	int32_t X;
	int32_t Y;
} FPROPOINT;

///
/// @typedef pixel_info_t FPROPIXELINFO
/// @brief Defines a location and value of a pixel within an image plane.
///
/// A composite structure to define a location and value of a pixel within an image plane.
/// Used by other structures in the API such as #FPROPLANESTATS to specify the location of the
/// dimmest and brightest pixels in an image plane.
///
/// @var FPROPIXELINFO::ptPosition 
/// @brief The x and y coordinate of the pixel within the plane.
/// @var FPROPIXELINFO::uiValue 
/// @brief The pixel value.
typedef struct pixel_info_t
{
	FPROPOINT ptPosition;
	uint32_t  uiValue;
} FPROPIXELINFO;

/// @typedef image_plane_stats_t FPROPLANESTATS
/// @brief Defines the set of statistics available for unpacked frames.
///
/// This structure provides the given statistics for an image plane when unpacked by
/// the API. See #FPROUNPACKEDSTATS for more information.
///
/// @var FPROPLANESTATS::uiLCutoff 
/// @brief The lower pixel value cutoff.
/// @var FPROPLANESTATS::uiUCutoff 
/// @brief The Upper pixel value cutoff.
/// @var FPROPLANESTATS::uiHistogramSize 
/// @brief The number of elements in the array pointed to by pdblHistogram.  
/// @var FPROPLANESTATS::pdblHistogram 
/// @brief The pixel value histogram. The index is the pixel value, the value at that index is the number of pixels with that pixel value. 
/// @var FPROPLANESTATS::dblMean 
/// @brief The mean of the pixel values in the plane. 
/// @var FPROPLANESTATS::dblMedian 
/// @brief The median of the pixel values in the plane. 
/// @var FPROPLANESTATS::dblMode 
/// @brief The mode of the pixel values in the plane. 
/// @var FPROPLANESTATS::dblStandardDeviation 
/// @brief The standard deviation of the pixels in the plane.
/// @var FPROPLANESTATS::pixBrightest 
/// @brief The location and value of the brightest pixel in the plane.
/// @var FPROPLANESTATS::pixDimmest 
/// @brief The location and value of the dimmest pixel in the plane.
typedef struct image_plane_stats_t
{
	uint32_t uiLCutoff;
	uint32_t uiUCutoff;
	uint32_t uiHistogramSize;
	double   *pdblHistogram;
	double   dblMean;
	double   dblMedian;
	double   dblMode;
	double   dblStandardDeviation;

	FPROPIXELINFO pixBrightest;
	FPROPIXELINFO pixDimmest;

} FPROPLANESTATS;

/// @typedef FPROUNPACKEDSTATS
/// @brief Statistics for unpacked image planes.
///
/// A structure to retrieve the statistics for unpacked frames.  The pointers within the
/// encapsulated structures are allocated and deallocated by the API.
/// See #FPROFrame_GetVideoFrameUnpacked() for a description
/// of how this structure is used for that particular call.
///
/// @var FPROUNPACKEDSTATS::statsLowImage 
/// @brief The statistics for the low image. 
/// @var FPROUNPACKEDSTATS::bLowRequest 
/// @brief Set to true to request the statistics for this image plane when unpacking.
/// @var FPROUNPACKEDSTATS::statsHighImage 
/// @brief The statistics for the high image. 
/// @var FPROUNPACKEDSTATS::bHighRequest 
/// @brief Set to true to request the statistics for this image plane when unpacking.
/// @var FPROUNPACKEDSTATS::statsMergedImage 
/// @brief The statistics for the merged image. 
/// @var FPROUNPACKEDSTATS::bMergedRequest 
/// @brief Set to true to request the statistics for this image plane when unpacking.
typedef struct unpacked_stats_t
{
	FPROPLANESTATS statsLowImage;
	bool     bLowRequest;

	FPROPLANESTATS statsHighImage;
	bool     bHighRequest;

	FPROPLANESTATS statsMergedImage;
	bool     bMergedRequest;

} FPROUNPACKEDSTATS;

///
/// @typedef FPRO_META_KEYS
/// @brief The list of available Meta Data keys.
///
/// The meta data of an image may be parsed and the values for the fields described
/// by this enumeration may be retrieved.  See the #FPROMETAVALUE structure and the 
/// #FPROFrame_MetaValueInit(), #FPROFrame_MetaValueInitBin(), #FPROFrame_MetaValueGet(), and
/// #FPROFrame_MetaValueGetNext() API calls for additional information.
///
#ifdef __cplusplus
enum class FPRO_META_KEYS
#else
typedef enum
#endif
{
	META_KEY_MAGIC,
	META_KEY_META_DATA_LENGTH,
	META_KEY_META_DATA_VERSION,

	META_KEY_BACK_SIDE_ILLUMINATED,
	META_KEY_BASE_TEMPERATURE,
	META_KEY_BINNING_X,
	META_KEY_BINNING_Y,
	META_KEY_BLACK_LEVEL_ADJUST,
	META_KEY_BLACK_SUN_ADJUST,
	META_KEY_BLACK_LEVEL_HIGH_ADJUST,
	META_KEY_BLACK_SUN_HIGH_ADJUST,
	META_KEY_CAMERA_MODEL,
	META_KEY_CAPTURE_DATE,                       ///< (uiYear << 16) | (uiMonth << 8) | (uiDay)
	META_KEY_CAPTURE_TIME_SECS,
	META_KEY_CAPTURE_TIME_NSECS,
	META_KEY_COOLER_TEMPERATURE,
	META_KEY_COOLER_DUTY_CYCLE,
	META_KEY_CONTROL_BLOCK,
	META_KEY_CORRELATED_MULTIPLE_SAMPLE,
	META_KEY_DATA_PIXEL_BIT_DEPTH,
	META_KEY_DATA_ZERO_POINT,
	META_KEY_DEAD_PIXEL_CORRECTION,
	META_KEY_EXPOSURE_TIME,
	META_KEY_FILE_CREATE_TIME,
	META_KEY_FPGA_TEMPERATURE,
	META_KEY_FRAME_NUMBER,
	META_KEY_GAIN_LOW,
	META_KEY_GAIN_GLOBAL,
	META_KEY_GAIN_HIGH,
	META_KEY_GEO_LAT_RAW,
	META_KEY_GEO_LAT_DEGREES,
	META_KEY_GEO_LAT_MINUTES,
	META_KEY_GEO_LAT_NORTH,
	META_KEY_GEO_LONG_RAW,
	META_KEY_GEO_LONG_DEGREES,
	META_KEY_GEO_LONG_MINUTES,
	META_KEY_GEO_LONG_EAST,
	META_KEY_GLOBAL_RESET,
	META_KEY_GPS_ERROR,
	META_KEY_GPS_LOCK,
	META_KEY_HDR_MODE,
	META_KEY_HIGH_ADUE,
	META_KEY_HORIZONTAL_PIXELS,
	META_KEY_HORIZONTAL_PIXEL_SIZE,
	META_KEY_HORIZONTAL_SCAN_DIRECTION_INVERT,
	META_KEY_ILLUMINATION_START_DELAY,
	META_KEY_ILLUMINATION_STOP_DELAY,
	META_KEY_IMAGE_HEIGHT,
	META_KEY_IMAGE_MODE,
	META_KEY_IMAGE_START_COLUMN,
	META_KEY_IMAGE_START_ROW,
	META_KEY_IMAGE_STOP_COLUMN,
	META_KEY_IMAGE_STOP_ROW,
	META_KEY_IMAGE_START_EXPOSURE_ROW,
	META_KEY_IMAGE_TYPE,
	META_KEY_IMAGE_WIDTH,
	META_KEY_IS_HIGH_FRAME,
	META_KEY_IS_HIGH_GAIN_ONLY_FRAME,
	META_KEY_IS_MERGED_FRAME,
	META_KEY_IS_SOFTWARE_BINNING,
	META_KEY_IS_STACKED_FRAME,
	META_KEY_LOW_DARK_CURRENT,
	META_KEY_LOW_NOISE,
	META_KEY_MERGE_GAIN_RATIO,
	META_KEY_MERGE_LINE_OFFSET,
	META_KEY_NON_ROW_ALLIGNED_IMAGE,
	META_KEY_NUM_OF_DATA_CHANNELS,
	META_KEY_PIXEL_ORDERED_IMAGE,
	META_KEY_POST_REFERENCE_ROW,
	META_KEY_PRE_REFERENCE_ROW,
	META_KEY_POST_REFERENCE_PIXELS_PER_ROW,
	META_KEY_PRE_REFERENCE_PIXELS_PER_ROW,
	META_KEY_SENSOR_PIXEL_BIT_DEPTH,
	META_KEY_SENSOR_READ_QUADRANTS,
	META_KEY_SENSOR_CHIP_TEMPERATURE,
	META_KEY_SERIAL_NUMBER,
	META_KEY_SHUTTER_CLOSE_DELAY,
	META_KEY_SHUTTER_OPEN_DELAY,
	META_KEY_TEMPERATURE_SETPOINT,
	META_KEY_TEST_HIGH_GAIN_ABSOLUTE,
	META_KEY_TEST_LOW_GAIN_ABSOLUTE,
	META_KEY_TRACKING_FRAMES_PER_IMAGE,
	META_KEY_TRACKING_START_COLUMN,
	META_KEY_TRACKING_START_ROW,
	META_KEY_TRACKING_STOP_COLUMN,
	META_KEY_TRACKING_STOP_ROW,
	META_KEY_USE_SHIFTED_AVERAGING,
	META_KEY_VERSION_API,
	META_KEY_VERSION_APPLICATION,
	META_KEY_VERSION_FIRMWARE,
	META_KEY_VERTICAL_PIXELS,
	META_KEY_VERTICAL_PIXEL_SIZE,
	META_KEY_VERTICAL_SCAN_DIRECTION_INVERT,

	META_KEY_V4IMAGE_OFFSET,
	META_KEY_V4META_INFO,
	META_KEY_V4PIXEL_FORMAT,
	META_KEY_V4OTHER_TYPE,
	META_KEY_V4OTHER_TYPE_LENGTH,
	META_KEY_V4OTHER_TYPE_OFFSET,
	META_KEY_V4RESERVED,

	META_KEY_NUM

#ifdef __cplusplus
};
#else
} FPRO_META_KEYS;
#endif


/// @brief Maximum length of a Meta Data string value See #FPROMETAVALUE.
#define FPRO_META_VALUE_STRING_LENGTH_MAX (64)

///
/// @typedef meta_data_value_t FPROMETAVALUE
/// @brief Defined a value for Meta Data fields.
///
/// A composite structure to define the value of a Met Data field as defined by the
/// FPRO_META_KEYS enumeration.
/// The value is either a number represented by a double, or a character string.  If the value 
/// is a character string, the iByteLength of the pMetaValue will be gretaer than or equal (>=) to zero.
/// If the value is represented by a number, the iByteLength field will be less than (<) zero.
///
/// @var FPROMETAVALUE::dblValue 
/// @brief The value of a given meta data key if it is a numerical value.
/// @var FPROMETAVALUE::iByteLength 
/// @brief If the value is a string value, this field is the string length.  If set to zero
/// indicates an empty string.  If set to < 0, the meta data value is represented by the number
/// in the dblValue field of this structure.
/// @var FPROMETAVALUE::cStringValue 
/// @brief The character string value is the meta data value is a string.
typedef struct meta_data_value_t
{
	double   dblValue;
	int32_t  iByteLength;
	uint8_t  cStringValue[FPRO_META_VALUE_STRING_LENGTH_MAX];

} FPROMETAVALUE;

//////////////////////////////////////////
// Camera Open, Close
//////////////////////////////////////////
//////////////////////////////////////////
//////////////////////////////////////////
/// 
/// @brief FPROCam_GetCameraList
/// 
/// Returns a list of cameras detected on the host.  Most often it is the
/// first function called in the API in order to provide a list of
/// available devices to the user.  The information provided in the 
/// FPRODEVICEINFO structure allows unique names to be constructed for
/// each camera. A pointer an FPRODEVICEINFO structure corresponding to a 
/// user selected device is passed to a subsequent call to FPROCam_Open() 
/// in order to connect to the camera.
///
///	@param pDeviceInfo - Pointer to user allocated memory to hold the list of devices.
/// @param pNumDevices - On entry, the max number of devices that may be assigned to the list.
///                      Note that the pDeviceInfo pointer must point to enough memory to hold
///                      the given pNumDevices.  On exit, it contains the number of devices
///                      detected and inserted in the list.  This can be less than the requested
///                      number.  If it returns the requested number, there may be additional 
///                      devices connected.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
///
LIBFLIPRO_API FPROCam_GetCameraList(FPRODEVICEINFO *pDeviceInfo, uint32_t *pNumDevices);

//////////////////////////////////////////
/// 
/// @brief FPROCam_GetDeviceInfo
/// 
/// Returns the device information structure FPRODEVICEINFO for the
/// connected camera.
///
/// @param iHandle - The handle to an open camera device returned from FPROCam_Open().
///	@param pDeviceInfo - Pointer to user allocated memory to hold the device connection information.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCam_GetDeviceInfo(int32_t iHandle, FPRODEVICEINFO* pDeviceInfo);

//////////////////////////////////////////
///
/// @brief Connects to the camera specified by the pDevInfo parameter.  
///
/// This call must complete successfully in order to use any calls in the API
/// that communicate with the camera.
/// The returned handle is passed to all such subsequent API calls.
///
///	@param pDevInfo - Pointer to device description as returned by FPROCam_GetCameraList().
/// @param pHandle - On successful completion, it contains the device handle to use in 
///                  subsequent API calls.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCam_Open(FPRODEVICEINFO *pDevInfo, int32_t *pHandle);


//////////////////////////////////////////
///
/// @brief Disconnects from the camera an releases the handle.
///
/// @param iHandle - Connected camera handle to close.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCam_Close(int32_t iHandle);

//////////////////////////////////////////
///
/// @brief Returns the version of this API Library.  
///
/// This function may be called
/// at any time, it does not need a device handle to report the API version.
///
///
/// @param pVersion - Buffer for returned NULL terminated version string.
/// @param uiLength - Length of supplied buffer.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCam_GetAPIVersion(wchar_t *pVersion, uint32_t uiLength);

//////////////////////////////////////////
///
/// @brief Returns the version information from the connected device.
///
/// @param iHandle - The handle to an open camera device returned from FPROCam_Open().
/// @param pVersion - Structure buffer to return version information.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCam_GetDeviceVersion(int32_t iHandle, FPRODEVICEVERS *pVersion);

//////////////////////////////////////////
///
/// @brief Returns information pertaining to the installed host Fibre/PCIE cards.
///
/// @param pHostInfo - Pointer to structure for return information.
/// @param pNum - On entry, the max number of entries to return.  On return, the actual number
///               of entries included.  Note, currently only 1 entry is supported.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCam_GetHostInterfaceInfo(FPROHOSTINFO *pHostInfo, uint32_t *pNum);


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Frame Data Functions
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////
///
/// @brief Aborts the active image capture.
///
/// The abort function is meant to be called to abort the current image capture.
/// It can be called from a different thread that is performing the image capture
/// as long as the recommended calling pattern for image capture is followed.
/// See FPROFrame_CaptureStart() for a description of
/// the recommended image capture calling pattern.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_CaptureAbort(int32_t iHandle);

/// @cond DO_NOT_DOCUMENT
//////////////////////////////////////////
///
/// @brief Ends the active image capture.
///
/// The end function is meant to be called to be called to end the current image capture
/// and allow the capturing thread to retrieve the image data.  It is intended to be
/// be used to end a long exposure prior to the full exposure completing.  Given that,
/// it will normally be called from a different thread that is performing the image capture
/// as long as the recommended calling pattern for image capture is followed.
/// See FPROFrame_CaptureStart(),  for a description of
/// the recommended image capture calling pattern.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
/// DEPRECATED
//LIBFLIPRO_API FPROFrame_CaptureEnd(int32_t iHandle);
/// @endcond

//////////////////////////////////////////
///
/// @brief Initiates the capture of the configured image.  
///
/// The image is retrieved using the FPROFrame_GetVideoFrame() API.
///
/// NOTE: In order to ensure data pipe integrity, FPROFrame_CaptureStart(), 
///       FPROFrame_GetVideoFrame(), and FPROFrame_CaptureStop() must be called
///	      from the same thread in a pattern similar to below:
///
/// @code
///
///     FPROFrame_CaptureStart();
///     while (frames_to_get)
///         FPROFrame_GetVideoFrame();
///	    FPROFrame_CaptureStop();
///
/// @endcode
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiFrameCount - Number of frames to capture. 0 == infinite stream.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_CaptureStart(int32_t iHandle, uint32_t uiFrameCount);

//////////////////////////////////////////
///
/// @brief Stops the active image capture.
///
/// NOTE: In order to ensure data pipe integrity, FPROFrame_CaptureStart(), 
///       FPROFrame_GetVideoFrame(), and FPROFrame_CaptureStop() must be called
///	      from the same thread in a pattern similar to below:
///
/// @code
///
///     FPROFrame_CaptureStart();
///     while (frames_to_get)
///         FPROFrame_GetVideoFrame();
///	    FPROFrame_CaptureStop();
///
/// @endcode
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_CaptureStop(int32_t iHandle);


//////////////////////////////////////////
///
/// @brief Initiates the capture of a thumbnail image.  
///
/// The image is transferred over the
/// image endpoint and is retrieved using the FPROFrame_GetThumbnail() API.
/// Thumbnail images are 512x512 pixels.  No Metadata or dummy pixels are included
/// in the image.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_CaptureThumbnail(int32_t iHandle);

//////////////////////////////////////////
///
/// @brief Computes the size in bytes of the image frame.  
///
/// This function uses the actual camera settings to determine the size of the image data that
/// will be received in bytes.  As such, all camera settings must be set up for your image prior to calling
/// this function.  Since communications with the camera is required to complete this function
/// successfully, it can fail on the communication.  In addition, this function can take tens of
/// milli-seconds to complete because of this communication so it should not be called in time 
/// critical situations.
/// <br>
/// <br>

/// IMPORTANT: On Fibre connections, when using the Hardware Merging capabilities of the PCIE card, this
/// function MUST be called after you have set up the camera and enabled the Hardware Merging function.
/// See #FPROAlgo_SetHardwareMergeEnables().  If you do not call this function, the hardware merging
/// function may be incorrectly initialized for the current setup and image corruption may result.
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///
/// @return The size of the expected image frame in bytes on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_ComputeFrameSize(int32_t iHandle);


//////////////////////////////////////////
///
/// @brief Computes the size in pixels of the image frame.  
///
/// This function uses the actual camera settings to determine the size of the image data that
/// will be received in pixels.  As such, all camera settings must be set up for your image prior to calling
/// this function.  Since communications with the camera is required to complete this function
/// successfully, it can fail on the communication.  In addition, this function can take tens of
/// milli-seconds to complete because of this communication so it should not be called in time 
/// critical situations.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pTotalWidth - If provided, the total width of the image (including reference pixels) is returned.
/// @param pTotalHeight - If provided, the total height of the image (including reference rows) is returned.
///
/// @return The size of the expected image frame in bytes on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_ComputeFrameSizePixels(int32_t iHandle, uint32_t *pTotalWidth, uint32_t *pTotalHeight);

//////////////////////////////////////////
///
/// @brief Frees the Unpacked Buffers within the given structure.  
///
/// See #FPROFrame_GetVideoFrameUnpacked() for additional information.
///
///	@param pUPBuffers - The buffers to free.
///
/// @return None.
LIBFLIPRO_VOID FPROFrame_FreeUnpackedBuffers(FPROUNPACKEDIMAGES *pUPBuffers);

//////////////////////////////////////////
///
/// @brief Frees the Unpacked Statistics Buffers within the given structure.  
///
/// See #FPROFrame_GetVideoFrameUnpacked() for additional information.
///
///	@param pStats - The statistics buffers to free.
///
/// @return None.
LIBFLIPRO_VOID FPROFrame_FreeUnpackedStatistics(FPROUNPACKEDSTATS *pStats);

//////////////////////////////////////////
///
/// @brief Retrieves the dummy pixel configuration to be appended row data.  
///
/// If enabled, dummy pixels are appended to every other row of image data 
/// starting with the second row of data.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pEnable - true: Dummy pixels will be appended to every other image row
///                  false: Dummy pixels will NOT be appended to every other image row
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_GetDummyPixelEnable(int32_t iHandle, bool *pEnable);

//////////////////////////////////////////
///
/// @brief Retrieves the reference row count to be appended to frame data.  
///
/// If the count is > 0, this number of reference rows are appended to the
/// frame data.  See #FPROCAPS capabilities structure for details about reference rows.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pPreRows - Pointer to the number of pre-frame reference rows appended to 
///                   the image frame data.
/// @param pPostRows - Pointer to the number of post-frame reference rows appended to 
///                   the image frame data.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_GetFrameReferenceRows(int32_t iHandle, uint32_t* pPreRows, uint32_t* pPostRows);

//////////////////////////////////////////
///
/// @brief Retrieves the Frame Type setting.  
///
/// Returns the frame type set by the #FPROFrame_SetFrameType() API.
/// The default frame type is FPRO_FRAMETYPE_NORMAL.  Typically this is only used for testing
/// purposes.  Consult your documentation for a description and availability of the different
/// frame types found on a given camera model.  See also #FPRO_FRAME_TYPE.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pType - The current frame type setting.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_GetFrameType(int32_t iHandle, FPRO_FRAME_TYPE *pType);


//////////////////////////////////////////
///
/// @brief Enables image data imaging.
///
/// Image data may be disabled allowing only reference rows to be
/// produced for image frames.  Reference rows are configured with the 
/// #FPROFrame_SetFrameReferenceRows() API call.
///
/// <br>
/// NOTE: Not all camera models support this feature.  Consult your camera documentation.
/// <br>
/// 
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pEnable - true: Image data pixels enabled, false: Image data pixels disabled.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_GetImageDataEnable(int32_t iHandle, bool *pEnable);

//////////////////////////////////////////
///
/// @brief Retrieves the test image data settings.
///
/// When enabled, the camera generates a test pattern rather than capturing image data from the
/// image sensor.  See #FPROFrame_SetTestImageEnable().
/// <br>
/// NOTE: Not all camera models support a test image.  Consult your camera documentation.
/// <br>
/// 
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pEnable - true: Enable test image data
///                  false: Disables test image data- normal image data produced
/// @param pFormat - Format of the test image data to produce.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_GetTestImageEnable(int32_t iHandle, bool *pEnable, FPROTESTIMAGETYPE *pFormat);

//////////////////////////////////////////
///
/// @brief Gets the area of the image sensor being used to produce image frame data.
///
/// Image frames are retrieved using the FPROFrame_GetVideoFrame() API.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pColOffset - Start column (pixel) of the image frame
/// @param pRowOffset - Start row (pixel) of the image frame
/// @param pWidth - Width of the image frame in pixels.
/// @param pHeight - Height of the image frame in pixels.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_GetImageArea(int32_t iHandle, uint32_t *pColOffset, uint32_t *pRowOffset, uint32_t *pWidth, uint32_t *pHeight);

//////////////////////////////////////////
///
/// @brief Retrieves the current pixel format configuration.
///
/// See #FPROFrame_GetSupportedPixelFormats() for additional information.
/// 
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pFormat - The current pixel format.
/// @param pPixelLSB - The current Pixel LSB offset.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_GetPixelFormat(int32_t iHandle, FPRO_PIXEL_FORMAT* pFormat, uint32_t* pPixelLSB);

//////////////////////////////////////////
///
/// @brief Retrieves the supported pixel formats.
///
/// Ths function returns the list of supported pixel formats.  If NULL
/// is passed for the pFormats parameter and non NULL for the pNumFormats
/// parameter, the pNumFormats parameter is updated with the number of formats
/// that would be returned allowing the application to allocate a properly sized
/// buffer and call this function again to obtain the actual list. The items in the
/// list may then be used as a paramter to #FPROFrame_SetPixelFormat().  Also see the
/// #FPROFrame_GetPixelFormat() API call to retrieve the current pixel format.
/// 
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pFormats - The returned list of supported formats.
/// @param pNumFormats - On entry, the size of the pFormats list.  On return, the actual
///                      number of entries returned.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_GetSupportedPixelFormats(int32_t iHandle, FPRO_PIXEL_FORMAT* pFormats, uint32_t* pNumFormats);

//////////////////////////////////////////
///
/// @brief Retrieves the thumbnail image from the camera.
///
/// The image is transferred over the
/// image endpoint and is retrieved using the FPROFrame_GetThumbnail() API.
/// Thumbnail images are 512x512 12 bit pixels in size.  That is, no Metadata,
/// reference rows, or dummy pixels are included in the image.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pFrameData - The buffer to store the frame data
/// @param pSize - Size the of the pFrameData buffer.
///                On return, the number of bytes actually received.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_GetThumbnailFrame(int32_t iHandle, uint8_t *pFrameData, uint32_t *pSize);

//////////////////////////////////////////
///
/// @brief Retrieves an image frame from the camera.
///
/// It is important to call this function with the
/// appropriate size buffer for the frame.  That is, the buffer size should be match the
/// expected frame size.  If it is too large, the function will try and read the given size and 
/// may stall the USB connection if no more frame data is available.
///
/// NOTE: In order to ensure data pipe integrity, FPROFrame_CaptureStart(), 
///       FPROFrame_GetVideoFrame(), and FPROFrame_CaptureStop() must be called
///	      from the same thread in a pattern similar to below:
///
/// @code
///
///     FPROFrame_CaptureStart();
///     while (frames_to_get)
///         FPROFrame_GetVideoFrame();
///	    FPROFrame_CaptureStop();
///
/// @endcode
///
/// <br>
/// <br>
/// NOTE: This function is only for use when triggering image capture using FPROFrame_CaptureStart() as described
/// above.  If you are using external triggers, use the the #FPROFrame_GetVideoFrameExt() or #FPROFrame_GetVideoFrameUnpackedExt()
/// API calls.
/// 
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pFrameData - The buffer to store the frame data.
/// @param pSize - Size the of the pFrameData buffer.
///                On return, the number of bytes actually received.
/// @param uiTimeoutMS - How long to wait for a frame to be available.
///                      Assuming you make this call very soon after the FPROFrame_CaptureStart()
///                      call, you should set this to the exposure time.  Internally, the API
///                      blocks (i.e. no communication with the camera) for some time less than 
///                      uiTimeoutMS and then attempts to retrieve the frame.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_GetVideoFrame(int32_t iHandle, uint8_t *pFrameData, uint32_t *pSize, uint32_t uiTimeoutMS);


//////////////////////////////////////////
///
/// @brief Retrieves an image frame from the camera and optionally unpacks and merges the image planes.
///
/// This function behaves identically as the #FPROFrame_GetVideoFrame() call with respect to starting, stopping,
/// and timeouts.  See #FPROFrame_GetVideoFrame() for details.
/// <br>
/// In addition, if you specify unpacking buffers, the function will also unpack the raw image data received
/// and return the planes requested (Low Gain, High Gain, Merged).  The planes to unpack are specified in the
/// pUPBuffers parameter.  If this parameter is NULL, the function behaves identically to that of #FPROFrame_GetVideoFrame()
/// call.  
/// <br>
/// <br>
/// To allocate the buffers, the first time you call this function for a given frame setup, you must set the buffer pointers
/// within the structure to NULL and set the corresponding 'request flag' in the structure to 'true'.  For example, to receive a merged
/// frame, set pUPBUffers->pMergedImage to NULL and set pUPBuffers->bMergedImageRequest to 'true'.  The API will allocate the 
/// requested buffers and return the requested planes.  If your frame setup does not change, you may reuse the buffers that have
/// been allocated for subsequent exposures.  If the buffers provided are of incorrect size, the API does it's best to re-allocate them.
/// If it can not, the function will return an error.  In this case the raw frame may still have been received correctly.  Check the
/// *pSize value to obtain the number of bytes in the raw frame that were successfully received. When you are done with the buffers, you 
/// must call #FPROFrame_FreeUnpackedBuffers() to free the memory.
/// <br>
/// <br>
/// Similarly, when you call this function the first time and request statistics for the frames, the API allocates the
/// needed memory within the statistics structure.  Make sure you initialize the structure with 0's in order for this to
/// work properly!  The structure can then be reused in subsequent calls.
/// When you are done with the statistics, you must call #FPROFrame_FreeUnpackedStatistics() to free the memory.
/// <br>
/// <br>
/// The meta data returned in the pUPBuffers structure is not the raw meta data received from the camera.  It has been modified to 
/// reflect the processing of the raw frame.  The raw meta data for the image is at the start of the pFrameData buffer just as it is
/// with #FPROFrame_GetVideoFrame().  If you want to process the raw frame, or determine the original image characteristics, you must
/// use the meta data in the pFrameData buffer.
/// <br>
/// <br>
/// Note that  the eMergeFormat member of the #FPROUNPACKEDIMAGES structure is the requested output format.  If this
/// format can not be produced, a TIFF format is produced and this field is modified to indicate that output/merged
/// image is in TIFF format.
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pFrameData - The buffer to store the frame data.
/// @param pSize - Size the of the pFrameData buffer.
///                On return, the number of bytes actually received.
/// @param uiTimeoutMS - How long to wait for a frame to be available.
///                      Assuming you make this call very soon after the FPROFrame_CaptureStart()
///                      call, you should set this to the exposure time.  Internally, the API
///                      blocks (i.e. no communication with the camera) for some time less than 
///                      uiTimeoutMS and then attempts to retrieve the frame.
/// @param pUPBuffers - If specified, the call will return the unpacked buffers requested.
/// @param pStats - If specified, the call will return the statistics requested.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_GetVideoFrameUnpacked(int32_t iHandle, uint8_t *pFrameData, uint32_t *pSize, uint32_t uiTimeoutMS, FPROUNPACKEDIMAGES *pUPBuffers, FPROUNPACKEDSTATS *pStats);

//////////////////////////////////////////
///
/// @brief Retrieves an externally triggered image frame from the camera.
///
/// This function is intended for use when using external trigger sources for image capture.
/// Unlike FPROFrame_GetVideoFrame(), no timeout is specified.  It waits forever until notification
/// of image frame data availability from the camera.  FPROFrame_CaptureAbort()
/// can be used to cancel the exposure as described in those API calls. 
/// <br>
/// <br>
/// FPROFrame_CaptureStart() is not expected to be called prior to this API because the External Trigger
/// will be supplying the trigger source.  However, if this call is awaiting image data, another thread may
/// call #FPROFrame_CaptureStart() to force a trigger.  Once exposed, this function will return the data as
/// if an external trigger occurred. If you do call FPROFrame_CaptureStart() to force a trigger, it is important
/// to call #FPROFrame_CaptureAbort() after the image is retrieved.
/// <br>
/// <br>
/// It is important to call this function with the
/// appropriate size buffer for the frame.  That is, the buffer size should be match the
/// expected frame size.  If it is too large, the function will try and read the given size and 
/// may stall the USB connection if no more frame data is available.
///
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pFrameData - The buffer to store the frame data.
/// @param pSize - Size the of the pFrameData buffer.
///                On return, the number of bytes actually received.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_GetVideoFrameExt(int32_t iHandle, uint8_t *pFrameData, uint32_t *pSize);


//////////////////////////////////////////
///
/// @brief Retrieves an externally triggered image frame from the camera and unpacks the image.
///
/// This function is intended for use when using external trigger sources for image capture.
/// See #FPROFrame_GetVideoFrameExt() for a complete description of the use of this API. 
/// See #FPROFrame_GetVideoFrameUnpacked() for a complete description of the use of unpacking
/// buffers for this API. 
/// <br>
/// <br>
/// 
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pFrameData - The buffer to store the frame data.
/// @param pSize - Size the of the pFrameData buffer.
///                On return, the number of bytes actually received.
/// @param pUPBuffers - If specified, the call will return the unpacked buffers requested.
/// @param pStats - If specified, the call will return the statistics requested.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_GetVideoFrameUnpackedExt(int32_t iHandle, uint8_t* pFrameData, uint32_t* pSize, FPROUNPACKEDIMAGES* pUPBuffers, FPROUNPACKEDSTATS* pStats);

//////////////////////////////////////////
///
/// @brief Unpack and merge the given file.
///
/// This function performs similarly as the #FPROFrame_GetVideoFrameUnpacked() API
/// except that it operates on the given file as opposed to the image data directly from the
/// camera.  As such, no connection to the camera is required for this API.
/// If you have retrieved a frame with #FPROFrame_GetVideoFrame() or #FPROFrame_GetVideoFrameExt(),
/// and saved it to a file, you may pass that file to this function to unpack and merge the image
/// planes.  For a description of the pUPBUffers and pStats parameters, see the 
/// #FPROFrame_GetVideoFrameUnpacked() API function. 
/// <br>
/// <br>
/// The  FPROFrame_UnpackFileEx() API was introduced to accomodate the Reference Frame merging algorithm.
/// The FPROFrame_UnpackFile() API simply calls FPROFrame_UnpackFileEx with NULL pointers for the reference
/// file names.  This causes Identity Frames to be used for the merge.  For a description of the 
/// pDSNUFile and pPRNUFile parameters, see #FPROAlgo_SetHardwareMergeReferenceFiles.
/// <br>
/// 
///	@param pFileName - The file to unpack and merge.
/// @param pUPBuffers - The unpacked buffers.
/// @param pStats - The unpacked statistics.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_UnpackFile(wchar_t* pFileName, FPROUNPACKEDIMAGES* pUPBuffers, FPROUNPACKEDSTATS* pStats);

//////////////////////////////////////////
///
/// @brief Unpack and merge the given file.
///
/// See #FPROFrame_UnpackFile for a complete description.
/// <br>
/// One or both of the  pDSNUFile and pPRNUFile parameters may be NULL.  In that case, an identity reference frame
/// will be used for the missing (NULL) frame.  For a description of the pDSNUFile and pPRNUFile
/// parameters, see #FPROAlgo_SetHardwareMergeReferenceFiles.
/// <br>
/// 
///	@param pFileName - The file to unpack and merge.
/// @param pUPBuffers - The unpacked buffers.
/// @param pStats - The unpacked statistics.
/// @param pDSNUFile - DSNU Reference file to use (may be NULL)
/// @param pPRNUFile - PRNU Reference File to use (may be NULL)
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_UnpackFileEx(wchar_t* pFileName, FPROUNPACKEDIMAGES* pUPBuffers, FPROUNPACKEDSTATS* pStats, const wchar_t* pDSNUFile, const wchar_t* pPRNUFile);


//////////////////////////////////////////
///
/// @brief Convert (and possibly) merge the given RCD file.
///
/// Convert the given RCD file to the file type specified.  
/// See #FPROAlgo_SetHardwareMergeReferenceFiles and #FPRO_CONV for
/// more information.
/// <br>
/// <br>
/// Only RCD files may be supplied as the input file to convert.  If a single plane
/// file is provided, that plane is unpacked and converted to the output format.  If a 
/// two plane RCD is provided (both Low Gain and High Gain frames), the frames are
/// unpacked, merged, and all 3 frames are converted to the specified output format.
/// 
///	@param pInRcdFile - The file to convert.
/// @param pConvInfo - Conversion specifications.
/// @param pOutFile - Ouput (converted) file name.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_ConvertFile(wchar_t* pInRcdFile, FPRO_CONV* pConvInfo, wchar_t *pOutFile);
/// @cond DO_NOT_DOCUMENT
/// Not Implemented yet
///LIBFLIPRO_API FPROFrame_ConvertFileEx(wchar_t* pInRcdFile, FPRO_CONV* pConvInfo, FPROUNPACKEDIMAGES* pUPBuffers, FPROUNPACKEDSTATS* pStats);
///@endcond

//////////////////////////////////////////
///
/// @brief Convert the meta data in the given file to string.
///
/// This function simply parses the meta data in the given file and returns a NULL terminated string
/// representation.  Like #FPROFrame_UnpackFile, this function does not require an active camera connection
/// to succeed.  This function is intended primarily for trouble shooting purposes.  Sufficient
/// space must be provided in the pMetaString buffer to contain all of the data.  4K bytes is typically 
/// enough.  Only RCD files may be supplied as the input file to parse.
///
///	@param pFileName - The file to parse.
/// @param pMetaString - The string buffer.
/// @param uiMaxChars - The size of the string buffer pMetaString in characters.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_MetaDataToString(wchar_t* pFileName, wchar_t *pMetaString, uint32_t uiMaxChars);

//////////////////////////////////////////
///
/// @brief Convert the meta data in the given file to string.
///
/// Similar to #FPROFrame_MetaDataToString() except this function operates on the binary image data provided.
/// This function simply parses the meta data in the given binary image data and returns a NULL terminated string
/// representation. Sufficient space must be provided in the pMetaString buffer to contain all of the data.  
/// 4K bytes is typically enough.  Only RCD files may be supplied as the input file to parse.
///
///	@param pImageData - The image data to parse.
/// @param uiImageSizeBytes - The size of the image data in bytes.
/// @param pMetaString - The string buffer.
/// @param uiMaxChars - The size of the string buffer pMetaString in characters.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_MetaDataToStringBin(uint8_t *pImageData, uint32_t uiImageSizeBytes, wchar_t* pMetaString, uint32_t uiMaxChars);

//////////////////////////////////////////
///
/// @brief Parses the meta data from the given file for access by the 
/// #FPROFrame_MetaValueGet() and #FPROFrame_MetaValueGetNext() API calls.
///
/// This function along with #FPROFrame_MetaValueGet() and #FPROFrame_MetaValueGetNext() provide
/// a key-value access method for the meta data fields defined by the #FPRO_META_KEYS enumeration.
/// This function, or its companion #FPROFrame_MetaValueInitBin() must be called prior to calling
/// one of the Get functions.
///
///	@param pFileName - The image file to parse.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_MetaValueInit(wchar_t* pFileName);

//////////////////////////////////////////
///
/// @brief Parses the meta data from the given image data for access by the 
/// #FPROFrame_MetaValueGet() and #FPROFrame_MetaValueGetNext() API calls.
///
/// This function along with #FPROFrame_MetaValueGet() and #FPROFrame_MetaValueGetNext() provide
/// a key-value access method for the meta data fields defined by the #FPRO_META_KEYS enumeration.
/// This function, or its companion #FPROFrame_MetaValueInit() must be called prior to calling
/// one of the Get functions.
///
///	@param pMetaData - The image data to parse.
/// @param uiLength - Length of the data buffer in bytes.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_MetaValueInitBin(uint8_t* pMetaData, uint32_t uiLength);

//////////////////////////////////////////
///
/// @brief Retrieve the value for the given meta key.
///
/// This function returns the value of the given meta data key.  The value is either
/// a number represented by a double, or a character string.  If the value is a character
/// string, the iByteLength of the pMetaValue will be gretaer than or equal (>=) to zero.
/// If the value is represented by a number, the iByteLength field will be less than (<) zero.
///
///	@param eMetaKey - The meta data key to retrieve.
/// @param pMetaValue - Structure pointer for the value.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_MetaValueGet(FPRO_META_KEYS eMetaKey, FPROMETAVALUE* pMetaValue);

//////////////////////////////////////////
///
/// @brief Retrieve the next meta key value.
///
/// This function returns the value of the next meta key defined by the FPRO_META_KEYS
/// enumeration.  The next key is set to the start of the enumeration after one of the 
/// init functions #FPROFrame_MetaValueInit() or FPROFrame_MetaValueInitBin() are called.
/// If #FPROFrame_MetaValueGet() is called, then FPROFrame_MetaValueGetNext() will return the
/// value for he key following the one passed to FPROFrame_MetaValueGet().
/// The value is either a number represented by a double, or a character string.  If the value 
/// is a character string, the iByteLength of the pMetaValue will be gretaer than or equal (>=) to zero.
/// If the value is represented by a number, the iByteLength field will be less than (<) zero.
///
/// @param pMetaValue - Structure pointer for the value.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_MetaValueGetNext(FPROMETAVALUE* pMetaValue);

//////////////////////////////////////////
///
/// @brief Returns whether or not Image Frame data is ready for retrieval.  
///
/// This API is primarily used in conjunction with an external trigger setup.
/// Since the external trigger is not natively synchronized with the software 
/// in any way, a method to determine when the image data is
/// available for retrieval is required.  Users can use this routine to query
/// the camera and wait for image data to be available.  When the function
/// succeeds and *pAvailable is 'true', the user can call the normal FPROFrame_GetVideoFrame()
/// API to retrieve the data.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pAvailable - Pointer for returned query. 
///                     true: Image Frame data is available for retrieval
///                     false: Image Frame Data is not available.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
//DEPRECATED - REMOVED
//LIBFLIPRO_API FPROFrame_IsAvailable(int32_t iHandle, bool *pAvailable);

//////////////////////////////////////////
///
/// @brief Sets the dummy pixel configuration to be appended row data.  
///
/// For the FPRO_CAM_DEVICE_TYPE_GSENSE400 and FPRO_CAM_DEVICE_TYPE_GSENSE4040 model
/// cameras, if enabled, dummy pixels are appended to every other row of image data
/// starting with the second row of data.  Consult your Camera Model user documentation 
/// as not all models support dummy pixels in the same way.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param bEnable - true: Dummy pixels will be appended to every other image row
///                  false: Dummy pixels will NOT be appended to every other image row
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_SetDummyPixelEnable(int32_t iHandle, bool bEnable);

//////////////////////////////////////////
///
/// @brief Sets the reference row count to be added to frame data.  
///
/// If the count is > 0, this number of reference rows are added to the
/// frame data.  See #FPROCAPS capabilities structure for details about reference rows.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiPreRows - The number of pre-frame reference rows added to 
///                   the image frame data.
/// @param uiPostRows - The number of post-frame reference rows added to 
///                   the image frame data.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_SetFrameReferenceRows(int32_t iHandle, uint32_t uiPreRows, uint32_t uiPostRows);

//////////////////////////////////////////
///
/// @brief Sets Frame Type produced by the camera.  
///
/// Sets Frame Type produced by the camera.  The frame type can be retrieved using the #FPROFrame_GetFrameType()
/// API. The default frame type is FPRO_FRAMETYPE_NORMAL.  Typically this is only used for testing
/// purposes.  Consult your documentation for a description and availability of the different
/// frame types found on a given camera model.  See also #FPRO_FRAME_TYPE.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param eType - The frame type to set.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_SetFrameType(int32_t iHandle, FPRO_FRAME_TYPE eType);

//////////////////////////////////////////
///
/// @brief Enables image data imaging.
///
/// Image data may be disabled allowing only reference rows to be
/// produced for image frames.  Reference rows are configured with the 
/// #FPROFrame_SetFrameReferenceRows() API call.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param bEnable - true: Enables image data pixels, false: disables image data pixels.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_SetImageDataEnable(int32_t iHandle, bool bEnable);

//////////////////////////////////////////
///
/// @brief Enables test image data to be generated rather than normal image data.
///
/// Use this to generate a test pattern rather than capturing image data from the
/// image sensor.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param bEnable - true: Enable test image data
///                  false: Disables test image data- normal image data produced
/// @param eFormat - Format of the test image data to produce. This parameter is
///                  ignored if bEnable == false.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_SetTestImageEnable(int32_t iHandle, bool bEnable, FPROTESTIMAGETYPE eFormat);

//////////////////////////////////////////
///
/// @brief Sets the area of the image sensor to be used for Tracking Frames during image capture.
///
/// The tracking frames are retrieved as normal image frames using the 
/// FPROFrame_GetVideoFrame() API.  The image frame follow the tracking 
/// frames in the USB stream.
/// The exposure time setting set with FPROCtrl_SetExposure() applies to the
/// tracking frames. As such, the total exposure time for your image frame will
/// be exposure_time * uiNumTrackingFrames.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param uiStartRow - Start row of the tracking frame (inclusive).
/// @param uiEndRow - End row of the tracking frame (inclusive).
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_SetTrackingArea(int32_t iHandle, uint32_t uiStartRow, uint32_t uiEndRow);

//////////////////////////////////////////
///
/// @brief Enables the production of Tracking Frames by the camera.
///
/// There will be uiNumTrackingFrames produced for every image frame
/// produced.  The image frame follow the tracking frames in the USB stream.
/// The exposure time setting set with FPROCtrl_SetExposure() applies to the
/// tracking frames. As such, the total exposure time for your image frame will
/// be exposure_time * uiNumTrackingFrames.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param uiNumTrackingFrames - Number of Tracking frames to produce.  0 disables Tracking Frames.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_SetTrackingAreaEnable (int32_t iHandle, uint32_t uiNumTrackingFrames);

//////////////////////////////////////////
///
/// @brief Sets the current pixel configuration to the specified values.
///
/// See #FPROFrame_GetPixelFormat() anc #FPROFrame_GetSupportedPixelFormats()
/// for additional information.
/// 
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pfPixelFormat - The pixel format to set.
/// @param uiPixelLSB - The Pixel LSB offset.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_SetPixelFormat(int32_t iHandle, FPRO_PIXEL_FORMAT pfPixelFormat, uint32_t uiPixelLSB);

//////////////////////////////////////////
///
/// @brief Sets the area of the image sensor to be used to produce image frame data.
///
/// Image frames are retrieved using the FPROFrame_GetVideoFrame() API.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param uiColOffset - Start column (pixel) of the image frame
/// @param uiRowOffset - Start row (pixel) of the image frame
/// @param uiWidth - Width of the image frame in pixels.
/// @param uiHeight - Height of the image frame in pixels.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_SetImageArea(int32_t iHandle, uint32_t uiColOffset, uint32_t uiRowOffset, uint32_t uiWidth, uint32_t uiHeight);

//////////////////////////////////////////
///
/// @brief Initializes the Streamer interfaces.
///
/// The Streamer interfaces enable an efficient stream-to-functionality.  Frames
/// are streamed directly from the camera to disk.  This function initializes the 
/// various sub modules, allocates resources, and enables the streaming capability.
///  This function must be called prior to #FPROFrame_StreamStart(). 
///
/// The streaming operation streams frames of the same size.  In order to change the frame size,
/// the streaming must be stopped and deinitialized using the #FPROFrame_StreamStop() and 
/// FPROFrame_StreamDeinitialize() API's respectively.  Then a new stream can be initialized with
/// a new frame size.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param uiFrameSizeBytes - Size of the frames that will be streamed.
/// @param pRootPath - Name of the root path to store the files on disk.
/// @param pFilePrefix - A file name prefix to be applied to each file being saved..
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_StreamInitialize(int32_t iHandle, uint32_t uiFrameSizeBytes, wchar_t *pRootPath, wchar_t *pFilePrefix);

//////////////////////////////////////////
///
/// @brief Deinitializes the Streamer interfaces.
///
/// This function deinitializes the streamer interfaces.  All streaming operations are stopped
/// and streaming resources are returned to the system.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_StreamDeinitialize(int32_t iHandle);

//////////////////////////////////////////
///
/// @brief Start the streaming operation.
///
/// This function starts the actual streaming operations.  The streaming interfaces must
/// have been previously initialized by a successful call to #FPROFrame_StreamInitialize().
/// <br>
/// This function returns immediately after the stream is started.  You should call 
/// #FPROFrame_StreamGetStatistics() to retrieve the current statistics of the stream and
/// check the iStatus member of the FPROSTREAMERSTATUS structure for FPRO_STREAMER_STOPPED 
/// or an error.  Note that FPRO_STREAMER_STOPPED indicates that streaming has stopped from
/// from the camera (i.e. the number of expected images have been received).  However, there still
/// may be images queued internally waiting to be written to disk.  If you need an explicit number
/// of images to be captured and written to disk, also check the uiDiskFramesWritten of the 
/// FPROSTREAMERSTATUS structure to make sure all the images have indeed been written to disk.
///
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param uiFrameCount - Number of frames to stream.  Zero (0) == infinite (until the disk fills
///                       or #FPROFrame_StreamStop is called.
///	@param uiFrameIntervalMS - The frame interval in milliseconds() (exposure time + delay).
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_StreamStart(int32_t iHandle,uint32_t uiFrameCount,uint64_t uiFrameIntervalMS);

//////////////////////////////////////////
///
/// @brief Stop the streaming operation.
///
/// This function stops the actual streaming operations.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_StreamStop(int32_t iHandle);

//////////////////////////////////////////
///
/// @brief Stop the streaming operation.
///
/// This function stops the actual streaming operations.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pStats - The returned statistics. See #FPROSTREAMSTATS.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFrame_StreamGetStatistics(int32_t iHandle, FPROSTREAMSTATS *pStats);

//////////////////////////////////////////
///
/// @brief Retrieve the next image available for preview from the image stream.
///
/// This function is to be used while image streaming is taking place.
/// It will retrieve the available preview from the image stream.  The image returned
/// is also written to disk as normal.  If you call this function faster than the 
/// exposure time, you will receive the same preview image.
/// <br>
/// FPROFrame_StreamGetPreviewImageEx() operates the same as FPROFrame_StreamGetPreviewImage() with the
/// exception of also returning information about the image and the current stream statistics.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pImage - The buffer for the image data.
/// @param pLength - On enter, the size of the buffer pointed to by pImage.  On exit, the actual
///                  number of bytes stored in pImage.
/// @param uiTimeoutMSecs - Timeout to wait for an image in milli-seconds.  Useful when
///                         streaming 1 image.  Once the first image arrives, there will
///                         always be an image available for preview until #FPROFrame_StreamDeinitialize
///                         is called.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.  On timeout, the function
///         returns success (0), and sets the *pLength parameter to 0.
LIBFLIPRO_API FPROFrame_StreamGetPreviewImage(int32_t iHandle, uint8_t* pImage, uint32_t* pLength, uint32_t uiTimeoutMSecs);


//////////////////////////////////////////
///
/// @brief Retrieve the next image available for preview from the image stream.
///
/// See #FPROFrame_StreamGetPreviewImage().
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pImage - The buffer for the image data.
/// @param pLength - On enter, the size of the buffer pointed to by pImage.  On exit, the actual
///                  number of bytes stored in pImage.
/// @param pInfo - Optional. If provided it contains the requisite information about the image and the stream.
/// @param uiTimeoutMSecs - Timeout to wait for an image in milli-seconds.  Useful when
///                         streaming 1 image.  Once the first image arrives, there will
///                         always be an image available for preview until #FPROFrame_StreamDeinitialize
///                         is called.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.  On timeout, the function
///         returns success (0), and sets the *pLength parameter to 0.
LIBFLIPRO_API FPROFrame_StreamGetPreviewImageEx(int32_t iHandle, uint8_t* pImage, uint32_t* pLength, FPROPREVIEW *pInfo, uint32_t uiTimeoutMSecs);


///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
// Control Functions
///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////
///
/// @brief Get Camera Burst Mode enable
///
/// Retrieves the current burst mode setting.  See #FPROCtrl_SetBurstModeEnable() for a description of this setting.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pEnable - True if burst mode is enabled, false otherwise.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetBurstModeEnable(int32_t iHandle, bool *pEnable);

//////////////////////////////////////////
///
/// @brief Reads the current duty cycle of the cooler.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pDutyCycle - Returned Cooler Duty Cycle 0-100%.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetCoolerDutyCycle(int32_t iHandle, uint32_t *pDutyCycle);

//////////////////////////////////////////
///
/// @brief Returns the current Camera Buffer Bypass state of the camera
///
/// Internally, the camera and Host PCIE Fibre boards contain memory used to buffer image data prior to sending
/// to the host over the connected interface.  This is required to compensate for the 
/// inherent performance differences of the camera and external interfaces to the host (USB or Fibre).
/// For USB connections, it is required to buffer the data internally on the camera or image data
/// may be lost.  For Fibre connections, the physical transfer rate is in theory fast enough to handle the
/// full throughput of the camera. This of course depends on the camera and the actual reliability of
/// your Fibre connection.  In order to achieve absolute maximum throughput over a Fibre connection, the
/// internal memory buffering may be turned off or bypassed using this feature.  Note, enabling the memory bypass on either
/// or both of the camera and host PCIE boards may result in lost data if your camera is set to deliver data at a higher
/// rate that can be processed by your host computer.  This is also highly dependent on the performance capability of your
/// computer.
/// See #FPROCtrl_SetCameraBufferBypass() to set this feature.
/// <br>
/// NOTE: When a USB connection is established to a camera, this feature is turned off automatically.  If you 
/// subsequently connect over Fibre and assuming you want the buffering turned off,  you must set the enable 
/// again if it was set the last time you were connected over Fibre.
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pCameraBypassEnable - Returned Buffering Bypass enable.
/// @param pHostBypassEnable - Returned Buffering Bypass enable.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetCameraBufferBypass(int32_t iHandle, bool* pCameraBypassEnable, bool* pHostBypassEnable);

//////////////////////////////////////////
///
/// @brief Returns the current Electrically Black Pixel Enable state from the camera
///
/// Some camera models support the ability to produce electrically black reference pixels rather
/// than imaging reference pixels. The #FPROCtrl_GetElectricallyBlackPixelEnable() and 
/// #FPROCtrl_SetElectricallyBlackPixelEnable() API functions are used to enable and disable this
/// feature.  Consult your camera documentation for the availability of this feature.
/// <br>
/// This function will return failure if the connected camera does not support this feature.
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pEnable - Returned Enable state.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetElectricallyBlackPixelEnable(int32_t iHandle, bool* pEnable);

//////////////////////////////////////////
///
/// @brief Reads the exposure time of the image sensor.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pExposureTime - Returned exposure time in nanoseconds.
/// @param pFrameDelay - Returned frame delay (end -to-start) time in nanoseconds.
///                      When performing multiple exposures with a single trigger, the frame 
///                      delay is the time from the end of exposure of a frame
///                      to the start of exposure of the next frame.
/// @param pImmediate - This parameter affects the way exposure starts when the 
///                     FPROFrame_CaptureStart() function is called.  The camera
///                     image sensor is continually exposing its pixels on a row
///                     by row basis. When this parameter is set to true, the exposure 
///                     for the frame begins at whatever image sensor row is currently
///                     being exposed.  The raw image data returned by the FPROFrame_GetVideoFrame()
///                     call begins with this row (most likely not row 0).  The row that
///                     starts the raw video frame is recorded in the meta data for the image frame.
///
///                     When this parameter is set to false, the camera waits until row 0 is
///                     being exposed before starting the frame exposure.  In this case, the
///                     image frame data returned by the FPROFrame_GetVideoFrame() call will
///                     start at row 0.
///
/// NOTE: The Exposure Time and Frame Delay values are translated to camera specific units which
///       are in turn dependent on the current imaging mode of the camera (e.g. LDR vs HDR).
///       The API does its best to perform the necessary conversions automatically
///       when the modes are changed. It is recommended, however,  to make sure the 
///       desired exposure times are set after imaging modes have changed using this 
///       and its counterpart API function FPROCtrl_SetExposure().
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetExposure(int32_t iHandle, uint64_t *pExposureTime, uint64_t *pFrameDelay, bool *pImmediate);


//////////////////////////////////////////
///
/// @brief Returns the external trigger settings of the camera.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pTrigInfo - External Trigger setup information.  See #FPROEXTTRIGINFO and #FPROEXTTRIGTYPE.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetExternalTriggerEnable(int32_t iHandle, FPROEXTTRIGINFO* pTrigInfo);


//////////////////////////////////////////
///
/// @brief Returns the current Fan status, on or off.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pOn - true: the fan is on, false: the fan is off
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetFanEnable(int32_t iHandle, bool *pOn);

//////////////////////////////////////////
///
/// @brief Returns the current state of an optionally attached GPS unit.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pState - One of FPROGPSSTATE enumerations. See #FPROGPSSTATE for the definitions
/// of the states.
/// @param pOptions - The current options set up for tracking.  This will be bitwise or'd values
/// from the #FPROGPSOPT enumeration.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetGPSState(int32_t iHandle, FPROGPSSTATE* pState, uint32_t* pOptions);

//////////////////////////////////////////
///
/// @brief Reads the current heater configuration.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pPwrPercentage - The setting of the heater in percent.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetHeaterPower(int32_t iHandle, uint32_t *pPwrPercentage);

//////////////////////////////////////////
///
/// @brief Gets the delay between setting the Illumination on/off via FPROCtrl_SetIlluminationOn()
/// and when the illumination actually activates.  Each increment is TBD msecs.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pOnDelay - Delay for turning the illumination on in TBD msec increments
/// @param pOffDelay - Delay for turning the illumination off in TBD msec increments
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetIlluminationDelay(int32_t iHandle, uint32_t *pOnDelay, uint32_t *pOffDelay);

//////////////////////////////////////////
///
/// @brief Returns the setting of External Illumination- on or off.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pOn - true: illumination on, false: illumination off
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetIlluminationOn(int32_t iHandle, bool *pOn);

//////////////////////////////////////////
///
/// @brief Returns the state of the LED on or off setting.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pOn - true: LED on, false: LED off
/// <br>
/// See #FPROCtrl_SetLED() for a description of the LED functionality.
/// <br>
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetLED(int32_t iHandle, bool *pOn);

//////////////////////////////////////////
///
/// @brief Get the LED Duration setting.
///
///	@param iHandle - The handle to an open camera device returned from #FPROCam_Open()
/// @param *pDurationUsec - Duration in micro seconds.  0xFFFFFFFF = always on.
/// <br>
/// <br>
/// Note that each camera model may have different resolution capability on the duration
/// (i.e. something larger than a micro-second).  The FPROCtrl_SetLEDDuration() converts the
/// micro-second value passed to the proper resolution for the camera.  This call reverses
/// the conversion.  As such, the value returned by this call may not match the value exactly
/// as that passed to FPROCtrl_SetLEDDuration().  On most cameras, the physical resolution for
/// this duration is 10usecs.
/// <br>
/// <br>
/// Also see #FPROCtrl_SetLEDDuration() for a description of the LED functionality.
/// <br>
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetLEDDuration(int32_t iHandle, uint32_t *pDurationUsec);

//////////////////////////////////////////
///
/// @brief Returns the temperatures on the Host PCIE Fibre Interface card
///
/// This function is only applicable to the Host PCIE Fibre Interface card.  If the 
/// iHandle parameter is a valid handle to a connected camera over a Fibre interface,
/// then iHandle is used to obtain the host PCIE card information.  If iHandle is a negative
/// number, then the API looks for an installed card and attempts to get the information from 
/// the first found card.
/// <br>
/// <br>
/// NOTE: This function only works on Host PCIE Fibre Interface cards version 2.0 or later.
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pPcieFpga - The PCIE side FPGA Temperature in degC.
/// @param pFibreFpga - The Fibre side FPGA Temperature in degC.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetPCIETemperatures(int32_t iHandle, double* pPcieFpga, double *pFibreFpga);

//////////////////////////////////////////
///
/// @brief Reads the internal sensor temperature of the camera.  
///
/// Note that if this
/// function is called during an exposure and 'Read During Exposure' is not
/// enabled, the internal sensor temperature is not explicitly read.  The
/// last value successfully read will be returned.  See the
/// FPROCtrl_SetSensorTemperatureReadEnable() API for more information.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pTemp - For the returned temperature, deg C.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetSensorTemperature(int32_t iHandle, int32_t *pTemp);

//////////////////////////////////////////
///
/// @brief Returns the 'read sensor temperature during exposure' enabled flag.
///
/// See FPROCtrl_SetSensorTemperatureReadEnable() for more details.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pEnable - true: 'read sensor temperature during exposure' Enabled
///                  false: 'read sensor temperature during exposure' Disabled
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetSensorTemperatureReadEnable(int32_t iHandle, bool *pEnable);


//////////////////////////////////////////
///
/// @brief Gets the current shutter setting.
///
/// By default it is up to the user to control the shutter during exposures
/// using the #FPROCtrl_SetShutterOpen() API.  Using this API, the user can 
/// retreive the currrent shutter open state when user control is enabled.
/// See #FPROCtrl_SetShutterOpen() and #FPROCtrl_SetShutterOverride for
/// additional information.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pOpen - true: Shutter open, false: Shutter closed
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetShutterOpen(int32_t iHandle, bool *pOpen);

//////////////////////////////////////////
///
/// @brief Gets the current shutter override setting.
///
/// By default it is up to the user to control the shutter during exposures
/// using the #FPROCtrl_SetShutterOpen() API.  Using this API, the user can 
/// retreive the currrent shutter override state (user or camera controlled).
/// See #FPROCtrl_SetShutterOpen() and #FPROCtrl_SetShutterOverride for
/// additional information.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pOverride - true: Allows user control of shutter, false: Camera controls shutter
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetShutterOverride(int32_t iHandle, bool *pOverride);

//////////////////////////////////////////
///
/// @brief Reads the various temperatures sensors of the camera.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pOtherTemp - For the returned device temperature, deg C.
/// @param pBaseTemp - For the returned Base temperature, deg C.
/// @param pCoolerTemp - For the returned Cooler temperature, deg C.
/// <br>
/// <br>
/// Not all cameras support all the temperatures.  Consult you camera documentation
/// for details.  In addition, NULL may be passed for any of the temperature pointers.
/// Tempeeratures are retrieved only for pointers that are not NULL. This can save 
/// a few milliseconds per temperature if timing is critical in your application.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetTemperatures(int32_t iHandle, double *pOtherTemp, double *pBaseTemp, double *pCoolerTemp);

//////////////////////////////////////////
///
/// @brief Returns the Base Temperature Set Point.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pSetPoint - The setpoint value in -75 to 70 degC.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_GetTemperatureSetPoint(int32_t iHandle, double *pSetPoint);

//////////////////////////////////////////
///
/// @brief Set Camera Burst Mode enable
///
/// Internally, the camera contains memory used to buffer image data prior to sending
/// to the host over the connected interface.  Burst Mode allows all of the memory to be used to buffer
/// images providing a burst of images to the host.  Disabling burst mode restricts the camera to buffer only
/// a single image, not reading out another from the sensor until the image has been fully sent to the host.
/// See #FPROCtrl_GetBurstModeEnable() to retrieve the current setting.
/// <br>
/// The API sets the camera to burst mode when a connection is established.  Disabling burst mode is useful for
/// being able to make adjustments between frames such as re-focusing.
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param bEnable - True to enable burst mode, false to disable.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetBurstModeEnable(int32_t iHandle, bool bEnable);

//////////////////////////////////////////
///
/// @brief Set Camera Buffer Bypass state of the camera
///
/// Internally, the camera contains memory used to buffer image data prior to sending
/// to the host over the connected interface.  See #FPROCtrl_GetCameraBufferBypass() for 
/// a complete description of this feature.
/// <br>
/// This feature is only supported on Fibre connections and certain camera models.
/// If you attempt to call this function on an unsupported connection, the call returns
/// a failure.
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param bCameraBypassEnable - Buffering Bypass enable.  Set to true to bypass internal buffering on the camera.
/// @param bHostBypassEnable - Buffering Bypass enable.  Set to true to bypass internal buffering on the host PCIE card.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetCameraBufferBypass(int32_t iHandle, bool bCameraBypassEnable, bool bHostBypassEnable);

//////////////////////////////////////////
///
/// @brief Returns the current Electrically Black Pixel Enable state from the camera
///
/// See #FPROCtrl_GetElectricallyBlackPixelEnable() for a description of this feature.
/// <br>
/// This function will return failure if the connected camera does not support this feature.
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param bEnable -  Enable state to set.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetElectricallyBlackPixelEnable(int32_t iHandle, bool bEnable);

//////////////////////////////////////////
///
/// @brief Sets the exposure time of the image sensor.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiExposureTime - Exposure time in nanosecond increments.
/// @param uiFrameDelay - Frame delay (end - to - start) time in nanoseconds.
///                       When performing multiple exposures with a single trigger, the frame 
///                       delay is the time from the end of exposure of a frame
///                       to the start of exposure of the next frame.
/// @param bImmediate - This parameter affects the way exposure starts when the 
///                     FPROFrame_CaptureStart() function is called.  The camera
///                     image sensor is continually exposing its pixels on a row
///                     by row basis. When this parameter is set to true, the exposure 
///                     for the frame begins at whatever image sensor row is currently
///                     being exposed.  The raw image data returned by the FPROFrame_GetVideoFrame()
///                     call begins with this row (most likely not row 0).  The row that
///                     starts the raw video frame is recorded in the meta data for the image frame.
///
///                     When this parameter is set to false, the camera waits until row 0 is
///                     being exposed before starting the frame exposure.  In this case, the
///                     image frame data returned by the FPROFrame_GetVideoFrame() call will
///                     start at row 0.
///
/// <br>
/// NOTE: The Exposure Time and Frame Delay values are translated to camera specific units which
///       are in turn dependent on the current imaging mode of the camera (e.g. LDR vs HDR),
///       as well as the camera model.
///       The API does its best to perform the necessary conversions automatically
///       when the modes are changed. It is recommended, however,  to make sure the 
///       desired exposure times are set after imaging modes have changed using this 
///       and its counterpart API function #FPROCtrl_GetExposure().  You may also use
///       FPROCtrl_SetExposureEx() to request exposure times and recieve the actuals in a single call.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetExposure(int32_t iHandle, uint64_t uiExposureTime, uint64_t uiFrameDelay, bool bImmediate);

//////////////////////////////////////////
///
/// @brief Sets the exposure time of the image sensor.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiExposureTime - Exposure time in nanosecond increments.
/// @param uiFrameDelay - Frame delay (end - to - start) time in nanoseconds.
///                       When performing multiple exposures with a single trigger, the frame 
///                       delay is the time from the end of exposure of a frame
///                       to the start of exposure of the next frame.
/// @param bImmediate - This parameter affects the way exposure starts when the 
///                     FPROFrame_CaptureStart() function is called.  The camera
///                     image sensor is continually exposing its pixels on a row
///                     by row basis. When this parameter is set to true, the exposure 
///                     for the frame begins at whatever image sensor row is currently
///                     being exposed.  The raw image data returned by the FPROFrame_GetVideoFrame()
///                     call begins with this row (most likely not row 0).  The row that
///                     starts the raw video frame is recorded in the meta data for the image frame.
///                     When this parameter is set to false, the camera waits until row 0 is
///                     being exposed before starting the frame exposure.  In this case, the
///                     image frame data returned by the FPROFrame_GetVideoFrame() call will
///                     start at row 0.
/// @param pActualExposureTime - If not NULL, the actual Exposure Time will be returned.  
///                              This is the same value that would be returned by #FPROCtrl_GetExposure().
///                              See NOTE below. 
/// @param pActualFrameDelay - If not NULL, the actual Frame Delay will be returned.
///                            This is the same value that would be returned by #FPROCtrl_GetExposure().
///                            See NOTE below.
///
/// <br>
/// NOTE: The Exposure Time and Frame Delay values are translated to camera specific units which
///       are in turn dependent on the current imaging mode of the camera (e.g. LDR vs HDR),
///       as well as the camera model.
///       The API does its best to perform the necessary conversions automatically
///       when the modes are changed. It is recommended, however,  to make sure the 
///       desired exposure times are set by checking the pActualExposurTime and pActualFrameDelay upon
///       return of this function.  You can also use #FPROCtrl_GetExposure() to get the actual times.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetExposureEx(int32_t iHandle, uint64_t uiExposureTime, uint64_t uiFrameDelay, bool bImmediate, uint64_t* pActualExposureTime, uint64_t* pActualFrameDelay);

//////////////////////////////////////////
///
/// @brief Enables or disables the external trigger of the camera.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiFrameCount - The number of images to get (dependent on pTrigInfo setup).
/// @param pTrigInfo - External Trigger setup information.  See #FPROEXTTRIGINFO and #FPROEXTTRIGTYPE.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetExternalTriggerEnable(int32_t iHandle, uint32_t uiFrameCount, FPROEXTTRIGINFO *pTrigInfo);

//////////////////////////////////////////
///
/// @brief Turns the Fan on or off.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param bOn - true: turns the fan on, false: turns the fan off
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetFanEnable(int32_t iHandle, bool bOn);

//////////////////////////////////////////
///
/// @brief Set the tracking options of an optionally attached GPS unit.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiOptions - The options to set up for tracking.  This is bitwise or'd values
/// from the #FPROGPSOPT enumeration.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetGPSOptions(int32_t iHandle, uint32_t uiOptions);

//////////////////////////////////////////
///
/// @brief Turns the Heater on or off at the specified power level.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiPwrPercentage -Specifies the power level as a percentage (0-100)
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetHeaterPower(int32_t iHandle,uint32_t uiPwrPercentage);

//////////////////////////////////////////
///
/// @brief Sets the illumination delay.
///
/// The illumination delay is the time between setting the Illumination on/off via 
/// #FPROCtrl_SetIlluminationOn and when the illumination actually activates.  
/// Each increment is TBD msecs.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiOnDelay - Delay for turning the illumination on in TBD msec increments
/// @param uiOffDelay - Delay for turning the illumination off in TBD msec increments
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetIlluminationDelay(int32_t iHandle, uint16_t uiOnDelay, uint16_t uiOffDelay);

//////////////////////////////////////////
///
/// @brief Turns External Illumination on or off.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param bOn - true: turns illumination on, false: turns illumination off
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetIlluminationOn(int32_t iHandle, bool bOn);

//////////////////////////////////////////
///
/// @brief Turn the LED on or off.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param bOn - true: Turns the LED on, false: Turns the LED off
/// <br>
/// <br>
/// Based on the camera model, this function may work in conjunction with the #FPROCtrl_SetLEDDuration()
/// API.  In those cases, this call must be made with a value of TRUE in order for the 
/// #FPROCtrl_SetLEDDuration() to toggle the LED.
/// <br>
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetLED(int32_t iHandle, bool bOn);

//////////////////////////////////////////
///
/// @brief Set LED Duration during exposure.
///
///	@param iHandle - The handle to an open camera device returned from #FPROCam_Open()
/// @param uiDurationUSec - Duration in micro seconds.  0xFFFFFFFF = always on.
/// <br>
/// <br>
/// This call sets the duration of the LED flash during exposure. This call is not available on all 
/// camera models. It was introduced on the 4040 models.  This call works in conjunction with the #FPROCtrl_SetLED()
/// API.  In order for the camera to turn the LED on for the given duration, #FPROCtrl_SetLED() must have been called
/// with a value of TRUE.  
/// <br>
/// <br>
/// To simply turn the LED on and leave it on, pass a value of 0xFFFFFFFF in the uiDurationUSec parameter.  As with other
/// duration values, #FPROCtrl_SetLED() must be called with a value of TRUE.
/// <br>
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetLEDDuration(int32_t iHandle, uint32_t uiDurationUSec);

//////////////////////////////////////////
///
/// @brief Enables/disables physical reading of the image sensor temperature during exposures.  
///
/// The sensor temperature is read using the #FPROCtrl_GetSensorTemperature
/// API call.  If that call is made during an exposure, it will physically read
/// the sensor temperature only if this call was made prior to enable the reading.
/// If this 'read sensor temperature during exposure' is not enabled, then the
/// #FPROCtrl_GetSensorTemperature call will return the previous successful temperature
/// read from the imaging sensor.
/// <br>NOTE: This enable only pertains to requests during an exposure. If temperature 
/// readings are requested outside of an exposure boundary, this enable has no effect.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param bEnable - true: Enable 'read sensor temperature during exposure'.
///                  false: Disable 'read sensor temperature during exposure'.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetSensorTemperatureReadEnable(int32_t iHandle, bool bEnable);


//////////////////////////////////////////
///
/// @brief Opens/Close the shutter.
///
/// By default it is up to the user to control the shutter during exposures
/// using this #FPROCtrl_SetShutterOpen() API.  This call works in conjunction
/// with the shutter override setting.  In order for this call to successfully 
/// control the shutter, the override setting must
/// be set for user control (true).  See #FPROCtrl_SetShutterOverride.  
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param bOpen - true: opens the shutter, false: closes the shutter
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetShutterOpen(int32_t iHandle, bool bOpen);

//////////////////////////////////////////
///
/// @brief Sets the shutter control override.
///
/// By default it is up to the user to control the shutter during exposures
/// using the #FPROCtrl_SetShutterOpen() API.  Using this API, the user can override
/// that setting and allow the camera to control the shutter for exposures.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param bOverride - true: Allows user control of shutter, false: Camera controls shutter
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetShutterOverride(int32_t iHandle, bool bOverride);

//////////////////////////////////////////
///
/// @brief Sets the Base Temperature Set Point.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param dblSetPoint - The setpoint value to set in -75 to 70 degC.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROCtrl_SetTemperatureSetPoint(int32_t iHandle, double dblSetPoint);

//////////////////////////////////////////
// Sensor Functions
//////////////////////////////////////////
//////////////////////////////////////////
///
/// @brief Retrieves the current pixel bin settings.
///
/// The bin setting for horizontal (x direction) and vertical (Y direction) binning.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pXBin - For returned horizontal bin setting.
/// @param pYBin - For returned vertical bin setting.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_GetBinning(int32_t iHandle, uint32_t *pXBin, uint32_t *pYBin);

//////////////////////////////////////////
///
/// @brief Retrieves the Binning table capability from the camera.
///
/// The available bin settings for horizontal (x direction) and vertical (Y direction) binning.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pBinTable - User allocated buffer for the bin settings.  The size of the table is
///                    reported in the  Capabilities structure returned by #FPROSensor_GetCapabilityList().
/// @param pTableSizeBytes - On entry, the size of the buffer.  On exit, the number of bytes used.
///                          
/// @return Greater than or equal to 0 on success, less than 0 on failure.
///         On success, the returned size may be 0 indicating no binning table exists and 
///         the only available binning is 1:1.
/// <br>
///         Each binning table entry is 32 bits wide.  The horizontal binning value is contained
///         in the upper 16 bits and the vertical binning value is contained in the lower 16 bits.  There
///         is a binning table entry for each of the horizontal and vertical binning combinations available
///         for the camera. One to one (1:1) binning is always allowed.  For example, if 2x2 and 2x4 binning 
///         (horizontal x vertical) is supported, the two binning table entries would be in the binning table as follows:
///         0x00020002, 0x00020004.
/// <br>
///         If the high bit is set for a given binning, then all binnings up to and including the value with the
///         high bit masked are valid.  For example, a binning table entry of 0x88008800 would allow all combinations
///         of binning values from 1:1 through 2048x2048.  A binning table entry of 0x88000001 indicates that all horizontal
///         binning values from 1 through 2048 are valid with a vertical binning value of 1;
/// <br>
///         If not enough room is given for the buffer, the function fails and the
///         size required is returned in pTableSizeBytes.  If a different failure occurs,
///         the returned size is set to zero (0).
LIBFLIPRO_API FPROSensor_GetBinningTable(int32_t iHandle, uint32_t *pBinTable, uint32_t *pTableSizeBytes);

//////////////////////////////////////////
///
/// @brief Retrieves the current Black Level Adjustment values for the given channel.
///
/// <br>
/// Note that not all cameras support multiple channels for the
/// Black Level Adjustment values.  Consult your users manual for specifics on your device.
/// If you call this function on a camera that does not support multiple channels,
/// the eChan parameter is ignored and the single supported channel is retrieved.
/// <br>
/// <br>
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param eChan -	The adjustment channel to retrieve.
/// @param pAdjustValue - For returned adjustment values.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_GetBlackLevelAdjust(int32_t iHandle, FPROBLACKADJUSTCHAN eChan, uint32_t *pAdjustValue);

//////////////////////////////////////////
///
/// @brief Retrieves the current Black Sun Adjustment values for the given channel.
///
/// <br>
/// Note that not all cameras support multiple channels for the
/// Black Sun Adjustment values.  Consult your users manual for specifics on your device.
/// If you call the #FPROSensor_GetBlackSunAdjust() API for a camera that does support
/// multiple channels, the channel defaults to the first channel on the device.
/// <br>
/// <br>
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param eChan -	The adjustment channel to retrieve.
/// @param pAdjustValue - For returned adjustment values.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_GetBlackSunAdjust(int32_t iHandle, FPROBLACKADJUSTCHAN eChan, uint32_t *pAdjustValue);

//////////////////////////////////////////
///
/// @brief Retrieves the capabilities list for the connected camera.  
/// 
/// The list you pass in is expected to be a list that can be indexed by the 
/// #FPROCAPS enumeration.  It is filled in up to the max number of capabilities
/// specified by the pNumCaps parameter.
/// <br><br>
/// If pNumCaps specifies a number larger than the actual number of capabilities present
/// on the camera, on return it contains the actual number of capabilities obtained.
/// In all cases, the list is indexed by the #FPROCAPS enumeration.
///
///	@param iHandle -  The handle to an open camera device returned from FPROCam_Open()
/// @param pCapList - User allocated buffer to store the capabilities list.
/// @param pNumCaps - On entry the number of capabilities able to be stored in pCapList.  On exit,
///                     the actual number of capabilities returned.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_GetCapabilityList(int32_t iHandle, uint32_t* pCapList, uint32_t* pNumCaps);

//////////////////////////////////////////
///
/// @brief Retrieves the current setting for the Gain for the specified table.
///
/// Note: The index returned is the index into the table as returned by 
///       FPROSensor_GetGainTable().
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param eTable - The table to retrieve.
/// @param pGainIndex - Buffer for the returned index.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_GetGainIndex(int32_t iHandle, FPROGAINTABLE eTable, uint32_t *pGainIndex);

//////////////////////////////////////////
///
/// @brief Retrieves the specified Gain Table. 
///
/// The pNumEntries command should be derived from the
/// uiLDRGain, uiHDRGain, or uiGlobalGain values in the #FPROCAPS capabilities structure.
///
/// Note: Each gain that is returned is scaled to produce an integer number.
///       The scale factor is defined as #FPRO_GAIN_SCALE_FACTOR in the API.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param eTable - The table to retrieve.
/// @param pGainValues - Buffer for the returned values.
/// @param pNumEntries - On entry, the number of locations available in the buffer.
///                      On exit, the actual number of values inserted in the table.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_GetGainTable(int32_t iHandle, FPROGAINTABLE eTable, FPROGAINVALUE *pGainValues, uint32_t *pNumEntries);

//////////////////////////////////////////
///
/// @brief Retrieves the current setting for HDR enable.
///
/// See #FPROHDR enumeration for details.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pHDREnable - Buffer for the returned HDR enable flag.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_GetHDREnable(int32_t iHandle, FPROHDR* pHDREnable);


//////////////////////////////////////////
///
/// @brief Retrieves the High Gain Only Mode.
///
/// Returns the High Gain Only Mode setting.
/// <br>
/// Note: The High Gain Only Mode is not applicable to all camera models.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pHighGainOnly - High Gain only mode flag.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_GetHighGainOnlyEnable(int32_t iHandle, bool *pHighGainOnly);

//////////////////////////////////////////
///
/// @brief Retrieves the current mode name for the specified index.  
///
/// The number of available modes are retrieved using the #FPROSensor_GetModeCount API.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiModeIndex - The index of the mode to retrieve.
/// @param pMode - Buffer for the returned mode information.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_GetMode(int32_t iHandle, uint32_t uiModeIndex, FPROSENSMODE *pMode);

//////////////////////////////////////////
///
/// @brief Retrieves the current mode count and current mode index setting.
///
/// Mode information for a given index is retrieved using the 
/// #FPROSensor_GetMode() API.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pCount - Buffer for the number of available modes.
/// @param pCurrentMode - Buffer for index of the currently assigned mode.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_GetModeCount(int32_t iHandle, uint32_t *pCount,uint32_t *pCurrentMode);

//////////////////////////////////////////
///
/// @brief Retrieves the current sensor read out configuration on supported models.
///
/// For camera models that support sensor channel read out configuration, this API
/// retrieves the current setting.  For models that do not support this feature, this
/// function always returns success with a value of 0 for the configuration.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pReadCfg - Buffer for the configuration.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_GetReadoutConfiguration(int32_t iHandle, FPROSENSREADCFG *pReadCfg);

//////////////////////////////////////////
///
/// @brief Retrieves the Samples Per Pixel settings on the sensor.
/// <br><br>
/// NOTE: This function is not supported on on all camera models.  Consult
/// your documentation for your specific camera.
/// <br>
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pSamplesPerPixel - Returned Samples Per Pixel Setting.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.  On devices that
/// do not support this setting, 0 is always returned with pSamplesPerPixel set to FPROCMS_1.
LIBFLIPRO_API FPROSensor_GetSamplesPerPixel(int32_t iHandle, FPROCMS *pSamplesPerPixel);

//////////////////////////////////////////
///
/// @brief Retrieves the current pixel scan direction settings on the sensor.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pHInverted - Returned horizontal scan direction: False = Normal, True = Inverted.
/// @param pVInverted - Returned vertical scan direction: False = Normal, True = Inverted.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_GetScanDirection(int32_t iHandle, bool *pHInverted,bool *pVInverted);

//////////////////////////////////////////
///
/// @brief Returns the sensor re-training setting.
///
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pEnable - true: Forces sensor to undergo training.  False - stops the sensor training.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_GetTrainingEnable(int32_t iHandle, bool *pEnable);

//////////////////////////////////////////
///
/// @brief Sets the analog gain for the sensor.
///
/// Note: Not all camera models support this feature.  Consult you camera
///       documentation for details. In addition, cameras do not currently
///       support read back of this value so there is no corresponding
///       FPROSensor_GetAnalogGain() API.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param iGainValue - The analog gain value to set.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_SetAnalogGain(int32_t iHandle, int32_t iGainValue);

//////////////////////////////////////////
///
/// @brief Sets the desired horizontal and vertical binning.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiXBin - Horizontal bin setting.
/// @param uiYBin - Vertical bin setting.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_SetBinning(int32_t iHandle, uint32_t uiXBin, uint32_t uiYBin);

//////////////////////////////////////////
///
/// @brief Sets the current Black Level Adjustment values.
///
/// <br>
/// Note that not all cameras support multiple channels for the
/// Black Level Adjustment values.  Consult your users manual for specifics on your device.
/// If you call the #FPROSensor_SetBlackLevelAdjust() API for a camera that does support
/// multiple channels, the channel defaults to the first channel on the device.
/// <br>
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param eChan -	The adjustment channel to set.
/// @param uiAdjustValue - Value to set.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_SetBlackLevelAdjust(int32_t iHandle, FPROBLACKADJUSTCHAN eChan, uint32_t uiAdjustValue);

//////////////////////////////////////////
///
/// @brief Sets the current Black Sun Adjustment value.
///
/// <br>
/// Note that not all cameras support multiple channels for the
/// Black Sun Adjustment values.  Consult your users manual for specifics on your device.
/// If you call the #FPROSensor_SetBlackSunAdjust() API for a camera that does support
/// multiple channels, the channel defaults to the first channel on the device.
/// <br>
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param eChan -	The adjustment channel to set.
/// @param uiAdjustValue - Value to set.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_SetBlackSunAdjust(int32_t iHandle, FPROBLACKADJUSTCHAN eChan, uint32_t uiAdjustValue);

//////////////////////////////////////////
///
/// @brief  Sets the setting for HDR enable.
///
/// See #FPROHDR enumeration for details.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param eHDREnable - The HDR setting.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_SetHDREnable(int32_t iHandle, FPROHDR eHDREnable);

//////////////////////////////////////////
///
/// @brief Sets the High Gain Only Mode.  
/// <br>
/// Note: The High Gain Only Mode is not applicable to all camera models.  In addition, it is only 
/// applicable in HDR modes.  When in an HDR mode, if the High Gain Only flag is set, only the High
/// Gain image will be returned by the camera.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param bHighGainOnly - High Gain only mode flag.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_SetHighGainOnlyEnable(int32_t iHandle, bool bHighGainOnly);

//////////////////////////////////////////
///
/// @brief Sets the current setting for the Gain for the specified table.
///
/// Note: The index is the index into the table as returned by 
///       FPROSensor_GetGainTable().
/// Note: When setting an LDR gain table index, if the camera is in an LDR mode,
///       as set by #FPROSensor_SetMode(), the HDR gain index is set to match to maintain 
///       image integrity. If you attempt
///       to set an HDR index when in an LDR mode, the function will return an error.  Set the
///       mode first using #FPROSensor_SetMode(), then override the gain settings as desired
///       using this function.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param eTable - The table to retrieve.
/// @param uiGainIndex - Index value to set.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_SetGainIndex(int32_t iHandle, FPROGAINTABLE eTable, uint32_t uiGainIndex);

//////////////////////////////////////////
///
/// @brief Sets the current mode specified by the given index.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiModeIndex - The index of the mode to set.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_SetMode(int32_t iHandle, uint32_t uiModeIndex);

//////////////////////////////////////////
///
/// @brief Sets the sensor read out configuration on supported models.
///
/// For camera models that support sensor channel read out configuration, this API
/// sets the given setting.  The function will return failure if the given configuration
/// is invalid for the camera model connected.  For models that do not support this feature, this
/// function has no effect.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param eReadCfg - The configuration to set.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_SetReadoutConfiguration(int32_t iHandle, FPROSENSREADCFG eReadCfg);

//////////////////////////////////////////
///
/// @brief Sets the Samples Per Pixel settings on the sensor.
/// <br><br>
/// NOTE: This function is not supported on on all camera models.  Consult
/// your documentation for your specific camera.
/// <br>
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param eSamplesPerPixel - The Samples Per Pixel Setting to set.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.  On devices that
/// do not support this setting, 0 is always returned.
LIBFLIPRO_API FPROSensor_SetSamplesPerPixel(int32_t iHandle, FPROCMS eSamplesPerPixel);

//////////////////////////////////////////
///
/// @brief Retrieves the current pixel scan direction settings on the sensor.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param bHInverted - Horizontal scan direction: False = Normal, True = Inverted.
/// @param bVInverted - Vertical scan direction: False = Normal, True = Inverted..
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_SetScanDirection(int32_t iHandle, bool bHInverted, bool bVInverted);

//////////////////////////////////////////
///
/// @brief Enables/Disables sensor re-training.
///
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param bEnable - true: Forces sensor to undergo training.  False - stops the sensor training.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROSensor_SetTrainingEnable(int32_t iHandle, bool bEnable);


//////////////////////////////////////////
// Auxiliary I/O Support Functions
//////////////////////////////////////////
//////////////////////////////////////////
///
/// @brief Gets the direction and state for given Auxiliary I/O pin.
///
/// For Output pins, the state of the pin will be the value last set with the
/// #FPROAuxIO_GetPin() call.  For Input pins, the state of the pin reflects the 
/// state of the physical input signal.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param eAuxIO - Auxiliary I/O pin to retrieve.
/// @param pDirection - Pin direction.
/// @param pState - Pin state. May be NULL if you do not want the state.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAuxIO_GetPin(int32_t iHandle, FPROAUXIO eAuxIO, FPROAUXIO_DIR *pDirection, FPROAUXIO_STATE *pState);

//////////////////////////////////////////
///
/// @brief Get Exposure Active Type Signal.
///
/// This function gets the Exposure Type Signal for the Exposure Type Auxiliary 
/// output pin.  Some camera models support choosing the AuxIO pin used for the
/// Wxposure Active signal. In these cases, the ePin parameter is used to obtain the
/// setting.  For cameras that have a dedicated Exposure Active pin, this parameter 
/// is ignored. Consult your documentation for signal timing details.  
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param ePin - If suppoerted by the camera, the pin used for exposure active signal.
/// If it is not supported by the camera, this parameter is ignored. 
/// @param pType - Exposure Active Signal Type. 
/// @param pActiveHigh - The polarity of the Exposure Active Signal.  True= active high.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAuxIO_GetExposureActiveType(int32_t iHandle, FPROAUXIO ePin, FPROAUXIO_EXPACTIVETYPE *pType, bool* pActiveHigh);

//////////////////////////////////////////
///
/// @brief Sets the direction and state for given Auxiliary I/O pin.
///
/// Note that the state is only applicable to output pins.  It is ignored
/// for input pins.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param eAuxIO - Auxiliary I/O pin to configure.
/// @param eDirection - Pin direction to set.
/// @param eState - Pin state to set for output pins.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAuxIO_SetPin(int32_t iHandle, FPROAUXIO eAuxIO, FPROAUXIO_DIR eDirection, FPROAUXIO_STATE eState);

//////////////////////////////////////////
///
/// @brief Exposure Active Type Signal.
///
/// This function sets the Exposure Type Signal for the Exposure Type Auxiliary 
/// output pin.  Some camera models support choosing the AuxIO pin used for the
/// Exposure Active signal. In these cases, the ePin parameter is used to apply the
/// setting.  For cameras that have a dedicated Exposure Active pin, this parameter 
/// is ignored. Consult your documentation for signal timing details.  
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param ePin - If suppoerted by the camera, the pin used for exposure active signal.
/// If it is not supported by the camera, this parameter is ignored. 
/// @param eType - Exposure Active Signal Type.
/// @param bActiveHigh - The polarity of the Exposure Active Signal.  True= active high.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAuxIO_SetExposureActiveType(int32_t iHandle, FPROAUXIO ePin, FPROAUXIO_EXPACTIVETYPE eType, bool bActiveHigh);


//////////////////////////////////////////
// Frame Acknowledgment Mode Support Functions
//////////////////////////////////////////
//////////////////////////////////////////
///
/// @brief Get Frame Acknowledgment Mode Enable.
///
/// This function gets the Frame Acknowledgment Mode Enable.  If true, Frame
/// Acknowledgment Mode is enabled.  
/// <br>
/// Frame Acknowledgment Mode is a camera mode that instructs the camera to store
/// each frame as it is exposed in an internal memory.  The frame in the memory
/// may be retransmitted to the host using the #FPROFAck_FrameResend() API.  Each frame -must- be
/// explicitly acknowledged by the user using the #FPROFAck_FrameAcknowledge() API.  This allows
/// the camera to delete the frame from its memory queue making it available for the next frame.
/// <br>
/// This mode is intended for users who require every image to be successfully transmitted to the host
/// even in the face of cable and unrecoverable transmission errors.  Because of the required acknowledgments,
/// this mode is significantly slower with respect to achievable frame rate and dependent on the host computer.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pEnable - The Frame Acknowledgment Mode enable- true if enabled, false otherwise.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFAck_GetEnable(int32_t iHandle, bool *pEnable);

//////////////////////////////////////////
///
/// @brief Set Frame Acknowledgment Mode Enable.
///
/// This function sets the Frame Acknowledgment Mode Enable.  If true, Frame
/// Acknowledgment Mode is enabled.  See #FPROFAck_GetEnable() for a description
/// of this mode.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param bEnable - The Frame Acknowledgment Mode enable- True if enabled, False otherwise.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFAck_SetEnable(int32_t iHandle, bool bEnable);

//////////////////////////////////////////
///
/// @brief Acknowledge the last frame sent in Frame Acknowledgment Mode.
///
/// This function acknowledges the last from sent to the host.
/// See #FPROFAck_GetEnable() for a description of this mode.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFAck_FrameAcknowledge(int32_t iHandle);


//////////////////////////////////////////
///
/// @brief Re-send the last frame in Frame Acknowledgment Mode.
///
/// This function instructs the camera to re-send the last image frame to the
/// host.  Is expected to be called by an application in the event of transmission 
/// errors or errors detected during parsing of the image data.
/// See #FPROFAck_GetEnable() for a description of this mode.
/// <br>
/// The frame data will be available immediately after this call so it is important
/// to call #FPROFrame_GetVideoFrame() with the proper parameters immediately after this call 
/// returns.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFAck_FrameResend(int32_t iHandle);

//////////////////////////////////////////
///
/// @brief Flush the in memory frame queue in Frame Acknowledgment Mode.
///
/// This function instructs the camera to flush all of the image frames contained
/// in its internal image frame queue.
/// See #FPROFAck_GetEnable() for a description of this mode.
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROFAck_FlushImageQueue(int32_t iHandle);

//////////////////////////////////////////
/// Merge algorithm related functions including
/// Image Stacking 
//////////////////////////////////////////
//////////////////////////////////////////
///
/// @brief Initialize the stacking process
///
/// This function initializes the stacking process.  It allocates memory based on the given
/// size and the current imaging parameters on the camera.  You must not change any camera
/// parameters after making this call and completing the stacking process.
/// This process, when complete, produces
/// a high and low gain mean image from a stack of images.  The result is suitable for merging
/// images both by software or hardware when using the FLI PCIE Fibre connection. 
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAlgo_StackInitialize(int32_t iHandle);

//////////////////////////////////////////
///
/// @brief Capture and retrieve the next frame to stack from the camera.
///
/// This function retrieves and applies the next frame to the stack computations.
/// It is expected that you have already called #FPROFrame_CaptureStart() to trigger the
/// frame capture on the camera.
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///	@param pFrameData - The buffer to store the original frame data that was stacked.
/// @param pSize - Size the of the pFrameData buffer.
///                On return, the number of bytes actually received.
/// @param uiTimeoutMS - How long to wait for a frame to be available.
///                      Assuming you make this call very soon after the FPROFrame_CaptureStart()
///                      call, you should set this to the exposure time.  Internally, the API
///                      blocks (i.e. no communication with the camera) for some time less than 
///                      uiTimeoutMS and then attempts to retrieve the frame.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAlgo_StackNextFrame(int32_t iHandle, uint8_t* pFrameData, uint32_t* pSize, uint32_t uiTimeoutMS);

//////////////////////////////////////////
///
/// @brief Finish the stacking process and retrieve the mean frames.
///
/// This function retrieves finalizes the stack computations and returns the mean frames.
/// Assuming you make no changes to the camera settings, after this call a new stack can 
/// be started by simply starting a new a sequence of #FPROAlgo_StackNextFrame() calls.
/// Any time you want to change camera settings, or you are done with the stacking process,
/// you must call FPROAlgo_StackDeinitialize() to free the memory allocated during the 
/// stacking process.
/// <br>
/// <br>
/// NOTE: The returned buffer pointers are allocated by the API and must be free by calling
///       the FPROAlgo_StackDeinitialize() function when you are finished with the stacking process.
///       If you run multiple stacking procedures between FPROAlgo_Stacknitialize() and 
///       FPROAlgo_StackDeinitialize(), the API reuses these buffers, so make sure to save 
///       them if you need to prior to making subsequent #FPROAlgo_StackNextFrame() calls.
/// <br>
///       The Image frames include the mean data only, no meta data.
/// <br>
/// <br>
///	      If you receive a ppMetaData pointer, do not free it.  The memory is handled by
///       the API.  Treat it as a static buffer.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param ppLowMeanFrame - Buffer pointer for Low Mean Frame.
/// @param ppHighMeanFrame - Buffer pointer for High Mean Frame.
/// @param pNumPixels - Buffer to return the number of pixels in each frame.
/// @param ppMetaData - Buffer pointer for returned meta data (may be NULL).  Do not free!
/// @param puiMetaSize - Pointer for returned size of meta data (may be NULL).
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAlgo_StackFinish(int32_t iHandle, uint16_t **ppLowMeanFrame, uint16_t **ppHighMeanFrame, uint32_t* pNumPixels, uint8_t** ppMetaData, uint32_t* puiMetaSize);

//////////////////////////////////////////
///
/// @brief Returns all the resources allocated during the stacking process to the system.
///
/// This function must be called when you are done with the stacking process to return
/// all of the resources allocated during the process to the system.  This includes the
/// buffers returned by #FPROAlgo_StackFinish() so make sure you have saved the buffers
/// if you need to prior to calling this function.
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAlgo_StackDeinitialize(int32_t iHandle);

//////////////////////////////////////////
///
/// @brief Sets the reference frames used in PCIE Fibre hardware image merging.
///
/// <br>
/// Use this function to setup the merging reference frames.  If you pass NULL for
/// one of the fields in pRefFrames, an identity reference frame is created for 
/// that plane.  If you are using identity frames, you must call this function after
/// changing the gain setting on the camera in order to get the proper merge results.  
/// When Identity farames are used, the API extracts the gain setting from the image
/// meta data and uses that to build an appropriate set of reference frames.  So if you change
/// the gain, you must reset the idneity frames in order to get the new set of reference frames
/// constructed for the merge process.
/// <br>
/// <br>
/// The example code also provides guidance for building this structure from 
/// your reference files.  See the SimpleAPIMerging and StreamingAndHWMerge 
/// examples for further information.
/// <br>
/// <br>
/// See #FPRO_REFFRAMES for additional information.
/// <br>

///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pRefFrames - Pointer to the reference frames to use.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAlgo_SetHardwareMergeReferenceFrames(int32_t iHandle, FPRO_REFFRAMES* pRefFrames);

//////////////////////////////////////////
///
/// @brief Sets the reference frames used in PCIE Fibre hardware image merging.
///
/// This function is analogous to the #FPROAlgo_SetHardwareMergeReferenceFrames API.
/// It expects RCD Files as generated by the FLI Pilot application rather than the raw 
/// reference frame data.  It builds the FPRO_REFFRAMES structure from the file data and
/// sets the reference frames as if #FPROAlgo_SetHardwareMergeReferenceFrames was called.
/// You need only call this API or #FPROAlgo_SetHardwareMergeReferenceFrames, you do not need
/// to call both.  This API is provided as a built in convenience when using the FLI Pilot 
/// application to generate the reference data files.  
/// <br>
/// <br>
/// At least one of the parameters must point to a valid file name; the other file name may be NULL.
/// Passing a NULL pointer for a file name will generate identity reference data for the frame: Zeros (0) 
/// for the DSNU frames and ones (1) for the PRNU frames.
/// <br>
/// <br>
/// See #FPRO_REFFRAMES for further information.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pDSNUFile - Pointer to the DSNU File.
/// @param pPRNUFile - Pointer to the PRNU File.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAlgo_SetHardwareMergeReferenceFiles(int32_t iHandle, const wchar_t *pDSNUFile, const wchar_t *pPRNUFile);

//////////////////////////////////////////
///
/// @brief Retrieve the current Hardware Merge Threshold values.
///
/// Note this functionality only exists on Host PCIE Fibre connections version 2 or later.
/// If you use hardware merging, you must call this API prior to calling #FPROFrame_ComputeFrameSize().
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pHighGainThreshold - The returned high gain pixel value threshold. See #FPROAlgo_SetHardwareMergeThresholds for a description.
/// @param pMergeDifferenceThreshold - The returned merge difference threshold. See #FPROAlgo_SetHardwareMergeThresholds for a description.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAlgo_GetHardwareMergeThresholds(int32_t iHandle, uint16_t *pHighGainThreshold, uint16_t*pMergeDifferenceThreshold);

//////////////////////////////////////////
///
/// @brief Retrieve the current Hardware Merge Threshold values.
///
/// Note this functionality only exists on Host PCIE Fibre connections version 2 or later.
/// If you use hardware merging, you must call this API prior to calling #FPROFrame_ComputeFrameSize().
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiHighGainThreshold - The high gain pixel value threshold (when over, the Low gain pixel is used).
/// @param uiMergeDifferenceThreshold - When the High gain pixel value if below the High Gain Threshold, the Low
///                                     pixel is used only if the difference between the is greater than the Merge
///                                     Difference Threshold ((low - high) > merge_difference).
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAlgo_SetHardwareMergeThresholds(int32_t iHandle, uint16_t uiHighGainThreshold, uint16_t uiMergeDifferenceThreshold);

//////////////////////////////////////////
///
/// @brief Retrieve the hardware merge enable settings.
///
/// Note this functionality only exists on Host PCIE Fibre connections version 2 or later.
/// See #FPROAlgo_SetHardwareMergeEnables().
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param pMergeEnables - The enables for various options.  See #FPRO_HWMERGEENABLE for more information.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAlgo_GetHardwareMergeEnables(int32_t iHandle, FPRO_HWMERGEENABLE *pMergeEnables);


//////////////////////////////////////////
///
/// @brief Enable/disable hardware merging options.
///
/// Note this functionality only exists on Host PCIE Fibre connections version 2 or later.
/// If you use hardware merging, you must call this API prior to calling #FPROFrame_ComputeFrameSize().
/// <br>
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param mergeEnables - The enables for various options.  See #FPRO_HWMERGEENABLE for more information.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPROAlgo_SetHardwareMergeEnables(int32_t iHandle, FPRO_HWMERGEENABLE mergeEnables);


///  @cond DO_NOT_DOCUMRNT
//LIBFLIPRO_API FPROAlgo_MergeRcdToFits(wchar_t *pRCDFileName, wchar_t *pDSNURef, wchar_t *pPRNURef);
/// @endcond

//////////////////////////////////////////
// NV Storage Functions
//////////////////////////////////////////
//////////////////////////////////////////
///
/// @brief Write the given data to the non volatile storage area on the camera.
///
/// This function simply writes the given data to the non volatile storage area on
/// the camera.  This API allows users to keep proprietary settings
/// linked with a given camera.  No structure is imposed on the data by this API nor the camera. 
/// That is, the data is simply treated as a byte stream.  It is up to the user to format the
/// data as required for their application.  
/// <br>
/// Note that not all cameras may support a non-volatile memory area.  You can determine if
/// it is available and the size by reading the capabilities of the camera using the
/// #FPROSensor_GetCapabilityList API.  The uiNVStorageAvailable field in this structure contains
/// the size in bytes of the NV storage area.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiOffset - The offset within the NV area to begin the write. 
/// @param pData - The data to write. 
/// @param uiLength - The number of bytes to write. 
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPRONV_WriteNVStorage(int32_t iHandle,uint32_t uiOffset, uint8_t *pData, uint32_t uiLength);

//////////////////////////////////////////
///
/// @brief Read the non volatile storage area on the camera.
///
/// This function simply reads the non volatile storage area on
/// the camera and returns it in the provided buffer.  See # FPRONV_WriteNVStorage
/// for more information.
///
///	@param iHandle - The handle to an open camera device returned from FPROCam_Open()
/// @param uiOffset - The offset within the NV area to begin the read. 
/// @param pData - The buffer for the data. 
/// @param uiLength - The number of bytes to read. 
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API FPRONV_ReadNVStorage(int32_t iHandle, uint32_t uiOffset, uint8_t *pData, uint32_t uiLength);

//////////////////////////////////////////
// Low level commands and functions
//////////////////////////////////////////
/// @private
LIBFLIPRO_API FPROCmd_SendRaw(int32_t iHandle, uint8_t *pData, uint32_t uiLength);
/// @private
LIBFLIPRO_API FPROCmd_RecvRaw(int32_t iHandle, uint8_t* pRxData, uint32_t* pRxLength);
/// @private
LIBFLIPRO_API FPROCmd_SendRecvRaw(int32_t iHandle, uint8_t *pTxData, uint32_t uiTxLength, uint8_t *pRxData, uint32_t *pRxLength);
/// @private
LIBFLIPRO_API FPROCmd_ReadReg(int32_t iHandle, uint32_t uiReg, uint32_t *pValue);
/// @private
LIBFLIPRO_API FPROCmd_WriteReg(int32_t iHandle, uint32_t uiReg, uint32_t uiValue, uint32_t uiMask);
/// @private
LIBFLIPRO_API FPROCmd_ReadDeviceReg(int32_t iHandle, uint32_t uiDevId, uint32_t uiReg, uint32_t* pValue);
/// @private
LIBFLIPRO_API FPROCmd_ReadDeviceRegEx(int32_t iHandle, uint32_t uiDevId, uint32_t uiReg, uint8_t* pRxData, uint32_t *pRxLength);
/// @private
LIBFLIPRO_API FPROCmd_WriteDeviceReg(int32_t iHandle, uint32_t uiDevId, uint32_t uiReg, uint32_t uiValue, uint32_t uiMask);
/// @private
LIBFLIPRO_API FPROCmd_PCIEReadReg(int32_t iHandle, uint32_t uiReg, uint32_t* pValue);
/// @private
LIBFLIPRO_API FPROCmd_PCIEWriteReg(int32_t iHandle, uint32_t uiReg, uint32_t uiValue);
/// @private
LIBFLIPRO_API FPROCmd_ReadSensorReg(int32_t iHandle, uint32_t uiChipId, uint32_t uiReg, uint32_t* pValue);
/// @private
LIBFLIPRO_API FPROCmd_WriteSensorReg(int32_t iHandle, uint32_t uiChipId, uint32_t uiReg, uint32_t uiValue, uint32_t uiMask);

// Debug Functions
/// @cond DO_NOT_DOCUMENT
// Conversion strings to aid in debug printing
// The Linux part is different because of the way
// swprintf() works- when %s is used, the argument is
// assumed to be a char pointer.  Hence we do not make it wide.
#if defined(WIN32) || defined(_WINDOWS)
#define STRINGIFY(x) L##x
#define MAKEWIDE(x) STRINGIFY(x)
#else
#define MAKEWIDE(x) (x)
#endif
/// @endcond

//////////////////////////////////////////
///
/// Enables the given debug level and above.
///
///	@param bEnable - Overall enable for debug output
/// @param eLevel - The level to enable if bEnable is true;
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API  FPRODebug_EnableLevel(bool bEnable, FPRODBGLEVEL eLevel);
//////////////////////////////////////////
///
/// Sets the log file path to the given folder.  The file name is auto generated.
///
///	@param pPath - Path (folder) in which to place the log file.
///
/// @return Greater than or equal to 0 on success, less than 0 on failure.
LIBFLIPRO_API  FPRODebug_SetLogPath(const wchar_t *pPath);
#ifdef WIN32 
//////////////////////////////////////////
///
/// Writes the given information to the log file if the given level is enabled.
/// <br>The parameters support the basic printf type of formatting.
///
///	@param eLevel - Debug level of the log message.
/// @param format - printf style formatting string
/// @param ...    - printf arguments for the format string
///
/// @return None.
LIBFLIPRO_VOID _cdecl FPRODebug_Write(FPRODBGLEVEL eLevel, const wchar_t *format, ...);
#else
//////////////////////////////////////////
/// FPRODebug_Write
/// Writes the given information to the log file if the given level is enabled.
/// <br>The parameters support the basic printf type of formatting.
///
///	@param eLevel - Debug level of the log message.
/// @param format - printf style formatting string
/// @param ...    - printf arguments for the format string
///
/// @return None.
LIBFLIPRO_VOID FPRODebug_Write(FPRODBGLEVEL eLevel, const wchar_t *format, ...);
#endif




#endif   // _LIBFLIPRO_H_

#ifdef __cplusplus
}

#endif
