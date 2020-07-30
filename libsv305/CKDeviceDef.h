//////////////////////////////////////////////////////
//CKDeviceDef.h


#ifndef __CK_DEVICE_DEF_H__
#define __CK_DEVICE_DEF_H__

#include <stdint.h>

#ifndef _WINDOWS


typedef uint32_t DWORD; 
typedef int32_t INT;
typedef uint32_t UINT;
typedef uint16_t WORD;
typedef uint64_t UINT64;
typedef int BOOL;
typedef uint8_t BYTE;
typedef long    LONG;
typedef void * HANDLE; 
typedef HANDLE * PHANDLE;
typedef void * LPVOID; 
typedef void * HDC;
typedef uint16_t WCHAR;
typedef unsigned short USHORT;
typedef char TCHAR;
typedef void * HWND;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define _access access
#define INFINITE 0xFFFFFFFF

#define WINAPI 
#define __stdcall


#pragma pack(push, 1)
typedef struct tagRECT
{
LONG left;
LONG top;
LONG right;
LONG bottom;

}RECT;

typedef struct tagBITMAPINFOHEADER{
        DWORD      biSize;
        LONG       biWidth;
        LONG       biHeight;
        WORD       biPlanes;
        WORD       biBitCount;
        DWORD      biCompression;
        DWORD      biSizeImage;
        LONG       biXPelsPerMeter;
        LONG       biYPelsPerMeter;
        DWORD      biClrUsed;
        DWORD      biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef struct tagRGBQUAD {
        BYTE    rgbBlue;
        BYTE    rgbGreen;
        BYTE    rgbRed;
        BYTE    rgbReserved;
} RGBQUAD;


typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER    bmiHeader;
    RGBQUAD             bmiColors[1];
} BITMAPINFO, *LPBITMAPINFO, *PBITMAPINFO;

typedef struct tagBITMAPFILEHEADER {
        WORD    bfType;
        DWORD   bfSize;
        WORD    bfReserved1;
        WORD    bfReserved2;
        DWORD   bfOffBits;
} BITMAPFILEHEADER, *LPBITMAPFILEHEADER, *PBITMAPFILEHEADER;

#pragma pack(pop)

#endif



#define CROSSLINE_MAX_NUM  9

/**
*@ingroup __CK_ERRCODE__
*@{ 
*/

/** @brief @~chinese 操作成功 @~english Operation succeeded */
#define CAMERA_STATUS_SUCCESS 0
/** @brief @~chinese 操作失败 @~english Operation failed */
#define CAMERA_STATUS_FAILED -1
/** @brief @~chinese 内部错误 @~english Internal error */
#define CAMERA_STATUS_INTERNAL_ERROR -2
/** @brief @~chinese 未知错误 @~english Unknown error */
#define CAMERA_STATUS_UNKNOW -3
/** @brief @~chinese 不支持该功能 @~english Don't support this feature */
#define CAMERA_STATUS_NOT_SUPPORTED -4
/** @brief @~chinese 初始化未完成 @~english Initialization not completed */
#define CAMERA_STATUS_NOT_INITIALIZED -5
/** @brief @~chinese 参数无效 @~english Parameter is invalid */
#define CAMERA_STATUS_PARAMETER_INVALID -6
/** @brief @~chinese 参数越界 @~english Parameter out of bounds */
#define CAMERA_STATUS_PARAMETER_OUT_OF_BOUND -7
/** @brief @~chinese 未使能 @~english It is not enabled */
#define CAMERA_STATUS_UNENABLED -8
/** @brief @~chinese 用户手动取消了，比如roi面板点击取消，返回 @~english The user canceled manually, such as the roi panel click cancel, return */
#define CAMERA_STATUS_USER_CANCEL -9
/** @brief @~chinese 注册表中没有找到对应的路径 @~english The corresponding path was not found in the registry */
#define CAMERA_STATUS_PATH_NOT_FOUND -10
/** @brief @~chinese 获得图像数据长度和定义的尺寸不匹配 @~english Get image data length and defined size does not match */
#define CAMERA_STATUS_SIZE_DISMATCH -11
/** @brief @~chinese 超时错误 @~english Timeout error */
#define CAMERA_STATUS_TIME_OUT -12
/** @brief @~chinese 硬件IO错误 @~english Hardware IO error */
#define CAMERA_STATUS_IO_ERROR -13
/** @brief @~chinese 通讯错误 @~english Communication error */
#define CAMERA_STATUS_COMM_ERROR -14
/** @brief @~chinese 总线错误 @~english Bus error */
#define CAMERA_STATUS_BUS_ERROR -15
/** @brief @~chinese 没有发现设备 @~english No devices found */
#define CAMERA_STATUS_NO_DEVICE_FOUND -16
/** @brief @~chinese 未找到逻辑设备 @~english No logical device found */
#define CAMERA_STATUS_NO_LOGIC_DEVICE_FOUND -17
/** @brief @~chinese 设备已经打开 @~english Device is already open */
#define CAMERA_STATUS_DEVICE_IS_OPENED -18
/** @brief @~chinese 设备已经关闭 @~english Device is turned off */
#define CAMERA_STATUS_DEVICE_IS_CLOSED -19
/** @brief @~chinese 没有打开设备视频，调用录像相关的函数时，如果相机视频没有打开，则回返回该错误。 @~english When the device video is not turned on, when the video related function is called, if the camera video is not turned on, the error is returned. */
#define CAMERA_STATUS_DEVICE_VEDIO_CLOSED -20
/** @brief @~chinese 没有足够系统内存 @~english Not enough system memory */
#define CAMERA_STATUS_NO_MEMORY -21
/** @brief @~chinese 创建文件失败 @~english Failed to create file */
#define CAMERA_STATUS_FILE_CREATE_FAILED -22
/** @brief @~chinese 文件格式无效 @~english File format is invalid */
#define CAMERA_STATUS_FILE_INVALID -23
/** @brief @~chinese 写保护，不可写 @~english Write protection, not writeable */
#define CAMERA_STATUS_WRITE_PROTECTED -24
/** @brief @~chinese 数据采集失败 @~english Data collection failed */
#define CAMERA_STATUS_GRAB_FAILED -25
/** @brief @~chinese 数据丢失，不完整 @~english Data is missing, incomplete */
#define CAMERA_STATUS_LOST_DATA -26
/** @brief @~chinese 未接收到帧结束符 @~english Frame end not received */
#define CAMERA_STATUS_EOF_ERROR -27
/** @brief @~chinese 正忙(上一次操作还在进行中)，此次操作不能进行 @~english Is busy (the last operation is still in progress), this operation cannot be done */
#define CAMERA_STATUS_BUSY -28
/** @brief @~chinese 需要等待(进行操作的条件不成立)，可以再次尝试 @~english Needs to wait (the conditions for the operation are not met), you can try again */
#define CAMERA_STATUS_WAIT -29
/** @brief @~chinese 正在进行，已经被操作过 @~english Is in progress and has been manipulated */
#define CAMERA_STATUS_IN_PROCESS -30
/** @brief @~chinese IIC传输错误 @~english IIC transmission error */
#define CAMERA_STATUS_IIC_ERROR -31
/** @brief @~chinese SPI传输错误 @~english SPI transmission error */
#define CAMERA_STATUS_SPI_ERROR -32
/** @brief @~chinese USB控制传输错误 @~english USB control transfer error */
#define CAMERA_STATUS_USB_CONTROL_ERROR -33
/** @brief @~chinese USB BULK传输错误@~english USB BULK transmission error */
#define CAMERA_STATUS_USB_BULK_ERROR -34
/** @brief @~chinese 网络传输套件初始化失败 @~english Network Transfer Kit initialization failed */
#define CAMERA_STATUS_SOCKET_INIT_ERROR -35
/** @brief @~chinese 网络相机内核过滤驱动初始化失败，请检查是否正确安装了驱动，或者重新安装。 @~english The network camera kernel filter driver failed to initialize. Please check if the driver is properly installed or reinstall. */
#define CAMERA_STATUS_GIGE_FILTER_INIT_ERROR -36
/** @brief @~chinese 网络数据发送错误 @~english Network data sending error */
#define CAMERA_STATUS_NET_SEND_ERROR -37
/** @brief @~chinese 与网络相机失去连接，心跳检测超时 @~english Lost connection with web camera, heartbeat detection timed out */
#define CAMERA_STATUS_DEVICE_LOST -38
/** @brief @~chinese 接收到的字节数比请求的少 @~english Received fewer bytes than requested */
#define CAMERA_STATUS_DATA_RECV_LESS -39
/** @brief @~chinese 从文件中加载程序失败 @~english Failed to load program from file */
#define CAMERA_STATUS_FUNCTION_LOAD_FAILED -40
/** @brief @~chinese 程序运行所必须的文件丢失。 @~english The files necessary for the program to run are missing. */
#define CAMERA_STATUS_CRITICAL_FILE_LOST -41
/** @brief @~chinese 固件和程序不匹配，原因是下载了错误的固件。 @~english The firmware and program do not match because the wrong firmware was downloaded. */
#define CAMERA_STATUS_SENSOR_ID_DISMATCH -42
/** @brief @~chinese 参数超出有效范围 @~english The parameter is outside the valid range. */
#define CAMERA_STATUS_OUT_OF_RANGE -43
/** @brief @~chinese 安装程序注册错误。请重新安装程序 @~english Installer registration error. Please reinstall the program */
#define CAMERA_STATUS_REGISTRY_ERROR -44
/** @brief @~chinese 禁止访问。指定相机已经被其他程序占用时，再申请访问该相机，会返回该状态。(一个相机不能被多个程序同时访问) @~english No access. When the specified camera is already occupied by another program, it will return to this state after requesting access to the camera. (A camera cannot be accessed simultaneously by multiple programs) */
#define CAMERA_STATUS_ACCESS_DENY -45
/** @brief @~chinese 表示相机需要复位后才能正常使用，此时请让相机断电重启，或者重启操作系统后，便可正常使用。 @~english Means that the camera needs to be reset before it can be used normally. In this case, please let the camera power off and restart, or restart the operating system, then it can be used normally. */
#define CAMERA_STATUS_CAMERA_NEED_RESET -46
/** @brief @~chinese ISP模块未初始化 @~english ISP module is not initialized */
#define CAMERA_STATUS_ISP_MOUDLE_NOT_INITIALIZED -47
/** @brief @~chinese 数据校验错误 @~english Data validation error */
#define CAMERA_STATUS_ISP_DATA_CRC_ERROR -48
/** @brief @~chinese 数据测试失败 @~english Data test failed */
#define CAMERA_STATUS_MV_TEST_FAILED -49
/** @brief @~chinese 等待触发帧 @~english Waiting for trigger frame */
#define CAMERA_STATUS_WAIT_TRIGGER -50
/** @brief @~chinese EE数据版本需要更新 @~english EE data version needs to be updated */
#define CAMERA_STATUS_EEPROM_UPDATA -51
/** @brief @~chinese 打开设备失败 @~english Failed to open device */
#define CAMERA_STATUS_OPEN_DEVICE_ERROR -52
/** @brief @~chinese 打开视频流失败 @~english Failed to open video stream */
#define CAMERA_STATUS_OPEN_STREAM_ERROR -53
/** @brief @~chinese 读错误 @~english Read error */
#define CAMERA_STATUS_READ_ERROR -54
/** @brief @~chinese 写错 @~english Write error */
#define CAMERA_STATUS_WRITE_ERROR -55
/** @brief @~chinese 没有打开的设备 @~english No open device */
#define CAMERA_STATUS_NO_OPEN_DEVICE -56
/** @brief @~chinese 未识别sensor @~english Sensor not recognized */
#define CAMERA_STATUS_UNKNOW_SENSOR -57
/** @brief @~chinese 没有写EE数据 @~english Don't write EE data */
#define CAMERA_STATUS_NO_WRITE_EEPROM -58
/** @brief @~chinese FPGA配置失败 @~english FPGA configuration failed */
#define CMAERA_STATUS_FPGA_CFG_ERROR -59

