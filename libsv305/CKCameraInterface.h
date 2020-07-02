
#ifndef __CK_CAMERA_INTERFACE_H__
#define __CK_CAMERA_INTERFACE_H__

#include "CKDeviceDef.h"

#ifdef _WINDOWS

#ifdef CKCAMERADLL_EXPORTS
#define CKSDK_API _declspec(dllexport)
#elif defined(CKCAMERALIB)
#define CKSDK_API
#else
#define CKSDK_API _declspec(dllimport)
#endif

#else

#define CKSDK_API __attribute__((visibility("default")))

#endif

typedef int CameraSdkStatus;


#ifdef __cplusplus
extern "C" {
#endif

	/**
	 *@ingroup __CK_DEVICE_ENUM__
	 *@~chinese
	 *@brief 枚举设备，并建立设备列表。在调用CameraInit 之前，必须调用该函数来获得设备的信息。    
	 *@param[out] pDeviceNum 设备的个数指针，函数返回时，保存实际找到的设备个数。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 通过该函数获得当前查询到的所有设备，返回实际找到的设备个数
	 *@~english
	 *@brief enumerates devices and builds a list of devices. This function must be called to get information about the device before calling CameraInit.
	 *@param[out] pDeviceNum The number of devices in the pDeviceNum device. When the function returns, the number of devices actually found is saved.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note Use this function to get all the devices currently queried, and return the number of devices actually found.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraEnumerateDevice(INT *pDeviceNum);

	CKSDK_API CameraSdkStatus __stdcall CameraEnumerateDeviceFromOpenedDevList(int *pDeviceNum, int DevNum, int fds[], int pids[], char *paths[]);
	/**
	 *@ingroup __CK_DEVICE_ENUM__
	 *@~chinese
	 *@brief 获得指定设备的信息
	 *@param[in] CameraIndex 相机标号，通过@link #CameraEnumerateDevice @endlink获得，选取(0-DeviceNum-1)
	 *@param[out] pDevInfo 指针，返回设备的信息。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 该函数通过 @link #CameraEnumerateDevice @endlink 获得设备列表的个数，确定相机
	 标号，从而查询相应设备信息， 描述信息以结构体的方式返回，比如
	 相机的名称、型号、序列号、接口类型等都会详细列出。
	 *@~english
	 *@brief Get information about the specified device
	 *@param[in] CameraIndex camera label, obtained by @link #CameraEnumerateDevice @endlink, select (0-DeviceNum-1)
	 *@param[out] pDevInfo Pointer, which returns information about the device.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note This function gets the number of device lists via @link #CameraEnumerateDevice @endlink, determines the camera
	 label to query the corresponding device information, the description information is returned in a structured manner, such as
	 The camera name, model number, serial number, interface type, etc. will be listed in detail.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetEnumIndexInfo(INT CameraIndex, tDevEnumInfo* pDevInfo);

	/**
	 *@ingroup __CK_DEVICE_OPEN_CLOSE__
	 *@~chinese
	 *@brief 相机初始化。初始化成功后，才能调用任何其他
	 相机相关的操作接口,默认使用上次退出时的参数。注意需要先调用@link #CameraEnumerateDevice @endlink枚举相机    
	 *@param[out] phCamera  相机的句柄指针，初始化成功后，该指针
	 返回该相机的有效句柄，在调用其他相机
	 相关的操作接口时，都需要传入该句柄，主要
	 用于多相机之间的区分。
	 *@param[in] CameraIndex 相机标号，通过@link #CameraEnumerateDevice @endlink获得
	 设备个数，从而设置要打开的相机标号，选取(0-DeviceNum-1)
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraEnumerateDevice
	 *@note CameraInit 只要传入相机的序号即可，例如初始化第 1 个相机，传入 0，第二个相机传入 1，以此类推
	 *@~english
	 *@brief Camera initialization. After the initialization is successful, you can call any other
	 The camera-related operation interface defaults to the last time the parameter was exited. Note that you need to call @link #CameraEnumerateDevice @endlink to enumerate the camera first.
	 *@param[out] phCamera The handle pointer of the camera. After the initialization is successful, the pointer
	 Return a valid handle to the camera when calling another camera
	 When the relevant operation interface is used, the handle needs to be passed in, mainly
	 Used for distinguishing between multiple cameras.
	 *@param[in] CameraIndex camera label, obtained by @link #CameraEnumerateDevice @endlink
	 Number of devices, thus setting the camera label to be opened, select (0-DeviceNum-1)
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraEnumerateDevice
	 *@note CameraInit Just pass the camera's serial number, for example, initialize the first camera, pass in 0, the second camera will pass in 1, and so on.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraInit(PHANDLE phCamera, INT CameraIndex);

	/**
	 *@ingroup __CK_DEVICE_OPEN_CLOSE__
	 *@~chinese
	 *@brief 相机初始化。初始化成功后，才能调用任何其他
	 相机相关的操作接口。注意需要先调用@link #CameraEnumerateDevice @endlink枚举相机	
	 *@param[out] phCamera  相机的句柄指针，初始化成功后，该指针
	 返回该相机的有效句柄，在调用其他相机
	 相关的操作接口时，都需要传入该句柄，主要用于多相机之间的区分。
	 *@param[in] CameraIndex 相机标号，通过@link #CameraEnumerateDevice @endlink获得
	 设备个数，从而设置要打开的相机标号，选取(0-DeviceNum-1)
	 *@param[in] iParamLoadMode  相机初始化时使用的参数加载方式。-1表示使用上一次退出时的保存的加载方式。
	 为 @link #emSdkParameterMode::PARAM_MODE_BY_MODEL @endlink 表示按型号加载;
	 为 @link #emSdkParameterMode::PARAM_MODE_BY_SN @endlink 表示按序列号加载;
	 为 @link #emSdkParameterMode::PARAM_MODE_BY_NAME @endlink 表示按昵称加载.
	 *@param[in] emTeam 初始化时使用的参数组。-1表示使用上一次退出时保存的参数组,通过iParamLoadMode和emTeam查找需要加载参数的文件
	 ,若没有找到对应的文件，按默认加载方式加载。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraEnumerateDevice
	 *@note CameraInitEx 不仅要传入相机的序号 ID ，而且还要传入参数加
	 载模式和组别，初始化加载相应参数，当iParamLoadMode和emTeam都位-1时，使用上一次退出时保存的参数组,通过iParamLoadMode
	 和emTeam查找需要加载参数的文件,若没有找到对应的文件，按默认加载方式加载。
	 *@~english
	 *@brief Camera initialization. After the initialization is successful, you can call any other
	 Camera related operation interface. Note that you need to call @link #CameraEnumerateDevice @endlink to enumerate the camera first.
	 *@param[out] phCamera The handle pointer of the camera. After the initialization is successful, the pointer
	 Return a valid handle to the camera when calling another camera
	 When the relevant operation interface is used, the handle needs to be passed in, which is mainly used for distinguishing between multiple cameras.
	 *@param[in] CameraIndex camera label, obtained by @link #CameraEnumerateDevice @endlink
	 Number of devices, thus setting the camera label to be opened, select (0-DeviceNum-1)
	 *@param[in] iParamLoadMode The parameter loading method used when the camera is initialized. -1 indicates the saved loading method when using the last exit.
	 For @link #emSdkParameterMode::PARAM_MODE_BY_MODEL @endlink means to load by model;
	 For @link #emSdkParameterMode::PARAM_MODE_BY_SN @endlink means to load by serial number;
	 For @link #emSdkParameterMode::PARAM_MODE_BY_NAME @endlink means to load by nickname.
	 *@param[in] emTeam The parameter group used by emTeam for initialization. -1 means to use the parameter group saved on the last exit, find the file that needs to load parameters through iParamLoadMode and emTeam
	 If the corresponding file is not found, it is loaded by default loading method.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraEnumerateDevice
	 *@note CameraInitEx not only to pass the camera's serial number ID, but also to pass in the parameter plus
	 Load mode and group, initialize the corresponding parameters, when iParamLoadMode and emTeam are both -1, use the parameter group saved on the last exit, through iParamLoadMode
	 and emTeam finds the file that needs to load the parameter. If the corresponding file is not found, it is loaded by default loading method.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraInitEx(PHANDLE phCamera, INT CameraIndex, INT iParamLoadMode, INT emTeam);

	/**
	 *@ingroup __CK_DEVICE_OPEN_CLOSE__
	 *@~chinese
	 *@brief 相机初始化。初始化成功后，才能调用任何其他
	 相机相关的操作接口。默认使用上次退出时的参数，注意需要先调用@link #CameraEnumerateDevice @endlink枚举相机
	 *@param[in] phCamera 相机的句柄指针，初始化成功后，该指针
	 返回该相机的有效句柄，在调用其他相机
	 相关的操作接口时，都需要传入该句柄，主要
	 用于多相机之间的区分。
	 *@param[in] pFriendlyName  相机昵称，用于找到相同昵称的相机，字节长度最大为32个字节
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraEnumerateDevice
	 *@see CameraGetEnumIndexInfo
	 *@note CameraInitEx2 不需要传入相机的序列 ID，只需要传入相机的名
	 称 ， 由于出厂时相机的昵称都是一样的，需要事先通过
	 @link #CameraSetFriendlyName @endlink 函数将相机名称改如"MyCamera1"，
	 CameraInitEx2 调用时，传入"MyCamera1"即可，对于多相机同时工作
	 的案例，这个方法可以有效的建立一一对应的关系，但是要注意，每
	 个相机的名称必须改成不同的，确保唯一性。
	 *@~english
	 *@brief Camera initialization. After the initialization is successful, you can call any other
	 Camera related operation interface. By default, the parameters used at the last exit are used. Note that you need to call @link #CameraEnumerateDevice @endlink to enumerate the camera first.
	 *@param[in] phCamera The handle pointer of the camera. After the initialization is successful, the pointer
	 Return a valid handle to the camera when calling another camera
	 When the relevant operation interface is used, the handle needs to be passed in, mainly
	 Used for distinguishing between multiple cameras.
	 *@param[in] pFriendlyName Camera nickname, used to find cameras with the same nickname, up to 32 bytes in length
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraEnumerateDevice
	 *@see CameraGetEnumIndexInfo
	 *@note CameraInitEx2 does not need to pass the camera's serial ID, just the name of the camera
	 Said that since the camera's nickname is the same at the factory, it needs to pass in advance.
	 @link #CameraSetFriendlyName @endlink function changes the camera name to "MyCamera1",
	 When CameraInitEx2 is called, you can pass "MyCamera1" and work for multiple cameras at the same time.
	 Case, this method can effectively establish one-to-one correspondence, but pay attention to each
	 The names of the cameras must be changed to be different to ensure uniqueness.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraInitEx2(PHANDLE phCamera, const char *pFriendlyName);

	/**
	 *@ingroup __CK_DEVICE_OPEN_CLOSE__
	 *@~chinese
	 *@brief 相机初始化。初始化成功后，才能调用任何其他
	 相机相关的操作接口。默认使用上次退出时的参数，注意需要先调用@link #CameraEnumerateDevice @endlink枚举相机
	 *@param[out] phCamera  相机的句柄指针，初始化成功后，该指针
	 返回该相机的有效句柄，在调用其他相机
	 相关的操作接口时，都需要传入该句柄，主要
	 用于多相机之间的区分。
	 *@param[in] pCameraSN	相机SN码，用于找到SN相同的相机，而且是唯一相机，字节长度最大为32个字节
	 可以通过调用@link #CameraGetEnumIndexInfo @endlink获得相应SN码的信息
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraEnumerateDevice
	 *@see CameraGetEnumIndexInfo
	 *@note CameraInitEx3 不需要传入相机的序列 ID，只需要传入相机的 SN
	 码，可以通过调用 @link #CameraGetEnumIndexInfo @endlink获取，对于多相机同时工作的案
	 例，这个方法可以有效的建立一一对应的关系，确保唯一性。
	 *@~english
	 *@brief Camera initialization. After the initialization is successful, you can call any other
	 Camera related operation interface. By default, the parameters used at the last exit are used. Note that you need to call @link #CameraEnumerateDevice @endlink to enumerate the camera first.
	 *@param[out] phCamera The handle pointer of the camera. After the initialization is successful, the pointer
	 Return a valid handle to the camera when calling another camera
	 When the relevant operation interface is used, the handle needs to be passed in, mainly
	 Used for distinguishing between multiple cameras.
	 *@param[in] pCameraSN camera SN code, used to find the same camera as SN, and is the only camera with a maximum byte length of 32 bytes
	 You can get the information of the corresponding SN code by calling @link #CameraGetEnumIndexInfo @endlink.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraEnumerateDevice
	 *@see CameraGetEnumIndexInfo
	 *@note CameraInitEx3 does not need to pass in the camera's sequence ID, only need to pass the camera's SN
	 Code, which can be obtained by calling @link #CameraGetEnumIndexInfo @endlink, for multiple cameras working simultaneously
	 For example, this method can effectively establish a one-to-one correspondence to ensure uniqueness.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraInitEx3(PHANDLE phCamera, const char *pCameraSN);

	/**
	 *@ingroup __CK_DEVICE_OPEN_CLOSE__
	 *@~chinese
	 *@brief 相机反初始化。释放资源。
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraInit
	 *@see CameraInitEx
	 *@see CameraInitEx2
	 *@see CameraInitEx3
	 *@note 无论采用哪种初始化方式，反初始化调用 CameraUnInit 即可。
	 *@~english
	 *@brief Camera reverse initialization. Release resources.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraInit
	 *@see CameraInitEx
	 *@see CameraInitEx2
	 *@see CameraInitEx3
	 *@note Regardless of the initialization method used, the inverse initialization calls CameraUnInit.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraUnInit(HANDLE hCamera);

	/**
	 *@ingroup __CK_DEVICE_INFO__
	 *@~chinese
	 *@brief 获得相机的特性描述结构体。该结构体中包含了相机
	 可设置的各种参数的范围信息。决定了相关函数的参数
	 返回，也可用于动态创建相机的配置界面。
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pCameraCap 指针，返回该相机特性描述的结构体。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the camera's characterization structure. The structure contains a camera
	 Range information for various parameters that can be set. Determine the parameters of the relevant function
	 Back, can also be used to dynamically create a camera's configuration interface.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pCameraCap The pointer returns the structure described by the camera's characterization.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetCapability(HANDLE hCamera, tSdkCameraCapbility* pCameraCap);

	/**
	 *@ingroup __CK_DEVICE_INFO__
	 *@~chinese
	 *@brief 获得指定设备的枚举信息
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pDevInfo 指针，返回设备的枚举信息和状态信息。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get enumerated information for the specified device
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pDevInfo The pointer returns the enumeration information and status information of the device.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetCurDevInfo(HANDLE hCamera, tDevLoadInfo* pDevInfo);

	/**
	 *@ingroup __CK_GET_IMAGE_BUFFER__
	 *@~chinese
	 *@brief 获得一帧图像原始数据。为了提高效率，SDK在图像抓取时采用了零拷贝机制，
	 CameraGetRawImageBuffer实际获得是内核中的一个缓冲区地址，
	 该函数成功调用后，必须调用@link #CameraReleaseFrameHandle @endlink,以便让内核继续使用
	 该缓冲区。  
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] phRawBuf   获得图像的数据的句柄。
	 *@param[in] uTimes 抓取图像的超时时间。单位毫秒。在mTimes时间内还未获得图像，则该函数会返回超时信息。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraReleaseFrameHandle
	 *@note 一般情况下，工业相机传输的都是原始的 RAW 数据，对于黑白相机，
	 就是灰度图像数据，对于彩色相机，就是 Bayer 格式的图像数据。
	 CameraGetRawImageBuffer 函数，得到的就是原始的 RAW 数据，默认是 8bit 。
	 该函数可以设置超时时间，例如设置 1000 毫秒的超时时间，则在
	 1000 毫秒内，如果没有获取到有效图像，则该函数会阻塞，线程会
	 被挂起，直到超过 1000 毫秒或者读到了有效图像，所以在软件结构
	 的设计上，用户可以创建一个专门采集图像的线程，然后一直进行图
	 像采集，只需要设定一个合理的超时时间即可，或者在需要采集图像
	 的时候调用一次函数，获取一张图像。
	 *@warning CameraGetRawImageBuffer 需要和 @link #CameraReleaseFrameHandle @endlink配套使用。
	 *@~english
	 *@brief Gets a frame of image raw data. In order to improve efficiency, the SDK uses a zero copy mechanism for image capture.
	 CameraGetRawImageBuffer is actually obtained as a buffer address in the kernel.
	 After the function is successfully called, you must call @link #CameraReleaseFrameHandle @endlink to let the kernel continue to use.
	 The buffer.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] phRawBuf Gets the handle of the image's data.
	 *@param[in] uTimes Timeout for capturing images. In milliseconds. If the image has not been obtained within mTimes, the function will return a timeout message.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraReleaseFrameHandle
	 *@note In general, industrial cameras transmit raw RAW data, for black and white cameras,
	 It is grayscale image data, and for color cameras, it is image data in Bayer format.
	 The CameraGetRawImageBuffer function gets the original RAW data, the default is 8bit.
	 This function can set the timeout period, for example setting a timeout of 1000 milliseconds, then
	 Within 1000 milliseconds, if a valid image is not obtained, the function will block and the thread will
	 Suspended until more than 1000 milliseconds or read a valid image, so in the software structure
	 In design, the user can create a thread that collects images and then keeps the graph
	 Like collecting, you only need to set a reasonable timeout, or you need to collect images.
	 Call the function once to get an image.
	 *@warning CameraGetRawImageBuffer needs to be used with @link #CameraReleaseFrameHandle @endlink.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetRawImageBuffer(HANDLE hCamera, PHANDLE phRawBuf, UINT uTimes);

	/**
	 *@ingroup __CK_GET_IMAGE_BUFFER__
	 *@~chinese
	 *@brief 释放由@link #CameraGetRawImageBuffer @endlink获得缓冲区
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] hRawBuf   通过CameraGetRawImageBuffer指向图像的数据的缓冲区句柄。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@return 成功时，返回帧数据的缓冲区地址
	 *@see CameraGetRawImageBuffer
	 *@see CameraGetImageInfo
	 *@~english
	 *@brief release buffer obtained by @link #CameraGetRawImageBuffer @endlink
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] hRawBuf A buffer handle that points to the image's data via CameraGetRawImageBuffer.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@return returns the buffer address of the frame data when successful
	 *@see CameraGetRawImageBuffer
	 *@see CameraGetImageInfo
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraReleaseFrameHandle(HANDLE hCamera, HANDLE hRawBuf);

	/**
	 *@ingroup __CK_GET_IMAGE_BUFFER__
	 *@~chinese
	 *@brief 获得当前帧图像信息
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] hRawBuf   通过@link #CameraGetRawImageBuffer @endlink获得图像的数据的缓冲区句柄。
	 *@param[out] pImgInfo	获得帧信息
	 *@return 返回原始图像数据的指针
	 *@see CameraGetRawImageBuffer
	 *@see CameraReleaseFrameHandle
	 *@note 通过调用 @link #CameraGetRawImageBuffer @endlink 获得一帧图像句柄，通过该句
	 柄获得图像数据缓冲区地址，以及帧头信息。
	 初始化完成之后，就可以开始相机取图工作。 按照取图方式， SDK
	 分为主动和被动（回调函数）两种。
	 *-# 主动取图方式
	 在完成相机初始化后工作，可以使用 @link #CameraGetRawImageBuffer @endlink 函
	 数和 CameraGetImageInfo 函数主动读取图像，二者的区别是，
	 @link #CameraGetRawImageBuffer @endlink 函数得到的是原始的 RAW 数据缓冲区，
	 需要使用 @link #CameraGetOutImageBuffer  @endlink函数将 RAW 数据转换为指定的
	 数据格式，在使用完得到的缓冲区指针后，要调用
	 @link #CameraReleaseFrameHandle @endlink 释放缓冲区使用权，需要解释的是
	 @link #CameraReleaseFrameHandle @endlink 只是释放由 @link #CameraGetRawImageBuffer @endlink
	 得到的数据缓冲区的使用权，并不会反复申请和释放内存，无需担心
	 效率问题；具体的格式由输入的参数决定，可以是 8 位灰度，也可
	 以是 24 位、 32 位彩色格式数据。
	 *-# 被动读取方式（回调函数）
	 如果需要使用回调函数进行读图处理，则需要在相机初始化以后设置
	 好读图的回调函数。
	 *@~english
	 *@brief Get current frame image information
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] hRawBuf Gets the buffer handle of the image's data via @link #CameraGetRawImageBuffer @endlink.
	 *@param[out] pImgInfo Get frame information
	 *@return Return a pointer to the original image data
	 *@see CameraGetRawImageBuffer
	 *@see CameraReleaseFrameHandle
	 *@note Get a frame of image handle by calling @link #CameraGetRawImageBuffer @endlink
	 The handle gets the image data buffer address, as well as the frame header information.
	 After the initialization is complete, you can start the camera fetching work. According to the drawing method, the SDK
	 Divided into active and passive (callback function).
	 *-# Active drawing mode
	 After completing the camera initialization, you can use @link #CameraGetRawImageBuffer @endlink
	 The number and the CameraGetImageInfo function actively read the image, the difference between the two is that
	 @link #CameraGetRawImageBuffer @endlink function gets the original RAW data buffer,
	 Need to use the @link #CameraGetOutImageBuffer @endlink function to convert RAW data to the specified
	 Data format, to be called after using the obtained buffer pointer
	 @link #CameraReleaseFrameHandle @endlink Release buffer usage rights, what needs to be explained is
	 @link #CameraReleaseFrameHandle @endlink Just released by @link #CameraGetRawImageBuffer @endlink
	 Get the right to use the data buffer, and do not repeatedly apply and release memory, no need to worry
	 Efficiency problem; the specific format is determined by the input parameters, which can be 8-bit grayscale, or
	 It is 24-bit, 32-bit color format data.
	 *-# Passive read mode (callback function)
	 If you need to use the callback function for reading processing, you need to set it after the camera is initialized.
	 Read the callback function of the graph.
	 */
	CKSDK_API BYTE* __stdcall CameraGetImageInfo(HANDLE hCamera, HANDLE hRawBuf, stImageInfo *pImgInfo);

	/**
	*@ingroup __CK_GET_IMAGE_BUFFER__
	*@~chinese
	*@brief 获得当前帧的时间戳，单位微秒
	*@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	*@param[in] hRawBuf   通过@link #CameraGetRawImageBuffer @endlink获得图像的数据的缓冲区句柄。
	*@param[out] puTimestamp	获得时间戳
	*@return @link #__CK_ERRCODE__ 状态码@endlink
	*@see CameraGetRawImageBuffer
	*
	*@~english
	*@brief Gets the timestamp of the current frame in microseconds
	*@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	*@param[in] hRawBuf Gets the buffer handle of the image's data via @link #CameraGetRawImageBuffer @endlink.
	*@param[out] puTimestamp get timestamp
	*@return @link #__CK_ERRCODE__ Status Code @endlink
	*@see CameraGetRawImageBuffer
	*/
	CKSDK_API CameraSdkStatus __stdcall CameraGetImageTimestamp(HANDLE hCamera, HANDLE hRawBuf, UINT64 *puTimestamp);

	/**
	 *@ingroup __CK_GET_IMAGE_BUFFER__
	 *@~chinese
	 *@brief 将获得的相机原始输出图像数据进行处理，叠加饱和度、
	 颜色增益和校正、降噪等处理效果，处理完后图像的输出格式
	 可以是
	 @link #CAMERA_MEDIA_TYPE_MONO @endlink
	 @link #CAMERA_MEDIA_TYPE_RGB8 @endlink
	 @link #CAMERA_MEDIA_TYPE_RGBA8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGR8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGRA8 @endlink
	 的其中一种。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] pImgInfo  输入图像的帧头信息，处理完成后，帧头信息 中的图像格式uiMediaType会随之改变。
	 *@param[in] pRawBuf    输入图像数据的缓冲区地址，不能为NULL，填入@link #CameraGetImageInfo @endlink函数的返回值。
	 *@param[out] pImgBuf   处理后图像输出的缓冲区地址，不能为NULL。	
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief processes the obtained camera raw output image data, superimposes saturation,
	 Color gain and correction, noise reduction and other processing effects, the output format of the processed image
	 Can be
	 @link #CAMERA_MEDIA_TYPE_MONO @endlink
	 @link #CAMERA_MEDIA_TYPE_RGB8 @endlink
	 @link #CAMERA_MEDIA_TYPE_RGBA8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGR8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGRA8 @endlink
	 One of them.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] pImgInfo Enter the header information of the image. After processing, the image format uiMediaType in the header information will change.
	 *@param[in] pRawBuf Enter the buffer address of the image data, which cannot be NULL.  Fill in the return value of the @link #CameraGetImageInfo @endlink function.
	 *@param[out] pImgBuf The buffer address of the image output after processing by pImgBuf cannot be NULL.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetOutImageBuffer(HANDLE hCamera, stImageInfo *pImgInfo, BYTE *pRawBuf, BYTE *pImgBuf);

	/**
	 *@ingroup __CK_GET_IMAGE_BUFFER__
	 *@~chinese
	 *@brief 获得一帧图像数据。该接口获得的图像是经过处理后的格式。该格式可以通过@link #CameraSetIspOutFormat @endlink设置
	 该函数调用后，该函数返回的图像数据缓冲区。适用于主动方式调用。
	 *@param[in] hCamera	  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pImgInfo			结构体，返回帧头信息
	 *@param[in] uTimes 抓取图像的超时时间。单位毫秒。在
	 wTimes时间内还未获得图像，则该函数会返回超时信息。
	 *@return 成功时，返回 RGB 数据缓冲区的首地址; 否则返回 NULL。
	 *@warning 区别于 @link #CameraGetRawImageBuffer @endlink 函数，该函数得到的就是按
	 输出格式输出图像数据。后续不需要再调用
	 @link #CameraGetOutImageBuffer @endlink函数。
	 *@~english
	 *@brief Gets a frame of image data. The image obtained by this interface is the processed format. This format can be set via @link #CameraSetIspOutFormat @endlink
	 After the function is called, the function returns the image data buffer. Applicable to active mode calls.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pImgInfo structure, returning frame header information
	 *@param[in] uTimes Timeout for capturing images. In milliseconds. in
	 The function returns a timeout message if the image has not been obtained within wTimes.
	 *@return Returns the first address of the RGB data buffer on success; otherwise returns NULL.
	 *@warning is different from @link #CameraGetRawImageBuffer @endlink function, the function gets the press
	 The output format outputs image data. No need to call later
	 @link #CameraGetOutImageBuffer @endlink function.
	 */
	CKSDK_API BYTE* __stdcall CameraGetImageBufferEx(HANDLE hCamera, stImageInfo *pImgInfo, UINT uTimes);

	/**
	 *@ingroup __CK_GET_IMAGE_BUFFER__
	 *@~chinese
	 *@brief 获得一帧图像数据。该接口获得的图像是经过处理后的格式。该格式可以通过@link #CameraSetIspOutFormat @endlink设置
	 该函数调用后，会将处理后的图像数据放入提供的缓冲区中，适用于主动方式调用。
	 *@param[in] hCamera	  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pImageData  图像数据缓冲区指针
	 *@param[out] pImgInfo	  结构体，返回帧头信息
	 *@param[in] uTimes 抓取图像的超时时间。单位毫秒。在wTimes时间内还未获得图像，则该函数会返回超时错误。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets a frame of image data. The image obtained by this interface is the processed format. This format can be set via @link #CameraSetIspOutFormat @endlink
	 After the function is called, the processed image data is placed in the provided buffer for active mode calls.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pImageData image data buffer pointer
	 *@param[out] pImgInfo structure, returning frame header information
	 *@param[in] uTimes Timeout for capturing images. In milliseconds. in
	 This function has not been obtained in wTimes time.
	 A timeout message will be returned.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetImageBufferEx1(HANDLE hCamera, BYTE* pImageData, stImageInfo *pImgInfo, UINT uTimes);

	/**
	*@ingroup __CK_GET_IMAGE_BUFFER__
	*@~chinese
	*@brief 获得一帧图像数据。该接口获得的图像是经过处理后的格式。该格式可以通过@link #CameraSetIspOutFormat @endlink设置
	*@param[in] hCamera 相机的句柄。
	*@param[out] pImageData  图像数据缓冲区指针
	*@param[out] pImgInfo	 结构体，返回帧头信息
	*@param[out] puTimeStamp 返回图像时间戳，单位微秒
	*@param[in] uTimes 抓取图像的超时时间。单位毫秒。在wTimes时间内还未获得图像，则该函数会返回超时错误。
	*@return @link #__CK_ERRCODE__ 状态码@endlink
	*@note 不需要调用 @link #CameraReleaseFrameHandle @endlink
	*@~english
	*@brief Gets one frame of image data. The image obtained by this interface is the processed format. This format can be set via @link #CameraSetIspOutFormat @endlink
	*@param[in] hCamera The handle of the camera.
	*@param[out] pImageData image data buffer pointer
	*@param[out] pImgInfo structure, returning frame header information
	*@param[out] puTimeStamp Returns the image timestamp in microseconds
	*@param[in] uTimes Timeout for capturing images. In milliseconds. If the image has not been obtained within wTimes, the function will return a timeout error.
	*@return @link #__CK_ERRCODE__ Status Code @endlink
	*@note does not need to call @link #CameraReleaseFrameHandle @endlink
	*/
	CKSDK_API CameraSdkStatus __stdcall CameraGetImageBufferEx2(HANDLE hCamera, BYTE *pImageData, stImageInfo *pImgInfo, UINT64 *puTimeStamp, UINT uTimes);
	
	/**
	*@ingroup __CK_GET_IMAGE_BUFFER__
	*@~chinese
	*@brief 复位图像采集的时间戳，从0开始。
	*@param[in] hCamera 相机的句柄。
	*@return @link #__CK_ERRCODE__ 状态码@endlink
	*@~english
	*@brief Resets the timestamp of image acquisition, starting at 0.
	*@param[in] hCamera The handle of the camera.
	*@return @link #__CK_ERRCODE__ Status Code @endlink
	*/
	CKSDK_API CameraSdkStatus __stdcall CameraResetTimestamp(HANDLE hCamera);

	/**
	 *@ingroup __CK_GET_IMAGE_BUFFER__
	 *@~chinese
	 *@brief 设置图像捕获的回调函数。当捕获到新的图像数据帧时，pCallBack所指向的回调函数就会被调用。 
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] pCallBack 回调函数指针。
	 *@param[in] lpParam  回调函数的附加参数，在回调函数被调用时
	 该附加参数会被传入，可以为NULL。多用于
	 多个相机时携带附加信息。
	 *@param[out] pCallbackOld  用于保存当前的回调函数。可以为NULL。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the callback function for image capture. When a new image data frame is captured, the callback function pointed to by pCallBack is called.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] pCallBack callback function pointer.
	 *@param[in] lpParam Additional parameters for the lpParam callback function, when the callback function is called
	 This additional parameter will be passed in and can be NULL. Used mostly
	 Carrying additional information when multiple cameras.
	 *@param[out] pCallbackOld is used to save the current callback function. Can be NULL.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetCallbackFunction(HANDLE hCamera, CAMERA_SNAP_PROC pCallBack, LPVOID lpParam, CAMERA_SNAP_PROC *pCallbackOld);
	/** @} end of __CK_GET_IMAGE_BUFFER__ */

	/**
	 *@ingroup __CK_CAMERA_PLAY_PAUSE__
	 *@~chinese
	 *@brief 让SDK进入暂停模式，不接收来自相机的图像数据，
	 同时也会发送命令让相机暂停输出，释放传输带宽。
	 暂停模式下，可以对相机的参数进行配置，并立即生效。  
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraPlay
	 *@~english
	 *@brief puts the SDK into pause mode and does not receive image data from the camera.
	 It also sends a command to let the camera pause the output and release the transmission bandwidth.
	 In the pause mode, the camera's parameters can be configured and take effect immediately.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraPlay
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraPause(HANDLE hCamera);

	/**
	 *@ingroup __CK_CAMERA_PLAY_PAUSE__
	 *@~chinese
	 *@brief 让SDK进入工作模式，开始接收来自相机发送的图像
	 数据。如果当前相机是触发模式，则需要接收到
	 触发帧以后才会更新图像。
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraPause
	 *@~english
	 *@brief puts the SDK into working mode and starts receiving images sent from the camera
	 Data. If the current camera is in trigger mode, you need to receive it.
	 The image will not be updated until the frame is triggered.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraPause
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraPlay(HANDLE hCamera);

	/**
	 *@ingroup __CK_DISPLAY__
	 *@~chinese
	 *@brief 初始化SDK内部的显示模块。在调用@link #CameraDisplay @endlink
	 前必须先调用该函数初始化。如果您在二次开发中，
	 使用自己的方式进行图像显示(不调用CameraDisplay)，
	 则不需要调用本函数。  
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] hWndDisplay 显示窗口的句柄，一般为窗口的m_hWnd成员。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraDisplay
	 *@warning 在调用 @link #CameraDisplay @endlink 前必须先调用该函数初始化。如果
	 您在二次开发中，使用自己的方式进行图像显示(不调用@link #CameraDisplay @endlink)，则不需要调用 @link #CameraDisplayInit @endlink 函数 。
	 *@~english
	 *@brief Initializes the display module inside the SDK. Call @link #CameraDisplay @endlink
	 This function must be called before initialization. If you are in secondary development,
	 Use your own way to display images (without calling CameraDisplay),
	 You do not need to call this function.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] hWndDisplay The handle of the display window, typically the m_hWnd member of the window.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraDisplay
	 *@warning must be called to initialize the function before calling @link #CameraDisplay @endlink. in case
	 In the second development, you use your own way to display the image (do not call @link #CameraDisplay @endlink), you do not need to call @link #CameraDisplayInit @endlink function.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraDisplayInit(HANDLE hCamera, HWND hWndDisplay);

	/**
	 *@ingroup __CK_DISPLAY__
	 *@~chinese
	 *@brief 设置显示的模式。必须调用过@link #CameraDisplayInit @endlink
	 进行初始化才能调用本函数。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iMode    显示模式，@link #emSdkDisplayMode::DISPLAYMODE_SCALE @endlink或者@link #emSdkDisplayMode::DISPLAYMODE_REAL @endlink,
	 具体参见CKDeviceDef.h中emSdkDisplayMode的定义。    
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the mode of display. Must call @link #CameraDisplayInit @endlink
	 This function can be called by initializing.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iMode display mode, @link #emSdkDisplayMode::DISPLAYMODE_SCALE @endlink or @link #emSdkDisplayMode::DISPLAYMODE_REAL @endlink,
	 See the definition of emSdkDisplayMode in CKDeviceDef.h for details.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetDisplayMode(HANDLE hCamera, INT iMode);

	/**
	 *@ingroup __CK_DISPLAY__
	 *@~chinese
	 *@brief 设置显示的缩放比率。仅当显示模式为@link #emSdkDisplayMode::DISPLAYMODE_REAL @endlink
	 时有效。例如100 是显示窗口和图像窗口为1:1,200是显示窗口和图像窗口为2:1
	 50是显示窗口和图像窗口为0.5:1，故该值为倍率X100，必须调用过
	 @link #CameraDisplayInit @endlink进行初始化才能调用本函数。
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iRadio  缩放比率。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the zoom ratio of the display. Only when the display mode is @link #emSdkDisplayMode::DISPLAYMODE_REAL @endlink
	 It is valid. For example, 100 is the display window and the image window is 1:1, 200 is the display window and the image window is 2:1.
	 50 is the display window and image window is 0.5:1, so the value is the magnification X100, must be called
	 @link #CameraDisplayInit @endlink is initialized to call this function.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iRadio scaling ratio.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetDisplayScaleRadio(HANDLE hCamera, INT iRadio);

	/**
	 *@ingroup __CK_DISPLAY__
	 *@~chinese
	 *@brief 设置显示的起始偏移值。仅当显示模式为@link #emSdkDisplayMode::DISPLAYMODE_REAL @endlink
	 时有效。例如显示控件的大小为320X240，而图像的
	 的尺寸为640X480，那么当iOffsetX = 160,iOffsetY = 120时
	 显示的区域就是图像的居中320X240的位置。必须调用过
	 @link #CameraDisplayInit @endlink进行初始化才能调用本函数。
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iOffsetX  偏移的X坐标。
	 *@param[in] iOffsetY  偏移的Y坐标。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the starting offset value for the display. Only when the display mode is @link #emSdkDisplayMode::DISPLAYMODE_REAL @endlink
	 It is valid. For example, the size of the display control is 320X240, while the image
	 The size is 640X480, then when iOffsetX = 160, iOffsetY = 120
	 The area shown is the center of the image at 320X240. Must be called
	 @link #CameraDisplayInit @endlink is initialized to call this function.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iOffsetX The X coordinate of the iOffsetX offset.
	 *@param[in] iOffsetY The Y coordinate of the offset of iOffsetY.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetDisplayOffset(HANDLE hCamera, INT iOffsetX, INT iOffsetY);

	/**
	 *@ingroup __CK_DISPLAY__
	 *@~chinese
	 *@brief 设置显示控件的尺寸。必须调用过
	 @link #CameraDisplayInit @endlink进行初始化才能调用本函数。
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iWidth    宽度
	 *@param[in] iHeight   高度
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the size of the display control. Must be called
	 @link #CameraDisplayInit @endlink is initialized to call this function.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iWidth width
	 *@param[in] iHeight height
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetDisplaySize(HANDLE hCamera, INT iWidth, INT iHeight);

	/**
	 *@ingroup __CK_DISPLAY__
	 *@~chinese
	 *@brief 设置指定十字线的参数。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iLine    表示要设置第几条十字线的状态。范围为[0,8]，共9条。    
	 *@param[in] x          十字线中心位置的横坐标值。
	 *@param[in] y      十字线中心位置的纵坐标值。
	 *@param[in] uColor     十字线的颜色，格式为(R|(G<<8)|(B<<16))
	 *@param[in] bVisible   十字线的显示状态。TRUE，表示显示。只有设置为显示状态的十字线，在调用
	 @link #CameraImageOverlay @endlink后才会被叠加到图像上。     
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the parameters for the specified crosshair.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iLine indicates the state of the first crosshairs to be set. The range is [0,8], a total of 9.
	 *@param[in] x The abscissa value of the center of the crosshair.
	 *@param[in] y The ordinate value of the center of the crosshair.
	 *@param[in] uColor The color of the crosshair, in the format (R|(G<<8)|(B<<16))
	 *@param[in] bVisible The display status of the crosshairs. TRUE, indicating display. Only the crosshair set to the display state is called
	 @link #CameraImageOverlay @endlink will be superimposed on the image.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetCrossLine(HANDLE hCamera, INT iLine, INT x, INT y, UINT uColor, BOOL bVisible);

	/**
	 *@ingroup __CK_DISPLAY__
	 *@~chinese
	 *@brief 获得指定十字线的状态。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iLine    表示要获取的第几条十字线的状态。范围为[0,8]，共9条。  
	 *@param[out] px     指针，返回该十字线中心位置的横坐标。
	 *@param[out] py     指针，返回该十字线中心位置的横坐标。
	 *@param[out] puColor     指针，返回该十字线的颜色，格式为(R|(G<<8)|(B<<16))。
	 *@param[out] pbVisible  指针，返回TRUE，则表示该十字线可见。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the status of the specified crosshair.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iLine indicates the status of the first crosshairs to be acquired. The range is [0,8], a total of 9.
	 *@param[out] px The pointer returns the abscissa of the center of the crosshair.
	 *@param[out] py The pointer returns the abscissa of the center of the crosshair.
	 *@param[out] puColor The ointer returns the color of the crosshair, in the format (R|(G<<8)|(B<<16)).
	 *@param[out] pbVisible The pointer returning TRUE, indicates that the crosshair is visible.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetCrossLine(HANDLE hCamera, INT iLine, INT *px, INT *py, UINT *puColor, BOOL *pbVisible);

	/**
	 *@ingroup __CK_DISPLAY__
	 *@~chinese
	 *@brief 将输入的图像数据上叠加十字线、白平衡参考窗口、
	 自动曝光参考窗口等图形。只有设置为可见状态的
	 十字线和参考窗口才能被叠加上。
	 注意，该函数的输入图像必须是RGB格式。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] pImgBuf 图像数据缓冲区。
	 *@param[in] pImgInfo    图像的帧头信息。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief superimposes the input image data on the crosshair, white balance reference window,
	 Auto exposure reference window and other graphics. Only set to visible
	 Crosshairs and reference windows can be superimposed.
	 Note that the input image for this function must be in RGB format.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] pImgBuf Image data buffer.
	 *@param[in] pImgInfo The header information of the image.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraImageOverlay(HANDLE hCamera, BYTE* pImgBuf, const stImageInfo* pImgInfo);

	/**
	 *@ingroup __CK_DISPLAY__
	 *@~chinese
	 *@brief 在输入的图像数据中绘制文字
	 *@param[in] pImgBuf 图像数据缓冲区
	 *@param[in] pImgInfo 图像的帧头信息
	 *@param[in] pFontFileName 字体文件名
	 *@param[in] FontWidth 字体宽度
	 *@param[in] FontHeight 字体高度
	 *@param[in] pText 要输出的文字
	 *@param[in] Left 文件输出矩形的左坐标
	 *@param[in] Top 文件输出矩形的上坐标
	 *@param[in] Width 文件输出矩形的宽度
	 *@param[in] Height 文件输出矩形的高度
	 *@param[in] TextColor 文字颜色RGB
	 *@param[in] uFlags 输出标志, 暂时未使用，填0即可
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief draws text in the input image data
	 *@param[in] pImgBuf image data buffer
	 *@param[in] pImgInfo image header information
	 *@param[in] pFontFileName font file name
	 *@param[in] FontWidth font width
	 *@param[in] FontHeight font height
	 *@param[in] pText The text to be output
	 *@param[in] Left left coordinate of the file output rectangle
	 *@param[in] Top The upper coordinate of the file output rectangle
	 *@param[in] Width The width of the output rectangle of the file
	 *@param[in] Height The height of the file output rectangle
	 *@param[in] TextColor text color RGB
	 *@param[in] uFlags output flag, not used yet, fill in 0
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraDrawText(BYTE* pImgBuf, const stImageInfo* pImgInfo,
			char const* pFontFileName,
			UINT FontWidth,
			UINT FontHeight,
			char const* pText,
			INT Left,
			INT Top,
			UINT Width,
			UINT Height,
			UINT TextColor,
			UINT uFlags
			);

	/**
	 *@ingroup __CK_DISPLAY__
	 *@~chinese
	 *@brief 显示图像。必须调用过CameraDisplayInit进行初始化才能调用本函数。  
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] pImgBuf 图像的数据缓冲区。
	 *@param[in] pImgInfo  图像的帧头信息。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief displays the image. You must call CameraDisplayInit to initialize this function.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] pImgBuf The data buffer for the pImgBuf image.
	 *@param[in] pImgInfo The header information of the image.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraDisplay(HANDLE hCamera, BYTE* pImgBuf, stImageInfo* pImgInfo);

	/**
	 *@ingroup __CK_DISPLAY__
	 *@~chinese
	 *@brief 设置显示回调。必须调用过@link #CameraDisplayInit @endlink进行
	 初始化才能调用本函数，同时当显示图像数据帧时，
	 pCallBack所指向的回调函数就会被调用。 
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] pCallBack 回调函数指针。
	 *@param[in] lpParam  回调函数的附加参数，在回调函数被调用时
	 该附加参数会被传入，可以为NULL。
	 *@param[out] pCallbackOld  用于保存当前的回调函数。可以为NULL。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the display callback. Must be called @link #CameraDisplayInit @endlink
	 This function can be called by initialization, and when displaying image data frames,
	 The callback function pointed to by pCallBack will be called.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] pCallBack callback function pointer.
	 *@param[in] lpParam Additional parameters for the lpParam callback function, when the callback function is called
	 This additional parameter will be passed in and can be NULL.
	 *@param[out] pCallbackOld is used to save the current callback function. Can be NULL.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetDisplayCallbackFun(HANDLE hCamera, CAMERA_DISPLAY_PROC pCallBack, LPVOID lpParam, CAMERA_DISPLAY_PROC *pCallbackOld);
	/** @} end of __CK_DISPLAY__ */

	/**
	 *@ingroup __CK_GET_IMAGE_BUFFER__
	 *@~chinese
	 *@brief 抓拍一张图像到缓冲区中。相机会进入抓拍模式，并且
	 自动切换到抓拍模式的分辨率进行图像捕获。然后将
	 捕获到的数据保存到缓冲区中。
	 该函数成功调用后，必须调用@link #CameraReleaseFrameHandle @endlink
	 释放由CameraSnapToBuffer得到的缓冲区。具体请参考
	 @link #CameraGetRawImageBuffer @endlink函数的功能描述部分。  
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] phBuf  指针，返回图像的数据的缓冲区句柄。用于释放数据，@link CameraReleaseFrameHandle @endlink
	 *@param[out] pImgInfo   指针，返回图像的帧头信息
	 *@param[out] ppImgBuf 指针，返回图像的数据指针
	 *@param[in] uWaitTimeMs 超时时间，单位毫秒。在该时间内，如果仍然没有成功捕获的数据，则返回超时信息。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Capture an image into the buffer. The camera will enter the capture mode, and
	 Automatically switch to the resolution of the capture mode for image capture. followed by
	 The captured data is saved to the buffer.
	 After the function is successfully called, you must call @link #CameraReleaseFrameHandle @endlink
	 Release the buffer obtained by CameraSnapToBuffer. Please refer to the specific
	 @link #CameraGetRawImageBuffer The functional description section of the @endlink function.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] phBuf Pointer, the buffer handle that returns the data of the image. Used to release data, CameraReleaseFrameHandle
	 *@param[out] pImgInfo pointer, returning the frame header information of the image
	 *@param[out] ppImgBuf pointer, returning the data pointer of the image
	 *@param[in] uWaitTimeMs Timeout in milliseconds. During this time, if there is still no data successfully captured, a timeout message is returned.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSnapToBuffer(HANDLE hCamera, PHANDLE phBuf, stImageInfo *pImgInfo, BYTE** ppImgBuf, UINT uWaitTimeMs);

	/**
	 *@ingroup __CK_ROI_SETTINGS__
	 *@~chinese
	 *@brief 设置抓拍模式下相机输出图像的分辨率。
	 *@param[in] hCamera       相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] pImageResolution 如果pImageResolution->iWidth 
	 和 pImageResolution->iHeight都为0，
	 则表示设定为跟随当前预览分辨率。抓
	 怕到的图像的分辨率会和当前设定的 
	 预览分辨率一样。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the resolution of the camera output image in snap mode.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] pImageResolution if pImageResolution->iWidth
	 And pImageResolution->iHeight are both 0,
	 It means that it is set to follow the current preview resolution. Grab
	 The resolution of the image that is afraid will be the same as the current setting.
	 The preview resolution is the same.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetResolutionForSnap(HANDLE hCamera, tSdkImageResolution* pImageResolution);

	/**
	 *@ingroup __CK_ROI_SETTINGS__
	 *@~chinese
	 *@brief 获得抓拍模式下的分辨率。
	 *@param[in] hCamera        相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pImageResolution 指针，返回抓拍模式的分辨率。 
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the resolution in snap mode.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pImageResolution The pointer returns the resolution of the snap mode.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetResolutionForSnap(HANDLE hCamera, tSdkImageResolution* pImageResolution);


	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 设置相机曝光的模式。自动或者手动。
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] bAeState    TRUE，使能自动曝光；FALSE，停止自动曝光。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the mode in which the camera is exposed. Automatic or manual.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] bAeState TRUE, enables auto exposure; FALSE, stops auto exposure.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetAeState(HANDLE hCamera, BOOL bAeState);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 获得相机当前的曝光模式。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pAeState   指针，用于返回自动曝光的使能状态。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the camera's current exposure mode.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pAeState Pointer to return the auto-exposure enable state.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetAeState(HANDLE hCamera, BOOL *pAeState);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 设定自动曝光的亮度目标值。设定范围由@link #CameraGetCapability @endlink函数获得。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] AeTarget  亮度目标值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the brightness target value for auto exposure. The setting range is obtained by the @link #CameraGetCapability @endlink function.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] AeTarget Brightness target value.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetAeTarget(HANDLE hCamera, WORD AeTarget);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 获得自动曝光的亮度目标值。
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pAeTarget 指针，返回目标值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the brightness target value for auto exposure.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pAeTarget The pointer returns the target value.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetAeTarget(HANDLE hCamera, WORD *pAeTarget);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 设置相机的图像模拟增益值。该值乘以@link #CameraGetCapability @endlink获得
	 的相机属性结构体中sExposeDesc.fAnalogGainStep，就
	 得到实际的图像信号放大倍数。
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] uAnalogGain 设定的模拟增益值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 设置相机的模拟增益值，这个值的大小只会影响图像亮度，不会影响
	 图像帧率，因为只是一个电路放大系数，不过该值增大后会提升图像
	 背景噪声，对于追求画质的应用场合，模拟增益应该设置到最小。
	 *@warning 该函数必须在手动模式下才可以生效。
	 *@~english
	 *@brief Sets the camera's image analog gain value. Multiply this value by @link #CameraGetCapability @endlink
	 sExposeDesc.fAnalogGainStep in the camera property structure
	 Obtain the actual image signal magnification.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] uAnalogGain Set the analog gain value.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note Sets the analog gain value of the camera. The size of this value will only affect the brightness of the image and will not affect it.
	 Image frame rate, because it is just a circuit amplification factor, but this value will increase the image
	 Background noise, for applications that require image quality, the analog gain should be set to a minimum.
	 *@warning This function must be in manual mode to take effect.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetAnalogGain(HANDLE hCamera, UINT uAnalogGain);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 获得图像信号的模拟增益值。参见@link #CameraSetAnalogGain @endlink详细说明。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] puAnalogGain 指针，返回当前的模拟增益值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the analog gain value of the image signal. See @link #CameraSetAnalogGain @endlink for details.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] puAnalogGain Pointer returns the current analog gain value.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetAnalogGain(HANDLE hCamera, UINT *puAnalogGain);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 设置曝光时间。单位为微秒。对于CMOS传感器，其曝光
	 的单位是按照行来计算的，因此，曝光时间并不能在微秒
	 级别连续可调。而是会按照整行来取舍。在调用
	 本函数设定曝光时间后，建议再调用@link #CameraGetExposureTime @endlink
	 来获得实际设定的值。
	 *@param[in] hCamera      相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] dExposureTime 曝光时间，单位微秒。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 该函数设置相机的曝光时间，单位为微秒。曝光时间越大，相机的帧
	 率越低，例如曝光时间设置到 500 毫秒，则一秒相机最多成像 2 次，
	 图像看上去就会比较卡顿。因此为了减少取图时间，曝先时间应该是
	 要越低越好，当然，曝先时间低，就需要外部光源加大光照，否则图
	 像就会很暗。
	 *@warning 该函数必须在手动模式下才可以生效。
	 *@~english
	 *@brief Set the exposure time. The unit is microseconds. For CMOS sensors, their exposure
	 The unit is calculated in rows, so the exposure time is not in microseconds.
	 The level is continuously adjustable. Instead, it will be based on the entire line. Invoking
	 After setting the exposure time with this function, it is recommended to call @link #CameraGetExposureTime @endlink again.
	 To get the actual set value.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] dExposureTime Exposure time in microseconds.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note This function sets the exposure time of the camera in microseconds. The larger the exposure time, the frame of the camera
	 The lower the rate, for example, the exposure time is set to 500 milliseconds, then the camera is imaged up to 2 times in one second.
	 The image will look more like a card. Therefore, in order to reduce the drawing time, the exposure time should be
	 The lower the better, of course, the exposure time is low, you need external light source to increase the illumination, otherwise
	 The image will be very dark.
	 *@warning This function must be in manual mode to take effect.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetExposureTime(HANDLE hCamera, double dExposureTime);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 获得相机的曝光时间。请参见@link #CameraSetExposureTime @endlink 的功能描述。
	 *@param[in] hCamera        相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pdExposureTime   指针，返回当前的曝光时间，单位微秒。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the camera's exposure time. See @link #CameraSetExposureTime @endlink Functional description.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pdExposureTime Pointer, which returns the current exposure time in microseconds.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetExposureTime(HANDLE hCamera, double *pdExposureTime);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 设置自动曝光的参考窗口。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] dwLeft    窗口左上角的横坐标
	 *@param[in] dwTop      窗口左上角的纵坐标
	 *@param[in] dwWidth     窗口的宽度 
	 *@param[in] dwHeight    窗口的高度
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the reference window for auto exposure.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] dwLeft The abscissa of the upper left corner of the window
	 *@param[in] dwTop The ordinate of the upper left corner of the window
	 *@param[in] dwWidth width of the window
	 *@param[in] dwHeight height of the window
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetAeWindow(HANDLE hCamera, DWORD dwLeft, DWORD dwTop, DWORD dwWidth, DWORD dwHeight);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 获得自动曝光参考窗口的位置。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pdwLeft   指针，返回参考窗口的左上角横坐标 。
	 *@param[out] pdwTop     指针，返回参考窗口的左上角纵坐标 。
	 *@param[out] pdwWidth    指针，返回参考窗口的宽度。
	 *@param[out] pdwHeight   指针，返回参考窗口的高度。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the location of the auto exposure reference window.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pdwLeft The pointer returns the abscissa of the upper left corner of the reference window.
	 *@param[out] pdwTop The pointer returns the ordinate of the upper left corner of the reference window.
	 *@param[out] pdwWidth Pointer, which returns the width of the reference window.
	 *@param[out] pdwHeight Pointer, which returns the height of the reference window.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetAeWindow(HANDLE hCamera, DWORD *pdwLeft, DWORD *pdwTop, DWORD *pdwWidth, DWORD *pdwHeight);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 设置自动曝光参考窗口的显示状态。当设置窗口状态
	 为显示，调用@link #CameraImageOverlay @endlink后，能够将窗口位置
	 以矩形的方式叠加在图像上。
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] bDisplayEn  TRUE，设置为显示；FALSE，不显示。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the display status of the auto exposure reference window. When setting the window status
	 For display, after calling @link #CameraImageOverlay @endlink, the window position can be
	 Superimposed on the image in a rectangular pattern.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] bDisplayEn TRUE, set to display; FALSE, not displayed.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetAeWinVisible(HANDLE hCamera, BOOL bDisplayEn);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 获得自动曝光参考窗口的显示状态。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pbDisplayEn  指针，返回TRUE，则表示当前窗口会被叠加在图像内容上。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the display status of the auto exposure reference window.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pbDisplayEn Pointer, returning TRUE means that the current window will be superimposed on the image content.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetAeWinVisible(HANDLE hCamera, BOOL *pbDisplayEn);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 设置自动曝光时抗频闪功能的使能状态。对于手动
	 曝光模式下无效。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] bEnable    TRUE，开启抗频闪功能;FALSE，关闭该功能。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the enable status of the anti-strobe function during auto exposure. For manual Inactive in exposure mode.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] bEnable TRUE, turn on the anti-strobe function; FALSE, turn off the function.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetAntiFlick(HANDLE hCamera, BOOL bEnable);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 获得自动曝光时抗频闪功能的使能状态。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pbEnable   指针，返回该功能的使能状态。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the anti-strobe function enabled state during auto exposure.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pbEnable The pointer returns the enable state of this function.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetAntiFlick(HANDLE hCamera, BOOL *pbEnable);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 设置自动曝光时消频闪的频率。
	 *@param[in] hCamera     相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iFrequencySel 1:50HZ , 2:60HZ 
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the frequency of the strobe flash during auto exposure.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iFrequencySel 1:50HZ , 2:60HZ
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetLightFrequency(HANDLE hCamera, INT iFrequencySel);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 获得自动曝光时，消频闪的频率选择。
	 *@param[in] hCamera      相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] piFrequencySel 指针，返回选择的索引号。1:50HZ 2:60HZ
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief When auto exposure is obtained, the frequency of the strobe is selected.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piFrequencySel Pointer, which returns the selected index number. 1:50HZ 2:60HZ
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetLightFrequency(HANDLE hCamera, INT *piFrequencySel);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 获得一行曝光的时间，单位：微妙。
	 *@param[in] hCamera      相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pExposureLineTime 指针，返回一行曝光时间，单位：微妙。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get a line of exposure time, in subtle.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pExposureLineTime Pointer, returns a line of exposure time, unit: subtle.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetExposureLineTime(HANDLE hCamera, double *pExposureLineTime);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 设定自动曝光模式下的曝光模式
	 *@param[in] hCamera      相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iMode 自动曝光模式，AE_FRAME_MODE帧率优先，AE_EXP_MODE曝光优先
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the exposure mode in auto exposure mode
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iMode auto exposure mode, AE_FRAME_MODE frame rate priority, AE_EXP_MODE exposure priority
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetAeExposureMode(HANDLE hCamera, INT iMode);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 获得自动曝光模式下的曝光模式
	 *@param[in] hCamera      相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] piMode 指针，获得自动曝光模式，AE_FRAME_MODE帧率优先，AE_EXP_MODE曝光优先
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get exposure mode in auto exposure mode
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piMode pointer, get auto exposure mode, AE_FRAME_MODE frame rate first, AE_EXP_MODE exposure priority
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetAeExposureMode(HANDLE hCamera, INT* piMode);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 设定自动曝光模式的曝光时间调节范围
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] dMinExposureTime 最小曝光时间（微秒）
	 *@param[in] dMaxExposureTime 最大曝光时间（微秒）
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the exposure time adjustment range of the auto exposure mode
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] dMinExposureTime minimum exposure time (microseconds)
	 *@param[in] dMaxExposureTime Maximum exposure time (microseconds)
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetAeExposureRange(HANDLE hCamera, double dMinExposureTime, double dMaxExposureTime);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 获得自动曝光模式的曝光时间调节范围
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pdMinExposureTime 最小曝光时间（微秒）
	 *@param[out] pdMaxExposureTime 最大曝光时间（微秒）
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the exposure time adjustment range of the auto exposure mode
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pdMinExposureTime Minimum exposure time (microseconds)
	 *@param[out] pdMaxExposureTime Maximum exposure time (microseconds)
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetAeExposureRange(HANDLE hCamera, double* pdMinExposureTime, double* pdMaxExposureTime);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 设定自动曝光模式的增益调节范围
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iMinAnalogGain 最小增益
	 *@param[in] iMaxAnalogGain 最大增益
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the gain adjustment range of the auto exposure mode
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iMinAnalogGain minimum gain
	 *@param[in] iMaxAnalogGain maximum gain
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetAeAnalogGainRange(HANDLE hCamera, INT iMinAnalogGain, INT iMaxAnalogGain);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 获得自动曝光模式的增益调节范围
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] iMinAnalogGain 最小增益
	 *@param[out] iMaxAnalogGain 最大增益
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the gain adjustment range of the auto exposure mode
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] iMinAnalogGain minimum gain
	 *@param[out] iMaxAnalogGain maximum gain
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetAeAnalogGainRange(HANDLE hCamera, INT* iMinAnalogGain, INT* iMaxAnalogGain);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 设置图像的数字增益。设定范围由CameraGetCapability
	 获得的相机属性结构体中sRgbGainRange成员表述。
	 实际的放大倍数是设定值/100。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] DGainR   红色通道的增益值。 
	 *@param[in] DGainG   绿色通道的增益值。
	 *@param[in] DGainB   蓝色通道的增益值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 该函数设置 R、 G、 B 三个颜色通道的增益值。
	 *@warning 该函数必须在手动模式下才可以生效。
	 *@~english
	 *@brief Sets the digital gain of the image. Setting range by CameraGetCapability
	 The obtained sRgbGainRange member representation in the camera attribute structure.
	 The actual magnification is the set value /100.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] DGainR The gain value of the red channel.
	 *@param[in] DGainG The gain value of the green channel.
	 *@param[in] DGainB Gain value of the blue channel.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note This function sets the gain values ??of the three color channels R, G, and B.
	 *@warning This function must be in manual mode to take effect.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetGain(HANDLE hCamera, WORD DGainR, WORD DGainG, WORD DGainB);

	/**
	 *@ingroup __CK_EXPOSURE__
	 *@~chinese
	 *@brief 获得图像处理的数字增益。具体请参见CameraSetGain的功能描述部分。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pDGainR  指针，返回红色通道的数字增益值。
	 *@param[out] pDGainG  指针，返回绿色通道的数字增益值。
	 *@param[out] pDGainB  指针，返回蓝色通道的数字增益值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the digital gain of image processing. See CameraSetGain for details. The functional description section.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pDGainR The pointer returns the digital gain value of the red channel.
	 *@param[out] pDGainG The pointer returns the digital gain value of the green channel.
	 *@param[out] pDGainB The pointer returns the digital gain value of the blue channel.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetGain(HANDLE hCamera, WORD *pDGainR, WORD *pDGainG, WORD *pDGainB);

	/**
	 *@ingroup __CK_WHITE_BLANCE__
	 *@~chinese
	 *@brief 设置相机白平衡模式。分为手动和自动两种方式。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] bAuto      TRUE，则表示使能自动模式。FALSE，则表示使用手动模式，通过调用 CameraSetOnceWB来进行一次白平衡。        
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the camera white balance mode. Divided into manual and automatic two ways.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] bAuto TRUE means that the automatic mode is enabled. FALSE, which means using manual mode, by calling
	 CameraSetOnceWB to perform a white balance.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetWbMode(HANDLE hCamera, BOOL bAuto);

	/**
	 *@ingroup __CK_WHITE_BLANCE__
	 *@~chinese
	 *@brief 获得当前的白平衡模式。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pbAuto   指针，返回TRUE表示自动模式，FALSE
	 为手动模式。 
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the current white balance mode.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pbAuto pointer, return TRUE for automatic mode, FALSE In manual mode.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetWbMode(HANDLE hCamera, BOOL *pbAuto);

	/**
	 *@ingroup __CK_WHITE_BLANCE__
	 *@~chinese
	 *@brief 在手动白平衡模式下，调用该函数会进行一次白平衡。生效的时间为接收到下一帧图像数据时。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@warning 该函数必须在手动模式下才可以生效。
	 *@~english
	 *@brief In manual white balance mode, calling this function will perform a white balance.
	 The effective time is when the next frame of image data is received.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@warning This function must be in manual mode to take effect.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetOnceWB(HANDLE hCamera);

	/**
	 *@ingroup __CK_WHITE_BLANCE__
	 *@~chinese
	 *@brief 设置白平衡参考窗口的位置。
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] dwLeft   参考窗口的左上角横坐标。
	 *@param[in] dwTop     参考窗口的左上角纵坐标。
	 *@param[in] dwWidth    参考窗口的宽度。
	 *@param[in] dwHeight   参考窗口的高度。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the position of the white balance reference window.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] dwLeft The upper left corner of the reference window.
	 *@param[in] dwTop The upper left ordinate of the reference window.
	 *@param[in] dwWidth The width of the reference window.
	 *@param[in] dwHeight The height of the reference window.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetWbWindow(HANDLE hCamera, DWORD dwLeft, DWORD dwTop, DWORD dwWidth, DWORD dwHeight);

	/**
	 *@ingroup __CK_WHITE_BLANCE__
	 *@~chinese
	 *@brief 获得白平衡参考窗口的位置。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pdwLeft   指针，返回参考窗口的左上角横坐标 。
	 *@param[out] pdwTop     指针，返回参考窗口的左上角纵坐标 。
	 *@param[out] pdwWidth    指针，返回参考窗口的宽度。
	 *@param[out] pdwHeight   指针，返回参考窗口的高度。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the position of the white balance reference window.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pdwLeft The pointer returns the abscissa of the upper left corner of the reference window.
	 *@param[out] pdwTop The pointer returns the ordinate of the upper left corner of the reference window.
	 *@param[out] pdwWidth Pointer returns the width of the reference window.
	 *@param[out] pdwHeight Pointer returns the height of the reference window.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetWbWindow(HANDLE hCamera, DWORD *pdwLeft, DWORD *pdwTop, DWORD *pdwWidth, DWORD *pdwHeight);

	/**
	 *@ingroup __CK_WHITE_BLANCE__
	 *@~chinese
	 *@brief 设置白平衡参考窗口显示使能。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] bDisplayEn   显示使能 0：不显示；1：显示
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the white balance reference window display enable.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] bDisplayEn display enable 0: no display; 1: display
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetWbWinVisible(HANDLE hCamera, BOOL bDisplayEn);

	/**
	 *@ingroup __CK_WHITE_BLANCE__
	 *@~chinese
	 *@brief 获得白平衡参考窗口显示使能状态。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pDisplayEn   指针，白平衡参考窗口显示使能状态 0：不显示；1：显示。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the white balance reference window display enable status.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pDisplayEn Pointer, white balance reference window display enable status 0: no display; 1: display.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetWbWinVisible(HANDLE hCamera, BOOL *pDisplayEn);


	/**
	 *@ingroup __CK_ISP_GAMMA__
	 *@~chinese
	 *@brief 设置相机的查表变换模式LUT模式。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] emLutMode  LUTMODE_PARAM_GEN 表示由伽马和对比度参数动态生成LUT表。
	 LUTMODE_PRESET    表示使用预设的LUT表。
	 LUTMODE_USER_DEF  表示使用用户自定的LUT表。
	 LUTMODE_PARAM_GEN的定义参考CameraDefine.h中emSdkLutMode类型。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the camera's look-up table conversion mode LUT mode.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] emLutMode LUTMODE_PARAM_GEN Indicates that the LUT table is dynamically generated by gamma and contrast parameters.
	 LUTMODE_PRESET indicates that the preset LUT table is used.
	 LUTMODE_USER_DEF indicates the use of a user-defined LUT table.
	 The definition of LUTMODE_PARAM_GEN refers to the emSdkLutMode type in CameraDefine.h.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetLutMode(HANDLE hCamera, INT emLutMode);

	/**
	 *@ingroup __CK_ISP_GAMMA__
	 *@~chinese
	 *@brief 获得相机的查表变换模式LUT模式。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pemLutMode 指针，返回当前LUT模式。意义与CameraSetLutMode中emLutMode参数相同。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the camera's look-up table conversion mode LUT mode.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pemLutMode The pointer returns the current LUT mode. Meaning with CameraSetLutMode
	 The emLutMode parameter is the same.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetLutMode(HANDLE hCamera, INT *pemLutMode);

	/**
	 *@ingroup __CK_ISP_GAMMA__
	 *@~chinese
	 *@brief 设定LUT动态生成模式下的Gamma值。设定的值会
	 马上保存在SDK内部，但是只有当相机处于动态
	 参数生成的LUT模式时，才会生效。请参考@link #CameraSetLutMode @endlink
	 的函数说明部分。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iGamma     要设定的Gamma值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the gamma value in the LUT dynamic generation mode. The set value will
	 Saved inside the SDK right away, but only when the camera is dynamic
	 The LUT mode generated by the parameter will not take effect. Please refer to @link #CameraSetLutMode @endlink
	 The function description section.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iGamma The gamma value to be set.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetGamma(HANDLE hCamera, INT iGamma);

	/**
	 *@ingroup __CK_ISP_GAMMA__
	 *@~chinese
	 *@brief 获得LUT动态生成模式下的Gamma值。请参考@link #CameraSetGamma @endlink函数的功能描述。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] piGamma    指针，返回当前的Gamma值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the gamma value in LUT dynamic generation mode. Please refer to @link #CameraSetGamma @endlink
	 A functional description of the function.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piGamma pointer, which returns the current gamma value.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetGamma(HANDLE hCamera, INT* piGamma);

	/**
	 *@ingroup __CK_ISP_GAMMA__
	 *@~chinese
	 *@brief 设定LUT动态生成模式下的对比度值。设定的值会
	 马上保存在SDK内部，但是只有当相机处于动态
	 参数生成的LUT模式时，才会生效。请参考CameraSetLutMode
	 的函数说明部分。
	 *@param[in] hCamera  相机的句柄，由CameraInit函数获得。
	 *@param[in] iContrast  设定的对比度值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Set the contrast value in LUT dynamic generation mode. The set value will
	 Save it inside the SDK right away, but only if the camera is dynamic
	 The LUT mode generated by the parameter will not take effect. Please refer to CameraSetLutMode
	 The function description section.
	 *param[in] hCamera The handle of the camera, obtained by the CameraInit function.
	 *param[in] iContrast The contrast value set by iContrast.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetContrastLut(HANDLE hCamera, INT iContrast);

	/**
	 *@ingroup __CK_ISP_GAMMA__
	 *@~chinese
	 *@brief 获得LUT动态生成模式下的对比度值。请参考@link CameraSetContrastLut @endlink函数的功能描述。
	 *@param[in] hCamera  相机的句柄，由CameraInit函数获得。
	 *@param[out] piContrast 指针，返回当前的对比度值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the contrast value in LUT dynamic generation mode. Please refer to
	 the functional description of the @link CameraSetContrastLut @endlink function.
	 *param[in] hCamera The handle of the camera, obtained by the CameraInit function.
	 *param[out] piContrast The pointer which returns the current contrast value.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraSetContrastLut
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetContrastLut(HANDLE hCamera, INT* piContrast);

	/**
	 *@ingroup __CK_ISP_GAMMA__
	 *@~chinese
	 *@brief 选择预设LUT模式下的LUT表。必须先使用CameraSetLutMode
	 *        将LUT模式设置为预设模式。
	 *@param[in] hCamera  相机的句柄，由CameraInit函数获得。
	 *@param[in] iSel  表的索引号。表的个数由CameraGetCapability获得。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Select the LUT table in the preset LUT mode. Must use CameraSetLutMode first
	 *set the LUT mode to the preset mode.
	 *@param[in] hCamera The handle of the camera, obtained by the CameraInit function.
	 *@param[in] iSel The index number of the table. The number of tables by CameraGetCapability get.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSelectLutPreset(HANDLE hCamera, INT iSel);

	/**
	 *@ingroup __CK_ISP_GAMMA__
	 *@~chinese
	 *@brief 获得预设LUT模式下的LUT表索引号。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] piSel      指针，返回表的索引号。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the LUT table index number in the default LUT mode.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piSel pointer, which returns the index number of the table.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetLutPresetSel(HANDLE hCamera, INT *piSel);

	/**
	 *@ingroup __CK_ISP_GAMMA__
	 *@~chinese
	 *@brief 设置自定义的LUT表。必须先使用@link #CameraSetLutMode @endlink
	 将LUT模式设置为自定义模式。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iChannel 指定要获得的LUT颜色通道。当为LUT_CHANNEL_ALL时，
	 返回红色通道的LUT表。参考@link #emSdkLutChannel @endlink定义。
	 *@param[in] pLut     指针，指向LUT表的地址。LUT表为无符号短整形数组，数组大小为
	 4096，分别代码颜色通道从0到4096(12bit颜色精度)对应的映射值。 
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets a custom LUT table. Must use @link #CameraSetLutMode @endlink first
	 set the LUT mode to custom mode.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iChannel Specifies the LUT color channel to be obtained. When it is LUT_CHANNEL_ALL,
	 Returns the LUT table of the red channel. See @link #emSdkLutChannel @endlink definition.
	 *@param[in] pLut pointer to the address of the LUT table. The LUT table is an unsigned short integer array with an array size of
	 4096, the mapping value corresponding to the code color channel from 0 to 4096 (12 bit color precision).
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraGetCustomLut
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetCustomLut(HANDLE hCamera, INT iChannel, USHORT* pLut);

	/**
	 *@ingroup __CK_ISP_GAMMA__
	 *@~chinese
	 *@brief 获得当前使用的自定义LUT表。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iChannel 指定要获得的LUT颜色通道。当为LUT_CHANNEL_ALL时，
	 返回红色通道的LUT表。参考@link #emSdkLutChannel @endlink定义。
	 *@param[out] pLut       指针，指向LUT表的地址。LUT表为无符号短整形数组，数组大小为
	 4096，分别代码颜色通道从0到4096(12bit颜色精度)对应的映射值。 
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the custom LUT table currently in use.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iChannel Specifies the LUT color channel to be obtained. When it is LUT_CHANNEL_ALL,
	 Returns the LUT table of the red channel. See @link #emSdkLutChannel @endlink definition.
	 *@param[out] pLut The pointer to the address of the LUT table. The LUT table is an unsigned short integer array with an array size of
	 4096, the mapping value corresponding to the code color channel from 0 to 4096 (12 bit color precision).
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraSetCustomLut
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetCustomLut(HANDLE hCamera, INT iChannel, USHORT* pLut);

	/**
	 *@ingroup __CK_ISP_GAMMA__
	 *@~chinese
	 *@brief 获得相机当前的LUT表，在任何LUT模式下都可以调用, 用来直观的观察LUT曲线的变化。
	 *@param[in] hCamera  相机的句柄，由CameraInit函数获得。
	 *@param[in] iChannel 指定要获得的LUT颜色通道。当为LUT_CHANNEL_ALL时，返回红色通道的LUT表。
	 *                      参考CKDeviceDef.h中emSdkLutChannel定义。
	 *@param[out] pLut 指针，指向LUT表的地址。LUT表为无符号短整形数组，数组大小为
	 *           4096，分别代码颜色通道从0到4096(12bit颜色精度)对应的映射值。 
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the camera's current LUT table, which can be called in any LUT mode.
	 Used to visually observe changes in the LUT curve.
	 *@param[in] hCamera The handle of the camera, obtained by the CameraInit function.
	 *@param[in] iChannel specifies the LUT color channel to get. When it is LUT_CHANNEL_ALL,
	 Returns the LUT table of the red channel. Refer to the emSdkLutChannel definition in CKDeviceDef.h.
	 *@param[out] pLut pointer to the address of the LUT table. The LUT table is an unsigned short integer array with an array size of
	 4096, the mapped value of the code color channel from 0 to 4096 (12bit color precision).
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetCurrentLut(HANDLE hCamera, INT iChannel, USHORT* pLut);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 设置图像的处理的锐化参数。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iSharpness 锐化参数。范围由@link #CameraGetCapability @endlink
	 获得，一般是[0,100]，0表示关闭锐化处理。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 该函数设置图像的锐化级别。锐化度越高，图像清晰度越好，但是噪
	 声也会越大；反之锐化度越低，图像朦胧感就强，但是噪声降低，显
	 得平滑。默认值是 0，就是没有锐化增强的效果。
	 *@~english
	 *@brief Sets the sharpening parameters for the processing of the image.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iSharpness Sharpen the parameters. Range by @link #CameraGetCapability @endlink
	 Obtained, generally [0, 100], 0 means that the sharpening process is turned off.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note This function sets the sharpening level of the image. The higher the sharpness, the better the image clarity, but the noise
	 The sound will be louder; on the contrary, the lower the sharpness, the stronger the image will be, but the noise will be reduced.
	 Smooth. The default value is 0, which means there is no sharpening enhancement.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetSharpness(HANDLE hCamera, INT iSharpness);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 获取当前锐化设定值。
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] piSharpness 指针，返回当前设定的锐化的设定值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the current sharpening setting.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piSharpness The pointer returns the currently set sharpening setting.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetSharpness(HANDLE hCamera, INT *piSharpness);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 设定图像处理的饱和度。对黑白相机无效。
	 设定范围由CameraGetCapability获得。100表示
	 表示原始色度，不增强。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iSaturation  设定的饱和度值。 
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 该函数设置图像饱和度。饱和度越大色彩越浓；反之越淡，饱和度如
	 果设置为 0，图像就完全没有色彩了，等效于黑白相机。默认值是
	 100。调节范围是 0 到 200。
	 *@~english
	 *@brief Sets the saturation of image processing. Not valid for black and white cameras.
	 The setting range is obtained by CameraGetCapability. 100 means
	 Indicates the original chroma, not enhanced.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iSaturation The saturation value set by iSaturation.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note This function sets the image saturation. The greater the saturation, the darker the color; the lighter the contrast, the more saturated
	 If set to 0, the image is completely colorless, equivalent to a black and white camera. The default value is
	 100. The adjustment range is from 0 to 200.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetSaturation(HANDLE hCamera, INT iSaturation);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 获得图像处理的饱和度。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] piSaturation 指针，返回当前图像处理的饱和度值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the saturation of image processing.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piSaturation Pointer, which returns the saturation value of the current image processing.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetSaturation(HANDLE hCamera, INT *piSaturation);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 设定图像处理的对比度
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iContrast  设定的对比度值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 该函数设置相机的对比度值，对比度越大，会使图像黑的区域越黑，
	 白的区域越白，使得图像看起来黑白分明，在一些视觉处理上可以有
	 效的捕捉轮廓；反之，如果对比度越小，会是黑白不分明，看起来比
	 较朦胧。对比度默认是 50，最小可以到 1，最大到 100。
	 *@~english
	 *@brief sets the contrast of image processing
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iContrast Contrast value set by iContrast.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note This function sets the contrast value of the camera. The higher the contrast, the darker the black area of ??the image.
	 The whiter the white area, the image looks black and white, and there may be some visual processing.
	 Effectively capture the contour; conversely, if the contrast is smaller, it will be black and white and it will look better than
	 More ambiguous. The contrast default is 50, and the minimum can be up to 1, up to 100.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetContrast(HANDLE hCamera, INT iContrast);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 获得对比度值。请参考CameraSetContrast函数的功能描述。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] piContrast 指针，返回当前的对比度值。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the contrast value. Please refer to
	 A functional description of the CameraSetContrast function.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piContrast The pointer returns the current contrast value.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetContrast(HANDLE hCamera, INT *piContrast);

	/**
	 *@ingroup __CK_ADVANCED_SETTINGS__
	 *@~chinese
	 *@brief 获得当前图像帧时间。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pdFrameTime 指针，返回当前的帧时间，单位微秒。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the current image frame time.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pdFrameTime The pointer returns the current frame time in microseconds.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetFrameTime(HANDLE hCamera, double *pdFrameTime);

	/**
	 *@ingroup __CK_ADVANCED_SETTINGS__
	 *@~chinese
	 *@brief 设定相机输出图像的帧率。相机可供选择的帧率模式由
	 CameraGetCapability获得的信息结构体中iFrameSpeedDesc表示最大帧率选择模式个数。
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iFrameSpeed 选择的帧率模式索引号，范围从0到
	 @link #CameraGetCapability @endlink获得的信息结构体中iFrameSpeedDesc - 1   
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 通过 CameraSetFrameSpeed 函数，可以动态设置相机的输出帧率。
	 CameraSetFrameSpeed 是设置速度的档位，我们将其分为高速、中速、
	 低速等几种模式，对于不同型号的相机，高中低速度模式下对应具体
	 的帧率是不一样的。
	 CameraSetFrameSpeed（hCamera,0） ;//设置为低速模式。
	 CameraSetFrameSpeed（hCamera,1） ;//设置为中速模式。
	 CameraSetFrameSpeed（hCamera,2） ;//设置为高速模式。
	 一般相机出厂时，默认就是最高速度的，如果需要降速运行，就可以
	 调用 CameraSetFrameSpeed 函数迕行降速。 另外，有些型号的相机，
	 支持 2 个速度模式，有些支持 4 个，具体支持模式的数量，可以通
	 过 CameraGetCapability 函数，得到相机的描述信息
	 tSdkCameraCapbility 结极体， tSdkCameraCapbility 结极体中的
	 iFrameSpeedDesc 成员，表明了当前型号的相机支持的速度模式的个
	 数。
	 *@brief Sets the frame rate of the camera output image. The camera's available frame rate mode consists of
	 iFrameSpeedDesc in the information structure obtained by CameraGetCapability
	 Indicates the maximum number of frame rate selection modes.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iFrameSpeed Frame rate mode index number selected by iFrameSpeed, ranging from 0 to
	 @link #CameraGetCapability @endlink obtained information structure in iFrameSpeedDesc - 1
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note The camera's output frame rate can be dynamically set via the CameraSetFrameSpeed ??function.
	 CameraSetFrameSpeed ??is the gear to set the speed, we divide it into high speed, medium speed,
	 Several modes, such as low speed, for different models of cameras, corresponding to specific high, medium and low speed modes
	 The frame rate is not the same.
	 CameraSetFrameSpeed(hCamera,0) ;// is set to low speed mode.
	 CameraSetFrameSpeed(hCamera,1) ;// is set to medium speed mode.
	 CameraSetFrameSpeed(hCamera,2) ;// is set to high speed mode.
	 Generally, when the camera is shipped from the factory, the default is the highest speed. If you need to run at a reduced speed, you can
	 Call the @link CameraSetFrameSpeed @endlink function to slow down. In addition, some models of cameras,
	 Support 2 speed modes, some support 4, the number of specific support modes, can pass
	 Get the description of the camera through the CameraGetCapability function
	 tSdkCameraCapbility junction body, tSdkCameraCapbility
	 iFrameSpeedDesc member, indicating the speed mode supported by the camera of the current model
	 Number.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetFrameSpeed(HANDLE hCamera, INT iFrameSpeed);

	/**
	 *@ingroup __CK_ADVANCED_SETTINGS__
	 *@~chinese
	 *@brief 获得相机输出图像的帧率选择索引号。具体用法参考
	 @link #CameraSetFrameSpeed @endlink函数的功能描述部分。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] piFrameSpeed 指针，返回选择的帧率模式索引号。 
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the frame rate selection index number of the camera output image. Specific usage reference
	 @link #CameraSetFrameSpeed ??The functional description section of the @endlink function.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piFrameSpeed ??pointer, which returns the selected frame rate mode index number.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetFrameSpeed(HANDLE hCamera, INT *piFrameSpeed);

	/**
	 *@ingroup __CK_ROI_SETTINGS__
	 *@~chinese
	 *@brief 设定相机输出图像的分辨率。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iResolutionIndex 选择的帧率模式索引号,相机可供选择的分辨率模式由
	 @link #CameraGetCapability @endlink获得的信息结构体中iImageSizeDesc
	 表示可以选择的分辨率模式个数。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the resolution of the camera output image.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iResolutionIndex The selected frame rate mode index number, the resolution mode that the camera can choose from
	 @link #CameraGetCapability @endlink Get the information structure in iImageSizeDesc
	 Indicates the number of resolution modes that can be selected.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetResolution(HANDLE hCamera, INT iResolutionIndex);

	/**
	 *@ingroup __CK_ROI_SETTINGS__
	 *@~chinese
	 *@brief 获得相机输出图像的分辨率。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] piResolutionIndex 指针，返回选择的帧率模式索引号,请参见emResolutionMode。 
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the resolution of the camera output image.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piResolutionIndex Pointer to return the selected frame rate mode index number, see emResolutionMode.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetResolution(HANDLE hCamera, INT *piResolutionIndex);

	/**
	 *@ingroup __CK_ROI_SETTINGS__
	 *@~chinese
	 *@brief 设置自定义预览的分辨率。
	 *@param[in] hCamera      相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] pImageResolution 结构体指针，用于返回当前的分辨率。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the resolution of the custom preview.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] pImageResolution Structure pointer to return the current resolution.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetResolutionEx(HANDLE hCamera, tSdkImageResolution* pImageResolution);

	/**
	 *@ingroup __CK_ROI_SETTINGS__
	 *@~chinese
	 *@brief 获得索引号的分辨率。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iResolution 获得分辨率的索引号，参见 emResolutionMode，自定义为0xff
	 *@param[out] pImageResolution 结构体指针，用于返回当前的分辨率。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the resolution of the index number.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iResolution Get the index number of the resolution, see emResolutionMode, custom 0xff
	 *@param[out] pImageResolution Structure pointer to return the current resolution.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetResolutionEx(HANDLE hCamera, INT iResolution, tSdkImageResolution* pImageResolution);

	/**
	 *@ingroup __CK_ROI_SETTINGS__
	 *@~chinese
	 *@brief 获得当前预览的分辨率。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pImageResolution 结构体指针，用于返回当前的分辨率。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the resolution of the current preview.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pImageResolution Structure pointer to return the current resolution.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetCurResolution(HANDLE hCamera, tSdkImageResolution* pImageResolution);

	/**
	 *@ingroup __CK_ROI_SETTINGS__
	 *@~chinese
	 *@brief 获得输出图像的大小。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pWidth	指针，用于返回输出图像的宽度。
	 *@param[out] pHeight	指针，用于返回输出图像的高度。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the size of the output image.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pWidth Pointer to return the width of the output image.
	 *@param[out] pHeight The pointer is used to return the height of the output image.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetOutImageSize(HANDLE hCamera, DWORD *pWidth, DWORD *pHeight);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 设置Bayer转RGB算法模式。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iAlgMode	 Bayer转RGB算法模式
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the Bayer to RGB algorithm mode.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iAlgMode Bayer to RGB algorithm mode
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetBayerAlgMode(HANDLE hCamera, INT iAlgMode);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 设置Bayer转RGB算法模式。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iAlgMode	 Bayer转RGB算法模式
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the Bayer to RGB algorithm mode.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iAlgMode Bayer to RGB algorithm mode
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetBayerAlgThreshold(HANDLE hCamera, INT iAlgMode);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 获得Bayer转RGB算法模式。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] piAlgMode	 指针 返回Bayer转RGB算法模式
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get Bayer to RGB algorithm mode.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piAlgMode pointer Returns Bayer to RGB algorithm mode
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetBayerAlgMode(HANDLE hCamera, INT *piAlgMode);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 设置CameraImageProcess函数的图像处理的输出格式，支持
	 @link #CAMERA_MEDIA_TYPE_MONO @endlink
	 @link #CAMERA_MEDIA_TYPE_RGB8 @endlink
	 @link #CAMERA_MEDIA_TYPE_RGBA8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGR8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGRA8 @endlink
	 (在CameraDefine.h中定义)5种，分别对应8位灰度图像和24RGB、32位RGB、24位BGR、32位BGR彩色图像。
	 默认输出是@link #CAMERA_MEDIA_TYPE_BGR8 @endlink格式。
	 *@param[in] hCamera		相机的句柄，由@link #CameraInit @endlink函数获得 
	 *@param[in] iFormat	要设定格式。
	 @link #CAMERA_MEDIA_TYPE_MONO @endlink
	 @link #CAMERA_MEDIA_TYPE_RGB8 @endlink
	 @link #CAMERA_MEDIA_TYPE_RGBA8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGR8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGRA8 @endlink
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 该函数是最终通过 @link #CameraGetOutImageBuffer @endlink 函数的得到的图像像
	 素格式，而并非相机输出的原始 RAW 的像素格式。
	 在相机初始化以后，通过调用 @link CameraSetIspOutFormat @endlink 来设置了。
	 *-# CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_MONO);
	 设置后， CameraGetOutImageBuffer 输出的图像就是 8bit 的灰度图
	 像了， 1 个像素占用 1 个字节，依次排列。
	 *-# CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_RGB8);
	 设置后， CameraGetOutImageBuffer 输出的图像就是 24bit RBG 的
	 彩色图像了， 1 个像素占用 3 个字节，依次是红色、 绿色、 蓝色
	 返样排列。
	 *-# CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_BGR8);
	 设置后， CameraGetOutImageBuffer 输出的图像就是 24bit BGR 的
	 彩色图像了， 1 个像素占用 3 个字节，依次是蓝色、 绿色、 红色
	 排列。
	 *-# CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_RGBA8);
	 设置后， CameraGetOutImageBuffer 输出的图像就是 32bit RBGA 的
	 彩色图像了， 1 个像素占用 4 个字节，依次是红色 8bit、 绿色
	 8bit、 蓝色 8bit、 Alpha8bit 排列。
	 *-# CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_BGRA8);
	 设置后， CameraGetOutImageBuffer 输出的图像就是 32bit BGRA 的
	 彩色图像了， 1 个像素占用 3 个字节，依次是蓝色、 绿色、 红色、
	 Alpha 排列。
	 *@~english
	 *@brief Sets the output format of the image processing of the CameraImageProcess function, which supports
	 @link #CAMERA_MEDIA_TYPE_MONO @endlink
	 @link #CAMERA_MEDIA_TYPE_RGB8 @endlink
	 @link #CAMERA_MEDIA_TYPE_RGBA8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGR8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGRA8 @endlink
	 Five types (defined in CameraDefine.h) correspond to 8-bit grayscale images and 24RGB, 32-bit RGB, 24-bit BGR, 32-bit BGR color images.
	 The default output is @link #CAMERA_MEDIA_TYPE_BGR8 @endlink format.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iFormat To format.
	 @link #CAMERA_MEDIA_TYPE_MONO @endlink
	 @link #CAMERA_MEDIA_TYPE_RGB8 @endlink
	 @link #CAMERA_MEDIA_TYPE_RGBA8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGR8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGRA8 @endlink
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note This function is the final image image obtained by @link #CameraGetOutImageBuffer @endlink function
	 The prime format, not the raw RAW pixel format of the camera output.
	 After the camera is initialized, it is set by calling @link CameraSetIspOutFormat @endlink.
	 *-# CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_MONO);
	 After setting, the image output by CameraGetOutImageBuffer is an 8-bit grayscale image.
	 Like, 1 pixel occupies 1 byte, arranged in order.
	 *-# CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_RGB8);
	 After setting, the image output by CameraGetOutImageBuffer is 24bit RBG
	 Color image, 1 pixel occupies 3 bytes, followed by red, green, blue
	 Return to the sample.
	 *-# CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_BGR8);
	 After setting, the image output by CameraGetOutImageBuffer is 24bit BGR
	 Color image, 1 pixel occupies 3 bytes, followed by blue, green, red
	Arrange.
		*-# CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_RGBA8);
	After setting, the image output by CameraGetOutImageBuffer is 32bit RBGA
		Color image, 1 pixel occupies 4 bytes, followed by red 8bit, green
		8bit, blue 8bit, Alpha8bit arrangement.
		*-# CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_BGRA8);
	After setting, the image output by CameraGetOutImageBuffer is 32bit BGRA
		Color image, 1 pixel occupies 3 bytes, followed by blue, green, red, Alpha arrangement.
	*/
	CKSDK_API CameraSdkStatus __stdcall CameraSetIspOutFormat(HANDLE hCamera, INT iFormat);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 获得@link #CameraGetOutImageBuffer @endlink函数图像处理的输出格式，支持
	 @link #CAMERA_MEDIA_TYPE_MONO @endlink
	 @link #CAMERA_MEDIA_TYPE_RGB8 @endlink
	 @link #CAMERA_MEDIA_TYPE_RGBA8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGR8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGRA8 @endlink
	 (在CameraDefine.h中定义)5种，分别对应8位灰度图像和24RGB、32位RGB、24位BGR、32位BGR彩色图像。
	 *@param[in] hCamera		相机的句柄，由@link #CameraInit @endlink函数获得 
	 *@param[out] piFormat	返回当前设定的格式。
	 @link #CAMERA_MEDIA_TYPE_MONO @endlink
	 @link #CAMERA_MEDIA_TYPE_RGB8 @endlink
	 @link #CAMERA_MEDIA_TYPE_RGBA8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGR8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGRA8 @endlink
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the output format of @link #CameraGetOutImageBuffer @endlink function image processing, support
	 @link #CAMERA_MEDIA_TYPE_MONO @endlink
	 @link #CAMERA_MEDIA_TYPE_RGB8 @endlink
	 @link #CAMERA_MEDIA_TYPE_RGBA8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGR8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGRA8 @endlink
	 Five types (defined in CameraDefine.h) correspond to 8-bit grayscale images and 24RGB, 32-bit RGB, 24-bit BGR, 32-bit BGR color images.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piFormat Returns the currently set format. 
	 @link #CAMERA_MEDIA_TYPE_MONO @endlink
	 @link #CAMERA_MEDIA_TYPE_RGB8 @endlink
	 @link #CAMERA_MEDIA_TYPE_RGBA8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGR8 @endlink
	 @link #CAMERA_MEDIA_TYPE_BGRA8 @endlink
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetIspOutFormat(HANDLE hCamera, INT *piFormat);

	/**
	 *@ingroup __CK_ADVANCED_SETTINGS__
	 *@~chinese
	 *@brief 获得相机接收帧率的统计信息，包括错误帧和丢帧的情况。
	 *@param[in] hCamera        相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] psFrameStatistic 指针，返回统计信息。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets statistics on the frame rate received by the camera, including error frames and dropped frames.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] psFrameStatistic pointer, which returns statistics.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetFrameStatistic(HANDLE hCamera, FrameStatistic *psFrameStatistic);

	/**
	 *@ingroup __CK_SAVE_IMAGE__
	 *@~chinese
	 *@brief 将图像缓冲区的数据保存成图片文件。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] lpszFileName   图片保存文件完整路径。
	 *@param[in] pImgBuf 图像的数据缓冲区。
	 *@param[in] pImgInfo        图像的帧头信息。
	 *@param[in] byFileType     @link #emFileType @endlink。目前支持  
	 BMP、JPG、PNG、RAW四种格式。其中RAW表示
	 相机输出的原始数据，保存RAW格式文件要求
	 pImgBuf和pImgInfo是由@link #CameraGetOutImageBuffer @endlink
	 获得的数据，而且未经CameraImageProcess转换
	 成BMP格式；反之，如果要保存成BMP格式，则pImgBuf和pImgInfo是由
	 CameraImageProcess处理后的RGB格式数据。
	 *@param[in] byQuality      图像保存的质量因子，仅当保存为JPG格式时该参数有效，范围1到100。其余格式可以写成0。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 图像保存功能。使用 CameraSaveImage，可以将图像保存为 RAW、
	 * BMP、 JPG、 PNG 的其中一种。一个典型的流程为：
	 *-# 使用 @link #CameraGetRawImageBuffer @endlink 函数和 @link #CameraGetImageInfo @endlink 函
	 * 数取到 RAW 图像数据；
	 *-# 使用 @link #CameraGetOutImageBuffer @endlink 函数将 RAW 转换成 BGR24 或者 8 位灰度格式。如果要保存 RAW 格式数据，返一步跳过。
	 *-# @link #CameraReleaseFrameHandle @endlink释放 @link #CameraGetRawImageBuffer @endlink 的
	 * 到的 RAW 数据缓冲区的使用权。
	 *-# @link #CameraSaveImage @endlink 函数将 @link #CameraGetOutImageBuffer @endlink 函数得到的
	 * 图像数据进行保存成BMP。如果是RAW数据保存则使用@link #CameraGetRawImageBuffer @endlink 函数和 @link #CameraGetImageInfo @endlink 函数得到的
	 RAW 数据 buffer。
	 *@~english
	 *@brief Saves the image buffer data as an image file.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] lpszFileName The full path to the image save file.
	 *@param[in] pImgBuf The data buffer for the pImgBuf image.
	 *@param[in] pImgInfo The header information of the image.
	 *@param[in] byFileType @link #emFileType @endlink. Currently supported
	 BMP, JPG, PNG, RAW four formats. Where RAW is represented
	 Raw data output by the camera, saving RAW format file requirements
	 pImgBuf and pImgInfo are by @link #CameraGetOutImageBuffer @endlink
	 The data obtained, and not converted by CameraImageProcess
	 In BMP format; conversely, if you want to save to BMP format, pImgBuf and pImgInfo are
	 CameraImageProcess processed RGB format data.
	 *@param[in] byQuality The quality factor of the image save. This parameter is valid only when saved as JPG format, ranging from 1 to 100. The rest of the format can be written as 0.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note Image saving function. Save images as RAW, using CameraSaveImage
	 * One of BMP, JPG, PNG. A typical process is:
	 *-# Use @link #CameraGetRawImageBuffer @endlink function and @link #CameraGetImageInfo @endlink
	 * Number is taken to RAW image data;
	 *-# Use the @link #CameraGetOutImageBuffer @endlink function to convert RAW to BGR24 or 8-bit grayscale format. If you want to save RAW format data, skip back.
	 *-# @link #CameraReleaseFrameHandle @endlink release frame buffer. @link #CameraGetRawImageBuffer @endlink The right to use the RAW data buffer.
	 *-# @link #CameraSaveImage @endlink function gets @link #CameraGetOutImageBuffer @endlink function
	 * Image data is saved as BMP. If the RAW data is saved, use the @link #CameraGetRawImageBuffer @endlink function and the @link #CameraGetImageInfo @endlink function.
	 RAW data buffer.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSaveImage(HANDLE hCamera, const char* lpszFileName, const BYTE* pImgBuf, const stImageInfo* pImgInfo, UINT byFileType, BYTE byQuality);

	/**
	 *@ingroup __CK_MIRROR_AND_ROTATE__
	 *@~chinese
	 *@brief 设置图像镜像操作。镜像操作分为水平和垂直两个方向。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iDir     表示镜像的方向。0，表示水平方向；1，表示垂直方向。
	 *@param[in] bEnable  TRUE，使能镜像;FALSE，禁止镜像
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the image mirroring operation. The mirroring operation is divided into two directions, horizontal and vertical.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iDir indicates the direction of the image. 0, indicating the horizontal direction; 1, indicating the vertical direction.
	 *@param[in] bEnable TRUE, enable mirroring; FALSE, disable mirroring
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetMirror(HANDLE hCamera, INT iDir, BOOL bEnable);

	/**
	 *@ingroup __CK_MIRROR_AND_ROTATE__
	 *@~chinese
	 *@brief 获得图像的镜像状态。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iDir     表示要获得的镜像方向。
	 0，表示水平方向；1，表示垂直方向。
	 *@param[out] pbEnable   指针，返回TRUE，则表示iDir所指的方向镜像被使能。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the mirrored state of the image.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iDir indicates the mirror direction to be obtained.
	 0, indicating the horizontal direction; 1, indicating the vertical direction.
	 *@param[out] pbEnable pointer, return TRUE, indicating the direction pointed to by iDir Mirroring is enabled.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetMirror(HANDLE hCamera, INT iDir, BOOL *pbEnable);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 设置彩色转为黑白功能的使能。
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] bEnable   TRUE，表示将彩色图像转为黑白。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the color to black and white function enable.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] bEnable TRUE, which means that the color image is converted to black and white.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetMonochrome(HANDLE hCamera, BOOL bEnable);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 获得彩色转换黑白功能的使能状况。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pbEnable   指针。返回TRUE表示开启了彩色图像转换为黑白图像的功能。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the enable status of the color conversion black and white function.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pbEnable pointer. Return TRUE to turn on color image
	 The ability to convert to black and white images.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetMonochrome(HANDLE hCamera, BOOL *pbEnable);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 设置图像的黑电平基准，默认值为0
	 *@param[in] hCamera   相机的句柄，由@link #CameraInit @endlink函数获得 
	 *@param[in] iBlackLevel 要设定的电平值。范围为0到255。  
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief sets the black level reference of the image. The default value is 0.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iBlackLevel The level value to be set. The range is from 0 to 255.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetBlackLevel(HANDLE hCamera, INT iBlackLevel);

	/**
	 *@ingroup __CK_ISP__
	 *@~chinese
	 *@brief 获得图像的黑电平基准，默认值为0
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得 
	 *@param[out] piBlackLevel 返回当前的黑电平值。范围为0到255。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the black level reference of the image, the default is 0
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piBlackLevel Returns the current black level value. The range is from 0 to 255.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetBlackLevel(HANDLE hCamera, INT* piBlackLevel);

	/**
	*@ingroup __CK_ISP__
	*@~chinese
	*@brief 设置原始图像数据的格式
	*@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	*@param[in] iFormat 原始图像数据的格式。
	*@return @link #__CK_ERRCODE__ 状态码@endlink
	*@note 相机支持哪些原始图像格式可以通过 @link #CameraGetCapability @endlink获取，支持格式在@link #tDeviceCfg @endlink的pBayerTypeDesc数组结构体中。
	*@~english
	*@brief Set the format of the raw image data
	*@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	*@param[out] iFormat The format of the raw image data.
	*@note The original image formats supported by the camera are available via @link #CameraGetCapability @endlink and are supported 
	* in the pBayerTypeDesc array structure of @link #tDeviceCfg @endlink.
	*@return @link #__CK_ERRCODE__ Status Code @endlink
	*@see CameraGetSensorOutPixelFormat
	*@see CameraGetCapability
	*/
	CKSDK_API CameraSdkStatus __stdcall CameraSetSensorOutPixelFormat(HANDLE hCamera, UINT iFormat);

	/**
	*@ingroup __CK_ISP__
	*@~chinese
	*@brief 获取原始图像数据的格式
	*@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	*@param[out] piFormat 原始图像数据的格式。
	*@return @link #__CK_ERRCODE__ 状态码@endlink
	*@~english
	*@brief Get the format of the raw image data
	*@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	*@param[out] piFormat The format of the raw image data.
	*@return @link #__CK_ERRCODE__ Status Code @endlink
	*@see CameraSetSensorOutPixelFormat
	*@see CameraGetCapability
	*/
	CKSDK_API CameraSdkStatus __stdcall CameraGetSensorOutPixelFormat(HANDLE hCamera, UINT *piFormat);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 设置相机的触发模式。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iTriggerModeSel   模式选择索引号。可设定的模式由
	 @link #CameraGetCapability @endlink函数获取。请参考
	 CameraDefine.h中@link #tSdkCameraCapbility @endlink的定义。
	 一般情况，
	 *-# 0.表示连续采集模式；
	 *-# 1.表示软件触发模式；
	 *-# 2.表示硬件触发模式。  
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 设置为硬件触发模式时，请先通过@link CameraSetInputIOMode @endlink 设置某个输入IO为触发工作模式。
	 *@see CameraSetInputIOMode
	 *@~english
	 *@brief Sets the camera's trigger mode.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iTriggerModeSel The mode selects the index number. Settable mode
	 @link #CameraGetCapability @endlink function gets. Please refer to
	 The definition of @link #tSdkCameraCapbility @endlink in CameraDefine.h.
	 generally,
	 *-# 0. indicates continuous acquisition mode;
	 *-# 1. indicates the software trigger mode;
	 *-# 2. Indicates the hardware trigger mode.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note When setting to hardware trigger mode, first set an input IO to trigger working mode by @link CameraSetInputIOMode @endlink.
	 *@see CameraSetInputIOMode
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetTriggerMode(HANDLE hCamera, INT iTriggerModeSel);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 获得相机的触发模式。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] piTriggerModeSel  指针，返回当前选择的相机触发模式的索引号。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the trigger mode of the camera.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piTriggerModeSel Pointer, which returns the index number of the currently selected camera trigger mode.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetTriggerMode(HANDLE hCamera, INT *piTriggerModeSel);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 执行一次软触发。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note相机出厂默认就处于连续取图模式，也可以通过
	 CameraSetTriggerMode(hCamera,0);切换为连续取图模式。
	 *-# 1.设置相机为软触发取图模式。通过
	 CameraSetTriggerMode(hCamera,1);切换为软触发取图模式。进入该
	 模式后，相机停止图像采集和发送。只有当用户调用
	 CameraSoftTrigger(hCamera)一次，相机就采集一次图像发送上来。
	 发完之后相机又会进入等待状态，直到下次用户调用
	 CameraSoftTrigger(hCamera)。
	 *-# 2.设置相机为硬触发取图模式。通过
	 CameraSetTriggerMode(hCamera,2);切换为硬触发取图模式。进入该
	 模式后，相机停止图像采集和发送。 只有当用户在相机外壳上的触
	 发端子上输入一个脉冲时，相机就采集一次图像发送上
	 来。发完之后相机又会进入等待状态，直到下次收到触发脉冲。
	 *@see CameraSetTriggerMode
	 *@see CameraSetTriggerFrameCount
	 *@see CameraSetTriggerDelayTime
	 *@~english
	 *@brief performs a soft trigger.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note camera defaults to continuous drawing mode and can also pass
	 CameraSetTriggerMode(hCamera,0); Switches to continuous drawing mode.
	 *-# 1. Set the camera to soft trigger capture mode. by
	 CameraSetTriggerMode(hCamera,1); Switch to soft trigger capture mode. Enter this
	 After the mode, the camera stops image acquisition and transmission. Only when the user calls
	 CameraSoftTrigger (hCamera) once, the camera collects an image and sends it up.
	 After the hair is sent, the camera will enter the waiting state again until the next user call.
	 CameraSoftTrigger (hCamera).
	 *-# 2. Set the camera to hard trigger capture mode. by
	 CameraSetTriggerMode(hCamera, 2); Switch to hard trigger capture mode. Enter this
	 After the mode, the camera stops image acquisition and transmission. Only when the user touches the camera case
	 When a pulse is input on the terminal, the camera collects an image and sends it on.
	 Come. After the hair is sent, the camera will enter the waiting state until the next time the trigger pulse is received.
	 *@see CameraSetTriggerMode
	 *@see CameraSetTriggerFrameCount
	 *@see CameraSetTriggerDelayTime
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSoftTrigger(HANDLE hCamera);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 设置触发时连续拍多少张图片
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] frameCount 连续拍多少张图片
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraSetTriggerMode
	 *@see CameraSoftTrigger
	 *@~english
	 *@brief Sets how many pictures to take continuously when camera triggers
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] frameCount How many pictures are taken in a row?
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraSetTriggerMode
	 *@see CameraSoftTrigger
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetTriggerFrameCount(HANDLE hCamera, INT frameCount);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 获取触发时连续拍多少张图片
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pFrameCount 连续拍多少张图片
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get how many pictures are taken continuously when the camera is triggered
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pFrameCount How many pictures are taken in a row?
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetTriggerFrameCount(HANDLE hCamera, INT *pFrameCount);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 设置触发后多少时间进行拍照
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] delayTimeUs 延时时间，单位微秒
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Set how much time to take a photo after camera trigger
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] delayTimeUs delay time in microseconds
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetTriggerDelayTime(HANDLE hCamera, UINT delayTimeUs);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 设置触发后多少时间进行拍照
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pDelayTimeUs 延时时间，单位微秒
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Set how much time to take a photo after camera trigger
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pDelayTimeUs delay time in microseconds
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetTriggerDelayTime(HANDLE hCamera, UINT *pDelayTimeUs);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 设置硬件触发后去抖时间
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] inputIOIndex 输入IO端口索引号
	 *@param[in] jitterTimeUs 去抖时间，单位微秒
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Set the hardware to trigger the debounce time
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] inputIOIndex Input IO port index number
	 *@param[in] jitterTimeUs debounce time in microseconds
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetTriggerJitterTime(HANDLE hCamera, INT inputIOIndex, UINT jitterTimeUs);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 获取硬件触发后去抖时间
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] inputIOIndex 输入IO端口索引号
	 *@param[out] pJitterTimeUs 去抖时间，单位微秒
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the hardware to trigger the debounce time
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] inputIOIndex Input IO port index number
	 *@param[out] pJitterTimeUs Debounce time, in microseconds
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetTriggerJitterTime(HANDLE hCamera, INT inputIOIndex, UINT *pJitterTimeUs);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 设置硬件触发信号类型
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] inputIOIndex 输入IO端口索引号
	 *@param[in] type 触发类型，请参考@link #emExtTrigSignal @endlink
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief sets the hardware trigger signal type
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] inputIOIndex Input IO port index number
	 *@param[in] type trigger type, please refer to @link #emExtTrigSignal @endlink
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetTriggerSignalType(HANDLE hCamera, INT inputIOIndex, INT type);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 获取硬件触发信号类型
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] inputIOIndex 输入IO端口索引号
	 *@param[out] pType 触发类型，请参考@link #emExtTrigSignal @endlink
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get hardware trigger signal type
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] inputIOIndex Input IO port index number
	 *@param[out] pType trigger type, please refer to @link #emExtTrigSignal @endlink
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetTriggerSignalType(HANDLE hCamera, INT inputIOIndex, INT *pType);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 设置闪光灯工作模式
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] outputIOIndex 输出IO端口索引号
	 *@param[in] mode 触发类型，请参考@link #emStrobeControl @endlink
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Set flash mode
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] outputIOIndex Output IO port index number
	 *@param[in] mode trigger type, please refer to @link #emStrobeControl @endlink
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetStrobeMode(HANDLE hCamera, INT outputIOIndex, INT mode);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 获取闪光灯工作模式
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] outputIOIndex 输出IO端口索引号
	 *@param[out] pMode 触发类型，请参考@link #emStrobeControl @endlink
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get flash mode
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] outputIOIndex Output IO port index number
	 *@param[out] pMode trigger type, please refer to @link #emStrobeControl @endlink
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetStrobeMode(HANDLE hCamera, INT outputIOIndex, INT *pMode);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 设置触发后延时多少时间点亮闪光灯
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] outputIOIndex 输出IO端口索引号
	 *@param[in] delayTimeUs 延时时间，单位微秒
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Set how long to delay the flash after the trigger
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] outputIOIndex Output IO port index number
	 *@param[in] delayTimeUs delay time in microseconds
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetStrobeDelayTime(HANDLE hCamera, INT outputIOIndex, UINT delayTimeUs);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 获取触发后点亮闪光灯的延时时间
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] outputIOIndex 输出IO端口索引号
	 *@param[out] pDevlayTimeUs 延时时间，单位微秒
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Gets the delay time to illuminate the flash after triggering
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] outputIOIndex Output IO port index number
	 *@param[out] pDevlayTimeUs delay time in microseconds
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetStrobeDelayTime(HANDLE hCamera, INT outputIOIndex, UINT *pDevlayTimeUs);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 设置闪光灯的脉宽宽度
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] outputIOIndex 输出IO端口索引号
	 *@param[in] pulseWidth 脉宽宽度，单位微秒
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief sets the pulse width of the flash
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] outputIOIndex Output IO port index number
	 *@param[in] pulseWidth width width in microseconds
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetStrobePulseWidth(HANDLE hCamera, INT outputIOIndex, UINT pulseWidth);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 获取闪光灯的脉宽宽度
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] outputIOIndex 输出IO端口索引号
	 *@param[out] pPulseWidth 脉宽宽度，单位微秒
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the pulse width of the flash
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] outputIOIndex Output IO port index number
	 *@param[out] pPulseWidth Pulse width in microseconds
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetStrobePulseWidth(HANDLE hCamera, INT outputIOIndex, UINT *pPulseWidth);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 设置闪光灯的有效电平
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] outputIOIndex 输出IO端口索引号
	 *@param[in] polarity 有效电平
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief sets the active level of the flash
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] outputIOIndex Output IO port index number
	 *@param[in] polarity active level
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetStrobePolarity(HANDLE hCamera, INT outputIOIndex, INT polarity);

	/**
	 *@ingroup __CK_TRIGGER__
	 *@~chinese
	 *@brief 获取闪光灯的有效电平
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] outputIOIndex 输出IO端口索引号
	 *@param[out] pPolarity 有效电平
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the active level of the flash
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] outputIOIndex Output IO port index number
	 *@param[out] pPolarity active level
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetStrobePolarity(HANDLE hCamera, INT outputIOIndex, INT *pPolarity);

	/**
	 *@ingroup __CK_IO_SETTINGS__
	 *@~chinese
	 *@brief 设置输出IO的工作模式， 预留可编程输出IO的个数由tSdkCameraCapbility中iOutputIoCounts决定。
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] outputIOIndex 输出IO的索引号，从0开始
	 *@param[in] mode 输出IO模式，参考@link #emSdkOutputIOMode @endlink
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the operating mode of the output IO. The number of reserved output IOs is determined by iOutputIoCounts in tSdkCameraCapbility.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] outputIOIndex Output IO port index number
	 *@param[in] outputIOIndex Outputs the index number of IO, starting from 0
	 *@param[in] mode Output IO mode, see @link #emSdkOutputIOMode @endlink
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetOutputIOMode(HANDLE hCamera, INT outputIOIndex, INT mode);

	/**
	 *@ingroup __CK_IO_SETTINGS__
	 *@~chinese
	 *@brief 获取输出IO的工作模式， 预留可编程输出IO的个数由tSdkCameraCapbility中iOutputIoCounts决定。
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] outputIOIndex 输出IO的索引号，从0开始
	 *@param[out] pMode 输出IO模式，参考@link #emSdkOutputIOMode @endlink
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the operating mode of the output IO. The number of reserved output IOs is determined by iOutputIoCounts in tSdkCameraCapbility.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] outputIOIndex Outputs the index number of IO, starting from 0
	 *@param[out] pMode Output IO mode, see @link #emSdkOutputIOMode @endlink
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	*/
	CKSDK_API CameraSdkStatus __stdcall CameraGetOutputIOMode(HANDLE hCamera, INT outputIOIndex, INT *pMode);

	/**
	 *@ingroup __CK_IO_SETTINGS__
	 *@~chinese
	 *@brief 设置输入IO的模式，相机预留可编程输出IO的个数由tSdkCameraCapbility中iInputIoCounts决定
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] inputIOIndex IO的索引号，从0开始
	 *@param[in] mode 输入IO模式, 参考@link #emSdkInputIOMode @endlink
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the mode of input IO. The number of programmable output IOs reserved by the camera is determined by iInputIoCounts in tSdkCameraCapbility.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] inputIOIndex IO index number, starting from 0
	 *@param[in] mode Enter IO mode, see @link #emSdkInputIOMode @endlink
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetInputIOMode(HANDLE hCamera, INT inputIOIndex, INT mode);

	/**
	*@ingroup __CK_IO_SETTINGS__
	*@~chinese
	*@brief 获取输入IO的模式，相机预留可编程输出IO的个数由tSdkCameraCapbility中iInputIoCounts决定
	*@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	*@param[in] inputIOIndex IO的索引号，从0开始
	*@param[out] pMode 输入IO模式, 参考@link #emSdkInputIOMode @endlink
	*@return @link #__CK_ERRCODE__ 状态码@endlink
	*@~english
	*@brief Get the mode of input IO. The number of programmable output IOs reserved by the camera is determined by iInputIoCounts in tSdkCameraCapbility.
	*@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	*@param[in] inputIOIndex IO index number, starting from 0
	*@param[out] pMode Enter IO mode, see @link #emSdkInputIOMode @endlink
	*@return @link #__CK_ERRCODE__ Status Code @endlink
	*/
	CKSDK_API CameraSdkStatus __stdcall CameraGetInputIOMode(HANDLE hCamera, INT inputIOIndex, INT *pMode);

	/**
	 *@ingroup __CK_IO_SETTINGS__
	 *@~chinese
	 *@brief 设置指定IO的电平状态，IO为输出型IO，相机预留可编程输出IO的个数由tSdkCameraCapbility中iOutputIoCounts决定。
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] outputIOIndex IO的索引号，从0开始。
	 *@param[in] state 要设定的状态，1为导通，0为不导通
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the level state of the specified IO. IO is the output type IO. The number of programmable output IOs reserved by the camera is determined by iOutputIoCounts in tSdkCameraCapbility.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] outputIOIndex The index number of IO, starting from 0.
	 *@param[in] state The state to be set, 1 is on, 0 is not on
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetOutputIOState(HANDLE hCamera, INT outputIOIndex, INT state);

	/**
	*@ingroup __CK_IO_SETTINGS__
	*@~chinese
	*@brief 获取指定IO的电平状态，IO为输出型IO，相机预留可编程输出IO的个数由tSdkCameraCapbility中iOutputIoCounts决定。
	*@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	*@param[in] outputIOIndex IO的索引号，从0开始。
	*@param[in] pState 要设定的状态，1为导通，0为不导通
	*@return @link #__CK_ERRCODE__ 状态码@endlink
	*@~english
	*@brief Get the level state of the specified IO. IO is the output type IO. The number of programmable output IOs reserved by the camera is determined by iOutputIoCounts in tSdkCameraCapbility.
	*@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	*@param[in] outputIOIndex The index number of IO, starting from 0.
	*@param[in] pState The state to be set, 1 is on, 0 is not on
	*@return @link #__CK_ERRCODE__ Status Code @endlink
	*/
	CKSDK_API CameraSdkStatus __stdcall CameraGetOutputIOState(HANDLE hCamera, INT outputIOIndex, INT *pState);

	/**
	 *@ingroup __CK_IO_SETTINGS__
	 *@~chinese
	 *@brief 设置PWM型输出的参数，相机预留可编程输出IO的个数由tSdkCameraCapbility中iOutputIoCounts决定。
	 *@param[in] hCamera 相机的句柄，由CameraInit函数获得。
	 *@param[in] outputIOIndex IO的索引号，从0开始。
	 *@param[in] cycle PWM的周期，单位(us)
	 *@param[in] duty  PWM高电平时间，单位(us)，小于cycle的值
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the parameters of the PWM output. The number of programmable output IOs reserved by the camera is determined by iOutputIoCounts in tSdkCameraCapbility.
	 *@param[in] hCamera The handle of the camera, obtained by the CameraInit function.
	 *@param[in] outputIOIndex The index number of IO, starting from 0.
	 *@param[in] cycle PWM period, unit (us)
	 *@param[in] duty PWM high time, unit (us), less than the value of cycle
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetOutputIOPWM(HANDLE hCamera, INT outputIOIndex, UINT cycle, UINT duty);

	/**
	*@ingroup __CK_IO_SETTINGS__
	*@~chinese
	*@brief 获取PWM型输出的参数，相机预留可编程输出IO的个数由tSdkCameraCapbility中iOutputIoCounts决定。
	*@param[in] hCamera 相机的句柄，由CameraInit函数获得。
	*@param[in] outputIOIndex IO的索引号，从0开始。
	*@param[out] pCycle PWM的周期，单位(us)
	*@param[out] pDuty  PWM高电平时间，单位(us)
	*@return @link #__CK_ERRCODE__ 状态码@endlink
	*@~english
	*@brief Get the parameters of the PWM output. The number of programmable output IOs reserved by the camera is determined by iOutputIoCounts in tSdkCameraCapbility.
	*@param[in] hCamera The handle of the camera, obtained by the CameraInit function.
	*@param[in] outputIOIndex The index number of IO, starting from 0.
	*@param[out] pCycle PWM period, unit (us)
	*@param[out] pDuty  PWM high time, unit (us)
	*@return @link #__CK_ERRCODE__ Status Code @endlink
	*/
	CKSDK_API CameraSdkStatus __stdcall CameraGetOutputIOPWM(HANDLE hCamera, INT outputIOIndex, INT *pCycle, INT *pDuty);

	/**
	 *@ingroup __CK_IO_SETTINGS__
	 *@~chinese
	 *@brief 设置指定IO的电平状态，IO为输入型IO，相机预留可编程输出IO的个数由tSdkCameraCapbility中iInputIoCounts决定。
	 *@param[in] hCamera 相机的句柄，由CameraInit函数获得。
	 *@param[in] inputIOIndex IO的索引号，从0开始。
	 *@param[in] pState 指针，返回IO状态,1为导通，0为不导通
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the level state of the specified IO. IO is the input type IO. The number of programmable output IOs reserved by the camera is determined by iInputIoCounts in tSdkCameraCapbility.
	 *@param[in] hCamera The handle of the camera, obtained by the CameraInit function.
	 *@param[in] inputIOIndex The index number of IO, starting from 0.
	 *@param[in] pState pointer, return IO status, 1 is on, 0 is not on
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetInputIOState(HANDLE hCamera, INT inputIOIndex, INT *pState);

	/**
	 *@ingroup __CK_PARAMETER__
	 *@~chinese
	 *@brief 设定参数存取的目标对象。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iMode  参数存取的模式。参考CameraDefine.h
	 中emSdkParameterMode的类型定义。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets the target object for parameter access.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iMode The mode in which the iMode parameter is accessed. Reference CameraDefine.h
	 The type definition of emSdkParameterMode.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetParameterMode(HANDLE hCamera, INT iMode);

	/**
	 *@ingroup __CK_PARAMETER__
	 *@~chinese
	 *@brief 获得参数存取的目标对象。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] piMode 指针，返回参数存取的模式
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the target object for parameter access.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piMode pointer, returning the mode of parameter access
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetParameterMode(HANDLE hCamera, INT* piMode);

	/**
	 *@ingroup __CK_PARAMETER__
	 *@~chinese
	 *@brief 保存当前相机参数到指定的参数组中。相机提供了A,B,C,D
	 A,B,C,D四组空间来进行参数的保存。 
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iTeam      PARAMETER_TEAM_A 保存到A组中,
	 PARAMETER_TEAM_B 保存到B组中,
	 PARAMETER_TEAM_C 保存到C组中,
	 PARAMETER_TEAM_D 保存到D组中
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Saves the current camera parameters to the specified parameter group. The camera provides A, B, C, D
	 The four groups A, B, C, and D are used to save the parameters.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iTeam PARAMETER_TEAM_A is saved to group A,
	 PARAMETER_TEAM_B is saved to group B,
	 PARAMETER_TEAM_C is saved to group C,
	 PARAMETER_TEAM_D is saved to group D
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSaveParameter(HANDLE hCamera, INT iTeam);

	/**
	 *@ingroup __CK_PARAMETER__
	 *@~chinese
	 *@brief 保存当前相机参数到指定的文件中。该文件可以复制到
	 别的电脑上供其他相机加载，也可以做参数备份用。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] sFileName  参数文件的完整路径。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Saves the current camera parameters to the specified file. The file can be copied to
	 Other computers can be loaded by other cameras, and can also be used for parameter backup.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] sFileName The full path to the sFileName parameter file.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSaveParameterToFile(HANDLE hCamera, const char* sFileName);

	/**
	 *@ingroup __CK_PARAMETER__
	 *@~chinese
	 *@brief 从PC上指定的参数文件中加载参数。我公司相机参数
	 保存在PC上为.config后缀的文件，位于安装下的
	 Camera\\Configs文件夹中。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] *sFileName 参数文件的完整路径。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Loads parameters from the parameter file specified on the PC. My company camera parameters
	 Save the file with the .config suffix on the PC, under the installation
	 In the Camera\\Configs folder.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] *sFileName The full path to the parameter file.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraReadParameterFromFile(HANDLE hCamera, const char* sFileName);

	/**
	 *@ingroup __CK_PARAMETER__
	 *@~chinese
	 *@brief 加载指定组的参数到相机中。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iTeam    PARAMETER_TEAM_A 加载A组参数,
	 @link #emSdkParameterTeam::PARAMETER_TEAM_B @endlink 加载B组参数,
	 @link #emSdkParameterTeam::PARAMETER_TEAM_C @endlink 加载C组参数,
	 @link #emSdkParameterTeam::PARAMETER_TEAM_D @endlink 加载D组参数,
	 @link #emSdkParameterTeam::PARAMETER_TEAM_DEFAULT @endlink 加载默认参数。    
	 类型定义参考CameraDefine.h中emSdkParameterTeam类型
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Loads the parameters of the specified group into the camera.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iTeam PARAMETER_TEAM_A Loads Group A parameters,
	 @link #emSdkParameterTeam::PARAMETER_TEAM_B @endlink Loads Group B parameters,
	 @link #emSdkParameterTeam::PARAMETER_TEAM_C @endlink Load C group parameters,
	 @link #emSdkParameterTeam::PARAMETER_TEAM_D @endlink Loads the D group parameters,
	 @link #emSdkParameterTeam::PARAMETER_TEAM_DEFAULT @endlink Loads the default parameters.
	 Type definition reference emSdkParameterTeam type in CameraDefine.h
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraLoadParameter(HANDLE hCamera, INT iTeam);

	/**
	 *@ingroup __CK_PARAMETER__
	 *@~chinese
	 *@brief 获得当前选择的参数组。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] piTeam     指针，返回当前选择的参数组。返回值
	 参考CameraLoadParameter中iTeam参数。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the currently selected parameter group.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] piTeam The pointer returns the currently selected parameter group. return value
	 Refer to the iTeam parameter in the CameraLoadParameter.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetCurrentParameterGroup(HANDLE hCamera, INT* piTeam);

	/**
	 *@ingroup __CK_USER_DATA__
	 *@~chinese
	 *@brief 设置相机的序列号。我公司相机序列号分为3级。0级的是我公司自定义的相机序列号，出厂时已经
	 设定好，1级和2级留给二次开发使用。每级序列
	 号长度都是32个字节。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] pbySN    序列号的缓冲区。 
	 *@param[in] iLevel   要设定的序列号级别，只能是1或者2。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraReadSN
	 *@note 相机自带三级序列号，其中一级序列号是可读的，不可以修改，出厂
	 的时候已经固定好了。二级和三级序列号可自由读写。每级序列号都 是32 个字节。
	 *@~english
	 *@brief Sets the serial number of the camera. Our company's camera serial number is divided into 3 levels. Level 0 is our company's custom camera serial number, already shipped from the factory
	 Set well, Level 1 and Level 2 are reserved for secondary development. Sequence of each level
	 The length of the number is 32 bytes.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] pbySN The buffer for the pbySN serial number.
	 *@param[in] iLevel The serial number level to be set can only be 1 or 2.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraReadSN
	 *@note The camera comes with a three-level serial number, where the primary serial number is readable and cannot be modified.
	 It has already been fixed. The secondary and tertiary serial numbers are freely readable and writable. Each serial number is 32 bytes.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraWriteSN(HANDLE hCamera, BYTE* pbySN, INT iLevel);

	/**
	 *@ingroup __CK_USER_DATA__
	 *@~chinese
	 *@brief 读取相机指定级别的序列号。序列号的定义请参考
	 @link #CameraWriteSN @endlink函数的功能描述部分。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pbySN    序列号的缓冲区。
	 *@param[in] iLevel     要读取的序列号级别。只能是0,1和2。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraWriteSN
	 *@~english
	 *@brief Reads the serial number of the camera's specified level. Please refer to the definition of serial number.
	 @link #CameraWriteSN The functional description section of the @endlink function.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pbySN The buffer for the serial number.
	 *@param[in] iLevel The serial number level to be read. Can only be 0, 1 and 2.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraWriteSN
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraReadSN(HANDLE hCamera, BYTE* pbySN, INT iLevel);

	/**
	 *@ingroup __CK_USER_DATA__
	 *@~chinese
	 *@brief 将用户自定义的数据保存到相机的非易性存储器中。每个型号的相机可能支持的用户数据区最大长度不一样。可以从设备的特性描述中获取该长度信息。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] uStartAddr  起始地址，从0开始。
	 *@param[in] pbData      数据缓冲区指针
	 *@param[in] iLen        写入数据的长度，ilen + uStartAddr必须小于用户区最大长度。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief saves user-defined data to the camera's non-volatile memory. The maximum length of the user data area that each model camera may support is different. 
	 This length information can be obtained from the characterization of the device.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] uStartAddr Start address, starting at 0.
	 *@param[in] pbData data buffer pointer
	 *@param[in] iLen The length of the data written by iLen, ilen + uStartAddr must be less than the maximum length of the user area.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSaveUserData(HANDLE hCamera, UINT uStartAddr, BYTE *pbData, INT iLen);

	/**
	 *@ingroup __CK_USER_DATA__
	 *@~chinese
	 *@brief 从相机的非易性存储器中读取用户自定义的数据。每个型号的相机可能支持的用户数据区最大长度不一样。可以从设备的特性描述中获取该长度信息。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] uStartAddr  起始地址，从0开始。
	 *@param[out] pbData      数据缓冲区指针，返回读到的数据。
	 *@param[in] iLen        读取数据的长度，ilen + uStartAddr必须小于用户区最大长度
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 我公司所有型号的相机，都带一段数据存储空间，用户可以自由读写。
	 并且掉电后也会保存，不会丢失数据。每个型号的相机，存储空间的
	 大小是不一样的，具体的大小可以通过 @link #CameraGetCapability @endlink 函数
	 得到， @link #tSdkCameraCapbility @endlink 结极体的 iUserDataMaxLen 表示相机
	 的最大存储字节数。
	 *@~english
	 *@brief reads user-defined data from the camera's non-volatile memory. The maximum length of the user data area that each model camera may support is different. 
	 This length information can be obtained from the characterization of the device.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] uStartAddr Start address, starting at 0.
	 *@param[out] pbData Data buffer pointer, returns the data read.
	 *@param[in] iLen reads the length of the data, ilen + uStartAddr must be less than the maximum length of the user area
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note All models of our company have a data storage space that users can read and write freely.
	 It will also be saved after power failure, without losing data. Each model of camera, storage space
	 The size is not the same, the specific size can be passed @link #CameraGetCapability @endlink function
	 Get, @link #tSdkCameraCapbility @endlink The iUserDataMaxLen of the pole represents the camera
	 The maximum number of bytes stored.
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraLoadUserData(HANDLE hCamera, UINT uStartAddr, BYTE *pbData, INT iLen);

	/**
	 *@ingroup __CK_USER_DATA__
	 *@~chinese
	 *@brief 读取用户自定义的设备昵称。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pName      指针，返回指向0结尾的字符串，
	 设备昵称不超过32个字节，因此该指针
	 指向的缓冲区必须大于等于32个字节空间。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Read user-defined device nicknames.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pName pointer, which returns a string pointing to the end of 0.
	 The device nickname does not exceed 32 bytes, so the pointer
	 The buffer pointed to must be greater than or equal to 32 bytes of space.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetFriendlyName(HANDLE hCamera, char* pName);

	/**
	 *@ingroup __CK_USER_DATA__
	 *@~chinese
	 *@brief 设置用户自定义的设备昵称。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] pName      指针，指向0结尾的字符串，设备昵称不超过32个字节，因此该指针指向字符串必须小于等于32个字节空间。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Sets a user-defined device nickname.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] pName Pointer to a string ending in 0. The device nickname is no more than 32 bytes, so the pointer to the string must be less than or equal to 32 bytes.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetFriendlyName(HANDLE hCamera, char* pName);

	/**
	 *@ingroup __CK_VERSION__
	 *@~chinese
	 *@brief 获得SDK版本信息
	 *@param[in] pVersionString 指针，返回SDK版本字符串。该指针指向的缓冲区大小必须大于32个字节
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get SDK version information
	 *@param[in] pVersionString Pointer, which returns the SDK version string. The buffer pointed to by this pointer must be larger than 32 bytes
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSdkGetVersionString(char* pVersionString);

	/**
	 *@ingroup __CK_VERSION__
	 *@~chinese
	 *@brief 检测固件版本，是否需要升级。
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pNeedUpdate 指针，返回固件检测状态，TRUE表示需要更新
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Detects the firmware version and needs to be upgraded.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pNeedUpdate pointer, return firmware detection status, TRUE indicates need to update
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraCheckFwUpdate(HANDLE hCamera, BOOL* pNeedUpdate);

	/**
	 *@ingroup __CK_VERSION__
	 *@~chinese
	 *@brief 获得固件版本的字符串
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pVersion 必须指向一个大于32字节的缓冲区，返回固件的版本字符串。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief gets the firmware version of the string
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pVersion must point to a buffer larger than 32 bytes. Returns the version string of the firmware.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetFirmwareVision(HANDLE hCamera, char* pVersion);

	/**
	 *@ingroup __CK_VERSION__
	 *@~chinese
	 *@brief 获得设备版本ID
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pDeviceID 必须指向一个4字节的缓冲区，返回设备版本ID。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get device version ID
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pDeviceID must point to a 4-byte buffer. Returns the device version ID.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetDeviceType(HANDLE hCamera, DWORD* pDeviceID);

	/**
	 *@ingroup __CK_VERSION__
	 *@~chinese
	 *@brief 获得指定设备接口的版本
	 *@param[in] hCamera 相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pVersion 指向一个大于32字节的缓冲区，返回接口版本字符串。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Get the version of the specified device interface
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pVersion points to a buffer larger than 32 bytes and returns the interface version string.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGetInerfaceVersion(HANDLE hCamera, char* pVersion);

	/**
	 *@ingroup __CK_SETTING_PAGE__
	 *@~chinese
	 *@brief 设置相机属性配置窗口显示状态。必须先调用@link #CameraCreateSettingPageEx @endlink 成功创建相机属性配置窗口后，才能调用本函数进行显示。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] bShow    TRUE，显示;FALSE，隐藏。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraCreateSettingPageEx
	 *@see CameraSetActivePage
	 *@~english
	 *@brief Sets the camera property configuration window display status. You must call @link #CameraCreateSettingPageEx @endlink 
	 to create the camera property configuration window successfully before you can call this function to display.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] bShow TRUE, display; FALSE, hidden.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraCreateSettingPageEx
	 *@see CameraSetActivePage
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraShowSettingPage(HANDLE hCamera, BOOL bShow);

	/**
	 *@ingroup __CK_SETTING_PAGE__
	 *@~chinese
	 *@brief 设置相机配置窗口的激活页面。相机配置窗口有多个子页面构成，该函数可以设定当前哪一个子页面为激活状态，显示在最前端。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] lActivePage   配置页面使能位 @link #emSettingPage::SETTING_PAGE_ALL @endlink
	 *@param[in] iDefault      子页面的索引号。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraCreateSettingPageEx
	 *@see CameraShowSettingPage
	 *@~english
	 *@brief Sets the activation page of the camera configuration window. The camera configuration window has multiple sub-pages. This function can set which sub-page is active and displayed at the forefront.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] lActivePage configuration page enable bit @link #emSettingPage::SETTING_PAGE_ALL @endlink
	 *@param[in] iDefault The index number of the iDefault subpage.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraCreateSettingPageEx
	 *@see CameraShowSettingPage
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraSetActivePage(HANDLE hCamera, LONG lActivePage, INT iDefault);

	/**
	 *@ingroup __CK_SETTING_PAGE__
	 *@~chinese
	 *@brief 创建相机配置窗口。创建之前要先调用CameraSetActivePage设置子页面构成。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Creates a camera configuration window. Before creating, you must first call CameraSetActivePage to set the subpage composition.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraCreateSettingPageEx(HANDLE hCamera);

	/**
	 *@ingroup __CK_SETTING_PAGE__
	 *@~chinese
	 *@brief 打开分辨率自定义面板，并通过可视化的方式来配置一个自定义分辨率。
	 *@param[in] hCamera    相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pImageCustom 指针，返回自定义的分辨率。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Opens the resolution custom panel and visually configures a custom resolution.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pImageCustom The pointer returns the custom resolution.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraCustomizeResolution(
			HANDLE hCamera,
			tSdkImageResolution* pImageCustom
			);

	/**
	 *@ingroup __CK_SETTING_PAGE__
	 *@~chinese
	 *@brief 打开参考窗口自定义面板。并通过可视化的方式来获得一个自定义窗口的位置。一般是用自定义白平衡和自动曝光的参考窗口。
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[in] iWinType   要生成的参考窗口的用途。0,自动曝光参考窗口；1,白平衡参考窗口。
	 *@param[out] piHOff     指针，返回自定义窗口的左上角横坐标。
	 *@param[out] piVOff     指针，返回自定义窗口的左上角纵坐标。
	 *@param[out] piWidth    指针，返回自定义窗口的宽度。 
	 *@param[out] piHeight   指针，返回自定义窗口的高度。
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@~english
	 *@brief Opens the reference window custom panel. And visually get the location of a custom window. A reference window for custom white balance and auto exposure is generally used.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[in] iWinType The purpose of the reference window to be generated by iWinType. 0, automatic exposure reference window; 1, white balance reference window.
	 *@param[out] piHOff Pointer, which returns the horizontal coordinate of the upper left corner of the custom window.
	 *@param[out] piVOff Pointer, which returns the ordinate of the upper left corner of the custom window.
	 *@param[out] piWidth Pointer, which returns the width of the custom window.
	 *@param[out] piHeight pointer, which returns the height of the custom window.
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraCustomizeReferWin(
			HANDLE hCamera,
			INT iWinType,
			INT* piHOff,
			INT* piVOff,
			INT* piWidth,
			INT* piHeight
			);

	/**
	 *@ingroup __CK_GIGE_PAGE__
	 *@~chinese
	 *@brief 通过相机序列号来设置相机的网络地址，这个接口只支持GIGE相机.
	 *@param[in] pCameraSN 相机的序列号，序列号可以通过@link #CameraEnumerateDevice @endlink获取
	 *@param[out] pNetworkInfo 相机通信的网络信息
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraGigeSetIp
	 *@~english
     *@brief Set the camera's network address by camera serial number. This interface only supports GIGE cameras.
     *@param[in] pCameraSN Camera serial number, serial number can be obtained by @link #CameraEnumerateDevice @endlink
	 *@param[out] pNetworkInfo Network information for camera communication
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraGigeSetIp
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGigeGetIp(const char *pCameraSN, tGigeNetworkInfo *pNetworkInfo);

	/**
	 *@ingroup __CK_GIGE_PAGE__
	 *@~chinese
	 *@brief 通过相机的序列号来设置相机的网络地址。
	 *@param[in] pCameraSN 相机的序列号，序列号可以通过@link #CameraEnumerateDevice @endlink获取
	 *@param[out] pNetworkInfo 相机通信的网络信息
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@see CameraGigeSetIp
	 *@~english
	 *@brief Set the camera's network address by the camera's serial number.
	 *@param[in] pCameraSN Camera serial number, serial number can be obtained by @link #CameraEnumerateDevice @endlink
	 *@param[out] pNetworkInfo Network information for camera communication
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@see CameraGigeSetIp
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGigeSetIp(const char *pCameraSN, const tGigeNetworkInfo *pNetworkInfo);

	/**
	 *@ingroup __CK_GIGE_PAGE__
	 *@~chinese
	 *@brief 设置GigE相机传输图像数据的网络包大小
	 *@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	 *@param[out] pPacketSize 传输网络包大小
	 *@return @link #__CK_ERRCODE__ 状态码@endlink
	 *@note 网络包传输大小的范围为1054至9014
	 *@see CameraGigeSetTransPacketSize
	 *@~english
	 *@brief Set the camera's network address by the camera's serial number.
	 *@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	 *@param[out] pPacketSize transport network packet size
	 *@return @link #__CK_ERRCODE__ Status Code @endlink
	 *@note Network packet transfer size ranges from 1054 to 9014
	 *@see CameraGigeSetTransPacketSize
	 */
	CKSDK_API CameraSdkStatus __stdcall CameraGigeGetTransPacketSize(HANDLE hCamera, UINT *pPacketSize);

	/**
	*@ingroup __CK_GIGE_PAGE__
	*@~chinese
	*@brief 获取GigE相机传输图像数据的网络包大小
	*@param[in] hCamera  相机的句柄，由@link #CameraInit @endlink函数获得
	*@param[in] packetSize 传输网络包大小
	*@return @link #__CK_ERRCODE__ 状态码@endlink
	*@note 网络包传输大小的范围为1054至9014
	*@see CameraGigeGetTransPacketSize
	*@~english
	*@brief Get the camera's network address by the camera's serial number.
	*@param[in] hCamera The handle of the camera, obtained by the @link #CameraInit @endlink function
	*@param[in] packetSize transport network packet size
	*@return @link #__CK_ERRCODE__ Status Code @endlink
	*@note Network packet transfer size ranges from 1054 to 9014
	*@see CameraGigeGetTransPacketSize
	*/
	CKSDK_API CameraSdkStatus __stdcall CameraGigeSetTransPacketSize(HANDLE hCamera, UINT packetSize);

#ifdef __cplusplus
}
#endif

#endif