/** @} end of __CK_ERRCODE__ */


/**
*@ingroup __CK_ENUM__
*@{
*/
/** @brief @~chinese 设备类型 @~english device type */
typedef enum
{
	DEVICE_TYPE_UNKNOW = 0,
	DEVICE_TYPE_USB20 = 0x1201,
	DEVICE_TYPE_USB30 = 0x1301,
	DEVICE_TYPE_GIGE = 0x2001,
}emDeviceType;

/** @brief @~chinese 分辨率 @~english Image resolution*/
typedef enum
{
	/** @brief @~chinese 分辨率 512x384 @~english Resolution 512x384 */
	IMAGEOUT_MODE_512X384 = 0,
	/** @brief @~chinese 分辨率 640x480 @~english Resolution 640x480 */
	IMAGEOUT_MODE_640X480 = 1,
	/** @brief @~chinese 分辨率 800x600 @~english Resolution 800x600 */
	IMAGEOUT_MODE_800X600 = 2,
	/** @brief @~chinese 分辨率 1024x768 @~english Resolution 1024x768 */
	IMAGEOUT_MODE_1024X768 = 3,
	/** @brief @~chinese 分辨率 1280x720 @~english Resolution 1280x720 */
	IMAGEOUT_MODE_1280X720 = 4,
	/** @brief @~chinese 分辨率 1280x960 @~english Resolution 1280x960 */
	IMAGEOUT_MODE_1280X960 = 5,
	/** @brief @~chinese 分辨率 1920x1280 @~english Resolution 1920x1280 */
	IMAGEOUT_MODE_1920X1280 = 6,
	/** @brief @~chinese 分辨率 2048x1536 @~english Resolution 2048x1536 */
	IMAGEOUT_MODE_2048X1536 = 7,
	/** @brief @~chinese 分辨率 320x240 @~english Resolution 320x240 */
	IMAGEOUT_MODE_320X240 = 8,
	/** @brief @~chinese 分辨率 1280x1024 @~english Resolution 1280x1024 */
	IMAGEOUT_MODE_1280X1024 = 9,
	/** @brief @~chinese 分辨率 1600x1200 @~english Resolution 1600x1200 */
	IMAGEOUT_MODE_1600X1200 = 10,
	/** @brief @~chinese 分辨率 2592x1944 @~english Resolution 2592x1944 */
	IMAGEOUT_MODE_2592X1944 = 11,
	/** @brief @~chinese 分辨率 752x480 @~english Resolution 752x480 */
	IMAGEOUT_MODE_752X480 = 12,
	/** @brief @~chinese 分辨率 768x576 @~english Resolution 768x576 */
	IMAGEOUT_MODE_768X576 = 13,
	/** @brief @~chinese 分辨率 3664x2748 @~english Resolution 3664x2748 */
	IMAGEOUT_MODE_3664X2748 = 14,
	/** @brief @~chinese 分辨率 1920x1080 @~english Resolution 1920x1080 */
	IMAGEOUT_MODE_1920X1080 = 15,
	/** @brief @~chinese 分辨率 2304x1296 @~english Resolution 2304x1296 */
	IMAGEOUT_MODE_2304X1296 = 16,
	/** @brief @~chinese 分辨率 2304x1728 @~english Resolution 2304x1728 */
	IMAGEOUT_MODE_2304X1728 = 17,
	/** @brief @~chinese 分辨率 4608x3456 @~english Resolution 4608x3456 */
	IMAGEOUT_MODE_4608X3456 = 18,
	/** @brief @~chinese SDK预定义分辨率的个数 @~english SDK predefined number of resolutions */
	IMAGEOUT_MODE_NUM,

	/** @brief @~chinese 自定义分辨率 @~english Custom Resolution */
	IMAGEOUT_MODE_CUSTOM = 0XFF,
}emResolutionMode;

/** @brief @~chinese 帧速度 @~english frame rate */
typedef enum
{
	/** @brief @~chinese 帧速度 低速 @~english Frame rate Low speed */
	FRAME_SPEED_LOW = 0,
	/** @brief @~chinese 帧速度 中速 @~english Frame rate Medium speed */
	FRAME_SPEED_MIDDLE,
	/** @brief @~chinese 帧速度 高速 @~english Frame speed High speed */
	FRAME_SPEED_HIGH,
}emFrameMode;

/** @brief @~chinese 灯光频闪 @~english Light strobe */
typedef enum
{
	/** @brief @~chinese 禁止消除频闪 @~english Prohibits the elimination of strobes */
	FREQ_FLICKER_MODE_NULL = 0,
	/** @brief @~chinese 消除50hz频闪 @~english Eliminate 50hz strobe */
	FREQ_FLICKER_MODE_50HZ,
	/** @brief @~chinese 消除60hz频闪 @~english Eliminate 60hz strobe */
	FREQ_FLICKER_MODE_60HZ,
}emFlickerMode;

/** @brief @~chinese 保存图片的文件格式 @~english Save the file format of the image */
typedef enum
{
	/** @brief @~chinese 保存为JPEG文件格式 @~english Save as JPEG file format */
	FILE_JPG = 1,
	/** @brief @~chinese 保存为BMP文件格式 @~english Save as BMP file format */
	FILE_BMP = 2, // ​​BMP 24bit
	/** @brief @~chinese 保存为BMP文件格式, 对于不支持bayer格式输出相机，无法保存为该格式 @~english Save as BMP file format, for cameras that do not support bayer format, cannot be saved as this format */
	FILE_RAW = 4,
	/** @brief @~chinese 保存为PNG 24BIT文件格式 @~english Save as PNG 24BIT file format */
	FILE_PNG = 8,
	/** @brief @~chinese 保存为BMP 8BIT文件格式 @~english Save as BMP 8BIT file format */
	FILE_BMP_8BIT = 16,
	/** @brief @~chinese 保存为PNG 8BIT文件格式 @~english Save as PNG 8BIT file format */
	FILE_PNG_8BIT = 32,
	/** @brief @~chinese 保存为BMP 16BIT文件格式, 对于不支持bayer格式输出相机，无法保存为该格式 @~english Save as BMP 16BIT file format, for cameras that do not support bayer format, cannot be saved as this format */
	FILE_RAW_16BIT = 64
}emFileType;

/** @brief @~chinese 曝光算法 @~english Exposure Algorithm */
typedef enum
{
	/** @brief @~chinese 均值统计 @~english Mean Statistics */
	STATISTICS_AVG = 0,
	/** @brief @~chinese 直方图的统计方式 @~english Histogram statistics */
	STATISTICS_HIST,
	/** @brief @~chinese 带权重3x3的统计方式 @~english Statistical method with weight 3x3 */
	STATISTICS_WINDOW,
	/** @brief @~chinese 参考窗口的统计方式 @~english Reference window statistics */
	STATISTICS_REFER_WINDOW,
}emStatisticsMode;

/** @brief @~chinese 参数文件加载方式 @~english Parameter file loading method */
typedef enum
{
	/** @brief @~chinese 根据相机型号名从文件中加载参数，例如CK-U2B300C @~english Loads parameters from a file based on the camera model name, such as CK-U2B300 */
	PARAM_MODE_BY_MODEL = 0,
	/** @brief @~chinese 根据设备的唯一序列号从文件中加载参数，序列号在出厂时已经写入设备，每台相机拥有不同的序列号。 根据设备昵称(tDevInfo.acFriendlyName)从文件中加载参数，例如CK-U2B300,该昵称可自定义 @~english Loads parameters from the file according to the device nickname (tDevInfo.acFriendlyName), such as CK-U2B300, which can be customized */
	PARAM_MODE_BY_NAME,
	/** @brief @~chinese 根据设备的唯一序列号从文件中加载参数，序列号在出厂时已经写入设备，每台相机拥有不同的序列号 @~english Loads parameters from a file based on its unique serial number. The serial number is already written to the device at the factory, and each camera has a different serial number. */
	PARAM_MODE_BY_SN,
	/** @brief @~chinese 从设备的固态存储器中加载参数。不是所有的型号都支持从相机中读写参数组，由tSdkCameraCapbility.bParamInDevice决定 @~english Loads parameters from the device's solid state memory. Not all models support reading and writing parameter sets from the camera, as determined by tSdkCameraCapbility.bParamInDevice */
	PARAM_MODE_IN_DEVICE,
}emSdkParameterMode;

/** @brief @~chinese 相机的配置参数，分为A,B,C,D共4组进行保存 @~english The camera's configuration parameters are divided into 4 groups of A, B, C, and D for saving. */
typedef enum
{
	PARAMETER_TEAM_A = 0,
	PARAMETER_TEAM_B = 1,
	PARAMETER_TEAM_C = 2,
	PARAMETER_TEAM_D = 3,
	PARAMETER_TEAM_DEFAULT = 0xff,
}emSdkParameterTeam;

/** @brief @~chinese 相机触发模式 @~english Camera Trigger Mode */
typedef enum
{
	/** @brief @~chinese 连续触发模式 @~english Continuous trigger mode */
	TRIGGER_MODE_CONTINUOUS = 0,
	/** @brief @~chinese 软件触发模式 @~english Software Trigger Mode */
	TRIGGER_MODE_SOFT,
	/** @brief @~chinese 硬件触发模式 @~english Hardware Trigger Mode */
	TRIGGER_MODE_HARD,
}emTriggerMode;

/** @brief @~chinese 闪光灯信号控制方式 @~english Strobe signal control method */
typedef enum
{
	/** @brief @~chinese 和触发信号同步，触发后，相机进行曝光时，自动生成STROBE信号。此时，有效极性可设置@link CameraSetStrobePolarity @endlink */
	/** @brief @~english Synchronize with the trigger signal. When triggered, the STROBE signal is automatically generated when the camera is exposed. At this time, the effective polarity can be set @link CameraSetStrobePolarity @endlink */
	STROBE_SYNC_WITH_TRIG_AUTO = 0,
	/** @brief @~chinese 和触发信号同步，触发后，STROBE延时指定的时间后@link CameraSetStrobeDelayTime @endlink，再持续指定时间的脉冲@link CameraSetStrobePulseWidth @endlink，有效极性可设置@link CameraSetStrobePolarity @endlink */
	/** @brief @~english Synchronize with the trigger signal. After the trigger, STROBE delays the specified time @link CameraSetStrobeDelayTime @endlink, and then continues the pulse of the specified time @link CameraSetStrobePulseWidth @endlink, the effective polarity can be set @link CameraSetStrobePolarity @endlink */
	STROBE_SYNC_WITH_TRIG_MANUAL,
}emStrobeControl;

/** @brief @~chinese 硬件外触发的信号种类 @~english Type of signal triggered by hardware */
typedef enum
{
	/** @brief @~chinese 上升沿触发，默认为该方式 @~english Rising edge trigger, defaults to this mode */
	EXT_TRIG_LEADING_EDGE = 0,
	/** @brief @~chinese 下降沿触发 @~english Falling edge trigger */
	EXT_TRIG_TRAILING_EDGE,
	/** @brief @~chinese 高电平触发,电平宽度决定曝光时间，仅部分型号的相机支持电平触发方式 @~english High level trigger, level width determines exposure time, only some models support level trigger mode */
	EXT_TRIG_HIGH_LEVEL,
	/** @brief @~chinese 低电平触发 @~english Low level trigger */
	EXT_TRIG_LOW_LEVEL,
	/** @brief @~chinese 双边沿触发 @~english Bilateral edge trigger */
	EXT_TRIG_DOUBLE_EDGE,
}emExtTrigSignal;

/** @brief @~chinese 图像镜像显示方式 @~english Image mirroring mode */
typedef enum
{
	/** @brief @~chinese 水平镜像 @~english horizontal mirroring */
	MIRROR_MODE_H = 0,
	/** @brief @~chinese 垂直镜像 @~english Vertical Mirror */
	MIRROR_MODE_V = 1,
}emMirrorMode;

/** @brief @~chinese SDK内部显示接口的显示方式 @~english SDK internal display interface display mode */
typedef enum
{
	/** @brief @~chinese 缩放显示模式，缩放到显示控件的尺寸 @~english Zoom display mode, zoom to display control size */
	DISPLAYMODE_SCALE = 0,
	/** @brief @~chinese 1显示模式，当图像尺寸大于显示控件的尺寸时，只显示局部 @~english 1:1 display mode, when the image size is larger than the size of the display control, only the partial */
	DISPLAYMODE_REAL,
}emSdkDisplayMode;

/** @brief @~chinese 自动曝光模式 @~english Auto Exposure Mode */
typedef enum
{
	/** @brief @~chinese 自动曝光帧率优先模式 @~english Auto exposure frame rate priority mode */
	AE_EXP_MODE = 0,
	/** @brief @~chinese 自动曝光曝光优先模式 @~english AE exposure priority mode */
	AE_FRAME_MODE,
}emAeExpMode;

/** @brief @~chinese 设备属性设置对话框显示的TAB页面的掩码 @~english The mask of the TAB page displayed in the device property settings dialog */
typedef enum
{
	/** @brief @~chinese 设备信息配置页面 @~english Device Information Configuration Page */
	SETTING_PAGE_DEVINFO = 0x01,
	/** @brief @~chinese 图像参数配置页面 @~english Image Parameter Configuration Page */
	SETTING_PAGE_IMG_SETTING = 0x02,
	/** @brief @~chinese 曝光配置页面 @~english Exposure configuration page */
	SETTING_PAGE_EXP_SETTING = 0x04,
	/** @brief @~chinese 白平衡配置页面 @~english White Balance Configuration Page */
	SETTING_PAGE_AWB_SETTING = 0x08,
	/** @brief @~chinese 十字线配置页面 @~english Crosshair configuration page */
	SETTING_PAGE_CROSS_LINE = 0x10,
	/** @brief @~chinese 视频输出配置页面 @~english Video Output Configuration Page */
	SETTING_PAGE_OUT_SETTING = 0x20,
	/** @brief @~chinese 分辨率配置页面 @~english Resolution Configuration Page */
	SETTING_PAGE_REL_SETTING = 0x40,
	/** @brief @~chinese 伽玛配置页面 @~english gamma configuration interface */
	SETTING_PAGE_GAMMA_SETTING = 0x80,
	/** @brief @~chinese GigE相机配置页面，只有GigE相机有效 @~english GigE camera configuration page, only GigE camera is valid */
	SETTING_PAGE_GIGE_SETTING = 0x100,
	/** @brief @~chinese 显示所有的属性页 @~english Show all property pages */
	SETTING_PAGE_ALL = 0xFFFFFF,
}emSettingPage;



/** @brief @~chinese LUT的颜色通道 @~english LUT color channel */
typedef enum
{
	/** @brief @~chinese R,B,G三通道同时调节 @~english R, B, G three channels simultaneously adjust */
	LUT_CHANNEL_ALL = 0,
	/** @brief @~chinese 红色通道 @~english Red channel */
	LUT_CHANNEL_RED,
	/** @brief @~chinese 绿色通道 @~english Green channel */
	LUT_CHANNEL_GREEN,
	/** @brief @~chinese 蓝色通道 @~english Blue channel */
	LUT_CHANNEL_BLUE,
}emSdkLutChannel;

/** @brief @~chinese LUT的模式 @~english LUT mode */
typedef enum
{
	/** @brief @~chinese 动态模式 @~english Dynamic mode */
	GAMMA_DYNAMIC_MODE = 0,
	/** @brief @~chinese 预置模式 @~english Preset mode */
	GAMMA_PRESET_MODE,
	/** @brief @~chinese 自定义模式 @~english Custom mode */
	GAMMA_USER_MODE,
}emSdkLutMode;

/** @brief @~chinese IO输出模式 @~english IO output mode */
typedef enum
{
	/** @brief @~chinese 闪光灯输出 @~english Strobe output */
	IOMODE_STROBE_OUTPUT = 0,
	/** @brief @~chinese 通用输出 @~english General output */
	IOMODE_GP_OUTPUT,
	/** @brief @~chinese PWM输出 @~english PWM output */
	IOMODE_PWM_OUTPUT,
}emSdkOutputIOMode;

/** @brief @~chinese IO输入模式 @~english IO input mode*/
typedef enum
{
	/** @brief @~chinese 触发输入 @~english Trigger input */
	IOMODE_TRIG_INPUT = 0,
	/** @brief @~chinese 通用输入 @~english General input */
	IOMODE_GP_INPUT,
}emSdkInputIOMode;

/** @} end of __CK_ENUM__ */

/** @ingroup __CK_IMAGE_FORMAT_DEFINE__
*@{
*/
#define CAMERA_MEDIA_TYPE_MONO                           0x01000000
#define CAMERA_MEDIA_TYPE_RGB                            0x02000000
#define CAMERA_MEDIA_TYPE_COLOR                          0x02000000
#define CAMERA_MEDIA_TYPE_OCCUPY1BIT                     0x00010000
#define CAMERA_MEDIA_TYPE_OCCUPY2BIT                     0x00020000
#define CAMERA_MEDIA_TYPE_OCCUPY4BIT                     0x00040000
#define CAMERA_MEDIA_TYPE_OCCUPY8BIT                     0x00080000
#define CAMERA_MEDIA_TYPE_OCCUPY10BIT                    0x000A0000
#define CAMERA_MEDIA_TYPE_OCCUPY12BIT                    0x000C0000
#define CAMERA_MEDIA_TYPE_OCCUPY16BIT                    0x00100000
#define CAMERA_MEDIA_TYPE_OCCUPY24BIT                    0x00180000
#define CAMERA_MEDIA_TYPE_OCCUPY32BIT                    0x00200000

#define CAMERA_FORMAT_BAYGR8							 0x00000008
#define CAMERA_FORMAT_BAYGR12							 0x00000010
#define CAMERA_FORMAT_RGB								 0x00000014
#define CAMERA_FORMAT_BGR								 0x00000015
#define CAMERA_FORMAT_MONO								 0x00000000
					 

#define CAMERA_MEDIA_TYPE_COLOR_MASK                     0xFF000000
#define CAMERA_MEDIA_TYPE_EFFECTIVE_PIXEL_SIZE_MASK      0x00FF0000
#define CAMERA_MEDIA_TYPE_ID_MASK                        0x0000FFFF
#define CAMERA_MEDIA_TYPE_MEDIA_FORMAT					 0x000000FF
/*Bayer */
#define CAMERA_MEDIA_TYPE_BAYGR8             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY8BIT | 0x0008)
#define CAMERA_MEDIA_TYPE_BAYRG8             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY8BIT | 0x0009)
#define CAMERA_MEDIA_TYPE_BAYGB8             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY8BIT | 0x000A)
#define CAMERA_MEDIA_TYPE_BAYBG8             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY8BIT | 0x000B)

#define CAMERA_MEDIA_TYPE_BAYGR10            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x000C)
#define CAMERA_MEDIA_TYPE_BAYRG10            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x000D)
#define CAMERA_MEDIA_TYPE_BAYGB10            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x000E)
#define CAMERA_MEDIA_TYPE_BAYBG10            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x000F)

#define CAMERA_MEDIA_TYPE_BAYGR12            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0010)
#define CAMERA_MEDIA_TYPE_BAYRG12            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0011)
#define CAMERA_MEDIA_TYPE_BAYGB12            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0012)
#define CAMERA_MEDIA_TYPE_BAYBG12            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0013)

#define CAMERA_MEDIA_TYPE_BAYGR10_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0026)
#define CAMERA_MEDIA_TYPE_BAYRG10_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0027)
#define CAMERA_MEDIA_TYPE_BAYGB10_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0028)
#define CAMERA_MEDIA_TYPE_BAYBG10_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0029)

#define CAMERA_MEDIA_TYPE_BAYGR12_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x002A)
#define CAMERA_MEDIA_TYPE_BAYRG12_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x002B)
#define CAMERA_MEDIA_TYPE_BAYGB12_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x002C)
#define CAMERA_MEDIA_TYPE_BAYBG12_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x002D)

#define CAMERA_MEDIA_TYPE_BAYGR16            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x002E)
#define CAMERA_MEDIA_TYPE_BAYRG16            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x002F)
#define CAMERA_MEDIA_TYPE_BAYGB16            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0030)
#define CAMERA_MEDIA_TYPE_BAYBG16            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0031)

/*RGB */
#define CAMERA_MEDIA_TYPE_RGB8               (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY24BIT | CAMERA_FORMAT_RGB)
#define CAMERA_MEDIA_TYPE_BGR8               (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY24BIT | CAMERA_FORMAT_BGR)
#define CAMERA_MEDIA_TYPE_RGBA8              (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY32BIT | CAMERA_FORMAT_RGB)
#define CAMERA_MEDIA_TYPE_BGRA8              (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY32BIT | CAMERA_FORMAT_BGR)

/*MONO*/
#define CAMERA_MEDIA_TYPE_MONO8              (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY8BIT | 0x0000)
#define CAMERA_MEDIA_TYPE_MONO8_SIGNED       (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY8BIT | 0x0002)
#define CAMERA_MEDIA_TYPE_MONO10             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0003)
#define CAMERA_MEDIA_TYPE_MONO10_PACKED      (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0004)
#define CAMERA_MEDIA_TYPE_MONO12             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0005)
#define CAMERA_MEDIA_TYPE_MONO12_PACKED      (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0006)
#define CAMERA_MEDIA_TYPE_MONO14             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0025)
#define CAMERA_MEDIA_TYPE_MONO16             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0007)
/** 
*@} end of __CK_IMAGE_FORMAT_DEFINE__
*/

#pragma pack(push, 8)

/** @brief @~chinese 曝光参数 @~english Exposure parameters */
typedef struct _tIspAe
{
	/** @brief @~chinese 在支持自动曝光功能的前提下，默认使能 @~english Enables auto exposure, it is enabled by default */
	int bAutoExposure;
	uint32_t res1;
	/** @brief @~chinese 在支持手动曝光功能的前提下设置 @~english Set with support for manual exposure */
	double ExposureTime;
	/** @brief @~chinese 在支持手动曝光功能的前提下设置 @~english Set with support for manual exposure */
	uint32_t AnalogGain;
	/** @brief @~chinese AE目标亮度 @~english AE target brightness */
	uint32_t AeTarget;
	/** @brief @~chinese AE自动曝光的模式，AE_FRAME_MODE帧率优先，AE_EXP_MODE曝光优先 @~english AE auto exposure mode, AE_FRAME_MODE frame rate priority, AE_EXP_MODE exposure priority */
	int  AeMode;
	/** @brief @~chinese 自动模式下的最小曝光时间(微秒) @~english Minimum exposure time in auto mode, us */
	uint32_t res2;
	double fMinExposureTime;
	/** @brief @~chinese 自动模式下的最大曝光时间(微秒) @~english Maximum exposure time in auto mode, us */
	double fMaxExposureTime;
	/** @brief @~chinese 自动模式下的最小模拟增益 @~english Minimum analog gain in automatic mode */
	int iMinAnalogGain;
	/** @brief @~chinese 自动模式下的最大模拟增益 @~english Maximum analog gain in automatic mode */
	int iMaxAnalogGain;
	/** @brief @~chinese 在支持自动曝光功能的前提下，默认关闭 @~english Turns off by default on the premise of auto-exposure support */
	int bAntiFlick;
	/** @brief @~chinese 频闪模式 @~english Flick mode */
	uint32_t FrequencySel;
	/** @brief @~chinese 设置窗口左 @~english Set window left */
	int Left;
	/** @brief @~chinese 设置窗口上 @~english Setting window */
	int Top;
	/** @brief @~chinese 设置窗口宽度 @~english Set window width */
	int Width;
	/** @brief @~chinese 设置窗口高度 @~english Set window height */
	int Height;
}tIspAe;

/** @brief @~chinese 白平衡参数 @~english White balance parameter */
typedef struct _tIspWb
{
	/** @brief @~chinese 在支持自动白平衡功能的前提下，默认关闭 @~english Turns off by default on the premise of auto white balance support */
	int bAutoWb;
	/** @brief @~chinese 红色数字增益 @~english Red digital gain */
	uint32_t DGainR;
	/** @brief @~chinese 绿色数字增益 @~english Green digital gain */
	uint32_t DGainG;
	/** @brief @~chinese 蓝色数字增益 @~english Blue digital gain */
	uint32_t DGainB;
	/** @brief @~chinese 设置窗口左坐标 @~english Set the left coordinate of the window */
	int Left;
	/** @brief @~chinese 设置窗口上坐标 @~english Set the coordinates on the window */
	int Top;
	/** @brief @~chinese 设置窗口宽度 @~english Window width */
	int Width;
	/** @brief @~chinese 设置窗口高度 @~english Window height */
	int Height;
	/** @brief @~chinese 色彩空间 @~english Color space */
	int CCM[9];
}tIspWb;

/** @brief @~chinese Gama参数 @~english Gama parameter */
typedef struct _tIspGamma
{
	uint32_t Mode;
	/** @brief @~chinese Gamma值 @~english gamma value */
	uint32_t Gamma;
	/** @brief @~chinese LUT的对比度值 @~english LUT contrast value */
	uint32_t GammaContrast;
	uint32_t PresetSel;
	int rev[5];					//保留参数
}tIspGamma;

/** @brief @~chinese 设备加载模式信息 @~english Device loading mode information */
typedef struct _tDevLoadMode
{
	/** @brief @~chinese AE参数 @~english AE parameters */
	tIspAe sIspAe;
	/** @brief @~chinese 白平衡参数 @~english White balance parameter */
	tIspWb sIspWb;
	/** @brief @~chinese 触发模式 @~english Trigger mode */
	uint32_t TriggerMode;
	/** @brief @~chinese 分辨率 @~english Resolution */
	uint32_t ResolutionMode;
	/** @brief @~chinese 输出格式 @~english Output format */
	uint32_t MediaTypeMode;
	/** @brief @~chinese 帧速度 @~english Frame rate */
	uint32_t FrameSpeedMode;
	/** @brief @~chinese 锐度 @~english Sharpness */
	uint32_t Sharpness;
	/** @brief @~chinese 饱和度 @~english Saturation */
	uint32_t Saturation;
	/** @brief @~chinese 对比度 @~english Contrast */
	uint32_t Contrast;
	/** @brief @~chinese Gamma参数 @~english Gamma parameters */
	tIspGamma sIspGamma;
	/** @brief @~chinese 参数的模式 @~english Pattern of parameter */
	uint32_t ParameterMode;
	/** @brief @~chinese 参数的组别 @~english Group of parameters */
	uint32_t ParameterTeam;
	/** @brief @~chinese 水平镜像使能 @~english Horizontal Mirroring Enable */
	uint32_t MirrorHEn;
	/** @brief @~chinese 垂直镜像使能 @~english Vertical Mirror Enable */
	uint32_t MirrorVEn;
	/** @brief @~chinese 彩色转灰度图 @~english Color to gray image */
	uint32_t ColorToGrayEn;
	/** @brief @~chinese 黑电平 @~english Black level */
	uint32_t BlackLevel;
	/**@brief @~chinese 保留字节 @~english 保留字节 */
	uint32_t rev;
} tDevLoadMode;

/** @brief @~chinese @~english Camera exposure function range definition */
typedef struct _tExpose
{
	/** @brief @~chinese 自动曝光亮度目标最小值 @~english Automatic exposure brightness target minimum */
	uint32_t  uiTargetMin;
	/** @brief @~chinese 自动曝光亮度目标最大值 @~english Auto exposure brightness target maximum */
	uint32_t  uiTargetMax;
	/** @brief @~chinese 模拟增益的最小值，单位为倍数，再扩大1000倍 @~english The minimum value of the analog gain in multiples, then 1000 times larger */
	uint32_t  uiAnalogGainMin;
	/** @brief @~chinese 模拟增益的最小值，单位为倍数，再扩大1000倍 @~english The maximum value of the analog gain in multiples, then 1000 times larger */
	uint32_t  uiAnalogGainMax;
	/** @brief @~chinese 手动模式下，曝光时间的最小值，单位:行。根据CameraGetExposureLineTime可以获得一行对应的时间(微秒),从而得到整帧的曝光时间 @~english The minimum exposure time in manual mode, in units of lines. According to CameraGetExposureLineTime, you can get the corresponding time (microseconds) of a line, so as to get the exposure time of the whole frame */
	uint32_t  uiExposeTimeMin;
	/** @brief @~chinese 手动模式下，曝光时间的最大值，单位:行 @~english Maximum value of exposure time in manual mode, unit: line */
	uint32_t  uiExposeTimeMax;
} tExpose;

/** @brief @~chinese 相机的分辨率设定范围，用于构件UI @~english Camera resolution setting range for widget UI */
typedef struct _tResolutionRange
{
	/** @brief @~chinese 图像最大高度 @~english Maximum height of image */
	int iHeightMax;
	/** @brief @~chinese 图像最小高度 @~english Image minimum height */
	int iHeightMin;
	/** @brief @~chinese 图像最大宽度 @~english Maximum width of image */
	int iWidthMax;
	/** @brief @~chinese 图像最小宽度 @~english Image minimum width */
	int iWidthMin;
	/** @brief @~chinese SKIP模式掩码，为0，表示不支持SKIP 。bit0为1,表示支持SKIP 2x2 ;bit1为1，表示支持SKIP 3x3 @~english SKIP mode mask, 0, indicating that SKIP is not supported. Bit0 is 1, indicating support for SKIP 2x2; bit1 is 1, indicating support for SKIP 3x3.... */
	uint32_t uSkipModeMask;
	/** @brief @~chinese BIN(求和)模式掩码，为0，表示不支持BIN 。bit0为1,表示支持BIN 2x2 ;bit1为1，表示支持BIN 3x3 @~english BIN (sum) mode mask, 0, indicating that BIN is not supported. Bit0 is 1, indicating that BIN 2x2 is supported; bit1 is 1, indicating that BIN 3x3 is supported.... */
	uint32_t uBinSumModeMask;
	/** @brief @~chinese BIN(求均值)模式掩码，为0，表示不支持BIN 。bit0为1,表示支持BIN 2x2 ;bit1为1，表示支持BIN 3x3 @~english BIN (average) mode mask, 0, indicating that BIN is not supported. Bit0 is 1, indicating that BIN 2x2 is supported; bit1 is 1, indicating that BIN 3x3 is supported.... */
	uint32_t uBinAverageModeMask;
	/** @brief @~chinese 硬件重采样的掩码 @~english Hardware resampling mask */
	uint32_t uResampleMask;
} tResolutionRange;

/** @brief @~chinese 触发模式描述 @~english Trigger Mode Description */
typedef struct _tSdkTrigger
{
	/** @brief @~chinese 模式索引号 @~english mode index number */
	int   iIndex;
	/** @brief @~chinese 该模式的描述信息 @~english Description of this mode */
	char acDescription[32];
} tSdkTrigger;

/** @brief @~chinese @~english Camera resolution description */
typedef struct _tSdkImageResolution
{
	/** @brief @~chinese 分辨率@link emResolutionMode @endlink @~english Resolution @link emResolutionMode @endlink */
	int     iIndex;
	/** @brief @~chinese 该分辨率的描述信息。仅预设分辨率时该信息有效。自定义分辨率可忽略该信息 @~english Description of the resolution. This information is valid only when the resolution is preset. Custom resolution ignores this information */
	char acDescription[32];
	/** @brief @~chinese BIN(求和)的模式,范围不能超过tSdkResolutionRange中uBinSumModeMask @~english BIN (sum) mode, the range cannot exceed uBinSumModeMask in tSdkResolutionRange */
	uint32_t    uBinSumMode;
	/** @brief @~chinese BIN(求均值)的模式,范围不能超过tSdkResolutionRange中uBinAverageModeMask @~english BIN (average) mode, the range cannot exceed uBinAverageModeMask in tSdkResolutionRange */
	uint32_t    uBinAverageMode;
	/** @brief @~chinese 是否SKIP的尺寸，为0表示禁止SKIP模式，范围不能超过tSdkResolutionRange中uSkipModeMask @~english Whether the size of SKIP is 0 means that SKIP mode is disabled, the range cannot exceed uSkipModeMask in tSdkResolutionRange */
	uint32_t    uSkipMode;
	/** @brief @~chinese 硬件重采样的掩码 @~english Hardware resampling mask */
	uint32_t    uResampleMask;
	/** @brief @~chinese 采集视场相对于Sensor最大视场左上角的水平偏移，偶数 @~english Captures the horizontal offset of the field of view relative to the upper left corner of the Sensor's maximum field of view, even */
	int     iHOffsetFOV;
	/** @brief @~chinese 采集视场相对于Sensor最大视场左上角的垂直偏移，偶数 @~english Captures the vertical offset of the field of view relative to the upper left corner of the Sensor's maximum field of view, even */
	int     iVOffsetFOV;
	/** @brief @~chinese 采集视场的宽度，必须为偶数 @~english The width of the field of view is collected and must be even */
	int     iWidthFOV;
	/** @brief @~chinese 采集视场的高度，必须为偶数 @~english The height of the field of view must be an even number */
	int     iHeightFOV;
	/** @brief @~chinese 相机最终输出的图像的宽度,偶数,且4字节对齐 @~english The width, even number, and 4-byte alignment of the final output image of the camera */
	int     iWidth;
	/** @brief @~chinese 相机最终输出的图像的高度，偶数 @~english The height of the image that the camera ultimately outputs, even */
	int     iHeight;
	/** @brief @~chinese 硬件缩放的宽度,不需要进行此操作的分辨率，此变量设置为0 @~english The width of the hardware scale, the resolution of this operation is not required, this variable is set to 0. */
	int     iWidthZoomHd;
	/** @brief @~chinese 硬件缩放的高度,不需要进行此操作的分辨率，此变量设置为0 @~english The height of the hardware scaling, the resolution of this operation is not required, this variable is set to 0. */
	int     iHeightZoomHd;
	/** @brief @~chinese 软件缩放的宽度,不需要进行此操作的分辨率，此变量设置为0.偶数,且4字节对齐 @~english The width of the software scaling, the resolution of this operation is not required, this variable is set to 0. Even, and 4 bytes aligned */
	int     iWidthZoomSw;
	/** @brief @~chinese 软件缩放的高度,不需要进行此操作的分辨率，此变量设置为0.偶数 @~english The height of the software zoom, the resolution of this operation is not required, this variable is set to 0. Even */
	int     iHeightZoomSw;
} tSdkImageResolution;

/** @brief @~chinese 相机白平衡色温模式描述信息 @~english Camera white balance color temperature mode description information */
typedef struct _tSdkColorTemperatureDes
{
	/** @brief @~chinese 模式索引号 @~english mode index number */
	int  iIndex;
	/** @brief @~chinese 描述信息 @~english Description information */
	char acDescription[32];
} tSdkColorTemperatureDes;

/** @brief @~chinese 传输分包大小描述(主要是针对网络相机有效) @~english Transfer packet size description (mainly valid for web cameras) */
typedef struct _tSdkPackLength
{
	/** @brief @~chinese 分包大小索引号 @~english Sub-package size index number */
	int  iIndex;
	/** @brief @~chinese 对应的描述信息 @~english Corresponding description information */
	char acDescription[32];
	/** @brief @~chinese 包大小 @~english packet size */
	uint32_t iPackSize;
} tSdkPackLength;

/** @brief @~chinese @~english Preset LUT table description */
typedef struct _tSdkPresetLut
{
	/** @brief @~chinese 索引号 @~english Index number */
	int  iIndex;
	/** @brief @~chinese 描述信息 @~english Description information */
	char acDescription[32];
} tSdkPresetLut;

/** @brief @~chinese AE算法描述 @~english AE algorithm description */
typedef struct _tSdkAeAlgorithm
{
	/** @brief @~chinese 索引号 @~english Index number */
	int  iIndex;
	/** @brief @~chinese 描述信息 @~english Description information */
	char acDescription[32];
} tSdkAeAlgorithm;

/** @brief @~chinese @~english RAW to RGB algorithm description */
typedef struct _tSdkBayerDecodeAlgorithm
{
	/** @brief @~chinese 算法索引号 @~english Algorithm index */
	int  iIndex;
	/** @brief @~chinese 描述信息 @~english Description information */
	char acDescription[32];
} tSdkBayerDecodeAlgorithm;

/** @brief @~chinese @~english Image data format output by the camera */
typedef struct _tSdkMediaType
{
	/** @brief @~chinese 媒体类型索引号 @~english Media type number */
	int iIndex;
	/** @brief @~chinese 描述信息 @~english Description information */
	char acDescription[32];
	/** @brief @~chinese 对应的图像格式编码，如CAMERA_MEDIA_TYPE_BAYGR8，在本文件中有定义。 @~english Corresponding image format encoding, such as CAMERA_MEDIA_TYPE_BAYGR8, is defined in this document. */
	uint32_t iMediaType;
} tSdkMediaType;

/** @brief @~chinese 相机输出的图像数据格式 @~english Image data format output by the camera */
typedef struct _tSdkBayerType
{
	/** @brief @~chinese Bayer种类索引号 @~english Bayer type index */
	int iIndex;
	/** @brief @~chinese 描述信息 @~english Description information */
	char acDescription[32];
	/** @brief @~chinese 对应的sensor支持的格式编码，如CAMERA_MEDIA_TYPE_BAYGR8 @~english The corresponding format encoding supported by sensor, such as CAMERA_MEDIA_TYPE_BAYGR8 */
	uint32_t iMediaType;
} tSdkBayerType;

/** @brief @~chinese 相机帧率描述信息 @~english Camera frame rate description information */
typedef struct _tSdkFrameSpeed
{
	/** @brief @~chinese 帧率索引号，一般0对应于低速模式，1对应于普通模式，2对应于高速模式 @~english Frame rate index number, generally 0 corresponds to low speed mode, 1 corresponds to normal mode, 2 corresponds to high speed mode */
	int iIndex;
	/** @brief @~chinese 描述信息 @~english Description information */
	char acDescription[32];
} tSdkFrameSpeed;

/** @brief @~chinese RGB三通道数字增益的设定范围 @~english RGB three-channel digital gain setting range */
typedef struct _tRgbGainRange
{
	/** @brief @~chinese 红色增益的最小值 @~english Minimum value of red gain */
	int iRGainMin;
	/** @brief @~chinese 红色增益的最大值 @~english Maximum value of red gain */
	int iRGainMax;
	/** @brief @~chinese 绿色增益的最小值 @~english Minimum value of green gain */
	int iGGainMin;
	/** @brief @~chinese 绿色增益的最大值 @~english Maximum value of green gain */
	int iGGainMax;
	/** @brief @~chinese 蓝色增益的最小值 @~english Minimum value of blue gain */
	int iBGainMin;
	/** @brief @~chinese 蓝色增益的最大值 @~english Maximum blue gain */
	int iBGainMax;
} tRgbGainRange;

/** @brief @~chinese 饱和度设定的范围 @~english Range of saturation setting */
typedef struct _tSaturationRange
{
	/** @brief @~chinese 最小值 @~english Minimum value */
	int iMin;
	/** @brief @~chinese 最大值 @~english Maximum value */
	int iMax;
} tSaturationRange;

/** @brief @~chinese 锐化的设定范围 @~english Sharpening setting range */
typedef struct _tSharpnessRange
{
	/** @brief @~chinese 最小值 @~english Minimum value */
	int iMin;
	/** @brief @~chinese 最大值 @~english Maximum value */
	int iMax;
} tSharpnessRange;

/** @brief @~chinese 伽马的设定范围 @~english Gamma setting range */
typedef struct _tGammaRange
{
	/** @brief @~chinese 最小值 @~english Minimum value */
	int iMin;
	/** @brief @~chinese 最大值 @~english Maximum value */
	int iMax;
} tGammaRange;

/** @brief @~chinese 伽马对比度的设定范围 @~english Gamma contrast setting range */
typedef struct _tGammaContrastRange
{
	/** @brief @~chinese 最小值 @~english Minimum value */
	int iMin;
	/** @brief @~chinese 最大值 @~english Maximum value */
	int iMax;
}tGammaContrastRange;

/** @brief @~chinese 对比度的设定范围 @~english Contrast setting range */
typedef struct _tContrastRange
{
	/** @brief @~chinese 最小值 @~english Minimum value */
	int iMin;
	/** @brief @~chinese 最大值 @~english Maximum value */
	int iMax;
} tContrastRange;

/** @brief @~chinese ISP模块的使能信息 @~english ISP module enable information */
typedef struct _tSdkIspCapacity
{
	/** @brief @~chinese 表示该型号相机是否为黑白相机,如果是黑白相机，则颜色相关的功能都无法调节 @~english Indicates whether the camera of this model is a black and white camera. If it is a black and white camera, the color related functions cannot be adjusted */
	int bMonoSensor;
	/** @brief @~chinese 表示该型号相机是否支持一次白平衡功能 @~english Indicates whether this model supports a white balance function */
	int bWbOnce;
	/** @brief @~chinese 表示该型号相机是否支持自动白平衡功能 @~english Indicates whether this model supports auto white balance*/
	int bAutoWb;
	/** @brief @~chinese 表示该型号相机是否支持自动曝光功能 @~english Indicates whether this model supports auto exposure*/
	int bAutoExposure;
	/** @brief @~chinese 表示该型号相机是否支持手动曝光功能 @~english Indicates whether the camera supports manual exposure*/
	int bManualExposure;
	/** @brief @~chinese 表示该型号相机是否支持抗频闪功能 @~english Indicates whether this model camera supports anti-strobe function */
	int bAntiFlick;
	/** @brief @~chinese 表示该型号相机是否支持Gamma功能 @~english indicates whether this model camera supports Gamma function */
	int bIspGamma;
	/** @brief @~chinese 表示该型号相机是否支持硬件ISP功能 @~english Indicates whether this model camera supports hardware ISP function */
	int bDeviceIsp;
	/** @brief @~chinese bDeviceIsp和bForceUseDeviceIsp同时为TRUE时，表示强制只用硬件ISP，不可取消 @~english When both bDeviceIsp and bForceUseDeviceIsp are TRUE, it means that the hardware ISP is mandatory and cannot be canceled. */
	int bForceUseDeviceIsp;
	/** @brief @~chinese 相机硬件是否支持图像缩放输出(只能是缩小) @~english Whether the camera hardware supports image scaling output (can only be reduced). */
	int bZoomHD;
	int rev[4];
} tSdkIspCapacity;

/** @brief @~chinese sensor能力级 @~english sensor level */
typedef struct _tSensorCfg
{
	/** @brief @~chinese 曝光的范围值 @~english Exposure range value */
	tExpose sExposeDesc;
	/** @brief @~chinese 分辨率范围描述 @~english Resolution range description */
	tResolutionRange sResolutionRange;
	/** @brief @~chinese 最大分辨率 @~english Maximum resolution */
	tSdkImageResolution sMaxResolution;
} tSensorCfg;

/** @brief @~chinese @~english Device Capability Level */
typedef struct _tDeviceCfg
{

	/** @brief @~chinese 图像数字增益范围描述 @~english Image Digital Gain Range Description */
	tRgbGainRange sRgbGainRange;
	/** @brief @~chinese 饱和度范围描述 @~english Description of saturation range */
	tSaturationRange sSaturationRange;
	/** @brief @~chinese 伽马范围描述 @~english Gamma range description */
	tGammaRange sGammaRange;
	/** @brief @~chinese 伽马对比度范围描述 @~english Gamma Contrast Range Description */
	tGammaContrastRange sGammaContrastRange;
	/** @brief @~chinese 对比度范围描述 @~english Contrast range description */
	tContrastRange sContrastRange;
	/** @brief @~chinese 锐化范围描述 @~english Sharpening range description */
	tSharpnessRange sSharpnessRange;
	/** @brief @~chinese ISP能力描述 @~english ISP Capability Description */
	tSdkIspCapacity sIspCapacity;

	/** @brief @~chinese 触发模式 @~english trigger mode */
	tSdkTrigger *pTriggerDesc;
	/** @brief @~chinese 触发模式的个数，即pTriggerDesc数组的大小 @~english The number of trigger modes, the size of the pTriggerDesc array */
	int iTriggerDesc;

	/** @brief @~chinese 预设分辨率选择 @~english Preset resolution selection */
	tSdkImageResolution *pImageSizeDesc;
	/** @brief @~chinese 预设分辨率的个数，即pImageSizeDesc数组的大小 @~english The number of preset resolutions, the size of the pImageSizeDesc array */
	int iImageSizeDesc;

	/** @brief @~chinese 预设色温模式，用于白平衡 @~english Preset color temperature mode for white balance */
	tSdkColorTemperatureDes *pClrTempDesc;
	int iClrTempDesc;

	/** @brief @~chinese 相机Bayer格式 @~english Camera Bayer format */
	tSdkBayerType *pBayerTypeDesc;
	/** @brief @~chinese 相机Bayer格式的种类个数，即pBayerTypeDesc数组的大小。 @~english The number of types of camera Bayer format, the size of the pBayerTypeDesc array. */
	int iBayerTypeDesc;

	/** @brief @~chinese 相机输出图像格式 @~english Camera output image format */
	tSdkMediaType *pMediaTypeDesc;
	/** @brief @~chinese 相机输出图像格式的种类个数，即pMediaTypeDesc数组的大小。 @~english The number of types of camera output image formats, the size of the pMediaTypeDesc array. */
	int iMediaTypeDesc;

	/** @brief @~chinese 可调节帧速类型，对应界面上普通高速和超级三种速度设置 @~english Adjustable frame rate type, corresponding to the normal high speed and super speed settings on the interface */
	tSdkFrameSpeed *pFrameSpeedDesc;
	/** @brief @~chinese 可调节帧速类型的个数，即pFrameSpeedDesc数组的大小 @~english Adjusts the number of frame rate types, which is the size of the pFrameSpeedDesc array. */
	int iFrameSpeedDesc;

	/** @brief @~chinese 传输包长度，一般用于网络设备 @~english Transport packet length, generally used for network devices */
	tSdkPackLength *pPackLenDesc;
	/** @brief @~chinese 可供选择的传输分包长度的个数，即pPackLenDesc数组的大小。 @~english The number of transport packet lengths to choose from, which is the size of the pPackLenDesc array. */
	int iPackLenDesc;

	/** @brief @~chinese 可编程输出IO的个数 @~english Number of programmable output IOs */
	int iOutputIoCounts;
	/** @brief @~chinese 可编程输入IO的个数 @~english Number of programmable input IOs */
	int iInputIoCounts;

	/** @brief @~chinese 相机预设的LUT表 @~english Camera preset LUT table */
	tSdkPresetLut *pPresetLutDesc;
	/** @brief @~chinese 相机预设的LUT表的个数，即pPresetLutDesc数组的大小 @~english The number of LUT tables preset by the camera, the size of the pPresetLutDesc array */
	int iPresetLut;

	/** @brief @~chinese 指示该相机中用于保存用户数据区的最大长度。为0表示无 @~english Indicates the maximum length in the camera that will be used to save the user data area. 0 means no. */
	int iUserDataMaxLen;
	/** @brief @~chinese 指示该设备是否支持从设备中读写参数组。1为支持，0不支持 @~english Indicates whether the device supports reading and writing parameter groups from the device. 1 is support, 0 is not supported. */
	int bParamInDevice;

	/** @brief @~chinese 软件自动曝光算法描述 @~english Software Auto Exposure Algorithm Description */
	tSdkAeAlgorithm *pAeAlmSwDesc;
	/** @brief @~chinese 软件自动曝光算法个数 @~english Software automatic exposure algorithm number */
	int iAeAlmSwDesc;

	/** @brief @~chinese 硬件自动曝光算法描述，为NULL表示不支持硬件自动曝光 @~english Hardware auto-exposure algorithm description, NULL means hardware auto-exposure is not supported */
	tSdkAeAlgorithm *pAeAlmHdDesc;
	/** @brief @~chinese 硬件自动曝光算法个数，为0表示不支持硬件自动曝光 @~english Number of hardware auto-exposure algorithms, 0 means hardware auto-exposure is not supported */
	int iAeAlmHdDesc;

	/** @brief @~chinese 软件Bayer转换为RGB数据的算法描述 @~english Software Bayer algorithm for converting to RGB data */
	tSdkBayerDecodeAlgorithm *pBayerDecAlmSwDesc;
	/** @brief @~chinese 软件Bayer转换为RGB数据的算法个数 @~english Software Bayer algorithm for converting to RGB data */
	int iBayerDecAlmSwDesc;

	/** @brief @~chinese 硬件Bayer转换为RGB数据的算法描述，为NULL表示不支持 @~english Algorithm description of hardware Bayer converted to RGB data, not supported for NULL */
	tSdkBayerDecodeAlgorithm *pBayerDecAlmHdDesc;
	/** @brief @~chinese 硬件Bayer转换为RGB数据的算法个数，为0表示不支持 @~english The number of algorithms that hardware Bayer converts to RGB data, 0 means not supported */
	int iBayerDecAlmHdDesc;

} tDeviceCfg;

/** @brief @~chinese 相机的设备信息 @~english Camera device information */
typedef struct _tDevInfo
{
	/** @brief @~chinese 产品系列 @~english Product Line */
	char acProductSeries[32];
	/** @brief @~chinese 产品名称 @~english Product Name */
	char acProductName[32];
	/** @brief @~chinese 产品昵称，用户可自定义改昵称，保存在相机内，用于区分多个相机同时使用,可以用CameraSetFriendlyName接口改变该昵称，设备重启后生效 @~english Product nickname, user can customize the nickname, save it in the camera, used to distinguish multiple cameras at the same time, you can use the CameraSetFriendlyName interface to change the nickname, the device will take effect after restart. */
	char acFriendlyName[32];
	/** @brief @~chinese 内核符号连接名，内部使用 @~english Kernel symbolic link name, internal use */
	char acLinkName[32];
	/** @brief @~chinese 驱动名称 @~english driver name */
	char acDriverName[128];
	/** @brief @~chinese 驱动版本 @~english driver version */
	char acDriverVersion[32];
	/** @brief @~chinese sensor类型@~english sensor type */
	char acSensorType[32];
	/** @brief @~chinese 接口类型 @~english interface type */
	char acPortType[32];
	/** @brief @~chinese 该型号相机在该电脑上的实例索引号，用于区分同型号多相机 @~english The instance index number of this model camera on this computer is used to distinguish the same model multi-camera */
	uint32_t uInstance;
	/** @brief @~chinese 产品唯一序列号 @~english Product unique serial number */
	char acSn[32];
	/** @brief @~chinese 厂商ID @~english Vendor ID */
	uint16_t VendorID;
	/** @brief @~chinese 设备驱动ID @~english Device Driver ID */
	uint16_t DeviceDrvID;
	/** @brief @~chinese 设备驱动版本ID @~english Device Driver Version ID */
	uint16_t DeviceDrvVersionID;
	/** @brief @~chinese 设备ID @see emDeviceType @~english Device ID @see emDeviceType */
	uint32_t DeviceID;
	/** @brief @~chinese 设备标号，内部使用 @~english device label, internal use */
	char SymbolicName[128];
	/** @brief @~chinese 设备标号，内部使用 @~english device label, internal use */
	char Name[64];
	/** @brief Sensor ID */
	uint32_t SensorID;
} tDevInfo;

/** @brief @~chinese 帧头信息 @~english Frame header information */
typedef struct _stImageInfo
{
	/** @brief @~chinese 当前图像宽 @~english Current image width */
	uint32_t iWidth;
	/** @brief @~chinese 当期图像高 @~english Current image height */
	uint32_t iHeight;
	/** @brief @~chinese 图像数据总字节数 @~english Image data bytes, Total bytes */
	uint32_t TotalBytes;
	/** @brief @~chinese 图像格式 @~english Image format */
	uint32_t uiMediaType;
	/** @brief @~chinese 当前图像的曝光时间，单位为微秒 @~english The exposure time of the current image in microseconds us */
	double ExpTime;
	/** @brief @~chinese 当前图像的行曝光时间，单位为微秒 @~english Line exposure time of the current image in microseconds us */
	double ExpLineTime;
	/** @brief @~chinese 当前图像的增益倍数 @~english Gain multiple of the current image */
	uint32_t Gain;
}stImageInfo, *PstImageInfo;

/** @brief @~chinese 帧率统计信息 @~english Frame rate statistics */
typedef struct _FrameStatistic
{
	/** @brief @~chinese 当前采集的总帧数（包括错误帧） @~english Total number of frames currently captured (including error frames) */
	uint32_t iTotal;
	/** @brief @~chinese 当前采集的有效帧的数量 @~english The number of valid frames currently being collected */
	uint32_t iCapture;
	/** @brief @~chinese 当前丢帧的数量 @~english The current number of dropped frames */
	int iLost;
} FrameStatistic;

/** @brief @~chinese 相机能力级 @~english Camera Ability Level */
typedef struct _tSdkCameraCapbility
{
	/** @brief @~chinese 设备能力级 @~english Device Capability Level */
	tDeviceCfg tDeviceCapbility;
	/** @brief @~chinese sensor能力级 @~english sensor level */
	tSensorCfg tSensorCapbility;

} tSdkCameraCapbility;

/** @brief @~chinese 显示窗口的参数 @~english Display window parameters */
typedef struct _DisplayWindow
{
	/** @brief @~chinese 图像宽度 @~english Image width */
	uint32_t ImageWidth;
	/** @brief @~chinese 图像高度 @~english Image height */
	uint32_t ImageHeight;
	/** @brief @~chinese 显示窗口矩形 @~english Display window rectangle */
	RECT WinRect;
	/** @brief @~chinese 有效图像矩形框 @~english Effective image rectangle */
	RECT ImageRect;
}DisplayWindow;

/** @brief @~chinese 枚举信息 @~english Enumeration information */
typedef struct _tDevEnumInfo
{
	/** @brief @~chinese 设备属性信息 @~english Device Attribute Information */
	tDevInfo DevAttribute;
	/** @brief @~chinese 当前设备是否打开 0:设备没有打开;1:设备已经打开 @~english Whether the current device is turned on 0: The device is not turned on; 1: The device is already turned on */
	int bDevOpen;
} tDevEnumInfo;

/** @brief @~chinese 设备的当前模式和枚举信息 @~english Current mode and enumeration information for the device */
typedef struct _tDevLoadInfo
{
	/** @brief @~chinese 枚举信息 @~english Enumeration information */
	tDevEnumInfo DevEnumInfo;
	/** @brief @~chinese 模式信息 @~english Device mode information */
	tDevLoadMode DevLoadMode;
} tDevLoadInfo;

/** @brief @~chinese GigE相机网络参数 @~english Network information of GigE camera */
typedef struct _tGigeNetworkInfo
{
	/** @brief @~chinese 本地网卡名称 @~english local NIC name */
	char acLocalNetCardName[64];
	/** @brief @~chinese 本地网卡IP地址 @~english local NIC IP */
	char acLocalIp[16];
	/** @brief @~chinese 本地网卡子网掩码 @~english local NIC submask */
	char acLocalNetmask[16];
	/** @brief @~chinese 本地网卡网关 @~english local NIC gateway */
	char acLocalGateway[16];
	/** @brief @~chinese GigE相机是否使用固定IP地址 @~english Does the GigE camera use a fixed IP address? */
	BOOL isPersistent;
	/** @brief @~chinese Gige相机IP地址 @~english GigE camera NIC IP */
	char acCameraIp[16];
	/** @brief @~chinese Gige相机网卡子网掩码 @~english GigE camera NIC submask */
	char acCameraNetmask[16];
	/** @brief @~chinese Gige相机网卡网关 @~english GigE camera NIC gateway */
	char acCameraGateway[16];
	/** @brief @~chinese Gige相机网卡MAC地址 @~english GigE camera NIC MAC address */
	BYTE byMac[6];
}tGigeNetworkInfo;

#pragma pack(pop)

/**
*@ingroup __CK_GET_IMAGE_BUFFER__
*@brief @~chinese 图像捕获的回调函数定义 @~english Callback function definition for image capture */
typedef void (WINAPI* CAMERA_SNAP_PROC)(HANDLE handle, BYTE *pFrameBuffer, stImageInfo* pFrameInfo, LPVOID lpParam);

/**
*@ingroup __CK_DISPLAY__
*@brief @~chinese 图像显示的回调函数 @~english Image display callback function */
typedef void (WINAPI* CAMERA_DISPLAY_PROC)(HANDLE handle, HDC hdc, DisplayWindow *pDisplayWindow, LPVOID lpParam);



#endif
