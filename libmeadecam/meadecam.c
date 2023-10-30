#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <libusb-1.0/libusb.h>
#include "meadecam.h"

#if defined(__APPLE__)
#define LIBTOUPCAM_NAME "libtoupcam.dylib"
#else
#define LIBTOUPCAM_NAME "libtoupcam.so"
#endif

static void *libtoupcam;

/* Create a function pointer for a given toupcam function. */
#define C_PTR(RET, DEC, PARAM, ARG) static TOUPCAM_API(RET) (*Meade_ ## DEC) PARAM;

/* Define a function that calls the corresponding toupcam function */
#define C_DEF(RET, DEC, PARAM, ARG) TOUPCAM_API(RET)  DEC PARAM \
{ \
    return Meade_ ## DEC ARG; \
}

#define MYDLSYM(X, Y) dlsym(X, #Y)
#define C_ASSIGN(RET, DEC, PARAM, ARG) Meade_ ## DEC = MYDLSYM(libtoupcam, DEC );

#define TOUPCAM_LIST \
C(const char*, Toupcam_Version, (), ()) \
C(HToupcam, Toupcam_Open, (const char* camId), (camId)) \
C(HToupcam, Toupcam_OpenByIndex, (unsigned index), (index)) \
C(void, Toupcam_Close, (HToupcam h), (h)) \
C(HRESULT, Toupcam_StartPullModeWithCallback, (HToupcam h, PTOUPCAM_EVENT_CALLBACK funEvent, void* ctxEvent), (h, funEvent, ctxEvent)) \
C(HRESULT, Toupcam_PullImageV3, (HToupcam h, void* pImageData, int bStill, int bits, int rowPitch, ToupcamFrameInfoV3* pInfo), (h, pImageData, bStill, bits, rowPitch, pInfo)) \
C(HRESULT, Toupcam_WaitImageV3, (HToupcam h, unsigned nWaitMS, void* pImageData, int bStill, int bits, int rowPitch, ToupcamFrameInfoV3* pInfo), (h, nWaitMS, pImageData, bStill, bits, rowPitch, pInfo)) \
C(HRESULT, Toupcam_PullImageV2, (HToupcam h, void* pImageData, int bits, ToupcamFrameInfoV2* pInfo), (h, pImageData, bits, pInfo)) \
C(HRESULT, Toupcam_PullStillImageV2, (HToupcam h, void* pImageData, int bits, ToupcamFrameInfoV2* pInfo), (h, pImageData, bits, pInfo)) \
C(HRESULT, Toupcam_PullImageWithRowPitchV2, (HToupcam h, void* pImageData, int bits, int rowPitch, ToupcamFrameInfoV2* pInfo), (h, pImageData, bits, rowPitch, pInfo)) \
C(HRESULT, Toupcam_PullStillImageWithRowPitchV2, (HToupcam h, void* pImageData, int bits, int rowPitch, ToupcamFrameInfoV2* pInfo), (h, pImageData, bits, rowPitch, pInfo)) \
C(HRESULT, Toupcam_PullImage, (HToupcam h, void* pImageData, int bits, unsigned* pnWidth, unsigned* pnHeight), (h, pImageData, bits, pnWidth, pnHeight)) \
C(HRESULT, Toupcam_PullStillImage, (HToupcam h, void* pImageData, int bits, unsigned* pnWidth, unsigned* pnHeight), (h, pImageData, bits, pnWidth, pnHeight)) \
C(HRESULT, Toupcam_PullImageWithRowPitch, (HToupcam h, void* pImageData, int bits, int rowPitch, unsigned* pnWidth, unsigned* pnHeight), (h, pImageData, bits, rowPitch, pnWidth, pnHeight)) \
C(HRESULT, Toupcam_PullStillImageWithRowPitch, (HToupcam h, void* pImageData, int bits, int rowPitch, unsigned* pnWidth, unsigned* pnHeight), (h, pImageData, bits, rowPitch, pnWidth, pnHeight)) \
C(HRESULT, Toupcam_StartPushModeV4, (HToupcam h, PTOUPCAM_DATA_CALLBACK_V4 funData, void* ctxData, PTOUPCAM_EVENT_CALLBACK funEvent, void* ctxEvent), (h, funData, ctxData, funEvent, ctxEvent)) \
C(HRESULT, Toupcam_StartPushModeV3, (HToupcam h, PTOUPCAM_DATA_CALLBACK_V3 funData, void* ctxData, PTOUPCAM_EVENT_CALLBACK funEvent, void* ctxEvent), (h, funData, ctxData, funEvent, ctxEvent)) \
C(HRESULT, Toupcam_Stop, (HToupcam h), (h)) \
C(HRESULT, Toupcam_Pause, (HToupcam h, int bPause), (h, bPause)) \
C(HRESULT, Toupcam_Snap, (HToupcam h, unsigned nResolutionIndex), (h, nResolutionIndex)) \
C(HRESULT, Toupcam_SnapN, (HToupcam h, unsigned nResolutionIndex, unsigned nNumber), (h, nResolutionIndex, nNumber)) \
C(HRESULT, Toupcam_SnapR, (HToupcam h, unsigned nResolutionIndex, unsigned nNumber), (h, nResolutionIndex, nNumber)) \
C(HRESULT, Toupcam_Trigger, (HToupcam h, unsigned short nNumber), (h, nNumber)) \
C(HRESULT, Toupcam_TriggerSync, (HToupcam h, unsigned nTimeout, void* pImageData, int bits, int rowPitch, ToupcamFrameInfoV3* pInfo), (h, nTimeout, pImageData, bits, rowPitch, pInfo)) \
C(HRESULT, Toupcam_put_Size, (HToupcam h, int nWidth, int nHeight), (h, nWidth, nHeight)) \
C(HRESULT, Toupcam_get_Size, (HToupcam h, int* pWidth, int* pHeight), (h, pWidth, pHeight)) \
C(HRESULT, Toupcam_put_eSize, (HToupcam h, unsigned nResolutionIndex), (h, nResolutionIndex)) \
C(HRESULT, Toupcam_get_eSize, (HToupcam h, unsigned* pnResolutionIndex), (h, pnResolutionIndex)) \
C(HRESULT, Toupcam_get_FinalSize, (HToupcam h, int* pWidth, int* pHeight), (h, pWidth, pHeight)) \
C(HRESULT, Toupcam_get_ResolutionNumber, (HToupcam h), (h)) \
C(HRESULT, Toupcam_get_Resolution, (HToupcam h, unsigned nResolutionIndex, int* pWidth, int* pHeight), (h, nResolutionIndex, pWidth, pHeight)) \
C(HRESULT, Toupcam_get_ResolutionRatio, (HToupcam h, unsigned nResolutionIndex, int* pNumerator, int* pDenominator), (h, nResolutionIndex, pNumerator, pDenominator)) \
C(HRESULT, Toupcam_get_Field, (HToupcam h), (h)) \
C(HRESULT, Toupcam_get_RawFormat, (HToupcam h, unsigned* pFourCC, unsigned* pBitsPerPixel), (h, pFourCC, pBitsPerPixel)) \
C(HRESULT, Toupcam_get_AutoExpoEnable, (HToupcam h, int* bAutoExposure), (h, bAutoExposure)) \
C(HRESULT, Toupcam_put_AutoExpoEnable, (HToupcam h, int bAutoExposure), (h, bAutoExposure)) \
C(HRESULT, Toupcam_get_AutoExpoTarget, (HToupcam h, unsigned short* Target), (h, Target)) \
C(HRESULT, Toupcam_put_AutoExpoTarget, (HToupcam h, unsigned short Target), (h, Target)) \
C(HRESULT, Toupcam_put_AutoExpoRange, (HToupcam h, unsigned maxTime, unsigned minTime, unsigned short maxGain, unsigned short minGain), (h, maxTime, minTime, maxGain, minGain)) \
C(HRESULT, Toupcam_get_AutoExpoRange, (HToupcam h, unsigned* maxTime, unsigned* minTime, unsigned short* maxGain, unsigned short* minGain), (h, maxTime, minTime, maxGain, minGain)) \
C(HRESULT, Toupcam_put_MaxAutoExpoTimeAGain, (HToupcam h, unsigned maxTime, unsigned short maxGain), (h, maxTime, maxGain)) \
C(HRESULT, Toupcam_get_MaxAutoExpoTimeAGain, (HToupcam h, unsigned* maxTime, unsigned short* maxGain), (h, maxTime, maxGain)) \
C(HRESULT, Toupcam_put_MinAutoExpoTimeAGain, (HToupcam h, unsigned minTime, unsigned short minGain), (h, minTime, minGain)) \
C(HRESULT, Toupcam_get_MinAutoExpoTimeAGain, (HToupcam h, unsigned* minTime, unsigned short* minGain), (h, minTime, minGain)) \
C(HRESULT, Toupcam_get_ExpoTime, (HToupcam h, unsigned* Time), (h, Time)) \
C(HRESULT, Toupcam_put_ExpoTime, (HToupcam h, unsigned Time), (h, Time)) \
C(HRESULT, Toupcam_get_RealExpoTime, (HToupcam h, unsigned* Time), (h, Time)) \
C(HRESULT, Toupcam_get_ExpTimeRange, (HToupcam h, unsigned* nMin, unsigned* nMax, unsigned* nDef), (h, nMin, nMax, nDef)) \
C(HRESULT, Toupcam_get_ExpoAGain, (HToupcam h, unsigned short* Gain), (h, Gain)) \
C(HRESULT, Toupcam_put_ExpoAGain, (HToupcam h, unsigned short Gain), (h, Gain)) \
C(HRESULT, Toupcam_get_ExpoAGainRange, (HToupcam h, unsigned short* nMin, unsigned short* nMax, unsigned short* nDef), (h, nMin, nMax, nDef)) \
C(HRESULT, Toupcam_AwbOnce, (HToupcam h, PITOUPCAM_TEMPTINT_CALLBACK funTT, void* ctxTT), (h, funTT, ctxTT)) \
C(HRESULT, Toupcam_AwbInit, (HToupcam h, PITOUPCAM_WHITEBALANCE_CALLBACK funWB, void* ctxWB), (h, funWB, ctxWB)) \
C(HRESULT, Toupcam_put_TempTint, (HToupcam h, int nTemp, int nTint), (h, nTemp, nTint)) \
C(HRESULT, Toupcam_get_TempTint, (HToupcam h, int* nTemp, int* nTint), (h, nTemp, nTint)) \
C(HRESULT, Toupcam_put_WhiteBalanceGain, (HToupcam h, int aGain[3]), (h, aGain)) \
C(HRESULT, Toupcam_get_WhiteBalanceGain, (HToupcam h, int aGain[3]), (h, aGain)) \
C(HRESULT, Toupcam_AbbOnce, (HToupcam h, PITOUPCAM_BLACKBALANCE_CALLBACK funBB, void* ctxBB), (h, funBB, ctxBB)) \
C(HRESULT, Toupcam_put_BlackBalance, (HToupcam h, unsigned short aSub[3]), (h, aSub)) \
C(HRESULT, Toupcam_get_BlackBalance, (HToupcam h, unsigned short aSub[3]), (h, aSub)) \
C(HRESULT, Toupcam_FfcOnce, (HToupcam h), (h)) \
C(HRESULT, Toupcam_FfcExport, (HToupcam h, const char* filepath), (h, filepath)) \
C(HRESULT, Toupcam_FfcImport, (HToupcam h, const char* filepath), (h, filepath)) \
C(HRESULT, Toupcam_DfcOnce, (HToupcam h), (h)) \
C(HRESULT, Toupcam_DfcExport, (HToupcam h, const char* filepath), (h, filepath)) \
C(HRESULT, Toupcam_DfcImport, (HToupcam h, const char* filepath), (h, filepath)) \
C(HRESULT, Toupcam_put_Hue, (HToupcam h, int Hue), (h, Hue)) \
C(HRESULT, Toupcam_get_Hue, (HToupcam h, int* Hue), (h, Hue)) \
C(HRESULT, Toupcam_put_Saturation, (HToupcam h, int Saturation), (h, Saturation)) \
C(HRESULT, Toupcam_get_Saturation, (HToupcam h, int* Saturation), (h, Saturation)) \
C(HRESULT, Toupcam_put_Brightness, (HToupcam h, int Brightness), (h, Brightness)) \
C(HRESULT, Toupcam_get_Brightness, (HToupcam h, int* Brightness), (h, Brightness)) \
C(HRESULT, Toupcam_get_Contrast, (HToupcam h, int* Contrast), (h, Contrast)) \
C(HRESULT, Toupcam_put_Contrast, (HToupcam h, int Contrast), (h, Contrast)) \
C(HRESULT, Toupcam_get_Gamma, (HToupcam h, int* Gamma), (h, Gamma)) \
C(HRESULT, Toupcam_put_Gamma, (HToupcam h, int Gamma), (h, Gamma)) \
C(HRESULT, Toupcam_get_Chrome, (HToupcam h, int* bChrome), (h, bChrome)) \
C(HRESULT, Toupcam_put_Chrome, (HToupcam h, int bChrome), (h, bChrome)) \
C(HRESULT, Toupcam_get_VFlip, (HToupcam h, int* bVFlip), (h, bVFlip)) \
C(HRESULT, Toupcam_put_VFlip, (HToupcam h, int bVFlip), (h, bVFlip)) \
C(HRESULT, Toupcam_get_HFlip, (HToupcam h, int* bHFlip), (h, bHFlip)) \
C(HRESULT, Toupcam_put_HFlip, (HToupcam h, int bHFlip), (h, bHFlip)) \
C(HRESULT, Toupcam_get_Negative, (HToupcam h, int* bNegative), (h, bNegative)) \
C(HRESULT, Toupcam_put_Negative, (HToupcam h, int bNegative), (h, bNegative)) \
C(HRESULT, Toupcam_put_Speed, (HToupcam h, unsigned short nSpeed), (h, nSpeed)) \
C(HRESULT, Toupcam_get_Speed, (HToupcam h, unsigned short* pSpeed), (h, pSpeed)) \
C(HRESULT, Toupcam_get_MaxSpeed, (HToupcam h), (h)) \
C(HRESULT, Toupcam_get_FanMaxSpeed, (HToupcam h), (h)) \
C(HRESULT, Toupcam_get_MaxBitDepth, (HToupcam h), (h)) \
C(HRESULT, Toupcam_put_HZ, (HToupcam h, int nHZ), (h, nHZ)) \
C(HRESULT, Toupcam_get_HZ, (HToupcam h, int* nHZ), (h, nHZ)) \
C(HRESULT, Toupcam_put_Mode, (HToupcam h, int bSkip), (h, bSkip)) \
C(HRESULT, Toupcam_get_Mode, (HToupcam h, int* bSkip), (h, bSkip)) \
C(HRESULT, Toupcam_put_AWBAuxRect, (HToupcam h, const RECT* pAuxRect), (h, pAuxRect)) \
C(HRESULT, Toupcam_get_AWBAuxRect, (HToupcam h, RECT* pAuxRect), (h, pAuxRect)) \
C(HRESULT, Toupcam_put_AEAuxRect, (HToupcam h, const RECT* pAuxRect), (h, pAuxRect)) \
C(HRESULT, Toupcam_get_AEAuxRect, (HToupcam h, RECT* pAuxRect), (h, pAuxRect)) \
C(HRESULT, Toupcam_put_ABBAuxRect, (HToupcam h, const RECT* pAuxRect), (h, pAuxRect)) \
C(HRESULT, Toupcam_get_ABBAuxRect, (HToupcam h, RECT* pAuxRect), (h, pAuxRect)) \
C(HRESULT, Toupcam_get_MonoMode, (HToupcam h), (h)) \
C(HRESULT, Toupcam_get_StillResolutionNumber, (HToupcam h), (h)) \
C(HRESULT, Toupcam_get_StillResolution, (HToupcam h, unsigned nResolutionIndex, int* pWidth, int* pHeight), (h, nResolutionIndex, pWidth, pHeight)) \
C(HRESULT, Toupcam_put_RealTime, (HToupcam h, int val), (h, val)) \
C(HRESULT, Toupcam_get_RealTime, (HToupcam h, int* val), (h, val)) \
C(HRESULT, Toupcam_Flush, (HToupcam h), (h)) \
C(HRESULT, Toupcam_get_Temperature, (HToupcam h, short* pTemperature), (h, pTemperature)) \
C(HRESULT, Toupcam_put_Temperature, (HToupcam h, short nTemperature), (h, nTemperature)) \
C(HRESULT, Toupcam_get_Revision, (HToupcam h, unsigned short* pRevision), (h, pRevision)) \
C(HRESULT, Toupcam_get_SerialNumber, (HToupcam h, char sn[32]), (h, sn)) \
C(HRESULT, Toupcam_get_FwVersion, (HToupcam h, char fwver[16]), (h, fwver)) \
C(HRESULT, Toupcam_get_HwVersion, (HToupcam h, char hwver[16]), (h, hwver)) \
C(HRESULT, Toupcam_get_ProductionDate, (HToupcam h, char pdate[10]), (h, pdate)) \
C(HRESULT, Toupcam_get_FpgaVersion, (HToupcam h, char fpgaver[16]), (h, fpgaver)) \
C(HRESULT, Toupcam_get_PixelSize, (HToupcam h, unsigned nResolutionIndex, float* x, float* y), (h, nResolutionIndex, x, y)) \
C(HRESULT, Toupcam_put_LevelRange, (HToupcam h, unsigned short aLow[4], unsigned short aHigh[4]), (h, aLow, aHigh)) \
C(HRESULT, Toupcam_get_LevelRange, (HToupcam h, unsigned short aLow[4], unsigned short aHigh[4]), (h, aLow, aHigh)) \
C(HRESULT, Toupcam_put_LevelRangeV2, (HToupcam h, unsigned short mode, const RECT* pRoiRect, unsigned short aLow[4], unsigned short aHigh[4]), (h, mode, pRoiRect, aLow, aHigh)) \
C(HRESULT, Toupcam_get_LevelRangeV2, (HToupcam h, unsigned short* pMode, RECT* pRoiRect, unsigned short aLow[4], unsigned short aHigh[4]), (h, pMode, pRoiRect, aLow, aHigh)) \
C(HRESULT, Toupcam_LevelRangeAuto, (HToupcam h), (h)) \
C(HRESULT, Toupcam_GetHistogram, (HToupcam h, PITOUPCAM_HISTOGRAM_CALLBACK funHistogram, void* ctxHistogram), (h, funHistogram, ctxHistogram)) \
C(HRESULT, Toupcam_GetHistogramV2, (HToupcam h, PITOUPCAM_HISTOGRAM_CALLBACKV2 funHistogramV2, void* ctxHistogramV2), (h, funHistogramV2, ctxHistogramV2)) \
C(HRESULT, Toupcam_put_LEDState, (HToupcam h, unsigned short iLed, unsigned short iState, unsigned short iPeriod), (h, iLed, iState, iPeriod)) \
C(HRESULT, Toupcam_write_EEPROM, (HToupcam h, unsigned addr, const unsigned char* pBuffer, unsigned nBufferLen), (h, addr, pBuffer, nBufferLen)) \
C(HRESULT, Toupcam_read_EEPROM, (HToupcam h, unsigned addr, unsigned char* pBuffer, unsigned nBufferLen), (h, addr, pBuffer, nBufferLen)) \
C(HRESULT, Toupcam_read_Pipe, (HToupcam h, unsigned pipeId, void* pBuffer, unsigned nBufferLen), (h, pipeId, pBuffer, nBufferLen)) \
C(HRESULT, Toupcam_write_Pipe, (HToupcam h, unsigned pipeId, const void* pBuffer, unsigned nBufferLen), (h, pipeId, pBuffer, nBufferLen)) \
C(HRESULT, Toupcam_feed_Pipe, (HToupcam h, unsigned pipeId), (h, pipeId)) \
C(HRESULT, Toupcam_put_Option, (HToupcam h, unsigned iOption, int iValue), (h, iOption, iValue)) \
C(HRESULT, Toupcam_get_Option, (HToupcam h, unsigned iOption, int* piValue), (h, iOption, piValue)) \
C(HRESULT, Toupcam_put_Roi, (HToupcam h, unsigned xOffset, unsigned yOffset, unsigned xWidth, unsigned yHeight), (h, xOffset, yOffset, xWidth, yHeight)) \
C(HRESULT, Toupcam_get_Roi, (HToupcam h, unsigned* pxOffset, unsigned* pyOffset, unsigned* pxWidth, unsigned* pyHeight), (h, pxOffset, pyOffset, pxWidth, pyHeight)) \
C(HRESULT, Toupcam_Replug, (const char* camId), (camId)) \
C(HRESULT, Toupcam_get_AfParam, (HToupcam h, ToupcamAfParam* pAfParam), (h, pAfParam)) \
C(HRESULT, Toupcam_IoControl, (HToupcam h, unsigned ioLineNumber, unsigned nType, int outVal, int* inVal), (h, ioLineNumber, nType, outVal, inVal)) \
C(HRESULT, Toupcam_rwc_Flash, (HToupcam h, unsigned action, unsigned addr, unsigned len, void* pData), (h, action, addr, len, pData)) \
C(HRESULT, Toupcam_write_UART, (HToupcam h, const unsigned char* pData, unsigned nDataLen), (h, pData, nDataLen)) \
C(HRESULT, Toupcam_read_UART, (HToupcam h, unsigned char* pBuffer, unsigned nBufferLen), (h, pBuffer, nBufferLen)) \
C(const ToupcamModelV2**, Toupcam_all_Model, (), ()) \
C(const ToupcamModelV2*, Toupcam_query_Model, (HToupcam h), (h)) \
C(const ToupcamModelV2*, Toupcam_get_Model, (unsigned short idVendor, unsigned short idProduct), (idVendor, idProduct)) \
C(HRESULT, Toupcam_Update, (const char* camId, const char* filePath, PITOUPCAM_PROGRESS funProgress, void* ctxProgress), (camId, filePath, funProgress, ctxProgress)) \
C(HRESULT, Toupcam_put_Linear, (HToupcam h, const unsigned char* v8, const unsigned short* v16), (h, v8, v16)) \
C(HRESULT, Toupcam_put_Curve, (HToupcam h, const unsigned char* v8, const unsigned short* v16), (h, v8, v16)) \
C(HRESULT, Toupcam_put_ColorMatrix, (HToupcam h, const double v[9]), (h, v)) \
C(HRESULT, Toupcam_put_InitWBGain, (HToupcam h, const unsigned short v[3]), (h, v)) \
C(HRESULT, Toupcam_get_FrameRate, (HToupcam h, unsigned* nFrame, unsigned* nTime, unsigned* nTotalFrame), (h, nFrame, nTime, nTotalFrame)) \
C(HRESULT, Toupcam_ST4PlusGuide, (HToupcam h, unsigned nDirect, unsigned nDuration), (h, nDirect, nDuration)) \
C(HRESULT, Toupcam_ST4PlusGuideState, (HToupcam h), (h)) \
C(HRESULT, Toupcam_Gain2TempTint, (const int gain[3], int* temp, int* tint), (gain, temp, tint)) \
C(void, Toupcam_TempTint2Gain, (const int temp, const int tint, int gain[3]), (temp, tint, gain)) \
C(double, Toupcam_calc_ClarityFactor, (const void* pImageData, int bits, unsigned nImgWidth, unsigned nImgHeight), (pImageData, bits, nImgWidth, nImgHeight)) \
C(double, Toupcam_calc_ClarityFactorV2, (const void* pImageData, int bits, unsigned nImgWidth, unsigned nImgHeight, unsigned xOffset, unsigned yOffset, unsigned xWidth, unsigned yHeight), (pImageData, bits, nImgWidth, nImgHeight, xOffset, yOffset, xWidth, yHeight)) \
C(void, Toupcam_deBayerV2, (unsigned nFourCC, int nW, int nH, const void* input, void* output, unsigned char nBitDepth, unsigned char nBitCount), (nFourCC, nW, nH, input, output, nBitDepth, nBitCount)) \
C(void, Toupcam_deBayer, (unsigned nFourCC, int nW, int nH, const void* input, void* output, unsigned char nBitDepth), (nFourCC, nW, nH, input, output, nBitDepth)) \
C(HRESULT, Toupcam_put_Demosaic, (HToupcam h, PTOUPCAM_DEMOSAIC_CALLBACK funDemosaic, void* ctxDemosaic), (h, funDemosaic, ctxDemosaic)) \
C(unsigned, Toupcam_Enum, (ToupcamDevice arr[TOUPCAM_MAX]), (arr)) \
C(HRESULT, Toupcam_StartPushModeV2, (HToupcam h, PTOUPCAM_DATA_CALLBACK_V2 funData, void* ctxData), (h, funData, ctxData)) \
C(HRESULT, Toupcam_StartPushMode, (HToupcam h, PTOUPCAM_DATA_CALLBACK funData, void* ctxData), (h, funData, ctxData)) \
C(HRESULT, Toupcam_put_ExpoCallback, (HToupcam h, PITOUPCAM_EXPOSURE_CALLBACK funExpo, void* ctxExpo), (h, funExpo, ctxExpo)) \
C(HRESULT, Toupcam_put_ChromeCallback, (HToupcam h, PITOUPCAM_CHROME_CALLBACK funChrome, void* ctxChrome), (h, funChrome, ctxChrome)) \
C(HRESULT, Toupcam_FfcOnePush, (HToupcam h), (h)) \
C(HRESULT, Toupcam_DfcOnePush, (HToupcam h), (h)) \
C(HRESULT, Toupcam_AwbOnePush, (HToupcam h, PITOUPCAM_TEMPTINT_CALLBACK funTT, void* ctxTT), (h, funTT, ctxTT)) \
C(HRESULT, Toupcam_AbbOnePush, (HToupcam h, PITOUPCAM_BLACKBALANCE_CALLBACK funBB, void* ctxBB), (h, funBB, ctxBB)) \
C(HRESULT, Toupcam_GigeEnable, (PTOUPCAM_HOTPLUG funHotPlug, void* ctxHotPlug), (funHotPlug, ctxHotPlug)) \
C(void, Toupcam_HotPlug, (PTOUPCAM_HOTPLUG funHotPlug, void* ctxHotPlug), (funHotPlug, ctxHotPlug)) \
C(HRESULT, Toupcam_AAF, (HToupcam h, int action, int outVal, int* inVal), (h, action, outVal, inVal)) \
C(HRESULT, Toupcam_put_TempTintInit, (HToupcam h, PITOUPCAM_TEMPTINT_CALLBACK funTT, void* ctxTT), (h, funTT, ctxTT)) \
C(HRESULT, Toupcam_put_ProcessMode, (HToupcam h, unsigned nProcessMode), (h, nProcessMode)) \
C(HRESULT, Toupcam_get_ProcessMode, (HToupcam h, unsigned* pnProcessMode), (h, pnProcessMode)) \
C(HRESULT, Toupcam_put_RoiMode, (HToupcam h, int bRoiMode, int xOffset, int yOffset), (h, bRoiMode, xOffset, yOffset)) \
C(HRESULT, Toupcam_get_RoiMode, (HToupcam h, int* pbRoiMode, int* pxOffset, int* pyOffset), (h, pbRoiMode, pxOffset, pyOffset)) \
C(HRESULT, Toupcam_put_VignetEnable, (HToupcam h, int bEnable), (h, bEnable)) \
C(HRESULT, Toupcam_get_VignetEnable, (HToupcam h, int* bEnable), (h, bEnable)) \
C(HRESULT, Toupcam_put_VignetAmountInt, (HToupcam h, int nAmount), (h, nAmount)) \
C(HRESULT, Toupcam_get_VignetAmountInt, (HToupcam h, int* nAmount), (h, nAmount)) \
C(HRESULT, Toupcam_put_VignetMidPointInt, (HToupcam h, int nMidPoint), (h, nMidPoint)) \
C(HRESULT, Toupcam_get_VignetMidPointInt, (HToupcam h, int* nMidPoint), (h, nMidPoint)) \
C(HRESULT, Toupcam_set_Name, (HToupcam h, const char* name), (h, name)) \
C(HRESULT, Toupcam_query_Name, (HToupcam h, char name[64]), (h, name)) \
C(HRESULT, Toupcam_put_Name, (const char* camId, const char* name), (camId, name)) \
C(HRESULT, Toupcam_get_Name, (const char* camId, char name[64]), (camId, name)) \
C(unsigned, Toupcam_EnumWithName, (ToupcamDeviceV2 pti[TOUPCAM_MAX]), (pti)) \
C(HRESULT, Toupcam_put_RoiN, (HToupcam h, unsigned xOffset[], unsigned yOffset[], unsigned xWidth[], unsigned yHeight[], unsigned Num), (h, xOffset, yOffset, xWidth, yHeight, Num)) \
C(HRESULT, Toupcam_log_File, (const char *filepath), (filepath)) \
C(HRESULT, Toupcam_log_Level, (unsigned level), (level))


#define C C_PTR
TOUPCAM_LIST
#undef C

#define C C_DEF
TOUPCAM_LIST
#undef C

const char *err;
static __attribute__((constructor)) void con() {
    libtoupcam = dlopen(LIBTOUPCAM_NAME, RTLD_LAZY);
#define C C_ASSIGN
TOUPCAM_LIST
#undef C
}

#if 0
/* These are the cameras that are supported by the Touptek driver. */
enum {
    TOUPCAM_MODEL_UA130CA,
    TOUPCAM_MODEL_G3CMOS10300KPA,
    TOUPCAM_MODEL_G3CMOS10300KPA_USB2,
    TOUPCAM_MODEL_E3CMOS01500KMA,
    TOUPCAM_MODEL_E3CMOS01500KMA_USB2,
    TOUPCAM_MODEL_MTR3CCD01400KPB,
    TOUPCAM_MODEL_MTR3CCD01400KPB_USB2,
    TOUPCAM_MODEL_UA1600CA,
    TOUPCAM_MODEL_MTR3CCD09000KPA,
    TOUPCAM_MODEL_MTR3CCD09000KPA_USB2,
    TOUPCAM_MODEL_G3M178M,
    TOUPCAM_MODEL_G3M178M_USB2,
    TOUPCAM_MODEL_G3M178C,
    TOUPCAM_MODEL_G3M178C_USB2,
    TOUPCAM_MODEL_U3CMOS16000KPB,
    TOUPCAM_MODEL_U3CMOS16000KPB_USB2,
    TOUPCAM_MODEL_E3ISPM02000KPA,
    TOUPCAM_MODEL_E3ISPM02000KPA_USB2,
    TOUPCAM_MODEL_G3CMOS20000KMA,
    TOUPCAM_MODEL_G3CMOS20000KMA_USB2,
    TOUPCAM_MODEL_U3CCD12000KPA,
    TOUPCAM_MODEL_U3CCD12000KPA_USB2,
    TOUPCAM_MODEL_ATR3CMOS16000KMA,
    TOUPCAM_MODEL_ATR3CMOS16000KMA_USB2,
    TOUPCAM_MODEL_G3CMOS20000KPA,
    TOUPCAM_MODEL_G3CMOS20000KPA_USB2,
    TOUPCAM_MODEL_G3CMOS02300KPA,
    TOUPCAM_MODEL_G3CMOS02300KPA_USB2,
    TOUPCAM_MODEL_I3ISPM12000KPA,
    TOUPCAM_MODEL_I3ISPM12000KPA_USB2,
    TOUPCAM_MODEL_U3CCD09000KPA,
    TOUPCAM_MODEL_U3CCD09000KPA_USB2,
    TOUPCAM_MODEL_G3CMOS02300KMC,
    TOUPCAM_MODEL_G3CMOS02300KMC_USB2,
    TOUPCAM_MODEL_E3ISPM12300KPA,
    TOUPCAM_MODEL_E3ISPM12300KPA_USB2,
    TOUPCAM_MODEL_ECMOS06600KPA,
    TOUPCAM_MODEL_ECMOS08300KPA,
    TOUPCAM_MODEL_UA1000CA,
    TOUPCAM_MODEL_UA1000CA_2,
    TOUPCAM_MODEL_UA510CA,
    TOUPCAM_MODEL_UA510CA_2,
    TOUPCAM_MODEL_UA310CA,
    TOUPCAM_MODEL_UA310CA_2,
    TOUPCAM_MODEL_E3ISPM08300KPA,
    TOUPCAM_MODEL_E3ISPM08300KPA_USB2,
    TOUPCAM_MODEL_G3CMOS16000KMA,
    TOUPCAM_MODEL_G3CMOS16000KMA_USB2,
    TOUPCAM_MODEL_G3CMOS16000KPA,
    TOUPCAM_MODEL_G3CMOS16000KPA_USB2,
    TOUPCAM_MODEL_G3M287M,
    TOUPCAM_MODEL_G3M287M_USB2,
    TOUPCAM_MODEL_G3M385C,
    TOUPCAM_MODEL_G3M385C_USB2,
    TOUPCAM_MODEL_EP3CMOS00400KMA,
    TOUPCAM_MODEL_EP3CMOS00400KMA_USB2,
    TOUPCAM_MODEL_EP3CMOS00400KPA,
    TOUPCAM_MODEL_EP3CMOS00400KPA_USB2,
    TOUPCAM_MODEL_E3CMOS00400KMA,
    TOUPCAM_MODEL_E3CMOS00400KMA_USB2,
    TOUPCAM_MODEL_E3CMOS00400KPA,
    TOUPCAM_MODEL_E3CMOS00400KPA_USB2,
    TOUPCAM_MODEL_G3M290C,
    TOUPCAM_MODEL_G3M290C_USB2,
    TOUPCAM_MODEL_G3M290M,
    TOUPCAM_MODEL_G3M290M_USB2,
    TOUPCAM_MODEL_G3M224C,
    TOUPCAM_MODEL_G3M224C_USB2,
    TOUPCAM_MODEL_U3ISPM16000KPB,
    TOUPCAM_MODEL_U3ISPM16000KPB_USB2,
    TOUPCAM_MODEL_E3CMOS05000KMA,
    TOUPCAM_MODEL_E3CMOS05000KMA_USB2,
    TOUPCAM_MODEL_E3CMOS03100KMC,
    TOUPCAM_MODEL_E3CMOS03100KMC_USB2,
    TOUPCAM_MODEL_E3ISPM09000KPA,
    TOUPCAM_MODEL_E3ISPM09000KPA_USB2,
    TOUPCAM_MODEL_I3CMOS01200KPA,
    TOUPCAM_MODEL_I3CMOS01200KPA_USB2,
    TOUPCAM_MODEL_U3ISPM16000KPA,
    TOUPCAM_MODEL_U3ISPM16000KPA_USB2,
    TOUPCAM_MODEL_U3CMOS16000KMB,
    TOUPCAM_MODEL_U3CMOS16000KMB_USB2,
    TOUPCAM_MODEL_G3CMOS02300KPC,
    TOUPCAM_MODEL_G3CMOS02300KPC_USB2,
    TOUPCAM_MODEL_G3M178M_2,
    TOUPCAM_MODEL_G3M178M_USB2_2,
    TOUPCAM_MODEL_G3M178C_2,
    TOUPCAM_MODEL_G3M178C_USB2_2,
    TOUPCAM_MODEL_E3ISPM03100KPA,
    TOUPCAM_MODEL_E3ISPM03100KPA_USB2,
    TOUPCAM_MODEL_E3ISPM05000KPA,
    TOUPCAM_MODEL_E3ISPM05000KPA_USB2,
    TOUPCAM_MODEL_MG3CMOS16000KPA,
    TOUPCAM_MODEL_MG3CMOS16000KPA_USB2,
    TOUPCAM_MODEL_MG3CMOS02300KPA,
    TOUPCAM_MODEL_MG3CMOS02300KPA_USB2,
    TOUPCAM_MODEL_U3ISPM18000KPA,
    TOUPCAM_MODEL_U3ISPM18000KPA_USB2,
    TOUPCAM_MODEL_E3ISPM03100KPB,
    TOUPCAM_MODEL_E3ISPM03100KPB_USB2,
    TOUPCAM_MODEL_EP3CMOS02300KPC,
    TOUPCAM_MODEL_EP3CMOS02300KPC_USB2,
    TOUPCAM_MODEL_EP3CMOS02300KMC,
    TOUPCAM_MODEL_EP3CMOS02300KMC_USB2,
    TOUPCAM_MODEL_EP3CMOS06300KMA,
    TOUPCAM_MODEL_EP3CMOS06300KMA_USB2,
    TOUPCAM_MODEL_EP3CMOS20000KPA,
    TOUPCAM_MODEL_EP3CMOS20000KPA_USB2,
    TOUPCAM_MODEL_GPCMOS02000KMA,
    TOUPCAM_MODEL_EXCCD01400KPA,
    TOUPCAM_MODEL_I3CMOS03100KPA,
    TOUPCAM_MODEL_I3CMOS03100KPA_USB2,
    TOUPCAM_MODEL_U3CMOS10000KMA,
    TOUPCAM_MODEL_U3CMOS10000KMA_USB2,
    TOUPCAM_MODEL_E3ISPM12000KPA,
    TOUPCAM_MODEL_E3ISPM12000KPA_USB2,
    TOUPCAM_MODEL_E3ISPM05000KPA_2,
    TOUPCAM_MODEL_E3ISPM05000KPA_USB2_2,
    TOUPCAM_MODEL_AF3CMOS06300KPA,
    TOUPCAM_MODEL_AF3CMOS06300KPA_USB2,
    TOUPCAM_MODEL_E3ISPM20000KPA,
    TOUPCAM_MODEL_E3ISPM20000KPA_USB2,
    TOUPCAM_MODEL_G3CMOS20000KPA_2,
    TOUPCAM_MODEL_G3CMOS20000KPA_USB2_2,
    TOUPCAM_MODEL_E3CMOS02300KMC,
    TOUPCAM_MODEL_E3CMOS02300KMC_USB2,
    TOUPCAM_MODEL_ATR3CMOS02300KMB,
    TOUPCAM_MODEL_ATR3CMOS02300KMB_USB2,
    TOUPCAM_MODEL_E3CMOS20000KPA,
    TOUPCAM_MODEL_E3CMOS20000KPA_USB2,
    TOUPCAM_MODEL_MTR3CMOS16000KPA,
    TOUPCAM_MODEL_MTR3CMOS16000KPA_USB2,
    TOUPCAM_MODEL_MTR3CMOS16000KMA,
    TOUPCAM_MODEL_MTR3CMOS16000KMA_USB2,
    TOUPCAM_MODEL_ECMOS05300KPA,
    TOUPCAM_MODEL_ECMOS03100KPA,
    TOUPCAM_MODEL_E3ISPM06300KPA,
    TOUPCAM_MODEL_E3ISPM06300KPA_USB2,
    TOUPCAM_MODEL_ECMOS01200KPA,
    TOUPCAM_MODEL_E3CMOS01200KPA,
    TOUPCAM_MODEL_E3CMOS01200KPA_USB2,
    TOUPCAM_MODEL_G3CMOS16000KMA_2,
    TOUPCAM_MODEL_G3CMOS16000KMA_USB2_2,
    TOUPCAM_MODEL_ATR3CMOS02300KPB,
    TOUPCAM_MODEL_ATR3CMOS02300KPB_USB2,
    TOUPCAM_MODEL_ATR3CMOS06300KPA,
    TOUPCAM_MODEL_ATR3CMOS06300KPA_USB2,
    TOUPCAM_MODEL_ATR3CMOS16000KPA,
    TOUPCAM_MODEL_ATR3CMOS16000KPA_USB2,
    TOUPCAM_MODEL_G3CMOS16000KPA_2,
    TOUPCAM_MODEL_G3CMOS16000KPA_USB2_2,
    TOUPCAM_MODEL_ECMOS02000KPA,
    TOUPCAM_MODEL_EP3CMOS06300KPA,
    TOUPCAM_MODEL_EP3CMOS06300KPA_USB2,
    TOUPCAM_MODEL_E3CMOS05000KMA_2,
    TOUPCAM_MODEL_E3CMOS05000KMA_USB2_2,
    TOUPCAM_MODEL_E3CMOS05000KPA,
    TOUPCAM_MODEL_E3CMOS05000KPA_USB2,
    TOUPCAM_MODEL_GPCMOS02000KPA,
    TOUPCAM_MODEL_EXCCD00440KPB,
    TOUPCAM_MODEL_GPCMOS01200KMA,
    TOUPCAM_MODEL_EXCCD00300KPA,
    TOUPCAM_MODEL_U3CMOS16000KMB_2,
    TOUPCAM_MODEL_U3CMOS16000KMB_USB2_2,
    TOUPCAM_MODEL_U3CCD01400KPB,
    TOUPCAM_MODEL_U3CCD01400KPB_USB2,
    TOUPCAM_MODEL_U3CCD01400KMB,
    TOUPCAM_MODEL_U3CCD01400KMB_USB2,
    TOUPCAM_MODEL_G3CMOS06300KMA,
    TOUPCAM_MODEL_G3CMOS06300KMA_USB2,
    TOUPCAM_MODEL_MTR3CCD01400KMB,
    TOUPCAM_MODEL_MTR3CCD01400KMB_USB2,
    TOUPCAM_MODEL_MTR3CCD01400KPB_2,
    TOUPCAM_MODEL_MTR3CCD01400KPB_USB2_2,
    TOUPCAM_MODEL_GPCMOS01200KMB,
    TOUPCAM_MODEL_U3CMOS16000KPB_2,
    TOUPCAM_MODEL_U3CMOS16000KPB_USB2_2,
    TOUPCAM_MODEL_GPCMOS01200KPB,
    TOUPCAM_MODEL_G3CMOS01200KPA,
    TOUPCAM_MODEL_G3CMOS01200KPA_USB2,
    TOUPCAM_MODEL_G3CMOS06300KPA,
    TOUPCAM_MODEL_G3CMOS06300KPA_USB2,
    TOUPCAM_MODEL_E3CMOS06300KMA,
    TOUPCAM_MODEL_E3CMOS06300KMA_USB2,
    TOUPCAM_MODEL_X05100KPA,
    TOUPCAM_MODEL_X01200KPA,
    TOUPCAM_MODEL_GPCMOS01200KPC,
    TOUPCAM_MODEL_ATR3CCD06000KPA,
    TOUPCAM_MODEL_ATR3CCD06000KPA_USB2,
    TOUPCAM_MODEL_ATR3CCD06000KMA,
    TOUPCAM_MODEL_ATR3CCD06000KMA_USB2,
    TOUPCAM_MODEL_MTR3CCD02800KMA,
    TOUPCAM_MODEL_MTR3CCD02800KMA_USB2,
    TOUPCAM_MODEL_MTR3CCD02800KPA,
    TOUPCAM_MODEL_MTR3CCD02800KPA_USB2,
    TOUPCAM_MODEL_U3CCD06000KMA,
    TOUPCAM_MODEL_U3CCD06000KMA_USB2,
    TOUPCAM_MODEL_MTR3CCD01400KMA,
    TOUPCAM_MODEL_MTR3CCD01400KMA_USB2,
    TOUPCAM_MODEL_G3CMOS02300KMC_2,
    TOUPCAM_MODEL_G3CMOS02300KMC_USB2_2,
    TOUPCAM_MODEL_MTR3CCD06000KMA,
    TOUPCAM_MODEL_MTR3CCD06000KMA_USB2,
    TOUPCAM_MODEL_U3CCD02800KMA,
    TOUPCAM_MODEL_U3CCD02800KMA_USB2,
    TOUPCAM_MODEL_GCMOS01200KMA,
    TOUPCAM_MODEL_GCMOS01200KMB,
    TOUPCAM_MODEL_G3CMOS02300KPB,
    TOUPCAM_MODEL_G3CMOS02300KPB_USB2,
    TOUPCAM_MODEL_GCMOS01200KPB,
    TOUPCAM_MODEL_U3CCD02800KPA,
    TOUPCAM_MODEL_U3CCD02800KPA_USB2,
    TOUPCAM_MODEL_MTR3CCD01400KPA,
    TOUPCAM_MODEL_MTR3CCD01400KPA_USB2,
    TOUPCAM_MODEL_U3CMOS16000KPA,
    TOUPCAM_MODEL_U3CMOS16000KPA_USB2,
    TOUPCAM_MODEL_U3CCD06000KPA,
    TOUPCAM_MODEL_U3CCD06000KPA_USB2,
    TOUPCAM_MODEL_EXCCD00300KMA,
    TOUPCAM_MODEL_EXCCD00300KMA_2,
    TOUPCAM_MODEL_EP3CMOS02300KPA,
    TOUPCAM_MODEL_EP3CMOS02300KPA_USB2,
    TOUPCAM_MODEL_EXCCD00440KPA,
    TOUPCAM_MODEL_EXCCD00440KMB,
    TOUPCAM_MODEL_EXCCD00440KMA,
    TOUPCAM_MODEL_MTR3CCD06000KPA,
    TOUPCAM_MODEL_MTR3CCD06000KPA_USB2,
    TOUPCAM_MODEL_EXCCD00440KPB_2,
    TOUPCAM_MODEL_ICMOS03100KPA,
    TOUPCAM_MODEL_E3CMOS12000KPA,
    TOUPCAM_MODEL_E3CMOS12000KPA_USB2,
    TOUPCAM_MODEL_E3CMOS03100KPB,
    TOUPCAM_MODEL_E3CMOS03100KPB_USB2,
    TOUPCAM_MODEL_UCMOS05100KPC,
    TOUPCAM_MODEL_E3CMOS02300KPB,
    TOUPCAM_MODEL_E3CMOS02300KPB_USB2,
    TOUPCAM_MODEL_UHCCD05200KMA,
    TOUPCAM_MODEL_G3CMOS02300KPA_2,
    TOUPCAM_MODEL_G3CMOS02300KPA_USB2_2,
    TOUPCAM_MODEL_GCMOS01200KMA_2,
    TOUPCAM_MODEL_E3CMOS06300KPA,
    TOUPCAM_MODEL_E3CMOS06300KPA_USB2,
    TOUPCAM_MODEL_ICMOS14000KPC,
    TOUPCAM_MODEL_EXCCD00300KMA_2_2,
    TOUPCAM_MODEL_E3CMOS02300KPA,
    TOUPCAM_MODEL_E3CMOS02300KPA_USB2,
    TOUPCAM_MODEL_E3CMOS03100KPA,
    TOUPCAM_MODEL_E3CMOS03100KPA_USB2,
    TOUPCAM_MODEL_EXCCD00440KMB_2,
    TOUPCAM_MODEL_EXCCD00440KPB_2_2,
    TOUPCAM_MODEL_EXCCD00440KMA_2,
    TOUPCAM_MODEL_AAICX429C,
    TOUPCAM_MODEL_U3CMOS18000KPA,
    TOUPCAM_MODEL_U3CMOS18000KPA_USB2,
    TOUPCAM_MODEL_U3CMOS03100KPC,
    TOUPCAM_MODEL_U3CMOS03100KPC_USB2,
    TOUPCAM_MODEL_L3CMOS03100KPB,
    TOUPCAM_MODEL_L3CMOS03100KPB_USB2,
    TOUPCAM_MODEL_GCMOS03100KPA,
    TOUPCAM_MODEL_UHCCD05200KPA,
    TOUPCAM_MODEL_UHCCD05000KPA,
    TOUPCAM_MODEL_UHCCD03100KPB,
    TOUPCAM_MODEL_UHCCD02000KPA,
    TOUPCAM_MODEL_UHCCD01400KPB,
    TOUPCAM_MODEL_UHCCD01400KMB,
    TOUPCAM_MODEL_EXCCD01400KPA_2,
    TOUPCAM_MODEL_EXCCD01400KMA,
    TOUPCAM_MODEL_ICMOS10000KPA,
    TOUPCAM_MODEL_ICMOS14000KPA,
    TOUPCAM_MODEL_LCMOS03100KPB,
    TOUPCAM_MODEL_UCMOS02000KPC,
    TOUPCAM_MODEL_WCMOS10000KPA,
    TOUPCAM_MODEL_UCMOS03100KPB,
    TOUPCAM_MODEL_GCMOS01200KMB_2,
    TOUPCAM_MODEL_GCMOS01200KPB_2,
    TOUPCAM_MODEL_GCMOS01200KPB_2_2,
    TOUPCAM_MODEL_UCMOS01200KPB,
    TOUPCAM_MODEL_U3CMOS14000KPB,
    TOUPCAM_MODEL_U3CMOS14000KPB_USB2,
    TOUPCAM_MODEL_LCMOS01200KPB,
    TOUPCAM_MODEL_LCMOS14000KPA,
    TOUPCAM_MODEL_LCMOS10000KPA,
    TOUPCAM_MODEL_LCMOS09000KPA,
    TOUPCAM_MODEL_LCMOS08000KPA,
    TOUPCAM_MODEL_LCMOS05100KPA,
    TOUPCAM_MODEL_LCMOS03100KPA,
    TOUPCAM_MODEL_LCMOS02000KPA,
    TOUPCAM_MODEL_LCMOS01300KPA,
    TOUPCAM_MODEL_SCCCD01400KMB,
    TOUPCAM_MODEL_UHCCD05200KPA_2,
    TOUPCAM_MODEL_UHCCD05100KPA,
    TOUPCAM_MODEL_UHCCD05100KPA_2,
    TOUPCAM_MODEL_UHCCD05000KPA_2,
    TOUPCAM_MODEL_UHCCD03100KPB_2,
    TOUPCAM_MODEL_UHCCD02000KPA_2,
    TOUPCAM_MODEL_UHCCD01400KPB_2,
    TOUPCAM_MODEL_UHCCD01400KPA,
    TOUPCAM_MODEL_UHCCD01400KMB_2,
    TOUPCAM_MODEL_UHCCD01400KMA,
    TOUPCAM_MODEL_UHCCD00800KPA,
    TOUPCAM_MODEL_UCMOS05100KMA,
    TOUPCAM_MODEL_UCMOS14000KPA,
    TOUPCAM_MODEL_UCMOS10000KPA,
    TOUPCAM_MODEL_UCMOS09000KPB,
    TOUPCAM_MODEL_UCMOS09000KPA,
    TOUPCAM_MODEL_UCMOS08000KPB,
    TOUPCAM_MODEL_UCMOS08000KPA,
    TOUPCAM_MODEL_UCMOS05100KPA,
    TOUPCAM_MODEL_UCMOS03100KPA,
    TOUPCAM_MODEL_UCMOS02000KPB,
    TOUPCAM_MODEL_UCMOS02000KPA,
    TOUPCAM_MODEL_UCMOS01300KPA,
    TOUPCAM_MODEL_UCMOS01300KMA,
    TOUPCAM_MODEL_UCMOS00350KPA,
    TOUPCAM_MODEL_U3CMOS14000KPA,
    TOUPCAM_MODEL_U3CMOS14000KPA_USB2,
    TOUPCAM_MODEL_U3CMOS10000KPA,
    TOUPCAM_MODEL_U3CMOS10000KPA_USB2,
    TOUPCAM_MODEL_U3CMOS08500KPA,
    TOUPCAM_MODEL_U3CMOS08500KPA_USB2,
    TOUPCAM_MODEL_U3CMOS05100KPA,
    TOUPCAM_MODEL_U3CMOS05100KPA_USB2,
    TOUPCAM_MODEL_U3CMOS03100KPB,
    TOUPCAM_MODEL_U3CMOS03100KPB_USB2,
    TOUPCAM_MODEL_U3CMOS03100KPA,
    TOUPCAM_MODEL_U3CMOS03100KPA_USB2,
    TOUPCAM_MODEL_SCCCD05200KPA,
    TOUPCAM_MODEL_SCCCD01400KMA,
    TOUPCAM_MODEL_SCCCD01400KPB,
    TOUPCAM_MODEL_SCCCD01400KPA,
    TOUPCAM_MODEL_EXCCD01400KPA_2_2,
    TOUPCAM_MODEL_EXCCD01400KMA_2,
    TOUPCAM_MODEL_EXCCD00300KMA_2_2_2,
    TOUPCAM_MODEL_ICMOS03100KPA_2,
    TOUPCAM_MODEL_ICMOS01300KMA,
    TOUPCAM_MODEL_UCMOS01200KMA,
    TOUPCAM_MODEL_L3CMOS14000KPA,
    TOUPCAM_MODEL_L3CMOS14000KPA_USB2,
    TOUPCAM_MODEL_L3CMOS10000KPA,
    TOUPCAM_MODEL_L3CMOS10000KPA_USB2,
    TOUPCAM_MODEL_L3CMOS05100KPA,
    TOUPCAM_MODEL_L3CMOS05100KPA_USB2,
    TOUPCAM_MODEL_L3CMOS03100KPA,
    TOUPCAM_MODEL_L3CMOS03100KPA_USB2,
    TOUPCAM_MODEL_MTR3CMOS20000KPA,
    TOUPCAM_MODEL_MTR3CMOS20000KPA_USB2,
    TOUPCAM_MODEL_ATR3CMOS20000KPA,
    TOUPCAM_MODEL_ATR3CMOS20000KPA_USB2,
    TOUPCAM_MODEL_MTR3CCD12000KPA,
    TOUPCAM_MODEL_MTR3CCD12000KPA_USB2,
    TOUPCAM_MODEL_E3ISPM01500KPA,
    TOUPCAM_MODEL_E3ISPM01500KPA_USB2,
    TOUPCAM_MODEL_E3CMOS20000KMA,
    TOUPCAM_MODEL_E3CMOS20000KMA_USB2,
    TOUPCAM_MODEL_G3CMOS20000KPA_2_2,
    TOUPCAM_MODEL_G3CMOS20000KPA_USB2_2_2,
    TOUPCAM_MODEL_BIGEYE10000KPA,
    TOUPCAM_MODEL_BIGEYE10000KPA_USB2,
    TOUPCAM_MODEL_BIGEYE4200KMB,
    TOUPCAM_MODEL_BIGEYE4200KMB_USB2,
    TOUPCAM_MODEL_G3CMOS20000KMA_2,
    TOUPCAM_MODEL_G3CMOS20000KMA_USB2_2,
    TOUPCAM_MODEL_ATR3CMOS20000KMA,
    TOUPCAM_MODEL_ATR3CMOS20000KMA_USB2,
    TOUPCAM_MODEL_MTR3CMOS20000KMA,
    TOUPCAM_MODEL_MTR3CMOS20000KMA_USB2,
    TOUPCAM_MODEL_MTR3CMOS16000KPA_2,
    TOUPCAM_MODEL_MTR3CMOS16000KPA_USB2_2,
    TOUPCAM_MODEL_E3CMOS01200KPA_2,
    TOUPCAM_MODEL_E3CMOS01200KPA_USB2_2,
    TOUPCAM_MODEL_UCMOS01300KPA_2,
    TOUPCAM_MODEL_ATR3CMOS16000KPA_2,
    TOUPCAM_MODEL_ATR3CMOS16000KPA_USB2_2,
    TOUPCAM_MODEL_E3CMOS12300KMA,
    TOUPCAM_MODEL_E3CMOS12300KMA_USB2,
    TOUPCAM_MODEL_E3ISPM08300KPB,
    TOUPCAM_MODEL_E3ISPM08300KPB_USB2,
    TOUPCAM_MODEL_ATR3CMOS16000KMA_2,
    TOUPCAM_MODEL_ATR3CMOS16000KMA_USB2_2,
    TOUPCAM_MODEL_ATR3CMOS10300KPA,
    TOUPCAM_MODEL_ATR3CMOS10300KPA_USB2,
    TOUPCAM_MODEL_LCMOS01300KPA_2,
    TOUPCAM_MODEL_MTR3CCD12000KMA,
    TOUPCAM_MODEL_MTR3CCD12000KMA_USB2,
    TOUPCAM_MODEL_I3ISPM01500KPA,
    TOUPCAM_MODEL_I3ISPM01500KPA_USB2,
    TOUPCAM_MODEL_I3ISPM01500KPA_2,
    TOUPCAM_MODEL_I3ISPM01500KPA_USB2_2,
    TOUPCAM_MODEL_MTR3CMOS10300KPA,
    TOUPCAM_MODEL_MTR3CMOS10300KPA_USB2,
    TOUPCAM_MODEL_I3ISPM02000KPA,
    TOUPCAM_MODEL_I3ISPM02000KPA_USB2,
    TOUPCAM_MODEL_ECMOS05000KPA,
    TOUPCAM_MODEL_G3CMOS16000KPA_2_2,
    TOUPCAM_MODEL_G3CMOS16000KPA_USB2_2_2,
    TOUPCAM_MODEL_G3CMOS16000KMA_2_2,
    TOUPCAM_MODEL_G3CMOS16000KMA_USB2_2_2,
    TOUPCAM_MODEL_E3ISPM06300KPB,
    TOUPCAM_MODEL_E3ISPM06300KPB_USB2,
    TOUPCAM_MODEL_G3CMOS10300KPA_2,
    TOUPCAM_MODEL_G3CMOS10300KPA_USB2_2,
    TOUPCAM_MODEL_C3CMOS05100KPA,
    TOUPCAM_MODEL_C3CMOS05100KPA_USB2,
    TOUPCAM_MODEL_MTR3CMOS16000KMA_2,
    TOUPCAM_MODEL_MTR3CMOS16000KMA_USB2_2,
    TOUPCAM_MODEL_MTR3CMOS16000KPA_2_2,
    TOUPCAM_MODEL_MTR3CMOS16000KPA_USB2_2_2,
    TOUPCAM_MODEL_I3CMOS01500KMA,
    TOUPCAM_MODEL_I3CMOS01500KMA_USB2,
    TOUPCAM_MODEL_I3CMOS01500KMA_2,
    TOUPCAM_MODEL_I3CMOS01500KMA_USB2_2,
    TOUPCAM_MODEL_BIGEYE4200KMA,
    TOUPCAM_MODEL_BIGEYE4200KMA_USB2,
    TOUPCAM_MODEL_E3CMOS20000KPB,
    TOUPCAM_MODEL_E3CMOS20000KPB_USB2,
    TOUPCAM_MODEL_ATR3CMOS01700KMA,
    TOUPCAM_MODEL_ATR3CMOS01700KMA_USB2,
    TOUPCAM_MODEL_ATR3CMOS01700KPA,
    TOUPCAM_MODEL_ATR3CMOS01700KPA_USB2,
    TOUPCAM_MODEL_ATR3CMOS07100KMA,
    TOUPCAM_MODEL_ATR3CMOS07100KMA_USB2,
    TOUPCAM_MODEL_ATR3CMOS07100KPA,
    TOUPCAM_MODEL_ATR3CMOS07100KPA_USB2,
    TOUPCAM_MODEL_MTR3CMOS07100KPA,
    TOUPCAM_MODEL_MTR3CMOS07100KPA_USB2,
    TOUPCAM_MODEL_MTR3CMOS07100KMA,
    TOUPCAM_MODEL_MTR3CMOS07100KMA_USB2,
    TOUPCAM_MODEL_MTR3CMOS01700KPA,
    TOUPCAM_MODEL_MTR3CMOS01700KPA_USB2,
    TOUPCAM_MODEL_MTR3CMOS01700KMA,
    TOUPCAM_MODEL_MTR3CMOS01700KMA_USB2,
    TOUPCAM_MODEL_SL170_C_M,
    TOUPCAM_MODEL_SL170_C_M_USB2,
    TOUPCAM_MODEL_PUM02000KPA,
    TOUPCAM_MODEL_GPCMOS02000KPB,
    TOUPCAM_MODEL_I3CMOS03100KMA,
    TOUPCAM_MODEL_I3CMOS03100KMA_USB2,
    TOUPCAM_MODEL_I3CMOS03100KMA_2,
    TOUPCAM_MODEL_I3CMOS03100KMA_USB2_2,
    TOUPCAM_MODEL_I3CMOS05000KMA,
    TOUPCAM_MODEL_I3CMOS05000KMA_USB2,
    TOUPCAM_MODEL_I3CMOS05000KMA_2,
    TOUPCAM_MODEL_I3CMOS05000KMA_USB2_2,
    TOUPCAM_MODEL_C3CMOS10000KPA,
    TOUPCAM_MODEL_C3CMOS10000KPA_USB2,
    TOUPCAM_MODEL_I3ISPM05000KPA,
    TOUPCAM_MODEL_I3ISPM05000KPA_USB2,
    TOUPCAM_MODEL_I3ISPM05000KPA_2,
    TOUPCAM_MODEL_I3ISPM05000KPA_USB2_2,
    TOUPCAM_MODEL_I3ISPM03100KPA,
    TOUPCAM_MODEL_I3ISPM03100KPA_USB2,
    TOUPCAM_MODEL_I3ISPM03100KPA_2,
    TOUPCAM_MODEL_I3ISPM03100KPA_USB2_2,
    TOUPCAM_MODEL_E3ISPM18000KPA,
    TOUPCAM_MODEL_E3ISPM18000KPA_USB2,
    TOUPCAM_MODEL_E3ISPM20000KPB,
    TOUPCAM_MODEL_E3ISPM20000KPB_USB2,
    TOUPCAM_MODEL_I3ISPM05000KPB,
    TOUPCAM_MODEL_I3ISPM05000KPB_USB2,
    TOUPCAM_MODEL_I3ISPM05000KPB_2,
    TOUPCAM_MODEL_I3ISPM05000KPB_USB2_2,
    TOUPCAM_MODEL_I3ISPM03100KPB,
    TOUPCAM_MODEL_I3ISPM03100KPB_USB2,
    TOUPCAM_MODEL_I3ISPM03100KPB_2,
    TOUPCAM_MODEL_I3ISPM03100KPB_USB2_2,
    TOUPCAM_MODEL_I3CMOS05000KMB,
    TOUPCAM_MODEL_I3CMOS05000KMB_USB2,
    TOUPCAM_MODEL_I3CMOS05000KMB_2,
    TOUPCAM_MODEL_I3CMOS05000KMB_USB2_2,
    TOUPCAM_MODEL_I3CMOS03100KMB,
    TOUPCAM_MODEL_I3CMOS03100KMB_USB2,
    TOUPCAM_MODEL_I3CMOS03100KMB_2,
    TOUPCAM_MODEL_I3CMOS03100KMB_USB2_2,
    TOUPCAM_MODEL_E3ISPM21000KPA,
    TOUPCAM_MODEL_E3ISPM21000KPA_USB2,
    TOUPCAM_MODEL_ECMOS05100KPA,
    TOUPCAM_MODEL_E3ISPM15600KPA,
    TOUPCAM_MODEL_E3ISPM15600KPA_USB2,
    TOUPCAM_MODEL_I3CMOS00500KMA,
    TOUPCAM_MODEL_I3CMOS00500KMA_USB2,
    TOUPCAM_MODEL_I3ISPM00500KPA,
    TOUPCAM_MODEL_I3ISPM00500KPA_USB2,
    TOUPCAM_MODEL_IUA6300KPA,
    TOUPCAM_MODEL_IUA6300KPA_USB2,
    TOUPCAM_MODEL_IUB4200KMB,
    TOUPCAM_MODEL_IUB4200KMB_USB2,
    TOUPCAM_MODEL_IUC31000KMA,
    TOUPCAM_MODEL_IUC31000KMA_USB2,
    TOUPCAM_MODEL_IUA6300KMA,
    TOUPCAM_MODEL_IUA6300KMA_USB2,
    TOUPCAM_MODEL_IUA1700KPA,
    TOUPCAM_MODEL_IUA1700KPA_USB2,
    TOUPCAM_MODEL_IUA7100KPA,
    TOUPCAM_MODEL_IUA7100KPA_USB2,
    TOUPCAM_MODEL_IUC31000KPA,
    TOUPCAM_MODEL_IUC31000KPA_USB2,
    TOUPCAM_MODEL_ATR3CMOS21000KPA,
    TOUPCAM_MODEL_ATR3CMOS21000KPA_USB2,
    TOUPCAM_MODEL_IUA20000KPA,
    TOUPCAM_MODEL_IUA20000KPA_USB2,
    TOUPCAM_MODEL_T3CMOS20000KPA,
    TOUPCAM_MODEL_T3CMOS20000KPA_USB2,
    TOUPCAM_MODEL_MTR3CMOS21000KPA,
    TOUPCAM_MODEL_MTR3CMOS21000KPA_USB2,
    TOUPCAM_MODEL_GPCMOS01200KPF,
    TOUPCAM_MODEL_I3CMOS06300KMA,
    TOUPCAM_MODEL_I3CMOS06300KMA_USB2,
    TOUPCAM_MODEL_I3ISPM06300KPA,
    TOUPCAM_MODEL_I3ISPM06300KPA_USB2,
    TOUPCAM_MODEL_IUA20000KMA,
    TOUPCAM_MODEL_IUA20000KMA_USB2,
    TOUPCAM_MODEL_IUB43000KMA,
    TOUPCAM_MODEL_IUB43000KMA_USB2,
    TOUPCAM_MODEL_IUC60000KMA,
    TOUPCAM_MODEL_IUC60000KMA_USB2,
    TOUPCAM_MODEL_IUC60000KPA,
    TOUPCAM_MODEL_IUC60000KPA_USB2,
    TOUPCAM_MODEL_IUA2300KPB,
    TOUPCAM_MODEL_IUA2300KPB_USB2,
    TOUPCAM_MODEL_IUC26000KPA,
    TOUPCAM_MODEL_IUC26000KPA_USB2,
    TOUPCAM_MODEL_MTR3CMOS45000KMA,
    TOUPCAM_MODEL_MTR3CMOS45000KMA_USB2,
    TOUPCAM_MODEL_C3CMOS05100KPB,
    TOUPCAM_MODEL_C3CMOS05100KPB_USB2,
    TOUPCAM_MODEL_C3CMOS03500KPA,
    TOUPCAM_MODEL_C3CMOS03500KPA_USB2,
    TOUPCAM_MODEL_MAX60000KPA,
    TOUPCAM_MODEL_MAX60000KPA_USB2,
    TOUPCAM_MODEL_BIGEYE4200KMB_2,
    TOUPCAM_MODEL_BIGEYE4200KMB_USB2_2,
    TOUPCAM_MODEL_IUA1700KMA,
    TOUPCAM_MODEL_IUA1700KMA_USB2,
    TOUPCAM_MODEL_IUA7100KMA,
    TOUPCAM_MODEL_IUA7100KMA_USB2,
    TOUPCAM_MODEL_ATR3CMOS26000KPA,
    TOUPCAM_MODEL_ATR3CMOS26000KPA_USB2,
    TOUPCAM_MODEL_E3ISPM09000KPB,
    TOUPCAM_MODEL_E3ISPM09000KPB_USB2,
    TOUPCAM_MODEL_ATR3CMOS09000KPA,
    TOUPCAM_MODEL_ATR3CMOS09000KPA_USB2,
    TOUPCAM_MODEL_G3CMOS21000KPA,
    TOUPCAM_MODEL_G3CMOS21000KPA_USB2,
    TOUPCAM_MODEL_ITR3CMOS16000KMA,
    TOUPCAM_MODEL_ITR3CMOS16000KMA_USB2,
    TOUPCAM_MODEL_MTR3CMOS26000KPA,
    TOUPCAM_MODEL_MTR3CMOS26000KPA_USB2,
    TOUPCAM_MODEL_BIGEYE4200KMB_2_2,
    TOUPCAM_MODEL_BIGEYE4200KMB_USB2_2_2,
    TOUPCAM_MODEL_SKYEYE62AM,
    TOUPCAM_MODEL_SKYEYE62AM_USB2,
    TOUPCAM_MODEL_MTR3CMOS09000KPA,
    TOUPCAM_MODEL_MTR3CMOS09000KPA_USB2,
    TOUPCAM_MODEL_BIGEYE4200KMD,
    TOUPCAM_MODEL_BIGEYE4200KMD_USB2,
    TOUPCAM_MODEL_IUA4100KPA,
    TOUPCAM_MODEL_IUA4100KPA_USB2,
    TOUPCAM_MODEL_IUA2100KPA,
    TOUPCAM_MODEL_IUA2100KPA_USB2,
    TOUPCAM_MODEL_E3CMOS45000KMA,
    TOUPCAM_MODEL_E3CMOS45000KMA_USB2,
    TOUPCAM_MODEL_SKYEYE62AC,
    TOUPCAM_MODEL_SKYEYE62AC_USB2,
    TOUPCAM_MODEL_SKYEYE24AC,
    TOUPCAM_MODEL_SKYEYE24AC_USB2,
    TOUPCAM_MODEL_MAX62AM,
    TOUPCAM_MODEL_MAX62AM_USB2,
    TOUPCAM_MODEL_MTR3CMOS08300KPA,
    TOUPCAM_MODEL_MTR3CMOS08300KPA_USB2,
    TOUPCAM_MODEL_G3M462C,
    TOUPCAM_MODEL_G3M462C_USB2,
    TOUPCAM_MODEL_E3ISPM08300KPC,
    TOUPCAM_MODEL_E3ISPM08300KPC_USB2,
    TOUPCAM_MODEL_BIGEYE4200KMA_2,
    TOUPCAM_MODEL_BIGEYE4200KMA_USB2_2,
    TOUPCAM_MODEL_ATR3CMOS10300KMA,
    TOUPCAM_MODEL_ATR3CMOS10300KMA_USB2,
    TOUPCAM_MODEL_E3ISPM45000KPA,
    TOUPCAM_MODEL_E3ISPM45000KPA_USB2,
    TOUPCAM_MODEL_E3ISPM02100KPA,
    TOUPCAM_MODEL_E3ISPM02100KPA_USB2,
    TOUPCAM_MODEL_G3CMOS08300KPA,
    TOUPCAM_MODEL_G3CMOS08300KPA_USB2,
    TOUPCAM_MODEL_BIGEYE4200KME,
    TOUPCAM_MODEL_BIGEYE4200KME_USB2,
    TOUPCAM_MODEL_ITR3CMOS08300KPA,
    TOUPCAM_MODEL_ITR3CMOS08300KPA_USB2,
    TOUPCAM_MODEL_MAX04AM,
    TOUPCAM_MODEL_MAX04AM_USB2,
    TOUPCAM_MODEL_G3M287C,
    TOUPCAM_MODEL_G3M287C_USB2,
    TOUPCAM_MODEL_ATR3CMOS16000KPB,
    TOUPCAM_MODEL_ATR3CMOS16000KPB_USB2,
    TOUPCAM_MODEL_ATR3CMOS10300KPA_2,
    TOUPCAM_MODEL_ATR3CMOS10300KPA_USB2_2,
    TOUPCAM_MODEL_L3CMOS08500KPA,
    TOUPCAM_MODEL_L3CMOS08500KPA_USB2,
};

/* This table was dumped from the touptek library */
static const struct toupcam_model_pid toupcam_models[] = {
    [TOUPCAM_MODEL_UA130CA               ] = { 0x1238, { "UA130CA                     ", 0x0000000081000001, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{1280, 720},}}},
    [TOUPCAM_MODEL_G3CMOS10300KPA        ] = { 0x11c6, { "G3CMOS10300KPA              ", 0x0000000087694649, 2, 4, 4, 1, 0, 4.630000, 4.630000, {{3704, 2778},{4096, 2160},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_G3CMOS10300KPA_USB2   ] = { 0x11c7, { "G3CMOS10300KPA(USB2.0)      ", 0x0000000087694709, 2, 4, 4, 1, 0, 4.630000, 4.630000, {{3704, 2778},{4096, 2160},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_E3CMOS01500KMA        ] = { 0x11ce, { "E3CMOS01500KMA              ", 0x0000001081002459, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{1440, 1080},{720, 540},}}},
    [TOUPCAM_MODEL_E3CMOS01500KMA_USB2   ] = { 0x11cf, { "E3CMOS01500KMA(USB2.0)      ", 0x0000001081002519, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{1440, 1080},{720, 540},}}},
    [TOUPCAM_MODEL_MTR3CCD01400KPB       ] = { 0x11d4, { "MTR3CCD01400KPB             ", 0x00000010811b44c2, 2, 2, 0, 4, 0, 6.450000, 6.450000, {{1376, 1040},{688, 520},}}},
    [TOUPCAM_MODEL_MTR3CCD01400KPB_USB2  ] = { 0x11d5, { "MTR3CCD01400KPB(USB2.0)     ", 0x00000010811b4582, 2, 2, 0, 4, 0, 6.450000, 6.450000, {{1376, 1040},{688, 520},}}},
    [TOUPCAM_MODEL_UA1600CA              ] = { 0x1189, { "UA1600CA                    ", 0x0000000081000001, 2, 3, 3, 0, 0, 1.335000, 1.335000, {{4632, 3488},{2320, 1740},{1536, 1160},}}},
    [TOUPCAM_MODEL_MTR3CCD09000KPA       ] = { 0x101d, { "MTR3CCD09000KPA             ", 0x00000010811b44c2, 1, 2, 0, 4, 0, 3.690000, 3.690000, {{3388, 2712},{1694, 1356},}}},
    [TOUPCAM_MODEL_MTR3CCD09000KPA_USB2  ] = { 0x1031, { "MTR3CCD09000KPA(USB2.0)     ", 0x00000010811b4582, 1, 2, 0, 4, 0, 3.690000, 3.690000, {{3388, 2712},{1694, 1356},}}},
    [TOUPCAM_MODEL_G3M178M               ] = { 0x11cc, { "G3M178M                     ", 0x0000000081484659, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3M178M_USB2          ] = { 0x11cd, { "G3M178M(USB2.0)             ", 0x0000000081484719, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3M178C               ] = { 0x11ca, { "G3M178C                     ", 0x0000000081484649, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3M178C_USB2          ] = { 0x11cb, { "G3M178C(USB2.0)             ", 0x0000000081484709, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_U3CMOS16000KPB        ] = { 0x11c8, { "U3CMOS16000KPB              ", 0x0000000081002049, 3, 5, 5, 0, 0, 3.800000, 3.800000, {{4640, 3506},{3840, 2160},{2304, 1750},{1920, 1080},{1536, 1168},}}},
    [TOUPCAM_MODEL_U3CMOS16000KPB_USB2   ] = { 0x11c9, { "U3CMOS16000KPB(USB2.0)      ", 0x0000000081002109, 3, 5, 5, 0, 0, 3.800000, 3.800000, {{4640, 3506},{3840, 2160},{2304, 1750},{1920, 1080},{1536, 1168},}}},
    [TOUPCAM_MODEL_E3ISPM02000KPA        ] = { 0x11c0, { "E3ISPM02000KPA              ", 0x0000000081041449, 2, 1, 0, 0, 0, 3.750000, 3.750000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_E3ISPM02000KPA_USB2   ] = { 0x11c1, { "E3ISPM02000KPA(USB2.0)      ", 0x0000000081041509, 2, 1, 0, 0, 0, 3.750000, 3.750000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_G3CMOS20000KMA        ] = { 0x11a0, { "G3CMOS20000KMA              ", 0x0000000081492659, 2, 4, 4, 2, 0, 2.400000, 2.400000, {{5440, 3648},{4080, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_G3CMOS20000KMA_USB2   ] = { 0x11a1, { "G3CMOS20000KMA(USB2.0)      ", 0x0000000081492719, 2, 4, 4, 2, 0, 2.400000, 2.400000, {{5440, 3648},{4080, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_U3CCD12000KPA         ] = { 0x1090, { "U3CCD12000KPA               ", 0x0000001081084042, 1, 2, 0, 0, 0, 3.100000, 3.100000, {{4248, 2836},{2124, 1418},}}},
    [TOUPCAM_MODEL_U3CCD12000KPA_USB2    ] = { 0x10a4, { "U3CCD12000KPA(USB2.0)       ", 0x0000001081084102, 1, 2, 0, 0, 0, 3.100000, 3.100000, {{4248, 2836},{2124, 1418},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KMA      ] = { 0x106d, { "ATR3CMOS16000KMA            ", 0x00000000816b24d9, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KMA_USB2 ] = { 0x1076, { "ATR3CMOS16000KMA(USB2.0)    ", 0x00000000816b2599, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_G3CMOS20000KPA        ] = { 0x11a2, { "G3CMOS20000KPA              ", 0x0000000081492649, 2, 4, 4, 2, 0, 2.400000, 2.400000, {{5440, 3648},{4080, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_G3CMOS20000KPA_USB2   ] = { 0x11a3, { "G3CMOS20000KPA(USB2.0)      ", 0x0000000081492709, 2, 4, 4, 2, 0, 2.400000, 2.400000, {{5440, 3648},{4080, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_G3CMOS02300KPA        ] = { 0x11c2, { "G3CMOS02300KPA              ", 0x0000000081492249, 2, 2, 0, 1, 0, 3.750000, 3.750000, {{1920, 1200},{960, 600},}}},
    [TOUPCAM_MODEL_G3CMOS02300KPA_USB2   ] = { 0x11c3, { "G3CMOS02300KPA(USB2.0)      ", 0x0000000081492309, 2, 2, 0, 1, 0, 3.750000, 3.750000, {{1920, 1200},{960, 600},}}},
    [TOUPCAM_MODEL_I3ISPM12000KPA        ] = { 0x11c4, { "I3ISPM12000KPA              ", 0x00000000810c2041, 3, 4, 4, 0, 0, 1.850000, 1.850000, {{4000, 3000},{2048, 1080},{2000, 1500},{1280, 1000},}}},
    [TOUPCAM_MODEL_I3ISPM12000KPA_USB2   ] = { 0x11c5, { "I3ISPM12000KPA(USB2.0)      ", 0x00000000810c2101, 3, 4, 4, 0, 0, 1.850000, 1.850000, {{4000, 3000},{2048, 1080},{2000, 1500},{1280, 1000},}}},
    [TOUPCAM_MODEL_U3CCD09000KPA         ] = { 0x108c, { "U3CCD09000KPA               ", 0x0000001081084042, 1, 2, 0, 0, 0, 3.690000, 3.690000, {{3388, 2712},{1694, 1356},}}},
    [TOUPCAM_MODEL_U3CCD09000KPA_USB2    ] = { 0x10a0, { "U3CCD09000KPA(USB2.0)       ", 0x0000001081084102, 1, 2, 0, 0, 0, 3.690000, 3.690000, {{3388, 2712},{1694, 1356},}}},
    [TOUPCAM_MODEL_G3CMOS02300KMC        ] = { 0x119c, { "G3CMOS02300KMC              ", 0x0000001081492259, 2, 1, 0, 1, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_G3CMOS02300KMC_USB2   ] = { 0x119d, { "G3CMOS02300KMC(USB2.0)      ", 0x0000001081492319, 2, 1, 0, 1, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_E3ISPM12300KPA        ] = { 0x118a, { "E3ISPM12300KPA              ", 0x0000001081042449, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{4096, 3000},{2048, 1500},}}},
    [TOUPCAM_MODEL_E3ISPM12300KPA_USB2   ] = { 0x118b, { "E3ISPM12300KPA(USB2.0)      ", 0x0000001081042509, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{4096, 3000},{2048, 1500},}}},
    [TOUPCAM_MODEL_ECMOS06600KPA         ] = { 0x117d, { "ECMOS06600KPA               ", 0x0000000081000009, 2, 4, 4, 0, 0, 1.620000, 1.620000, {{3072, 2160},{2592, 1944},{3072, 1728},{2160, 2160},}}},
    [TOUPCAM_MODEL_ECMOS08300KPA         ] = { 0x117c, { "ECMOS08300KPA               ", 0x0000000081000009, 2, 2, 2, 0, 0, 1.620000, 1.620000, {{3840, 2160},{1920, 1080},}}},
    [TOUPCAM_MODEL_UA1000CA              ] = { 0x1237, { "UA1000CA                    ", 0x0000000081000029, 3, 3, 3, 0, 0, 1.670000, 1.670000, {{3664, 2748},{1832, 1374},{912, 686},}}},
    [TOUPCAM_MODEL_UA1000CA_2            ] = { 0x1188, { "UA1000CA                    ", 0x0000000081000029, 5, 3, 3, 0, 0, 1.670000, 1.670000, {{3584, 2748},{1792, 1374},{896, 684},}}},
    [TOUPCAM_MODEL_UA510CA               ] = { 0x1236, { "UA510CA                     ", 0x0000000081000029, 6, 3, 3, 0, 0, 2.200000, 2.200000, {{2592, 1944},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_UA510CA_2             ] = { 0x1187, { "UA510CA                     ", 0x0000000081000029, 6, 3, 3, 0, 0, 2.200000, 2.200000, {{2592, 1944},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_UA310CA               ] = { 0x1235, { "UA310CA                     ", 0x0000000081000009, 5, 3, 3, 0, 0, 3.200000, 3.200000, {{2048, 1536},{1024, 768},{680, 510},}}},
    [TOUPCAM_MODEL_UA310CA_2             ] = { 0x1186, { "UA310CA                     ", 0x0000000081000009, 5, 3, 3, 0, 0, 3.200000, 3.200000, {{2048, 1536},{1024, 768},{680, 510},}}},
    [TOUPCAM_MODEL_E3ISPM08300KPA        ] = { 0x116c, { "E3ISPM08300KPA              ", 0x0000000081042049, 2, 2, 2, 0, 0, 1.620000, 1.620000, {{3840, 2160},{1920, 1080},}}},
    [TOUPCAM_MODEL_E3ISPM08300KPA_USB2   ] = { 0x116d, { "E3ISPM08300KPA(USB2.0)      ", 0x0000000081042109, 2, 2, 2, 0, 0, 1.620000, 1.620000, {{3840, 2160},{1920, 1080},}}},
    [TOUPCAM_MODEL_G3CMOS16000KMA        ] = { 0x1184, { "G3CMOS16000KMA              ", 0x0000000081692259, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_G3CMOS16000KMA_USB2   ] = { 0x1185, { "G3CMOS16000KMA(USB2.0)      ", 0x0000000081692319, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_G3CMOS16000KPA        ] = { 0x1182, { "G3CMOS16000KPA              ", 0x0000000081692249, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_G3CMOS16000KPA_USB2   ] = { 0x1183, { "G3CMOS16000KPA(USB2.0)      ", 0x0000000081692309, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_G3M287M               ] = { 0x1178, { "G3M287M                     ", 0x0000001081482659, 2, 1, 0, 0, 0, 6.900000, 6.900000, {{720, 540},}}},
    [TOUPCAM_MODEL_G3M287M_USB2          ] = { 0x1179, { "G3M287M(USB2.0)             ", 0x0000001081482719, 2, 1, 0, 0, 0, 6.900000, 6.900000, {{720, 540},}}},
    [TOUPCAM_MODEL_G3M385C               ] = { 0x117a, { "G3M385C                     ", 0x0000000085482649, 2, 1, 0, 0, 0, 3.750000, 3.750000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_G3M385C_USB2          ] = { 0x117b, { "G3M385C(USB2.0)             ", 0x0000000085482709, 2, 1, 0, 0, 0, 3.750000, 3.750000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_EP3CMOS00400KMA       ] = { 0x117e, { "EP3CMOS00400KMA             ", 0x0000001081482459, 2, 1, 0, 0, 0, 6.900000, 6.900000, {{720, 540},}}},
    [TOUPCAM_MODEL_EP3CMOS00400KMA_USB2  ] = { 0x117f, { "EP3CMOS00400KMA(USB2.0)     ", 0x0000001081482519, 2, 1, 0, 0, 0, 6.900000, 6.900000, {{720, 540},}}},
    [TOUPCAM_MODEL_EP3CMOS00400KPA       ] = { 0x1180, { "EP3CMOS00400KPA             ", 0x0000001081482449, 2, 1, 0, 0, 0, 6.900000, 6.900000, {{720, 540},}}},
    [TOUPCAM_MODEL_EP3CMOS00400KPA_USB2  ] = { 0x1181, { "EP3CMOS00400KPA(USB2.0)     ", 0x0000001081482509, 2, 1, 0, 0, 0, 6.900000, 6.900000, {{720, 540},}}},
    [TOUPCAM_MODEL_E3CMOS00400KMA        ] = { 0x116e, { "E3CMOS00400KMA              ", 0x0000001081002459, 2, 1, 0, 0, 0, 6.900000, 6.900000, {{720, 540},}}},
    [TOUPCAM_MODEL_E3CMOS00400KMA_USB2   ] = { 0x116f, { "E3CMOS00400KMA(USB2.0)      ", 0x0000001081002519, 2, 1, 0, 0, 0, 6.900000, 6.900000, {{720, 540},}}},
    [TOUPCAM_MODEL_E3CMOS00400KPA        ] = { 0x1170, { "E3CMOS00400KPA              ", 0x0000001081002449, 2, 1, 0, 0, 0, 6.900000, 6.900000, {{720, 540},}}},
    [TOUPCAM_MODEL_E3CMOS00400KPA_USB2   ] = { 0x1171, { "E3CMOS00400KPA(USB2.0)      ", 0x0000001081002509, 2, 1, 0, 0, 0, 6.900000, 6.900000, {{720, 540},}}},
    [TOUPCAM_MODEL_G3M290C               ] = { 0x115e, { "G3M290C                     ", 0x0000000085482649, 2, 1, 0, 0, 0, 2.900000, 2.900000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_G3M290C_USB2          ] = { 0x115f, { "G3M290C(USB2.0)             ", 0x0000000085482709, 2, 1, 0, 0, 0, 2.900000, 2.900000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_G3M290M               ] = { 0x1160, { "G3M290M                     ", 0x0000000085482659, 2, 1, 0, 0, 0, 2.900000, 2.900000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_G3M290M_USB2          ] = { 0x1161, { "G3M290M(USB2.0)             ", 0x0000000085482719, 2, 1, 0, 0, 0, 2.900000, 2.900000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_G3M224C               ] = { 0x1162, { "G3M224C                     ", 0x0000000085482669, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_G3M224C_USB2          ] = { 0x1163, { "G3M224C(USB2.0)             ", 0x0000000085482729, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_U3ISPM16000KPB        ] = { 0x113e, { "U3ISPM16000KPB              ", 0x0000000081041049, 3, 4, 4, 0, 0, 3.800000, 3.800000, {{4640, 3506},{3360, 2526},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_U3ISPM16000KPB_USB2   ] = { 0x113f, { "U3ISPM16000KPB(USB2.0)      ", 0x0000000081041109, 3, 4, 4, 0, 0, 3.800000, 3.800000, {{4640, 3506},{3360, 2526},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_E3CMOS05000KMA        ] = { 0x1172, { "E3CMOS05000KMA              ", 0x0000001081002459, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_E3CMOS05000KMA_USB2   ] = { 0x1173, { "E3CMOS05000KMA(USB2.0)      ", 0x0000001081002519, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_E3CMOS03100KMC        ] = { 0x1174, { "E3CMOS03100KMC              ", 0x0000001081002459, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_E3CMOS03100KMC_USB2   ] = { 0x1175, { "E3CMOS03100KMC(USB2.0)      ", 0x0000001081002519, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_E3ISPM09000KPA        ] = { 0x116a, { "E3ISPM09000KPA              ", 0x0000001081042449, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{4096, 2160},{2048, 1080},}}},
    [TOUPCAM_MODEL_E3ISPM09000KPA_USB2   ] = { 0x116b, { "E3ISPM09000KPA(USB2.0)      ", 0x0000001081042509, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{4096, 2160},{2048, 1080},}}},
    [TOUPCAM_MODEL_I3CMOS01200KPA        ] = { 0x10c1, { "I3CMOS01200KPA              ", 0x0000000081002069, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_I3CMOS01200KPA_USB2   ] = { 0x10ce, { "I3CMOS01200KPA(USB2.0)      ", 0x0000000081002129, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_U3ISPM16000KPA        ] = { 0x113c, { "U3ISPM16000KPA              ", 0x0000000081042041, 2, 3, 3, 0, 0, 1.335000, 1.335000, {{4632, 3488},{2320, 1740},{1536, 1160},}}},
    [TOUPCAM_MODEL_U3ISPM16000KPA_USB2   ] = { 0x113d, { "U3ISPM16000KPA(USB2.0)      ", 0x0000000081042101, 2, 3, 3, 0, 0, 1.335000, 1.335000, {{4632, 3488},{2320, 1740},{1536, 1160},}}},
    [TOUPCAM_MODEL_U3CMOS16000KMB        ] = { 0x1168, { "U3CMOS16000KMB              ", 0x0000000081002059, 3, 5, 5, 0, 0, 3.800000, 3.800000, {{4640, 3506},{3840, 2160},{2304, 1750},{1920, 1080},{1536, 1168},}}},
    [TOUPCAM_MODEL_U3CMOS16000KMB_USB2   ] = { 0x1169, { "U3CMOS16000KMB(USB2.0)      ", 0x0000000081002119, 3, 5, 5, 0, 0, 3.800000, 3.800000, {{4640, 3506},{3840, 2160},{2304, 1750},{1920, 1080},{1536, 1168},}}},
    [TOUPCAM_MODEL_G3CMOS02300KPC        ] = { 0x1009, { "G3CMOS02300KPC              ", 0x0000001081492249, 2, 1, 0, 1, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_G3CMOS02300KPC_USB2   ] = { 0x100e, { "G3CMOS02300KPC(USB2.0)      ", 0x0000001081492309, 2, 1, 0, 1, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_G3M178M_2             ] = { 0x115c, { "G3M178M                     ", 0x0000000081484259, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3M178M_USB2_2        ] = { 0x115d, { "G3M178M(USB2.0)             ", 0x0000000081484319, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3M178C_2             ] = { 0x115a, { "G3M178C                     ", 0x0000000081484249, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3M178C_USB2_2        ] = { 0x115b, { "G3M178C(USB2.0)             ", 0x0000000081484309, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_E3ISPM03100KPA        ] = { 0x1138, { "E3ISPM03100KPA              ", 0x0000001081042449, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_E3ISPM03100KPA_USB2   ] = { 0x1139, { "E3ISPM03100KPA(USB2.0)      ", 0x0000001081042509, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_E3ISPM05000KPA        ] = { 0x114c, { "E3ISPM05000KPA              ", 0x0000001081042449, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_E3ISPM05000KPA_USB2   ] = { 0x114d, { "E3ISPM05000KPA(USB2.0)      ", 0x0000001081042509, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_MG3CMOS16000KPA       ] = { 0x1158, { "MG3CMOS16000KPA             ", 0x0000000081012041, 2, 3, 3, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_MG3CMOS16000KPA_USB2  ] = { 0x1159, { "MG3CMOS16000KPA(USB2.0)     ", 0x0000000081012101, 2, 3, 3, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_MG3CMOS02300KPA       ] = { 0x1156, { "MG3CMOS02300KPA             ", 0x0000000081012041, 2, 2, 2, 1, 0, 3.750000, 3.750000, {{1920, 1200},{960, 600},}}},
    [TOUPCAM_MODEL_MG3CMOS02300KPA_USB2  ] = { 0x1157, { "MG3CMOS02300KPA(USB2.0)     ", 0x0000000081012101, 2, 2, 2, 1, 0, 3.750000, 3.750000, {{1920, 1200},{960, 600},}}},
    [TOUPCAM_MODEL_U3ISPM18000KPA        ] = { 0x1142, { "U3ISPM18000KPA              ", 0x0000000081042469, 3, 3, 3, 0, 0, 1.250000, 1.250000, {{4912, 3684},{2456, 1842},{1228, 922},}}},
    [TOUPCAM_MODEL_U3ISPM18000KPA_USB2   ] = { 0x1143, { "U3ISPM18000KPA(USB2.0)      ", 0x0000000081042529, 3, 3, 3, 0, 0, 1.250000, 1.250000, {{4912, 3684},{2456, 1842},{1228, 922},}}},
    [TOUPCAM_MODEL_E3ISPM03100KPB        ] = { 0x113a, { "E3ISPM03100KPB              ", 0x0000000081042049, 2, 2, 0, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1920, 1080},}}},
    [TOUPCAM_MODEL_E3ISPM03100KPB_USB2   ] = { 0x113b, { "E3ISPM03100KPB(USB2.0)      ", 0x0000000081042109, 2, 2, 0, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1920, 1080},}}},
    [TOUPCAM_MODEL_EP3CMOS02300KPC       ] = { 0x1154, { "EP3CMOS02300KPC             ", 0x0000001081082049, 2, 1, 0, 0, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_EP3CMOS02300KPC_USB2  ] = { 0x1155, { "EP3CMOS02300KPC(USB2.0)     ", 0x0000001081082109, 2, 1, 0, 0, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_EP3CMOS02300KMC       ] = { 0x1152, { "EP3CMOS02300KMC             ", 0x0000001081082059, 2, 1, 0, 0, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_EP3CMOS02300KMC_USB2  ] = { 0x1153, { "EP3CMOS02300KMC(USB2.0)     ", 0x0000001081082119, 2, 1, 0, 0, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_EP3CMOS06300KMA       ] = { 0x1150, { "EP3CMOS06300KMA             ", 0x0000000081084051, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_EP3CMOS06300KMA_USB2  ] = { 0x1151, { "EP3CMOS06300KMA(USB2.0)     ", 0x0000000081084111, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_EP3CMOS20000KPA       ] = { 0x114e, { "EP3CMOS20000KPA             ", 0x0000000081492241, 2, 3, 3, 1, 0, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_EP3CMOS20000KPA_USB2  ] = { 0x114f, { "EP3CMOS20000KPA(USB2.0)     ", 0x0000000081492301, 2, 3, 3, 1, 0, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_GPCMOS02000KMA        ] = { 0x10ff, { "GPCMOS02000KMA              ", 0x0000000085482219, 2, 1, 0, 0, 0, 2.900000, 2.900000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_EXCCD01400KPA         ] = { 0x814d, { "EXCCD01400KPA               ", 0x0000001080082002, 3, 1, 0, 0, 0, 6.450000, 6.450000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_I3CMOS03100KPA        ] = { 0x10c7, { "I3CMOS03100KPA              ", 0x0000001081004041, 2, 2, 0, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1920, 1080},}}},
    [TOUPCAM_MODEL_I3CMOS03100KPA_USB2   ] = { 0x10d4, { "I3CMOS03100KPA(USB2.0)      ", 0x0000001081004101, 2, 2, 0, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1920, 1080},}}},
    [TOUPCAM_MODEL_U3CMOS10000KMA        ] = { 0x1148, { "U3CMOS10000KMA              ", 0x0000000081000071, 3, 3, 3, 0, 0, 1.670000, 1.670000, {{3584, 2746},{1792, 1372},{896, 680},}}},
    [TOUPCAM_MODEL_U3CMOS10000KMA_USB2   ] = { 0x1149, { "U3CMOS10000KMA(USB2.0)      ", 0x0000000081000131, 3, 3, 3, 0, 0, 1.670000, 1.670000, {{3584, 2746},{1792, 1372},{896, 680},}}},
    [TOUPCAM_MODEL_E3ISPM12000KPA        ] = { 0x1134, { "E3ISPM12000KPA              ", 0x0000000081042041, 3, 2, 2, 0, 0, 1.850000, 1.850000, {{4000, 3000},{2048, 1080},}}},
    [TOUPCAM_MODEL_E3ISPM12000KPA_USB2   ] = { 0x1135, { "E3ISPM12000KPA(USB2.0)      ", 0x0000000081042101, 3, 2, 2, 0, 0, 1.850000, 1.850000, {{4000, 3000},{2048, 1080},}}},
    [TOUPCAM_MODEL_E3ISPM05000KPA_2      ] = { 0x111c, { "E3ISPM05000KPA              ", 0x0000001081042449, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{2448, 2048},{1216, 1024},}}},
    [TOUPCAM_MODEL_E3ISPM05000KPA_USB2_2 ] = { 0x111d, { "E3ISPM05000KPA(USB2.0)      ", 0x0000001081042509, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{2448, 2048},{1216, 1024},}}},
    [TOUPCAM_MODEL_AF3CMOS06300KPA       ] = { 0x1146, { "AF3CMOS06300KPA             ", 0x00000420930c2049, 2, 2, 2, 0, 0, 2.400000, 2.400000, {{3080, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_AF3CMOS06300KPA_USB2  ] = { 0x1147, { "AF3CMOS06300KPA(USB2.0)     ", 0x00000420930c2109, 2, 2, 2, 0, 0, 2.400000, 2.400000, {{3080, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_E3ISPM20000KPA        ] = { 0x1122, { "E3ISPM20000KPA              ", 0x0000000081042041, 2, 3, 3, 0, 0, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_E3ISPM20000KPA_USB2   ] = { 0x1123, { "E3ISPM20000KPA(USB2.0)      ", 0x0000000081042101, 2, 3, 3, 0, 0, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_G3CMOS20000KPA_2      ] = { 0x1112, { "G3CMOS20000KPA              ", 0x0000000081492241, 2, 3, 3, 1, 0, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_G3CMOS20000KPA_USB2_2 ] = { 0x1113, { "G3CMOS20000KPA(USB2.0)      ", 0x0000000081492301, 2, 3, 3, 1, 0, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_E3CMOS02300KMC        ] = { 0x1144, { "E3CMOS02300KMC              ", 0x0000001081002059, 2, 1, 0, 0, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_E3CMOS02300KMC_USB2   ] = { 0x1145, { "E3CMOS02300KMC(USB2.0)      ", 0x0000001081002119, 2, 1, 0, 0, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_ATR3CMOS02300KMB      ] = { 0x1071, { "ATR3CMOS02300KMB            ", 0x00000010814b26d9, 2, 1, 0, 1, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_ATR3CMOS02300KMB_USB2 ] = { 0x107a, { "ATR3CMOS02300KMB(USB2.0)    ", 0x00000010814b2799, 2, 1, 0, 1, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_E3CMOS20000KPA        ] = { 0x1110, { "E3CMOS20000KPA              ", 0x0000000081002041, 2, 3, 3, 0, 0, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_E3CMOS20000KPA_USB2   ] = { 0x1111, { "E3CMOS20000KPA(USB2.0)      ", 0x0000000081002101, 2, 3, 3, 0, 0, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_MTR3CMOS16000KPA      ] = { 0x1063, { "MTR3CMOS16000KPA            ", 0x00000000810b24c9, 2, 3, 0, 1, 0, 3.800000, 3.800000, {{4632, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_MTR3CMOS16000KPA_USB2 ] = { 0x1067, { "MTR3CMOS16000KPA(USB2.0)    ", 0x00000000810b2589, 2, 3, 0, 1, 0, 3.800000, 3.800000, {{4632, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_MTR3CMOS16000KMA      ] = { 0x1064, { "MTR3CMOS16000KMA            ", 0x00000000810b24d9, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4632, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_MTR3CMOS16000KMA_USB2 ] = { 0x1068, { "MTR3CMOS16000KMA(USB2.0)    ", 0x00000000810b2599, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4632, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_ECMOS05300KPA         ] = { 0x1108, { "ECMOS05300KPA               ", 0x0000000080000009, 2, 2, 2, 0, 0, 2.400000, 2.400000, {{3072, 1728},{1280, 720},}}},
    [TOUPCAM_MODEL_ECMOS03100KPA         ] = { 0x1107, { "ECMOS03100KPA               ", 0x0000000080000009, 2, 2, 2, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1920, 1080},}}},
    [TOUPCAM_MODEL_E3ISPM06300KPA        ] = { 0x111a, { "E3ISPM06300KPA              ", 0x0000000081042049, 2, 2, 2, 0, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_E3ISPM06300KPA_USB2   ] = { 0x111b, { "E3ISPM06300KPA(USB2.0)      ", 0x0000000081042109, 2, 2, 2, 0, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_ECMOS01200KPA         ] = { 0x110f, { "ECMOS01200KPA               ", 0x0000000081000029, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_E3CMOS01200KPA        ] = { 0x110d, { "E3CMOS01200KPA              ", 0x0000000081002069, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_E3CMOS01200KPA_USB2   ] = { 0x110e, { "E3CMOS01200KPA(USB2.0)      ", 0x0000000081002129, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_G3CMOS16000KMA_2      ] = { 0x1104, { "G3CMOS16000KMA              ", 0x0000000081492251, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_G3CMOS16000KMA_USB2_2 ] = { 0x1105, { "G3CMOS16000KMA(USB2.0)      ", 0x0000000081492311, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_ATR3CMOS02300KPB      ] = { 0x1070, { "ATR3CMOS02300KPB            ", 0x00000010814b26c9, 3, 1, 0, 1, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_ATR3CMOS02300KPB_USB2 ] = { 0x1079, { "ATR3CMOS02300KPB(USB2.0)    ", 0x00000010814b2789, 3, 1, 0, 1, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_ATR3CMOS06300KPA      ] = { 0x1072, { "ATR3CMOS06300KPA            ", 0x00000000814b44c9, 2, 2, 0, 1, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_ATR3CMOS06300KPA_USB2 ] = { 0x107b, { "ATR3CMOS06300KPA(USB2.0)    ", 0x00000000814b4589, 2, 2, 0, 1, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KPA      ] = { 0x106b, { "ATR3CMOS16000KPA            ", 0x00000000816b24c9, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KPA_USB2 ] = { 0x1075, { "ATR3CMOS16000KPA(USB2.0)    ", 0x00000000816b2589, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_G3CMOS16000KPA_2      ] = { 0x1102, { "G3CMOS16000KPA              ", 0x0000000081492241, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_G3CMOS16000KPA_USB2_2 ] = { 0x1103, { "G3CMOS16000KPA(USB2.0)      ", 0x0000000081492301, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_ECMOS02000KPA         ] = { 0x1106, { "ECMOS02000KPA               ", 0x0000000080000009, 2, 1, 0, 0, 0, 2.900000, 2.900000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_EP3CMOS06300KPA       ] = { 0x1100, { "EP3CMOS06300KPA             ", 0x0000000081084041, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_EP3CMOS06300KPA_USB2  ] = { 0x1101, { "EP3CMOS06300KPA(USB2.0)     ", 0x0000000081084101, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_E3CMOS05000KMA_2      ] = { 0x10a9, { "E3CMOS05000KMA              ", 0x0000001081002479, 4, 2, 2, 0, 0, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_E3CMOS05000KMA_USB2_2 ] = { 0x10ab, { "E3CMOS05000KMA(USB2.0)      ", 0x0000001081002539, 4, 2, 2, 0, 0, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_E3CMOS05000KPA        ] = { 0x10a8, { "E3CMOS05000KPA              ", 0x0000001081002449, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_E3CMOS05000KPA_USB2   ] = { 0x10aa, { "E3CMOS05000KPA(USB2.0)      ", 0x0000001081002509, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_GPCMOS02000KPA        ] = { 0x10fe, { "GPCMOS02000KPA              ", 0x0000000085482209, 2, 1, 0, 0, 0, 2.900000, 2.900000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_EXCCD00440KPB         ] = { 0x805a, { "EXCCD00440KPB               ", 0x0000001300080004, 3, 1, 0, 0, 0, 8.600000, 8.300000, {{748, 576},}}},
    [TOUPCAM_MODEL_GPCMOS01200KMA        ] = { 0x1002, { "GPCMOS01200KMA              ", 0x0000000080682219, 4, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_EXCCD00300KPA         ] = { 0x830a, { "EXCCD00300KPA               ", 0x0000001080082002, 3, 1, 0, 0, 0, 5.600000, 5.600000, {{640, 480},}}},
    [TOUPCAM_MODEL_U3CMOS16000KMB_2      ] = { 0x107d, { "U3CMOS16000KMB              ", 0x0000000081001051, 2, 3, 3, 0, 0, 3.800000, 3.800000, {{4648, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_U3CMOS16000KMB_USB2_2 ] = { 0x107f, { "U3CMOS16000KMB(USB2.0)      ", 0x0000000081001111, 2, 3, 3, 0, 0, 3.800000, 3.800000, {{4648, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_U3CCD01400KPB         ] = { 0x1082, { "U3CCD01400KPB               ", 0x0000001081084042, 2, 1, 0, 0, 0, 6.450000, 6.450000, {{1376, 1040},}}},
    [TOUPCAM_MODEL_U3CCD01400KPB_USB2    ] = { 0x1096, { "U3CCD01400KPB(USB2.0)       ", 0x0000001081084102, 2, 1, 0, 0, 0, 6.450000, 6.450000, {{1376, 1040},}}},
    [TOUPCAM_MODEL_U3CCD01400KMB         ] = { 0x1083, { "U3CCD01400KMB               ", 0x0000001081084052, 2, 1, 0, 0, 0, 6.450000, 6.450000, {{1376, 1040},}}},
    [TOUPCAM_MODEL_U3CCD01400KMB_USB2    ] = { 0x1097, { "U3CCD01400KMB(USB2.0)       ", 0x0000001081084112, 2, 1, 0, 0, 0, 6.450000, 6.450000, {{1376, 1040},}}},
    [TOUPCAM_MODEL_G3CMOS06300KMA        ] = { 0x10f6, { "G3CMOS06300KMA              ", 0x0000000081494259, 2, 2, 0, 1, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3CMOS06300KMA_USB2   ] = { 0x10f7, { "G3CMOS06300KMA(USB2.0)      ", 0x0000000081494319, 2, 2, 0, 1, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_MTR3CCD01400KMB       ] = { 0x101c, { "MTR3CCD01400KMB             ", 0x00000010811b44d2, 2, 1, 0, 4, 2, 6.450000, 6.450000, {{1376, 1040},}}},
    [TOUPCAM_MODEL_MTR3CCD01400KMB_USB2  ] = { 0x1030, { "MTR3CCD01400KMB(USB2.0)     ", 0x00000010811b4592, 2, 1, 0, 4, 2, 6.450000, 6.450000, {{1376, 1040},}}},
    [TOUPCAM_MODEL_MTR3CCD01400KPB_2     ] = { 0x101b, { "MTR3CCD01400KPB             ", 0x00000010811b44c2, 2, 1, 0, 4, 0, 6.450000, 6.450000, {{1376, 1040},}}},
    [TOUPCAM_MODEL_MTR3CCD01400KPB_USB2_2] = { 0x102f, { "MTR3CCD01400KPB(USB2.0)     ", 0x00000010811b4582, 2, 1, 0, 4, 0, 6.450000, 6.450000, {{1376, 1040},}}},
    [TOUPCAM_MODEL_GPCMOS01200KMB        ] = { 0x1004, { "GPCMOS01200KMB              ", 0x0000000080682219, 4, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_U3CMOS16000KPB_2      ] = { 0x107c, { "U3CMOS16000KPB              ", 0x0000000081001041, 2, 3, 3, 0, 0, 3.800000, 3.800000, {{4648, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_U3CMOS16000KPB_USB2_2 ] = { 0x107e, { "U3CMOS16000KPB(USB2.0)      ", 0x0000000081001101, 2, 3, 3, 0, 0, 3.800000, 3.800000, {{4648, 3506},{2304, 1750},{1536, 1168},}}},
    [TOUPCAM_MODEL_GPCMOS01200KPB        ] = { 0x1003, { "GPCMOS01200KPB              ", 0x0000000080682209, 4, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_G3CMOS01200KPA        ] = { 0x1007, { "G3CMOS01200KPA              ", 0x0000000085492269, 2, 2, 0, 1, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_G3CMOS01200KPA_USB2   ] = { 0x100c, { "G3CMOS01200KPA(USB2.0)      ", 0x0000000085492329, 2, 2, 0, 1, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_G3CMOS06300KPA        ] = { 0x100b, { "G3CMOS06300KPA              ", 0x0000000081494249, 2, 2, 0, 1, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3CMOS06300KPA_USB2   ] = { 0x1010, { "G3CMOS06300KPA(USB2.0)      ", 0x0000000081494309, 2, 2, 0, 1, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_E3CMOS06300KMA        ] = { 0x10f8, { "E3CMOS06300KMA              ", 0x0000000081004051, 2, 2, 2, 0, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_E3CMOS06300KMA_USB2   ] = { 0x10f9, { "E3CMOS06300KMA(USB2.0)      ", 0x0000000081004111, 2, 2, 2, 0, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_X05100KPA             ] = { 0x0a51, { "X05100KPA                   ", 0x0000000080000029, 6, 3, 3, 0, 0, 2.200000, 2.200000, {{2592, 1944},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_X01200KPA             ] = { 0x0a12, { "X01200KPA                   ", 0x0000000080000209, 4, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_GPCMOS01200KPC        ] = { 0x1005, { "GPCMOS01200KPC              ", 0x0000000084482229, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_ATR3CCD06000KPA       ] = { 0x1039, { "ATR3CCD06000KPA             ", 0x00000010815b44c2, 1, 2, 0, 4, 0, 4.540000, 4.540000, {{2748, 2200},{2748, 1092},}}},
    [TOUPCAM_MODEL_ATR3CCD06000KPA_USB2  ] = { 0x104d, { "ATR3CCD06000KPA(USB2.0)     ", 0x00000010815b4582, 1, 2, 0, 4, 0, 4.540000, 4.540000, {{2748, 2200},{2748, 1092},}}},
    [TOUPCAM_MODEL_ATR3CCD06000KMA       ] = { 0x103a, { "ATR3CCD06000KMA             ", 0x00000010815b44d2, 1, 2, 0, 4, 0, 4.540000, 4.540000, {{2748, 2200},{2748, 1092},}}},
    [TOUPCAM_MODEL_ATR3CCD06000KMA_USB2  ] = { 0x104e, { "ATR3CCD06000KMA(USB2.0)     ", 0x00000010815b4592, 1, 2, 0, 4, 0, 4.540000, 4.540000, {{2748, 2200},{2748, 1092},}}},
    [TOUPCAM_MODEL_MTR3CCD02800KMA       ] = { 0x1016, { "MTR3CCD02800KMA             ", 0x00000010811b44d2, 1, 3, 0, 4, 0, 4.540000, 4.540000, {{1938, 1460},{1610, 1212},{1930, 1092},}}},
    [TOUPCAM_MODEL_MTR3CCD02800KMA_USB2  ] = { 0x102a, { "MTR3CCD02800KMA(USB2.0)     ", 0x00000010811b4592, 1, 3, 0, 4, 0, 4.540000, 4.540000, {{1938, 1460},{1610, 1212},{1930, 1092},}}},
    [TOUPCAM_MODEL_MTR3CCD02800KPA       ] = { 0x1015, { "MTR3CCD02800KPA             ", 0x00000010811b44c2, 1, 3, 0, 4, 0, 4.540000, 4.540000, {{1938, 1460},{1610, 1212},{1930, 1092},}}},
    [TOUPCAM_MODEL_MTR3CCD02800KPA_USB2  ] = { 0x1029, { "MTR3CCD02800KPA(USB2.0)     ", 0x00000010811b4582, 1, 3, 0, 4, 0, 4.540000, 4.540000, {{1938, 1460},{1610, 1212},{1930, 1092},}}},
    [TOUPCAM_MODEL_U3CCD06000KMA         ] = { 0x1089, { "U3CCD06000KMA               ", 0x0000001081084052, 1, 2, 0, 0, 0, 4.540000, 4.540000, {{2748, 2200},{2748, 1092},}}},
    [TOUPCAM_MODEL_U3CCD06000KMA_USB2    ] = { 0x109d, { "U3CCD06000KMA(USB2.0)       ", 0x0000001081084112, 1, 2, 0, 0, 0, 4.540000, 4.540000, {{2748, 2200},{2748, 1092},}}},
    [TOUPCAM_MODEL_MTR3CCD01400KMA       ] = { 0x101a, { "MTR3CCD01400KMA             ", 0x00000010811b44d2, 1, 1, 0, 4, 0, 6.450000, 6.450000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_MTR3CCD01400KMA_USB2  ] = { 0x102e, { "MTR3CCD01400KMA(USB2.0)     ", 0x00000010811b4592, 1, 1, 0, 4, 0, 6.450000, 6.450000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_G3CMOS02300KMC_2      ] = { 0x100a, { "G3CMOS02300KMC              ", 0x0000001081492259, 2, 1, 0, 1, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_G3CMOS02300KMC_USB2_2 ] = { 0x100f, { "G3CMOS02300KMC(USB2.0)      ", 0x0000001081492319, 2, 1, 0, 1, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_MTR3CCD06000KMA       ] = { 0x1012, { "MTR3CCD06000KMA             ", 0x00000010811b44d2, 1, 2, 0, 4, 0, 4.540000, 4.540000, {{2748, 2200},{2748, 1092},}}},
    [TOUPCAM_MODEL_MTR3CCD06000KMA_USB2  ] = { 0x1026, { "MTR3CCD06000KMA(USB2.0)     ", 0x00000010811b4592, 1, 2, 0, 4, 0, 4.540000, 4.540000, {{2748, 2200},{2748, 1092},}}},
    [TOUPCAM_MODEL_U3CCD02800KMA         ] = { 0x1085, { "U3CCD02800KMA               ", 0x0000001081084052, 1, 3, 0, 0, 0, 4.540000, 4.540000, {{1938, 1460},{1610, 1212},{1930, 1092},}}},
    [TOUPCAM_MODEL_U3CCD02800KMA_USB2    ] = { 0x1099, { "U3CCD02800KMA(USB2.0)       ", 0x0000001081084112, 1, 3, 0, 0, 0, 4.540000, 4.540000, {{1938, 1460},{1610, 1212},{1930, 1092},}}},
    [TOUPCAM_MODEL_GCMOS01200KMA         ] = { 0xb135, { "GCMOS01200KMA               ", 0x0000000080682219, 4, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_GCMOS01200KMB         ] = { 0xb134, { "GCMOS01200KMB               ", 0x0000000080682219, 4, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_G3CMOS02300KPB        ] = { 0x1008, { "G3CMOS02300KPB              ", 0x0000001081492241, 2, 1, 0, 1, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_G3CMOS02300KPB_USB2   ] = { 0x100d, { "G3CMOS02300KPB(USB2.0)      ", 0x0000001081492301, 2, 1, 0, 1, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_GCMOS01200KPB         ] = { 0xb133, { "GCMOS01200KPB               ", 0x0000000080682209, 4, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_U3CCD02800KPA         ] = { 0x4004, { "U3CCD02800KPA               ", 0x0000001081084042, 1, 3, 0, 0, 0, 4.540000, 4.540000, {{1938, 1460},{1610, 1212},{1930, 1092},}}},
    [TOUPCAM_MODEL_U3CCD02800KPA_USB2    ] = { 0x4005, { "U3CCD02800KPA(USB2.0)       ", 0x0000001081084102, 1, 3, 0, 0, 0, 4.540000, 4.540000, {{1938, 1460},{1610, 1212},{1930, 1092},}}},
    [TOUPCAM_MODEL_MTR3CCD01400KPA       ] = { 0x4020, { "MTR3CCD01400KPA             ", 0x00000010811b44c2, 1, 1, 0, 4, 0, 6.450000, 6.450000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_MTR3CCD01400KPA_USB2  ] = { 0x4021, { "MTR3CCD01400KPA(USB2.0)     ", 0x00000010811b4582, 1, 1, 0, 4, 0, 6.450000, 6.450000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_U3CMOS16000KPA        ] = { 0x3016, { "U3CMOS16000KPA              ", 0x0000000081002041, 2, 3, 3, 0, 0, 1.335000, 1.335000, {{4632, 3488},{2320, 1740},{1536, 1160},}}},
    [TOUPCAM_MODEL_U3CMOS16000KPA_USB2   ] = { 0x4016, { "U3CMOS16000KPA(USB2.0)      ", 0x0000000081002101, 2, 3, 3, 0, 0, 1.335000, 1.335000, {{4632, 3488},{2320, 1740},{1536, 1160},}}},
    [TOUPCAM_MODEL_U3CCD06000KPA         ] = { 0x2106, { "U3CCD06000KPA               ", 0x0000001081084042, 1, 2, 0, 0, 0, 4.540000, 4.540000, {{2748, 2200},{2748, 1092},}}},
    [TOUPCAM_MODEL_U3CCD06000KPA_USB2    ] = { 0x2107, { "U3CCD06000KPA(USB2.0)       ", 0x0000001081084102, 1, 2, 0, 0, 0, 4.540000, 4.540000, {{2748, 2200},{2748, 1092},}}},
    [TOUPCAM_MODEL_EXCCD00300KMA         ] = { 0x8309, { "EXCCD00300KMA               ", 0x0000001080082012, 3, 1, 0, 0, 0, 5.600000, 5.600000, {{640, 480},}}},
    [TOUPCAM_MODEL_EXCCD00300KMA_2       ] = { 0x8307, { "EXCCD00300KMA               ", 0x0000001080080012, 1, 1, 0, 0, 0, 5.600000, 5.600000, {{640, 480},}}},
    [TOUPCAM_MODEL_EP3CMOS02300KPA       ] = { 0x400c, { "EP3CMOS02300KPA             ", 0x0000000081082041, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1920, 1200},{960, 600},}}},
    [TOUPCAM_MODEL_EP3CMOS02300KPA_USB2  ] = { 0x400d, { "EP3CMOS02300KPA(USB2.0)     ", 0x0000000081082101, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1920, 1200},{960, 600},}}},
    [TOUPCAM_MODEL_EXCCD00440KPA         ] = { 0x804b, { "EXCCD00440KPA               ", 0x0000001100080004, 1, 1, 0, 0, 0, 8.600000, 8.300000, {{748, 576},}}},
    [TOUPCAM_MODEL_EXCCD00440KMB         ] = { 0x804c, { "EXCCD00440KMB               ", 0x0000001080080014, 1, 1, 0, 0, 0, 8.600000, 8.300000, {{748, 578},}}},
    [TOUPCAM_MODEL_EXCCD00440KMA         ] = { 0x804d, { "EXCCD00440KMA               ", 0x0000001080080014, 1, 1, 0, 0, 0, 8.600000, 8.300000, {{748, 578},}}},
    [TOUPCAM_MODEL_MTR3CCD06000KPA       ] = { 0x400a, { "MTR3CCD06000KPA             ", 0x00000010811b44c2, 1, 2, 0, 4, 0, 4.540000, 4.540000, {{2748, 2200},{2748, 1092},}}},
    [TOUPCAM_MODEL_MTR3CCD06000KPA_USB2  ] = { 0x400b, { "MTR3CCD06000KPA(USB2.0)     ", 0x00000010811b4582, 1, 2, 0, 4, 0, 4.540000, 4.540000, {{2748, 2200},{2748, 1092},}}},
    [TOUPCAM_MODEL_EXCCD00440KPB_2       ] = { 0x804a, { "EXCCD00440KPB               ", 0x0000001100080004, 1, 1, 0, 0, 0, 8.600000, 8.300000, {{748, 576},}}},
    [TOUPCAM_MODEL_ICMOS03100KPA         ] = { 0xa312, { "ICMOS03100KPA               ", 0x0000000080380009, 5, 3, 0, 0, 0, 3.200000, 3.200000, {{2048, 1536},{1024, 768},{680, 510},}}},
    [TOUPCAM_MODEL_E3CMOS12000KPA        ] = { 0x4002, { "E3CMOS12000KPA              ", 0x0000000081002041, 2, 2, 2, 0, 0, 1.850000, 1.850000, {{4000, 3000},{2048, 1080},}}},
    [TOUPCAM_MODEL_E3CMOS12000KPA_USB2   ] = { 0x4003, { "E3CMOS12000KPA(USB2.0)      ", 0x0000000081002101, 2, 2, 2, 0, 0, 1.850000, 1.850000, {{4000, 3000},{2048, 1080},}}},
    [TOUPCAM_MODEL_E3CMOS03100KPB        ] = { 0x4008, { "E3CMOS03100KPB              ", 0x0000000081004041, 2, 2, 0, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1920, 1080},}}},
    [TOUPCAM_MODEL_E3CMOS03100KPB_USB2   ] = { 0x4009, { "E3CMOS03100KPB(USB2.0)      ", 0x0000000081004101, 2, 2, 0, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1920, 1080},}}},
    [TOUPCAM_MODEL_UCMOS05100KPC         ] = { 0x6518, { "UCMOS05100KPC               ", 0x0000000080000029, 6, 3, 3, 0, 0, 2.200000, 2.200000, {{2592, 1944},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_E3CMOS02300KPB        ] = { 0x4006, { "E3CMOS02300KPB              ", 0x0000001081002041, 2, 1, 0, 0, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_E3CMOS02300KPB_USB2   ] = { 0x4007, { "E3CMOS02300KPB(USB2.0)      ", 0x0000001081002101, 2, 1, 0, 0, 0, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_UHCCD05200KMA         ] = { 0x8527, { "UHCCD05200KMA               ", 0x0000001080000012, 1, 2, 2, 0, 0, 3.450000, 3.450000, {{2448, 2040},{816, 680},}}},
    [TOUPCAM_MODEL_G3CMOS02300KPA_2      ] = { 0xe317, { "G3CMOS02300KPA              ", 0x0000000081492241, 2, 2, 0, 1, 0, 3.750000, 3.750000, {{1920, 1200},{960, 600},}}},
    [TOUPCAM_MODEL_G3CMOS02300KPA_USB2_2 ] = { 0xe217, { "G3CMOS02300KPA(USB2.0)      ", 0x0000000081492301, 2, 2, 0, 1, 0, 3.750000, 3.750000, {{1920, 1200},{960, 600},}}},
    [TOUPCAM_MODEL_GCMOS01200KMA_2       ] = { 0xb121, { "GCMOS01200KMA               ", 0x0000000080402219, 4, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_E3CMOS06300KPA        ] = { 0x4000, { "E3CMOS06300KPA              ", 0x0000000081004041, 2, 2, 2, 0, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_E3CMOS06300KPA_USB2   ] = { 0x4001, { "E3CMOS06300KPA(USB2.0)      ", 0x0000000081004101, 2, 2, 2, 0, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_ICMOS14000KPC         ] = { 0xfc15, { "ICMOS14000KPC               ", 0x0000000081000001, 1, 8, 0, 0, 0, 1.400000, 1.400000, {{1280, 720},{1280, 720},{1100, 800},{1100, 800},{1100, 800},{1024, 768},{1024, 768},{1024, 768},}}},
    [TOUPCAM_MODEL_EXCCD00300KMA_2_2     ] = { 0x8306, { "EXCCD00300KMA               ", 0x0000001080000012, 1, 1, 0, 0, 0, 5.600000, 5.600000, {{640, 480},}}},
    [TOUPCAM_MODEL_E3CMOS02300KPA        ] = { 0x3723, { "E3CMOS02300KPA              ", 0x0000000081002041, 2, 2, 2, 0, 0, 3.750000, 3.750000, {{1920, 1200},{960, 600},}}},
    [TOUPCAM_MODEL_E3CMOS02300KPA_USB2   ] = { 0x4723, { "E3CMOS02300KPA(USB2.0)      ", 0x0000000081002101, 2, 2, 2, 0, 0, 3.750000, 3.750000, {{1920, 1200},{960, 600},}}},
    [TOUPCAM_MODEL_E3CMOS03100KPA        ] = { 0x3731, { "E3CMOS03100KPA              ", 0x0000000081002041, 2, 2, 2, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_E3CMOS03100KPA_USB2   ] = { 0x4731, { "E3CMOS03100KPA(USB2.0)      ", 0x0000000081002101, 2, 2, 2, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_EXCCD00440KMB_2       ] = { 0x8047, { "EXCCD00440KMB               ", 0x0000001080000014, 1, 1, 0, 0, 0, 8.600000, 8.300000, {{748, 578},}}},
    [TOUPCAM_MODEL_EXCCD00440KPB_2_2     ] = { 0x8046, { "EXCCD00440KPB               ", 0x0000001100000004, 1, 1, 0, 0, 0, 8.600000, 8.300000, {{748, 576},}}},
    [TOUPCAM_MODEL_EXCCD00440KMA_2       ] = { 0x8045, { "EXCCD00440KMA               ", 0x0000001080000014, 1, 1, 0, 0, 0, 8.600000, 8.300000, {{748, 578},}}},
    [TOUPCAM_MODEL_AAICX429C             ] = { 0x8044, { "AAICX429C                   ", 0x0000001080000004, 1, 1, 0, 0, 0, 8.600000, 8.300000, {{748, 578},}}},
    [TOUPCAM_MODEL_U3CMOS18000KPA        ] = { 0x3018, { "U3CMOS18000KPA              ", 0x0000000081000461, 3, 3, 3, 0, 0, 1.250000, 1.250000, {{4912, 3684},{2456, 1842},{1228, 922},}}},
    [TOUPCAM_MODEL_U3CMOS18000KPA_USB2   ] = { 0x4018, { "U3CMOS18000KPA(USB2.0)      ", 0x0000000081000521, 3, 3, 3, 0, 0, 1.250000, 1.250000, {{4912, 3684},{2456, 1842},{1228, 922},}}},
    [TOUPCAM_MODEL_U3CMOS03100KPC        ] = { 0x3314, { "U3CMOS03100KPC              ", 0x0000000081000041, 2, 2, 2, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_U3CMOS03100KPC_USB2   ] = { 0x4314, { "U3CMOS03100KPC(USB2.0)      ", 0x0000000081000101, 2, 2, 2, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_L3CMOS03100KPB        ] = { 0xc312, { "L3CMOS03100KPB              ", 0x0000000081000041, 2, 2, 2, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_L3CMOS03100KPB_USB2   ] = { 0xd312, { "L3CMOS03100KPB(USB2.0)      ", 0x0000000081000101, 2, 2, 2, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_GCMOS03100KPA         ] = { 0xb310, { "GCMOS03100KPA               ", 0x0000000080400201, 2, 2, 0, 0, 0, 2.200000, 2.200000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_UHCCD05200KPA         ] = { 0x8526, { "UHCCD05200KPA               ", 0x0000001080000002, 1, 2, 2, 0, 0, 3.450000, 3.450000, {{2448, 2040},{816, 680},}}},
    [TOUPCAM_MODEL_UHCCD05000KPA         ] = { 0x8506, { "UHCCD05000KPA               ", 0x0000001080000004, 1, 2, 1, 0, 0, 3.400000, 3.400000, {{2560, 1920},{1280, 960},}}},
    [TOUPCAM_MODEL_UHCCD03100KPB         ] = { 0x8316, { "UHCCD03100KPB               ", 0x0000001080000004, 1, 2, 1, 0, 0, 3.450000, 3.450000, {{2048, 1536},{640, 480},}}},
    [TOUPCAM_MODEL_UHCCD02000KPA         ] = { 0x8206, { "UHCCD02000KPA               ", 0x0000001080000002, 1, 1, 0, 0, 0, 4.400000, 4.400000, {{1600, 1200},}}},
    [TOUPCAM_MODEL_UHCCD01400KPB         ] = { 0x8148, { "UHCCD01400KPB               ", 0x0000001080000002, 1, 1, 0, 0, 0, 4.650000, 4.650000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_UHCCD01400KMB         ] = { 0x8149, { "UHCCD01400KMB               ", 0x0000001080000012, 1, 1, 0, 0, 0, 4.650000, 4.650000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_EXCCD01400KPA_2       ] = { 0x814e, { "EXCCD01400KPA               ", 0x0000001080000002, 1, 1, 0, 0, 0, 6.450000, 6.450000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_EXCCD01400KMA         ] = { 0x814f, { "EXCCD01400KMA               ", 0x0000001080000012, 1, 1, 0, 0, 0, 6.450000, 6.450000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_ICMOS10000KPA         ] = { 0xa010, { "ICMOS10000KPA               ", 0x0000000080000029, 5, 3, 3, 0, 0, 1.670000, 1.670000, {{3584, 2748},{1792, 1374},{896, 684},}}},
    [TOUPCAM_MODEL_ICMOS14000KPA         ] = { 0xa014, { "ICMOS14000KPA               ", 0x0000000080000021, 5, 3, 3, 0, 0, 1.400000, 1.400000, {{4096, 3288},{2048, 1644},{1024, 822},}}},
    [TOUPCAM_MODEL_LCMOS03100KPB         ] = { 0xf312, { "LCMOS03100KPB               ", 0x0000000081000001, 2, 3, 3, 0, 0, 2.500000, 2.500000, {{2048, 1536},{1024, 768},{684, 512},}}},
    [TOUPCAM_MODEL_UCMOS02000KPC         ] = { 0x5002, { "UCMOS02000KPC               ", 0x0000000080000001, 5, 3, 3, 0, 0, 3.200000, 3.200000, {{1600, 1200},{800, 600},{400, 300},}}},
    [TOUPCAM_MODEL_WCMOS10000KPA         ] = { 0x6b10, { "WCMOS10000KPA               ", 0x0000000080000021, 5, 3, 3, 0, 0, 1.670000, 1.670000, {{3584, 2748},{1792, 1374},{896, 684},}}},
    [TOUPCAM_MODEL_UCMOS03100KPB         ] = { 0x5003, { "UCMOS03100KPB               ", 0x0000000080000001, 5, 3, 3, 0, 0, 3.200000, 3.200000, {{2048, 1536},{1024, 768},{512, 384},}}},
    [TOUPCAM_MODEL_GCMOS01200KMB_2       ] = { 0xb124, { "GCMOS01200KMB               ", 0x0000000080402219, 4, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_GCMOS01200KPB_2       ] = { 0xb123, { "GCMOS01200KPB               ", 0x0000000080402209, 4, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_GCMOS01200KPB_2_2     ] = { 0xb122, { "GCMOS01200KPB               ", 0x0000000080400209, 4, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_UCMOS01200KPB         ] = { 0x6122, { "UCMOS01200KPB               ", 0x0000000080000001, 4, 2, 2, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_U3CMOS14000KPB        ] = { 0x3bb4, { "U3CMOS14000KPB              ", 0x0000000081000061, 3, 3, 3, 0, 0, 1.400000, 1.400000, {{4096, 3286},{2048, 1644},{1024, 822},}}},
    [TOUPCAM_MODEL_U3CMOS14000KPB_USB2   ] = { 0x4bb4, { "U3CMOS14000KPB(USB2.0)      ", 0x0000000081000121, 3, 3, 3, 0, 0, 1.400000, 1.400000, {{4096, 3286},{2048, 1644},{1024, 822},}}},
    [TOUPCAM_MODEL_LCMOS01200KPB         ] = { 0xf132, { "LCMOS01200KPB               ", 0x0000000080000001, 5, 2, 2, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_LCMOS14000KPA         ] = { 0xf014, { "LCMOS14000KPA               ", 0x0000000081000021, 5, 3, 3, 0, 0, 1.400000, 1.400000, {{4096, 3288},{2048, 1644},{1024, 822},}}},
    [TOUPCAM_MODEL_LCMOS10000KPA         ] = { 0xf010, { "LCMOS10000KPA               ", 0x0000000081000029, 5, 3, 3, 0, 0, 1.670000, 1.670000, {{3584, 2748},{1792, 1374},{896, 684},}}},
    [TOUPCAM_MODEL_LCMOS09000KPA         ] = { 0xf900, { "LCMOS09000KPA               ", 0x0000000081000029, 5, 3, 3, 0, 0, 1.670000, 1.670000, {{3488, 2616},{1744, 1308},{872, 654},}}},
    [TOUPCAM_MODEL_LCMOS08000KPA         ] = { 0xf800, { "LCMOS08000KPA               ", 0x0000000081000029, 5, 3, 3, 0, 0, 1.670000, 1.670000, {{3264, 2448},{1600, 1200},{800, 600},}}},
    [TOUPCAM_MODEL_LCMOS05100KPA         ] = { 0xf510, { "LCMOS05100KPA               ", 0x0000000081000029, 6, 3, 3, 0, 0, 2.200000, 2.200000, {{2592, 1944},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_LCMOS03100KPA         ] = { 0xf310, { "LCMOS03100KPA               ", 0x0000000081000009, 5, 3, 3, 0, 0, 3.200000, 3.200000, {{2048, 1536},{1024, 768},{680, 510},}}},
    [TOUPCAM_MODEL_LCMOS02000KPA         ] = { 0xf200, { "LCMOS02000KPA               ", 0x0000000080000001, 5, 2, 2, 0, 0, 3.200000, 3.200000, {{1600, 1200},{800, 600},}}},
    [TOUPCAM_MODEL_LCMOS01300KPA         ] = { 0xf130, { "LCMOS01300KPA               ", 0x0000000080000001, 2, 3, 0, 0, 0, 3.600000, 3.600000, {{1280, 1024},{640, 512},{320, 256},}}},
    [TOUPCAM_MODEL_SCCCD01400KMB         ] = { 0x9143, { "SCCCD01400KMB               ", 0x0000001080000092, 1, 1, 0, 0, 0, 4.650000, 4.650000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_UHCCD05200KPA_2       ] = { 0x8520, { "UHCCD05200KPA               ", 0x0000001080000002, 1, 2, 1, 0, 0, 3.450000, 3.450000, {{2448, 2040},{816, 680},}}},
    [TOUPCAM_MODEL_UHCCD05100KPA         ] = { 0x8516, { "UHCCD05100KPA               ", 0x0000001080000004, 1, 2, 1, 0, 0, 2.775000, 2.775000, {{2592, 1944},{560, 420},}}},
    [TOUPCAM_MODEL_UHCCD05100KPA_2       ] = { 0x8510, { "UHCCD05100KPA               ", 0x0000001080000004, 1, 2, 1, 0, 0, 2.775000, 2.775000, {{2592, 1944},{560, 420},}}},
    [TOUPCAM_MODEL_UHCCD05000KPA_2       ] = { 0x8500, { "UHCCD05000KPA               ", 0x0000001080000004, 1, 2, 1, 0, 0, 3.400000, 3.400000, {{2560, 1920},{1280, 960},}}},
    [TOUPCAM_MODEL_UHCCD03100KPB_2       ] = { 0x8311, { "UHCCD03100KPB               ", 0x0000001080000004, 1, 2, 1, 0, 0, 3.450000, 3.450000, {{2048, 1536},{640, 480},}}},
    [TOUPCAM_MODEL_UHCCD02000KPA_2       ] = { 0x8200, { "UHCCD02000KPA               ", 0x0000001080000002, 1, 1, 0, 0, 0, 4.400000, 4.400000, {{1600, 1200},}}},
    [TOUPCAM_MODEL_UHCCD01400KPB_2       ] = { 0x8146, { "UHCCD01400KPB               ", 0x0000001080000002, 1, 1, 0, 0, 0, 4.650000, 4.650000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_UHCCD01400KPA         ] = { 0x8140, { "UHCCD01400KPA               ", 0x0000001080000002, 1, 1, 0, 0, 0, 4.650000, 4.650000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_UHCCD01400KMB_2       ] = { 0x7141, { "UHCCD01400KMB               ", 0x0000001080000012, 1, 1, 0, 0, 0, 4.650000, 4.650000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_UHCCD01400KMA         ] = { 0x7140, { "UHCCD01400KMA               ", 0x0000001080000012, 1, 1, 0, 0, 0, 4.650000, 4.650000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_UHCCD00800KPA         ] = { 0x8080, { "UHCCD00800KPA               ", 0x0000001080000002, 1, 1, 0, 0, 0, 4.650000, 4.650000, {{1024, 768},}}},
    [TOUPCAM_MODEL_UCMOS05100KMA         ] = { 0x6511, { "UCMOS05100KMA               ", 0x0000000080000039, 6, 3, 3, 0, 0, 2.200000, 2.200000, {{2592, 1944},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_UCMOS14000KPA         ] = { 0x6014, { "UCMOS14000KPA               ", 0x0000000080000021, 5, 3, 3, 0, 0, 1.400000, 1.400000, {{4096, 3288},{2048, 1644},{1024, 822},}}},
    [TOUPCAM_MODEL_UCMOS10000KPA         ] = { 0x6010, { "UCMOS10000KPA               ", 0x0000000080000029, 5, 3, 3, 0, 0, 1.670000, 1.670000, {{3584, 2748},{1792, 1374},{896, 684},}}},
    [TOUPCAM_MODEL_UCMOS09000KPB         ] = { 0x6901, { "UCMOS09000KPB               ", 0x0000000080000029, 5, 3, 3, 0, 0, 1.670000, 1.670000, {{3488, 2616},{1744, 1308},{872, 654},}}},
    [TOUPCAM_MODEL_UCMOS09000KPA         ] = { 0x6900, { "UCMOS09000KPA               ", 0x0000000080000021, 6, 3, 3, 0, 0, 1.750000, 1.750000, {{3488, 2616},{1744, 1308},{872, 654},}}},
    [TOUPCAM_MODEL_UCMOS08000KPB         ] = { 0x6801, { "UCMOS08000KPB               ", 0x0000000080000029, 5, 3, 3, 0, 0, 1.670000, 1.670000, {{3264, 2448},{1600, 1200},{800, 600},}}},
    [TOUPCAM_MODEL_UCMOS08000KPA         ] = { 0x6800, { "UCMOS08000KPA               ", 0x0000000080000021, 6, 3, 3, 0, 0, 1.750000, 1.750000, {{3264, 2448},{1600, 1200},{800, 600},}}},
    [TOUPCAM_MODEL_UCMOS05100KPA         ] = { 0x6510, { "UCMOS05100KPA               ", 0x0000000080000029, 6, 3, 3, 0, 0, 2.200000, 2.200000, {{2592, 1944},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_UCMOS03100KPA         ] = { 0x6310, { "UCMOS03100KPA               ", 0x0000000080000009, 5, 3, 3, 0, 0, 3.200000, 3.200000, {{2048, 1536},{1024, 768},{680, 510},}}},
    [TOUPCAM_MODEL_UCMOS02000KPB         ] = { 0x6201, { "UCMOS02000KPB               ", 0x0000000080000001, 5, 2, 2, 0, 0, 3.200000, 3.200000, {{1600, 1200},{800, 600},}}},
    [TOUPCAM_MODEL_UCMOS02000KPA         ] = { 0x6200, { "UCMOS02000KPA               ", 0x0000000400000001, 6, 2, 0, 0, 0, 2.800000, 2.800000, {{1600, 1200},{800, 600},}}},
    [TOUPCAM_MODEL_UCMOS01300KPA         ] = { 0x6130, { "UCMOS01300KPA               ", 0x0000000080000001, 1, 3, 0, 0, 0, 3.600000, 3.600000, {{1280, 1024},{640, 512},{320, 256},}}},
    [TOUPCAM_MODEL_UCMOS01300KMA         ] = { 0x6131, { "UCMOS01300KMA               ", 0x0000000080000011, 1, 1, 0, 0, 0, 5.200000, 5.200000, {{1280, 1024},}}},
    [TOUPCAM_MODEL_UCMOS00350KPA         ] = { 0x6035, { "UCMOS00350KPA               ", 0x0000000080000001, 2, 2, 0, 0, 0, 5.600000, 5.600000, {{640, 480},{320, 240},}}},
    [TOUPCAM_MODEL_U3CMOS14000KPA        ] = { 0x3014, { "U3CMOS14000KPA              ", 0x0000000081000061, 3, 3, 3, 0, 0, 1.400000, 1.400000, {{4096, 3286},{2048, 1644},{1024, 822},}}},
    [TOUPCAM_MODEL_U3CMOS14000KPA_USB2   ] = { 0x4014, { "U3CMOS14000KPA(USB2.0)      ", 0x0000000081000121, 3, 3, 3, 0, 0, 1.400000, 1.400000, {{4096, 3286},{2048, 1644},{1024, 822},}}},
    [TOUPCAM_MODEL_U3CMOS10000KPA        ] = { 0x3010, { "U3CMOS10000KPA              ", 0x0000000081000061, 3, 3, 3, 0, 0, 1.670000, 1.670000, {{3584, 2746},{1792, 1372},{896, 680},}}},
    [TOUPCAM_MODEL_U3CMOS10000KPA_USB2   ] = { 0x4010, { "U3CMOS10000KPA(USB2.0)      ", 0x0000000081000121, 3, 3, 3, 0, 0, 1.670000, 1.670000, {{3584, 2746},{1792, 1372},{896, 680},}}},
    [TOUPCAM_MODEL_U3CMOS08500KPA        ] = { 0x3850, { "U3CMOS08500KPA              ", 0x0000000081000061, 3, 2, 2, 0, 0, 1.670000, 1.670000, {{3328, 2548},{1664, 1272},}}},
    [TOUPCAM_MODEL_U3CMOS08500KPA_USB2   ] = { 0x4850, { "U3CMOS08500KPA(USB2.0)      ", 0x0000000081000121, 3, 2, 2, 0, 0, 1.670000, 1.670000, {{3328, 2548},{1664, 1272},}}},
    [TOUPCAM_MODEL_U3CMOS05100KPA        ] = { 0x3510, { "U3CMOS05100KPA              ", 0x0000000081000061, 6, 3, 3, 0, 0, 2.200000, 2.200000, {{2560, 1922},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_U3CMOS05100KPA_USB2   ] = { 0x4510, { "U3CMOS05100KPA(USB2.0)      ", 0x0000000081000121, 6, 3, 3, 0, 0, 2.200000, 2.200000, {{2560, 1922},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_U3CMOS03100KPB        ] = { 0x3312, { "U3CMOS03100KPB              ", 0x0000000081000041, 2, 2, 2, 0, 0, 2.200000, 2.200000, {{1920, 1080},{1280, 720},}}},
    [TOUPCAM_MODEL_U3CMOS03100KPB_USB2   ] = { 0x4312, { "U3CMOS03100KPB(USB2.0)      ", 0x0000000081000101, 2, 2, 2, 0, 0, 2.200000, 2.200000, {{1920, 1080},{1280, 720},}}},
    [TOUPCAM_MODEL_U3CMOS03100KPA        ] = { 0x3310, { "U3CMOS03100KPA              ", 0x0000000081000041, 2, 2, 2, 0, 0, 2.200000, 2.200000, {{2048, 1534},{1024, 770},}}},
    [TOUPCAM_MODEL_U3CMOS03100KPA_USB2   ] = { 0x4310, { "U3CMOS03100KPA(USB2.0)      ", 0x0000000081000101, 2, 2, 2, 0, 0, 2.200000, 2.200000, {{2048, 1534},{1024, 770},}}},
    [TOUPCAM_MODEL_SCCCD05200KPA         ] = { 0x9520, { "SCCCD05200KPA               ", 0x0000001080000082, 1, 2, 1, 0, 0, 3.450000, 3.450000, {{2448, 2040},{816, 680},}}},
    [TOUPCAM_MODEL_SCCCD01400KMA         ] = { 0x9142, { "SCCCD01400KMA               ", 0x0000001080000092, 1, 1, 0, 0, 0, 6.450000, 6.450000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_SCCCD01400KPB         ] = { 0x9146, { "SCCCD01400KPB               ", 0x0000001080000082, 1, 1, 0, 0, 0, 4.650000, 4.650000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_SCCCD01400KPA         ] = { 0x9141, { "SCCCD01400KPA               ", 0x0000001080000082, 1, 1, 0, 0, 0, 6.450000, 6.450000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_EXCCD01400KPA_2_2     ] = { 0x8141, { "EXCCD01400KPA               ", 0x0000001080000002, 1, 1, 0, 0, 0, 6.450000, 6.450000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_EXCCD01400KMA_2       ] = { 0x8142, { "EXCCD01400KMA               ", 0x0000001080000012, 1, 1, 0, 0, 0, 6.450000, 6.450000, {{1360, 1024},}}},
    [TOUPCAM_MODEL_EXCCD00300KMA_2_2_2   ] = { 0x8031, { "EXCCD00300KMA               ", 0x0000001080000012, 1, 1, 0, 0, 0, 5.600000, 5.600000, {{640, 480},}}},
    [TOUPCAM_MODEL_ICMOS03100KPA_2       ] = { 0xa310, { "ICMOS03100KPA               ", 0x0000000080000009, 5, 3, 3, 0, 0, 3.200000, 3.200000, {{2048, 1536},{1024, 768},{680, 510},}}},
    [TOUPCAM_MODEL_ICMOS01300KMA         ] = { 0xa131, { "ICMOS01300KMA               ", 0x0000000080000011, 1, 1, 0, 0, 0, 5.200000, 5.200000, {{1280, 1024},}}},
    [TOUPCAM_MODEL_UCMOS01200KMA         ] = { 0x6121, { "UCMOS01200KMA               ", 0x0000000080000011, 1, 1, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},}}},
    [TOUPCAM_MODEL_L3CMOS14000KPA        ] = { 0xc014, { "L3CMOS14000KPA              ", 0x0000000081000061, 3, 3, 3, 0, 0, 1.400000, 1.400000, {{4096, 3286},{2048, 1644},{1024, 822},}}},
    [TOUPCAM_MODEL_L3CMOS14000KPA_USB2   ] = { 0xd014, { "L3CMOS14000KPA(USB2.0)      ", 0x0000000081000121, 3, 3, 3, 0, 0, 1.400000, 1.400000, {{4096, 3286},{2048, 1644},{1024, 822},}}},
    [TOUPCAM_MODEL_L3CMOS10000KPA        ] = { 0xc010, { "L3CMOS10000KPA              ", 0x0000000081000061, 3, 3, 3, 0, 0, 1.670000, 1.670000, {{3584, 2746},{1792, 1372},{896, 680},}}},
    [TOUPCAM_MODEL_L3CMOS10000KPA_USB2   ] = { 0xd010, { "L3CMOS10000KPA(USB2.0)      ", 0x0000000081000121, 3, 3, 3, 0, 0, 1.670000, 1.670000, {{3584, 2746},{1792, 1372},{896, 680},}}},
    [TOUPCAM_MODEL_L3CMOS05100KPA        ] = { 0xc510, { "L3CMOS05100KPA              ", 0x0000000081000061, 6, 3, 3, 0, 0, 2.200000, 2.200000, {{2560, 1922},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_L3CMOS05100KPA_USB2   ] = { 0xd510, { "L3CMOS05100KPA(USB2.0)      ", 0x0000000081000121, 6, 3, 3, 0, 0, 2.200000, 2.200000, {{2560, 1922},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_L3CMOS03100KPA        ] = { 0xc310, { "L3CMOS03100KPA              ", 0x0000000081000041, 2, 2, 2, 0, 0, 2.200000, 2.200000, {{2048, 1534},{1024, 770},}}},
    [TOUPCAM_MODEL_L3CMOS03100KPA_USB2   ] = { 0xd310, { "L3CMOS03100KPA(USB2.0)      ", 0x0000000081000101, 2, 2, 2, 0, 0, 2.200000, 2.200000, {{2048, 1534},{1024, 770},}}},
    [TOUPCAM_MODEL_MTR3CMOS20000KPA      ] = { 0x1116, { "MTR3CMOS20000KPA            ", 0x00000000836b24c9, 3, 4, 4, 1, 0, 2.400000, 2.400000, {{5440, 3648},{4096, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_MTR3CMOS20000KPA_USB2 ] = { 0x1117, { "MTR3CMOS20000KPA(USB2.0)    ", 0x00000000836b2589, 3, 4, 4, 1, 0, 2.400000, 2.400000, {{5440, 3648},{4096, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_ATR3CMOS20000KPA      ] = { 0x1114, { "ATR3CMOS20000KPA            ", 0x00000000836b24c9, 3, 4, 0, 1, 0, 2.400000, 2.400000, {{5440, 3648},{4096, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_ATR3CMOS20000KPA_USB2 ] = { 0x1115, { "ATR3CMOS20000KPA(USB2.0)    ", 0x00000000836b2589, 3, 4, 0, 1, 0, 2.400000, 2.400000, {{5440, 3648},{4096, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_MTR3CCD12000KPA       ] = { 0x1021, { "MTR3CCD12000KPA             ", 0x00000010811b44c2, 1, 2, 0, 4, 0, 3.100000, 3.100000, {{4248, 2836},{2124, 1418},}}},
    [TOUPCAM_MODEL_MTR3CCD12000KPA_USB2  ] = { 0x1035, { "MTR3CCD12000KPA(USB2.0)     ", 0x00000010811b4582, 1, 2, 0, 4, 0, 3.100000, 3.100000, {{4248, 2836},{2124, 1418},}}},
    [TOUPCAM_MODEL_E3ISPM01500KPA        ] = { 0x11d0, { "E3ISPM01500KPA              ", 0x0000001081042449, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{1440, 1080},{720, 540},}}},
    [TOUPCAM_MODEL_E3ISPM01500KPA_USB2   ] = { 0x11d1, { "E3ISPM01500KPA(USB2.0)      ", 0x0000001081042509, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{1440, 1080},{720, 540},}}},
    [TOUPCAM_MODEL_E3CMOS20000KMA        ] = { 0x11de, { "E3CMOS20000KMA              ", 0x0000000081002059, 2, 4, 4, 0, 0, 2.400000, 2.400000, {{5440, 3648},{4080, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_E3CMOS20000KMA_USB2   ] = { 0x11df, { "E3CMOS20000KMA(USB2.0)      ", 0x0000000081002119, 2, 4, 4, 0, 0, 2.400000, 2.400000, {{5440, 3648},{4080, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_G3CMOS20000KPA_2_2    ] = { 0x11e6, { "G3CMOS20000KPA              ", 0x0000000083492649, 3, 4, 4, 2, 0, 2.400000, 2.400000, {{5440, 3648},{4096, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_G3CMOS20000KPA_USB2_2_2] = { 0x11e7, { "G3CMOS20000KPA(USB2.0)      ", 0x0000000083492709, 3, 4, 4, 2, 0, 2.400000, 2.400000, {{5440, 3648},{4096, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_BIGEYE10000KPA        ] = { 0x11e4, { "BigEye10000KPA              ", 0x0000000081042449, 2, 5, 5, 0, 0, 4.630000, 4.630000, {{3704, 2778},{4096, 2160},{2760, 2072},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_BIGEYE10000KPA_USB2   ] = { 0x11e5, { "BigEye10000KPA(USB2.0)      ", 0x0000000081042509, 2, 5, 5, 0, 0, 4.630000, 4.630000, {{3704, 2778},{4096, 2160},{2760, 2072},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_BIGEYE4200KMB         ] = { 0x1203, { "BigEye4200KMB               ", 0x0000000887002051, 3, 2, 0, 0, 0, 6.500000, 6.500000, {{2048, 2046},{1024, 1022},}}},
    [TOUPCAM_MODEL_BIGEYE4200KMB_USB2    ] = { 0x1204, { "BigEye4200KMB(USB2.0)       ", 0x0000000887002111, 3, 2, 0, 0, 0, 6.500000, 6.500000, {{2048, 2046},{1024, 1022},}}},
    [TOUPCAM_MODEL_G3CMOS20000KMA_2      ] = { 0x11ec, { "G3CMOS20000KMA              ", 0x0000000083492659, 3, 4, 4, 2, 0, 2.400000, 2.400000, {{5440, 3648},{4096, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_G3CMOS20000KMA_USB2_2 ] = { 0x11ed, { "G3CMOS20000KMA(USB2.0)      ", 0x0000000083492719, 3, 4, 4, 2, 0, 2.400000, 2.400000, {{5440, 3648},{4096, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_ATR3CMOS20000KMA      ] = { 0x11d2, { "ATR3CMOS20000KMA            ", 0x00000000836b24d9, 3, 4, 0, 1, 0, 2.400000, 2.400000, {{5440, 3648},{4096, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_ATR3CMOS20000KMA_USB2 ] = { 0x11d3, { "ATR3CMOS20000KMA(USB2.0)    ", 0x00000000836b2599, 3, 4, 0, 1, 0, 2.400000, 2.400000, {{5440, 3648},{4096, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_MTR3CMOS20000KMA      ] = { 0x11ee, { "MTR3CMOS20000KMA            ", 0x00000000836b24d9, 3, 4, 4, 1, 0, 2.400000, 2.400000, {{5440, 3648},{4096, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_MTR3CMOS20000KMA_USB2 ] = { 0x11ef, { "MTR3CMOS20000KMA(USB2.0)    ", 0x00000000836b2599, 3, 4, 4, 1, 0, 2.400000, 2.400000, {{5440, 3648},{4096, 2160},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_MTR3CMOS16000KPA_2    ] = { 0x11f0, { "MTR3CMOS16000KPA            ", 0x00000000812b24c9, 2, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_MTR3CMOS16000KPA_USB2_2] = { 0x11f1, { "MTR3CMOS16000KPA(USB2.0)    ", 0x00000000812b2589, 2, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_E3CMOS01200KPA_2      ] = { 0x11f8, { "E3CMOS01200KPA              ", 0x0000000081001049, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_E3CMOS01200KPA_USB2_2 ] = { 0x11f9, { "E3CMOS01200KPA(USB2.0)      ", 0x0000000081001109, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_UCMOS01300KPA_2       ] = { 0x11fc, { "UCMOS01300KPA               ", 0x0000000080000001, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{1280, 720},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KPA_2    ] = { 0x11ea, { "ATR3CMOS16000KPA            ", 0x00000000836b24c9, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KPA_USB2_2] = { 0x11eb, { "ATR3CMOS16000KPA(USB2.0)    ", 0x00000000836b2589, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_E3CMOS12300KMA        ] = { 0x1201, { "E3CMOS12300KMA              ", 0x0000001081002459, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{4096, 3000},{2048, 1500},}}},
    [TOUPCAM_MODEL_E3CMOS12300KMA_USB2   ] = { 0x1202, { "E3CMOS12300KMA(USB2.0)      ", 0x0000001081002519, 2, 2, 2, 0, 0, 3.450000, 3.450000, {{4096, 3000},{2048, 1500},}}},
    [TOUPCAM_MODEL_E3ISPM08300KPB        ] = { 0x11fa, { "E3ISPM08300KPB              ", 0x0000000081042449, 2, 2, 2, 0, 0, 2.000000, 2.000000, {{3840, 2160},{1920, 1080},}}},
    [TOUPCAM_MODEL_E3ISPM08300KPB_USB2   ] = { 0x11fb, { "E3ISPM08300KPB(USB2.0)      ", 0x0000000081042509, 2, 2, 2, 0, 0, 2.000000, 2.000000, {{3840, 2160},{1920, 1080},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KMA_2    ] = { 0x11f6, { "ATR3CMOS16000KMA            ", 0x00000000836b24d9, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KMA_USB2_2] = { 0x11f7, { "ATR3CMOS16000KMA(USB2.0)    ", 0x00000000836b2599, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_ATR3CMOS10300KPA      ] = { 0x11fd, { "ATR3CMOS10300KPA            ", 0x00000000876b44c9, 2, 4, 4, 1, 0, 4.630000, 4.630000, {{3704, 2778},{4096, 2160},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_ATR3CMOS10300KPA_USB2 ] = { 0x11fe, { "ATR3CMOS10300KPA(USB2.0)    ", 0x00000000876b4589, 2, 4, 4, 1, 0, 4.630000, 4.630000, {{3704, 2778},{4096, 2160},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_LCMOS01300KPA_2       ] = { 0x1207, { "LCMOS01300KPA               ", 0x0000000080000001, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{1280, 720},}}},
    [TOUPCAM_MODEL_MTR3CCD12000KMA       ] = { 0x1022, { "MTR3CCD12000KMA             ", 0x00000010811b44d2, 1, 2, 0, 4, 0, 3.100000, 3.100000, {{4248, 2836},{2124, 1418},}}},
    [TOUPCAM_MODEL_MTR3CCD12000KMA_USB2  ] = { 0x1036, { "MTR3CCD12000KMA(USB2.0)     ", 0x00000010811b4592, 1, 2, 0, 4, 0, 3.100000, 3.100000, {{4248, 2836},{2124, 1418},}}},
    [TOUPCAM_MODEL_I3ISPM01500KPA        ] = { 0x1209, { "I3ISPM01500KPA              ", 0x00000050811c1449, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{1440, 1080},{720, 540},}}},
    [TOUPCAM_MODEL_I3ISPM01500KPA_USB2   ] = { 0x120a, { "I3ISPM01500KPA(USB2.0)      ", 0x00000050811c1509, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{1440, 1080},{720, 540},}}},
    [TOUPCAM_MODEL_I3ISPM01500KPA_2      ] = { 0x127a, { "I3ISPM01500KPA              ", 0x00000050831c1449, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{1440, 1080},{720, 540},}}},
    [TOUPCAM_MODEL_I3ISPM01500KPA_USB2_2 ] = { 0x127b, { "I3ISPM01500KPA(USB2.0)      ", 0x00000050831c1509, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{1440, 1080},{720, 540},}}},
    [TOUPCAM_MODEL_MTR3CMOS10300KPA      ] = { 0x11ff, { "MTR3CMOS10300KPA            ", 0x00000000876b44c9, 2, 4, 4, 1, 0, 4.630000, 4.630000, {{3704, 2778},{4096, 2160},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_MTR3CMOS10300KPA_USB2 ] = { 0x1200, { "MTR3CMOS10300KPA(USB2.0)    ", 0x00000000876b4589, 2, 4, 4, 1, 0, 4.630000, 4.630000, {{3704, 2778},{4096, 2160},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_I3ISPM02000KPA        ] = { 0x120d, { "I3ISPM02000KPA              ", 0x00000000810c1449, 2, 1, 0, 0, 0, 3.750000, 3.750000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_I3ISPM02000KPA_USB2   ] = { 0x120e, { "I3ISPM02000KPA(USB2.0)      ", 0x00000000810c1509, 2, 1, 0, 0, 0, 3.750000, 3.750000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_ECMOS05000KPA         ] = { 0x1208, { "ECMOS05000KPA               ", 0x0000000081000009, 2, 2, 2, 0, 0, 2.000000, 2.000000, {{2592, 1944},{1296, 972},}}},
    [TOUPCAM_MODEL_G3CMOS16000KPA_2_2    ] = { 0x1213, { "G3CMOS16000KPA              ", 0x0000000083692249, 3, 3, 0, 2, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_G3CMOS16000KPA_USB2_2_2] = { 0x1214, { "G3CMOS16000KPA(USB2.0)      ", 0x0000000083692309, 3, 3, 0, 2, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_G3CMOS16000KMA_2_2    ] = { 0x1215, { "G3CMOS16000KMA              ", 0x0000000083692259, 3, 3, 0, 2, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_G3CMOS16000KMA_USB2_2_2] = { 0x1216, { "G3CMOS16000KMA(USB2.0)      ", 0x0000000083692319, 3, 3, 0, 2, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_E3ISPM06300KPB        ] = { 0x1217, { "E3ISPM06300KPB              ", 0x0000000083041049, 2, 2, 2, 0, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_E3ISPM06300KPB_USB2   ] = { 0x1218, { "E3ISPM06300KPB(USB2.0)      ", 0x0000000083041109, 2, 2, 2, 0, 0, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_G3CMOS10300KPA_2      ] = { 0x121e, { "G3CMOS10300KPA              ", 0x0000000087694649, 2, 4, 4, 1, 0, 4.630000, 4.630000, {{4128, 2808},{4096, 2160},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_G3CMOS10300KPA_USB2_2 ] = { 0x121f, { "G3CMOS10300KPA(USB2.0)      ", 0x0000000087694709, 2, 4, 4, 1, 0, 4.630000, 4.630000, {{4128, 2808},{4096, 2160},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_C3CMOS05100KPA        ] = { 0x120f, { "C3CMOS05100KPA              ", 0x0000000081000069, 6, 3, 3, 0, 0, 2.200000, 2.200000, {{2592, 1944},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_C3CMOS05100KPA_USB2   ] = { 0x1210, { "C3CMOS05100KPA(USB2.0)      ", 0x0000000081000129, 6, 3, 3, 0, 0, 2.200000, 2.200000, {{2592, 1944},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_MTR3CMOS16000KMA_2    ] = { 0x11f4, { "MTR3CMOS16000KMA            ", 0x00000000836b24d9, 3, 3, 3, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_MTR3CMOS16000KMA_USB2_2] = { 0x11f5, { "MTR3CMOS16000KMA(USB2.0)    ", 0x00000000836b2599, 3, 3, 3, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_MTR3CMOS16000KPA_2_2  ] = { 0x11f2, { "MTR3CMOS16000KPA            ", 0x00000000836b24c9, 3, 3, 3, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_MTR3CMOS16000KPA_USB2_2_2] = { 0x11f3, { "MTR3CMOS16000KPA(USB2.0)    ", 0x00000000836b2589, 3, 3, 3, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_I3CMOS01500KMA        ] = { 0x120b, { "I3CMOS01500KMA              ", 0x0000005081182459, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{1440, 1080},{720, 540},}}},
    [TOUPCAM_MODEL_I3CMOS01500KMA_USB2   ] = { 0x120c, { "I3CMOS01500KMA(USB2.0)      ", 0x0000005081182519, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{1440, 1080},{720, 540},}}},
    [TOUPCAM_MODEL_I3CMOS01500KMA_2      ] = { 0x126e, { "I3CMOS01500KMA              ", 0x0000005083182459, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{1440, 1080},{720, 540},}}},
    [TOUPCAM_MODEL_I3CMOS01500KMA_USB2_2 ] = { 0x126f, { "I3CMOS01500KMA(USB2.0)      ", 0x0000005083182519, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{1440, 1080},{720, 540},}}},
    [TOUPCAM_MODEL_BIGEYE4200KMA         ] = { 0x1205, { "BigEye4200KMA               ", 0x0000000887002059, 3, 2, 0, 0, 0, 6.500000, 6.500000, {{2048, 2046},{1024, 1022},}}},
    [TOUPCAM_MODEL_BIGEYE4200KMA_USB2    ] = { 0x1206, { "BigEye4200KMA(USB2.0)       ", 0x0000000887002119, 3, 2, 0, 0, 0, 6.500000, 6.500000, {{2048, 2046},{1024, 1022},}}},
    [TOUPCAM_MODEL_E3CMOS20000KPB        ] = { 0x1233, { "E3CMOS20000KPB              ", 0x0000000081002041, 2, 3, 3, 0, 0, 1.200000, 1.200000, {{5200, 3888},{2592, 1944},{1728, 1296},}}},
    [TOUPCAM_MODEL_E3CMOS20000KPB_USB2   ] = { 0x1234, { "E3CMOS20000KPB(USB2.0)      ", 0x0000000081002101, 2, 3, 3, 0, 0, 1.200000, 1.200000, {{5200, 3888},{2592, 1944},{1728, 1296},}}},
    [TOUPCAM_MODEL_ATR3CMOS01700KMA      ] = { 0x124d, { "ATR3CMOS01700KMA            ", 0x00000010876b24d9, 2, 1, 0, 1, 0, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_ATR3CMOS01700KMA_USB2 ] = { 0x124e, { "ATR3CMOS01700KMA(USB2.0)    ", 0x00000010876b2599, 2, 1, 0, 1, 0, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_ATR3CMOS01700KPA      ] = { 0x1249, { "ATR3CMOS01700KPA            ", 0x00000010876b24c9, 2, 1, 0, 1, 0, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_ATR3CMOS01700KPA_USB2 ] = { 0x124a, { "ATR3CMOS01700KPA(USB2.0)    ", 0x00000010876b2589, 2, 1, 0, 1, 0, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_ATR3CMOS07100KMA      ] = { 0x1245, { "ATR3CMOS07100KMA            ", 0x00000010876b24d9, 2, 2, 0, 1, 0, 4.500000, 4.500000, {{3200, 2200},{1600, 1100},}}},
    [TOUPCAM_MODEL_ATR3CMOS07100KMA_USB2 ] = { 0x1246, { "ATR3CMOS07100KMA(USB2.0)    ", 0x00000010876b2599, 2, 2, 0, 1, 0, 4.500000, 4.500000, {{3200, 2200},{1600, 1100},}}},
    [TOUPCAM_MODEL_ATR3CMOS07100KPA      ] = { 0x1241, { "ATR3CMOS07100KPA            ", 0x00000010876b24c9, 2, 2, 0, 1, 0, 4.500000, 4.500000, {{3200, 2200},{1600, 1100},}}},
    [TOUPCAM_MODEL_ATR3CMOS07100KPA_USB2 ] = { 0x1242, { "ATR3CMOS07100KPA(USB2.0)    ", 0x00000010876b2589, 2, 2, 0, 1, 0, 4.500000, 4.500000, {{3200, 2200},{1600, 1100},}}},
    [TOUPCAM_MODEL_MTR3CMOS07100KPA      ] = { 0x1243, { "MTR3CMOS07100KPA            ", 0x00000010876b24c9, 2, 2, 2, 1, 0, 4.500000, 4.500000, {{3200, 2200},{1600, 1100},}}},
    [TOUPCAM_MODEL_MTR3CMOS07100KPA_USB2 ] = { 0x1244, { "MTR3CMOS07100KPA(USB2.0)    ", 0x00000010876b2589, 2, 2, 2, 1, 0, 4.500000, 4.500000, {{3200, 2200},{1600, 1100},}}},
    [TOUPCAM_MODEL_MTR3CMOS07100KMA      ] = { 0x1247, { "MTR3CMOS07100KMA            ", 0x00000010876b24d9, 2, 2, 2, 1, 0, 4.500000, 4.500000, {{3200, 2200},{1600, 1100},}}},
    [TOUPCAM_MODEL_MTR3CMOS07100KMA_USB2 ] = { 0x1248, { "MTR3CMOS07100KMA(USB2.0)    ", 0x00000010876b2599, 2, 2, 2, 1, 0, 4.500000, 4.500000, {{3200, 2200},{1600, 1100},}}},
    [TOUPCAM_MODEL_MTR3CMOS01700KPA      ] = { 0x124b, { "MTR3CMOS01700KPA            ", 0x00000010876b24c9, 2, 1, 0, 1, 0, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_MTR3CMOS01700KPA_USB2 ] = { 0x124c, { "MTR3CMOS01700KPA(USB2.0)    ", 0x00000010876b2589, 2, 1, 0, 1, 0, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_MTR3CMOS01700KMA      ] = { 0x124f, { "MTR3CMOS01700KMA            ", 0x00000010876b24d9, 2, 1, 0, 1, 0, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_MTR3CMOS01700KMA_USB2 ] = { 0x1250, { "MTR3CMOS01700KMA(USB2.0)    ", 0x00000010876b2599, 2, 1, 0, 1, 0, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_SL170_C_M             ] = { 0x129f, { "SL170-C-M                   ", 0x00000010876b24d9, 2, 1, 0, 1, 0, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_SL170_C_M_USB2        ] = { 0x12a0, { "SL170-C-M(USB2.0)           ", 0x00000010876b2599, 2, 1, 0, 1, 0, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_PUM02000KPA           ] = { 0x1251, { "PUM02000KPA                 ", 0x0000000081000001, 2, 1, 0, 0, 0, 2.900000, 2.900000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_GPCMOS02000KPB        ] = { 0x1219, { "GPCMOS02000KPB              ", 0x0000000084482209, 2, 1, 0, 0, 0, 2.900000, 2.900000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_I3CMOS03100KMA        ] = { 0x10c8, { "I3CMOS03100KMA              ", 0x0000005081182459, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3CMOS03100KMA_USB2   ] = { 0x10d5, { "I3CMOS03100KMA(USB2.0)      ", 0x0000005081182519, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3CMOS03100KMA_2      ] = { 0x1270, { "I3CMOS03100KMA              ", 0x0000005083182459, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3CMOS03100KMA_USB2_2 ] = { 0x1271, { "I3CMOS03100KMA(USB2.0)      ", 0x0000005083182519, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3CMOS05000KMA        ] = { 0x10ca, { "I3CMOS05000KMA              ", 0x0000005081182459, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3CMOS05000KMA_USB2   ] = { 0x10d7, { "I3CMOS05000KMA(USB2.0)      ", 0x0000005081182519, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3CMOS05000KMA_2      ] = { 0x1274, { "I3CMOS05000KMA              ", 0x0000005083182459, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3CMOS05000KMA_USB2_2 ] = { 0x1275, { "I3CMOS05000KMA(USB2.0)      ", 0x0000005083182519, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_C3CMOS10000KPA        ] = { 0x123b, { "C3CMOS10000KPA              ", 0x0000000081000069, 3, 3, 3, 0, 0, 1.670000, 1.670000, {{3664, 2748},{1832, 1374},{912, 686},}}},
    [TOUPCAM_MODEL_C3CMOS10000KPA_USB2   ] = { 0x123c, { "C3CMOS10000KPA(USB2.0)      ", 0x0000000081000129, 3, 3, 3, 0, 0, 1.670000, 1.670000, {{3664, 2748},{1832, 1374},{912, 686},}}},
    [TOUPCAM_MODEL_I3ISPM05000KPA        ] = { 0x123d, { "I3ISPM05000KPA              ", 0x00000050811c2449, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3ISPM05000KPA_USB2   ] = { 0x123e, { "I3ISPM05000KPA(USB2.0)      ", 0x00000050811c2509, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3ISPM05000KPA_2      ] = { 0x1280, { "I3ISPM05000KPA              ", 0x00000050831c2449, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3ISPM05000KPA_USB2_2 ] = { 0x1281, { "I3ISPM05000KPA(USB2.0)      ", 0x00000050831c2509, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3ISPM03100KPA        ] = { 0x123f, { "I3ISPM03100KPA              ", 0x00000050811c2449, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3ISPM03100KPA_USB2   ] = { 0x1240, { "I3ISPM03100KPA(USB2.0)      ", 0x00000050811c2509, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3ISPM03100KPA_2      ] = { 0x127c, { "I3ISPM03100KPA              ", 0x00000050831c2449, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3ISPM03100KPA_USB2_2 ] = { 0x127d, { "I3ISPM03100KPA(USB2.0)      ", 0x00000050831c2509, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_E3ISPM18000KPA        ] = { 0x1252, { "E3ISPM18000KPA              ", 0x0000000081042041, 2, 3, 3, 0, 0, 1.200000, 1.200000, {{4880, 3720},{2448, 1836},{1632, 1224},}}},
    [TOUPCAM_MODEL_E3ISPM18000KPA_USB2   ] = { 0x1253, { "E3ISPM18000KPA(USB2.0)      ", 0x0000000081042101, 2, 3, 3, 0, 0, 1.200000, 1.200000, {{4880, 3720},{2448, 1836},{1632, 1224},}}},
    [TOUPCAM_MODEL_E3ISPM20000KPB        ] = { 0x1254, { "E3ISPM20000KPB              ", 0x0000000081042041, 2, 3, 3, 0, 0, 1.200000, 1.200000, {{5200, 3888},{2592, 1944},{1728, 1296},}}},
    [TOUPCAM_MODEL_E3ISPM20000KPB_USB2   ] = { 0x1255, { "E3ISPM20000KPB(USB2.0)      ", 0x0000000081042101, 2, 3, 3, 0, 0, 1.200000, 1.200000, {{5200, 3888},{2592, 1944},{1728, 1296},}}},
    [TOUPCAM_MODEL_I3ISPM05000KPB        ] = { 0x1262, { "I3ISPM05000KPB              ", 0x00000050811c2449, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3ISPM05000KPB_USB2   ] = { 0x1263, { "I3ISPM05000KPB(USB2.0)      ", 0x00000050811c2509, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3ISPM05000KPB_2      ] = { 0x1282, { "I3ISPM05000KPB              ", 0x00000050831c2449, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3ISPM05000KPB_USB2_2 ] = { 0x1283, { "I3ISPM05000KPB(USB2.0)      ", 0x00000050831c2509, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3ISPM03100KPB        ] = { 0x1260, { "I3ISPM03100KPB              ", 0x00000050811c2449, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3ISPM03100KPB_USB2   ] = { 0x1261, { "I3ISPM03100KPB(USB2.0)      ", 0x00000050811c2509, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3ISPM03100KPB_2      ] = { 0x127e, { "I3ISPM03100KPB              ", 0x00000050831c2449, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3ISPM03100KPB_USB2_2 ] = { 0x127f, { "I3ISPM03100KPB(USB2.0)      ", 0x00000050831c2509, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3CMOS05000KMB        ] = { 0x125e, { "I3CMOS05000KMB              ", 0x0000005081182459, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3CMOS05000KMB_USB2   ] = { 0x125f, { "I3CMOS05000KMB(USB2.0)      ", 0x0000005081182519, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3CMOS05000KMB_2      ] = { 0x1276, { "I3CMOS05000KMB              ", 0x0000005083182459, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3CMOS05000KMB_USB2_2 ] = { 0x1277, { "I3CMOS05000KMB(USB2.0)      ", 0x0000005083182519, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2448, 2048},{1224, 1024},}}},
    [TOUPCAM_MODEL_I3CMOS03100KMB        ] = { 0x125c, { "I3CMOS03100KMB              ", 0x0000005081182459, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3CMOS03100KMB_USB2   ] = { 0x125d, { "I3CMOS03100KMB(USB2.0)      ", 0x0000005081182519, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3CMOS03100KMB_2      ] = { 0x1272, { "I3CMOS03100KMB              ", 0x0000005083182459, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_I3CMOS03100KMB_USB2_2 ] = { 0x1273, { "I3CMOS03100KMB(USB2.0)      ", 0x0000005083182519, 9, 2, 0, 0, 3, 3.450000, 3.450000, {{2048, 1536},{1024, 768},}}},
    [TOUPCAM_MODEL_E3ISPM21000KPA        ] = { 0x1256, { "E3ISPM21000KPA              ", 0x0000000085042049, 3, 5, 5, 0, 0, 3.300000, 3.300000, {{5280, 3954},{3952, 3952},{2640, 1976},{1760, 1316},{584, 438},}}},
    [TOUPCAM_MODEL_E3ISPM21000KPA_USB2   ] = { 0x1257, { "E3ISPM21000KPA(USB2.0)      ", 0x0000000085042109, 3, 5, 5, 0, 0, 3.300000, 3.300000, {{5280, 3954},{3952, 3952},{2640, 1976},{1760, 1316},{584, 438},}}},
    [TOUPCAM_MODEL_ECMOS05100KPA         ] = { 0x128c, { "ECMOS05100KPA               ", 0x0000000080000009, 2, 3, 3, 0, 0, 2.400000, 2.400000, {{2592, 1944},{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_E3ISPM15600KPA        ] = { 0x128d, { "E3ISPM15600KPA              ", 0x0000000085042049, 3, 3, 3, 0, 0, 3.300000, 3.300000, {{3952, 3952},{1976, 1976},{1316, 1316},}}},
    [TOUPCAM_MODEL_E3ISPM15600KPA_USB2   ] = { 0x128e, { "E3ISPM15600KPA(USB2.0)      ", 0x0000000085042109, 3, 3, 3, 0, 0, 3.300000, 3.300000, {{3952, 3952},{1976, 1976},{1316, 1316},}}},
    [TOUPCAM_MODEL_I3CMOS00500KMA        ] = { 0x1264, { "I3CMOS00500KMA              ", 0x0000005081182459, 9, 1, 0, 0, 4, 9.000000, 9.000000, {{800, 620},}}},
    [TOUPCAM_MODEL_I3CMOS00500KMA_USB2   ] = { 0x1265, { "I3CMOS00500KMA(USB2.0)      ", 0x0000005081182519, 9, 1, 0, 0, 4, 9.000000, 9.000000, {{800, 620},}}},
    [TOUPCAM_MODEL_I3ISPM00500KPA        ] = { 0x1266, { "I3ISPM00500KPA              ", 0x00000050811c2449, 9, 1, 0, 0, 4, 9.000000, 9.000000, {{800, 620},}}},
    [TOUPCAM_MODEL_I3ISPM00500KPA_USB2   ] = { 0x1267, { "I3ISPM00500KPA(USB2.0)      ", 0x00000050811c2509, 9, 1, 0, 0, 4, 9.000000, 9.000000, {{800, 620},}}},
    [TOUPCAM_MODEL_IUA6300KPA            ] = { 0x1221, { "IUA6300KPA                  ", 0x00000040831c2049, 9, 2, 0, 0, 4, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_IUA6300KPA_USB2       ] = { 0x1222, { "IUA6300KPA(USB2.0)          ", 0x00000040831c2109, 9, 2, 0, 0, 4, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_IUB4200KMB            ] = { 0x122b, { "IUB4200KMB                  ", 0x0000000887182059, 3, 2, 0, 0, 4, 6.500000, 6.500000, {{2048, 2046},{1024, 1022},}}},
    [TOUPCAM_MODEL_IUB4200KMB_USB2       ] = { 0x122c, { "IUB4200KMB(USB2.0)          ", 0x0000000887182119, 3, 2, 0, 0, 4, 6.500000, 6.500000, {{2048, 2046},{1024, 1022},}}},
    [TOUPCAM_MODEL_IUC31000KMA           ] = { 0x122f, { "IUC31000KMA                 ", 0x0000005083182459, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{6464, 4852},{3216, 2426},}}},
    [TOUPCAM_MODEL_IUC31000KMA_USB2      ] = { 0x1230, { "IUC31000KMA(USB2.0)         ", 0x0000005083182519, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{6464, 4852},{3216, 2426},}}},
    [TOUPCAM_MODEL_IUA6300KMA            ] = { 0x1223, { "IUA6300KMA                  ", 0x0000004083182059, 9, 2, 0, 0, 4, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_IUA6300KMA_USB2       ] = { 0x1224, { "IUA6300KMA(USB2.0)          ", 0x0000004083182119, 9, 2, 0, 0, 4, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_IUA1700KPA            ] = { 0x1288, { "IUA1700KPA                  ", 0x00000450831c2449, 9, 1, 0, 0, 4, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_IUA1700KPA_USB2       ] = { 0x1289, { "IUA1700KPA(USB2.0)          ", 0x00000450831c2509, 9, 1, 0, 0, 4, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_IUA7100KPA            ] = { 0x1284, { "IUA7100KPA                  ", 0x00000450831c2449, 9, 2, 0, 0, 4, 4.500000, 4.500000, {{3200, 2200},{1584, 1100},}}},
    [TOUPCAM_MODEL_IUA7100KPA_USB2       ] = { 0x1285, { "IUA7100KPA(USB2.0)          ", 0x00000450831c2509, 9, 2, 0, 0, 4, 4.500000, 4.500000, {{3200, 2200},{1584, 1100},}}},
    [TOUPCAM_MODEL_IUC31000KPA           ] = { 0x1231, { "IUC31000KPA                 ", 0x00000050831c2449, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{6464, 4852},{3216, 2426},}}},
    [TOUPCAM_MODEL_IUC31000KPA_USB2      ] = { 0x1232, { "IUC31000KPA(USB2.0)         ", 0x00000050831c2509, 9, 2, 0, 0, 4, 3.450000, 3.450000, {{6464, 4852},{3216, 2426},}}},
    [TOUPCAM_MODEL_ATR3CMOS21000KPA      ] = { 0x1258, { "ATR3CMOS21000KPA            ", 0x00000000876b24c9, 3, 5, 0, 1, 0, 3.300000, 3.300000, {{5280, 3956},{3952, 3952},{2640, 1978},{1760, 1318},{584, 440},}}},
    [TOUPCAM_MODEL_ATR3CMOS21000KPA_USB2 ] = { 0x1259, { "ATR3CMOS21000KPA(USB2.0)    ", 0x00000000876b2589, 3, 5, 0, 1, 0, 3.300000, 3.300000, {{5280, 3956},{3952, 3952},{2640, 1978},{1760, 1318},{584, 440},}}},
    [TOUPCAM_MODEL_IUA20000KPA           ] = { 0x1225, { "IUA20000KPA                 ", 0x00000040831c2049, 9, 3, 0, 0, 4, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_IUA20000KPA_USB2      ] = { 0x1226, { "IUA20000KPA(USB2.0)         ", 0x00000040831c2109, 9, 3, 0, 0, 4, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_T3CMOS20000KPA        ] = { 0x1295, { "T3CMOS20000KPA              ", 0x00000000836f24c9, 2, 3, 3, 1, 0, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_T3CMOS20000KPA_USB2   ] = { 0x1296, { "T3CMOS20000KPA(USB2.0)      ", 0x00000000836f2589, 2, 3, 3, 1, 0, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_MTR3CMOS21000KPA      ] = { 0x125a, { "MTR3CMOS21000KPA            ", 0x00000000876b24c9, 3, 5, 0, 1, 0, 3.300000, 3.300000, {{5280, 3956},{3952, 3952},{2640, 1978},{1760, 1318},{584, 440},}}},
    [TOUPCAM_MODEL_MTR3CMOS21000KPA_USB2 ] = { 0x125b, { "MTR3CMOS21000KPA(USB2.0)    ", 0x00000000876b2589, 3, 5, 0, 1, 0, 3.300000, 3.300000, {{5280, 3956},{3952, 3952},{2640, 1978},{1760, 1318},{584, 440},}}},
    [TOUPCAM_MODEL_GPCMOS01200KPF        ] = { 0x129e, { "GPCMOS01200KPF              ", 0x0000000084482229, 2, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_I3CMOS06300KMA        ] = { 0x1268, { "I3CMOS06300KMA              ", 0x0000004083182059, 9, 2, 0, 0, 3, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_I3CMOS06300KMA_USB2   ] = { 0x1269, { "I3CMOS06300KMA(USB2.0)      ", 0x0000004083182119, 9, 2, 0, 0, 3, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_I3ISPM06300KPA        ] = { 0x126a, { "I3ISPM06300KPA              ", 0x00000040831c2049, 9, 2, 0, 0, 3, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_I3ISPM06300KPA_USB2   ] = { 0x126b, { "I3ISPM06300KPA(USB2.0)      ", 0x00000040831c2109, 9, 2, 0, 0, 3, 2.400000, 2.400000, {{3072, 2048},{1536, 1024},}}},
    [TOUPCAM_MODEL_IUA20000KMA           ] = { 0x1227, { "IUA20000KMA                 ", 0x0000004083182059, 9, 3, 0, 0, 4, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_IUA20000KMA_USB2      ] = { 0x1228, { "IUA20000KMA(USB2.0)         ", 0x0000004083182119, 9, 3, 0, 0, 4, 2.400000, 2.400000, {{5440, 3648},{2736, 1824},{1824, 1216},}}},
    [TOUPCAM_MODEL_IUB43000KMA           ] = { 0x122d, { "IUB43000KMA                 ", 0x0000000083182059, 3, 1, 0, 0, 4, 2.800000, 2.800000, {{7904, 5432},}}},
    [TOUPCAM_MODEL_IUB43000KMA_USB2      ] = { 0x122e, { "IUB43000KMA(USB2.0)         ", 0x0000000083182119, 3, 1, 0, 0, 4, 2.800000, 2.800000, {{7904, 5432},}}},
    [TOUPCAM_MODEL_IUC60000KMA           ] = { 0x128f, { "IUC60000KMA                 ", 0x0000004083188059, 9, 4, 4, 0, 4, 3.760000, 3.760000, {{9568, 6380},{4784, 3190},{3184, 2124},{1040, 706},}}},
    [TOUPCAM_MODEL_IUC60000KMA_USB2      ] = { 0x1290, { "IUC60000KMA(USB2.0)         ", 0x0000004083188119, 9, 4, 4, 0, 4, 3.760000, 3.760000, {{9568, 6380},{4784, 3190},{3184, 2124},{1040, 706},}}},
    [TOUPCAM_MODEL_IUC60000KPA           ] = { 0x1291, { "IUC60000KPA                 ", 0x00000040831c8049, 9, 4, 4, 0, 4, 3.760000, 3.760000, {{9568, 6380},{4784, 3190},{3184, 2124},{1040, 706},}}},
    [TOUPCAM_MODEL_IUC60000KPA_USB2      ] = { 0x1292, { "IUC60000KPA(USB2.0)         ", 0x00000040831c8109, 9, 4, 4, 0, 4, 3.760000, 3.760000, {{9568, 6380},{4784, 3190},{3184, 2124},{1040, 706},}}},
    [TOUPCAM_MODEL_IUA2300KPB            ] = { 0x12a5, { "IUA2300KPB                  ", 0x00000050831c2049, 9, 1, 0, 0, 4, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_IUA2300KPB_USB2       ] = { 0x12a6, { "IUA2300KPB(USB2.0)          ", 0x00000050831c2109, 9, 1, 0, 0, 4, 5.860000, 5.860000, {{1920, 1200},}}},
    [TOUPCAM_MODEL_IUC26000KPA           ] = { 0x1293, { "IUC26000KPA                 ", 0x00000040831c8049, 9, 3, 3, 0, 4, 3.760000, 3.760000, {{6224, 4168},{3104, 2084},{2064, 1386},}}},
    [TOUPCAM_MODEL_IUC26000KPA_USB2      ] = { 0x1294, { "IUC26000KPA(USB2.0)         ", 0x00000040831c8109, 9, 3, 3, 0, 4, 3.760000, 3.760000, {{6224, 4168},{3104, 2084},{2064, 1386},}}},
    [TOUPCAM_MODEL_MTR3CMOS45000KMA      ] = { 0x12ab, { "MTR3CMOS45000KMA            ", 0x00000000834b24d9, 2, 2, 0, 1, 0, 2.315000, 2.315000, {{8256, 5616},{4128, 2808},}}},
    [TOUPCAM_MODEL_MTR3CMOS45000KMA_USB2 ] = { 0x12ac, { "MTR3CMOS45000KMA(USB2.0)    ", 0x00000000834b2599, 2, 2, 0, 1, 0, 2.315000, 2.315000, {{8256, 5616},{4128, 2808},}}},
    [TOUPCAM_MODEL_C3CMOS05100KPB        ] = { 0x12ad, { "C3CMOS05100KPB              ", 0x0000000081000049, 2, 2, 2, 0, 0, 2.000000, 2.000000, {{2592, 1944},{1296, 972},}}},
    [TOUPCAM_MODEL_C3CMOS05100KPB_USB2   ] = { 0x12ae, { "C3CMOS05100KPB(USB2.0)      ", 0x0000000081000109, 2, 2, 2, 0, 0, 2.000000, 2.000000, {{2592, 1944},{1296, 972},}}},
    [TOUPCAM_MODEL_C3CMOS03500KPA        ] = { 0x1239, { "C3CMOS03500KPA              ", 0x0000000081000041, 2, 2, 2, 0, 0, 2.500000, 2.500000, {{2304, 1536},{1152, 768},}}},
    [TOUPCAM_MODEL_C3CMOS03500KPA_USB2   ] = { 0x123a, { "C3CMOS03500KPA(USB2.0)      ", 0x0000000081000101, 2, 2, 2, 0, 0, 2.500000, 2.500000, {{2304, 1536},{1152, 768},}}},
    [TOUPCAM_MODEL_MAX60000KPA           ] = { 0x12b1, { "MAX60000KPA                 ", 0x000000c0835f84c9, 9, 4, 4, 4, 4, 3.760000, 3.760000, {{9568, 6380},{4784, 3190},{3184, 2124},{1040, 706},}}},
    [TOUPCAM_MODEL_MAX60000KPA_USB2      ] = { 0x12b2, { "MAX60000KPA(USB2.0)         ", 0x000000c0835f8589, 9, 4, 4, 4, 4, 3.760000, 3.760000, {{9568, 6380},{4784, 3190},{3184, 2124},{1040, 706},}}},
    [TOUPCAM_MODEL_BIGEYE4200KMB_2       ] = { 0x12af, { "BigEye4200KMB               ", 0x0000000887182051, 3, 2, 0, 0, 4, 6.500000, 6.500000, {{2048, 2048},{1024, 1024},}}},
    [TOUPCAM_MODEL_BIGEYE4200KMB_USB2_2  ] = { 0x12b0, { "BigEye4200KMB(USB2.0)       ", 0x0000000887182111, 3, 2, 0, 0, 4, 6.500000, 6.500000, {{2048, 2048},{1024, 1024},}}},
    [TOUPCAM_MODEL_IUA1700KMA            ] = { 0x128a, { "IUA1700KMA                  ", 0x0000045083182459, 9, 1, 0, 0, 4, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_IUA1700KMA_USB2       ] = { 0x128b, { "IUA1700KMA(USB2.0)          ", 0x0000045083182519, 9, 1, 0, 0, 4, 9.000000, 9.000000, {{1600, 1100},}}},
    [TOUPCAM_MODEL_IUA7100KMA            ] = { 0x1286, { "IUA7100KMA                  ", 0x0000045083182459, 9, 2, 0, 0, 4, 4.500000, 4.500000, {{3200, 2200},{1584, 1100},}}},
    [TOUPCAM_MODEL_IUA7100KMA_USB2       ] = { 0x1287, { "IUA7100KMA(USB2.0)          ", 0x0000045083182519, 9, 2, 0, 0, 4, 4.500000, 4.500000, {{3200, 2200},{1584, 1100},}}},
    [TOUPCAM_MODEL_ATR3CMOS26000KPA      ] = { 0x12b7, { "ATR3CMOS26000KPA            ", 0x00000180876b84c9, 2, 3, 0, 1, 0, 3.760000, 3.760000, {{6224, 4168},{3104, 2084},{2064, 1386},}}},
    [TOUPCAM_MODEL_ATR3CMOS26000KPA_USB2 ] = { 0x12b8, { "ATR3CMOS26000KPA(USB2.0)    ", 0x00000180876b8589, 2, 3, 0, 1, 0, 3.760000, 3.760000, {{6224, 4168},{3104, 2084},{2064, 1386},}}},
    [TOUPCAM_MODEL_E3ISPM09000KPB        ] = { 0x12c7, { "E3ISPM09000KPB              ", 0x0000010087044069, 3, 3, 3, 0, 0, 3.760000, 3.760000, {{3008, 3000},{1488, 1500},{992, 998},}}},
    [TOUPCAM_MODEL_E3ISPM09000KPB_USB2   ] = { 0x12c8, { "E3ISPM09000KPB(USB2.0)      ", 0x0000010087044129, 3, 3, 3, 0, 0, 3.760000, 3.760000, {{3008, 3000},{1488, 1500},{992, 998},}}},
    [TOUPCAM_MODEL_ATR3CMOS09000KPA      ] = { 0x12bb, { "ATR3CMOS09000KPA            ", 0x00000180876b44c9, 2, 3, 0, 1, 0, 3.760000, 3.760000, {{3008, 3000},{1488, 1500},{992, 998},}}},
    [TOUPCAM_MODEL_ATR3CMOS09000KPA_USB2 ] = { 0x12bc, { "ATR3CMOS09000KPA(USB2.0)    ", 0x00000180876b4589, 2, 3, 0, 1, 0, 3.760000, 3.760000, {{3008, 3000},{1488, 1500},{992, 998},}}},
    [TOUPCAM_MODEL_G3CMOS21000KPA        ] = { 0x12cd, { "G3CMOS21000KPA              ", 0x0000000087692649, 3, 5, 0, 1, 0, 3.300000, 3.300000, {{5280, 3956},{3952, 3952},{2640, 1978},{1760, 1318},{584, 440},}}},
    [TOUPCAM_MODEL_G3CMOS21000KPA_USB2   ] = { 0x12ce, { "G3CMOS21000KPA(USB2.0)      ", 0x0000000087692709, 3, 5, 0, 1, 0, 3.300000, 3.300000, {{5280, 3956},{3952, 3952},{2640, 1978},{1760, 1318},{584, 440},}}},
    [TOUPCAM_MODEL_ITR3CMOS16000KMA      ] = { 0x12d4, { "ITR3CMOS16000KMA            ", 0x00000000832b24d9, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_ITR3CMOS16000KMA_USB2 ] = { 0x12d5, { "ITR3CMOS16000KMA(USB2.0)    ", 0x00000000832b2599, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_MTR3CMOS26000KPA      ] = { 0x12b9, { "MTR3CMOS26000KPA            ", 0x00000180876f84c9, 2, 3, 3, 1, 0, 3.760000, 3.760000, {{6224, 4168},{3104, 2084},{2064, 1386},}}},
    [TOUPCAM_MODEL_MTR3CMOS26000KPA_USB2 ] = { 0x12ba, { "MTR3CMOS26000KPA(USB2.0)    ", 0x00000180876f8589, 2, 3, 3, 1, 0, 3.760000, 3.760000, {{6224, 4168},{3104, 2084},{2064, 1386},}}},
    [TOUPCAM_MODEL_BIGEYE4200KMB_2_2     ] = { 0x12d6, { "BigEye4200KMB               ", 0x0000020887182059, 3, 2, 0, 0, 4, 6.500000, 6.500000, {{2048, 2048},{1024, 1024},}}},
    [TOUPCAM_MODEL_BIGEYE4200KMB_USB2_2_2] = { 0x12d7, { "BigEye4200KMB(USB2.0)       ", 0x0000020887182119, 3, 2, 0, 0, 4, 6.500000, 6.500000, {{2048, 2048},{1024, 1024},}}},
    [TOUPCAM_MODEL_SKYEYE62AM            ] = { 0x12e2, { "SkyEye62AM                  ", 0x00000180876b84d9, 2, 3, 0, 1, 0, 3.760000, 3.760000, {{9568, 6380},{4784, 3190},{3184, 2124},}}},
    [TOUPCAM_MODEL_SKYEYE62AM_USB2       ] = { 0x12e3, { "SkyEye62AM(USB2.0)          ", 0x00000180876b8599, 2, 3, 0, 1, 0, 3.760000, 3.760000, {{9568, 6380},{4784, 3190},{3184, 2124},}}},
    [TOUPCAM_MODEL_MTR3CMOS09000KPA      ] = { 0x12bd, { "MTR3CMOS09000KPA            ", 0x00000180876f44c9, 2, 3, 3, 1, 0, 3.760000, 3.760000, {{2992, 3000},{1488, 1500},{992, 998},}}},
    [TOUPCAM_MODEL_MTR3CMOS09000KPA_USB2 ] = { 0x12be, { "MTR3CMOS09000KPA(USB2.0)    ", 0x00000180876f4589, 2, 3, 3, 1, 0, 3.760000, 3.760000, {{2992, 3000},{1488, 1500},{992, 998},}}},
    [TOUPCAM_MODEL_BIGEYE4200KMD         ] = { 0x12f4, { "BigEye4200KMD               ", 0x0000000887188059, 3, 2, 0, 0, 4, 6.500000, 6.500000, {{2048, 2048},{1024, 1024},}}},
    [TOUPCAM_MODEL_BIGEYE4200KMD_USB2    ] = { 0x12f5, { "BigEye4200KMD(USB2.0)       ", 0x0000000887188119, 3, 2, 0, 0, 4, 6.500000, 6.500000, {{2048, 2048},{1024, 1024},}}},
    [TOUPCAM_MODEL_IUA4100KPA            ] = { 0x12d8, { "IUA4100KPA                  ", 0x00000040871c2449, 9, 1, 0, 0, 4, 2.900000, 2.900000, {{2688, 1520},}}},
    [TOUPCAM_MODEL_IUA4100KPA_USB2       ] = { 0x12d9, { "IUA4100KPA(USB2.0)          ", 0x00000040871c2509, 9, 1, 0, 0, 4, 2.900000, 2.900000, {{2688, 1520},}}},
    [TOUPCAM_MODEL_IUA2100KPA            ] = { 0x12da, { "IUA2100KPA                  ", 0x00000040871c2449, 9, 1, 0, 0, 4, 2.900000, 2.900000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_IUA2100KPA_USB2       ] = { 0x12db, { "IUA2100KPA(USB2.0)          ", 0x00000040871c2509, 9, 1, 0, 0, 4, 2.900000, 2.900000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_E3CMOS45000KMA        ] = { 0x12e6, { "E3CMOS45000KMA              ", 0x0000000083002459, 2, 2, 2, 0, 0, 2.315000, 2.315000, {{8256, 5616},{4128, 2808},}}},
    [TOUPCAM_MODEL_E3CMOS45000KMA_USB2   ] = { 0x12e7, { "E3CMOS45000KMA(USB2.0)      ", 0x0000000083002519, 2, 2, 2, 0, 0, 2.315000, 2.315000, {{8256, 5616},{4128, 2808},}}},
    [TOUPCAM_MODEL_SKYEYE62AC            ] = { 0x12e0, { "SkyEye62AC                  ", 0x00000180876b84c9, 2, 3, 0, 1, 0, 3.760000, 3.760000, {{9568, 6380},{4784, 3190},{3184, 2124},}}},
    [TOUPCAM_MODEL_SKYEYE62AC_USB2       ] = { 0x12e1, { "SkyEye62AC(USB2.0)          ", 0x00000180876b8589, 2, 3, 0, 1, 0, 3.760000, 3.760000, {{9568, 6380},{4784, 3190},{3184, 2124},}}},
    [TOUPCAM_MODEL_SKYEYE24AC            ] = { 0x12e4, { "SkyEye24AC                  ", 0x00000080876b44c9, 2, 3, 0, 1, 0, 5.940000, 5.940000, {{6064, 4040},{3024, 2012},{2016, 1342},}}},
    [TOUPCAM_MODEL_SKYEYE24AC_USB2       ] = { 0x12e5, { "SkyEye24AC(USB2.0)          ", 0x00000080876b4589, 2, 3, 0, 1, 0, 5.940000, 5.940000, {{6064, 4040},{3024, 2012},{2016, 1342},}}},
    [TOUPCAM_MODEL_MAX62AM               ] = { 0x12ea, { "MAX62AM                     ", 0x00000180875f84d9, 2, 4, 4, 4, 4, 3.760000, 3.760000, {{9568, 6380},{4784, 3190},{3184, 2124},{1040, 706},}}},
    [TOUPCAM_MODEL_MAX62AM_USB2          ] = { 0x12eb, { "MAX62AM(USB2.0)             ", 0x00000180875f8599, 2, 4, 4, 4, 4, 3.760000, 3.760000, {{9568, 6380},{4784, 3190},{3184, 2124},{1040, 706},}}},
    [TOUPCAM_MODEL_MTR3CMOS08300KPA      ] = { 0x12f6, { "MTR3CMOS08300KPA            ", 0x00000000876f24c9, 2, 2, 2, 1, 0, 2.900000, 2.900000, {{3840, 2160},{1920, 1080},}}},
    [TOUPCAM_MODEL_MTR3CMOS08300KPA_USB2 ] = { 0x12f7, { "MTR3CMOS08300KPA(USB2.0)    ", 0x00000000876f2589, 2, 2, 2, 1, 0, 2.900000, 2.900000, {{3840, 2160},{1920, 1080},}}},
    [TOUPCAM_MODEL_G3M462C               ] = { 0x12de, { "G3M462C                     ", 0x0000000085482649, 2, 1, 0, 0, 0, 2.900000, 2.900000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_G3M462C_USB2          ] = { 0x12df, { "G3M462C(USB2.0)             ", 0x0000000085482709, 2, 1, 0, 0, 0, 2.900000, 2.900000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_E3ISPM08300KPC        ] = { 0x12cb, { "E3ISPM08300KPC              ", 0x0000000085042449, 2, 2, 2, 0, 0, 2.900000, 2.900000, {{3840, 2160},{1920, 1080},}}},
    [TOUPCAM_MODEL_E3ISPM08300KPC_USB2   ] = { 0x12cc, { "E3ISPM08300KPC(USB2.0)      ", 0x0000000085042509, 2, 2, 2, 0, 0, 2.900000, 2.900000, {{3840, 2160},{1920, 1080},}}},
    [TOUPCAM_MODEL_BIGEYE4200KMA_2       ] = { 0x12fe, { "BigEye4200KMA               ", 0x0000020887182059, 3, 2, 0, 0, 4, 6.500000, 6.500000, {{2048, 2048},{1024, 1024},}}},
    [TOUPCAM_MODEL_BIGEYE4200KMA_USB2_2  ] = { 0x12ff, { "BigEye4200KMA(USB2.0)       ", 0x0000020887182119, 3, 2, 0, 0, 4, 6.500000, 6.500000, {{2048, 2048},{1024, 1024},}}},
    [TOUPCAM_MODEL_ATR3CMOS10300KMA      ] = { 0x12cf, { "ATR3CMOS10300KMA            ", 0x00000000876b44d9, 2, 4, 0, 1, 0, 4.630000, 4.630000, {{4128, 2808},{8184, 5616},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_ATR3CMOS10300KMA_USB2 ] = { 0x12d0, { "ATR3CMOS10300KMA(USB2.0)    ", 0x00000000876b4599, 2, 4, 0, 1, 0, 4.630000, 4.630000, {{4128, 2808},{8184, 5616},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_E3ISPM45000KPA        ] = { 0x1300, { "E3ISPM45000KPA              ", 0x0000000083042449, 2, 8, 8, 0, 0, 4.630000, 4.630000, {{8176, 5616},{4088, 2808},{7408, 5556},{3704, 2778},{8176, 4320},{4096, 2160},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_E3ISPM45000KPA_USB2   ] = { 0x1301, { "E3ISPM45000KPA(USB2.0)      ", 0x0000000083042509, 2, 8, 8, 0, 0, 4.630000, 4.630000, {{8176, 5616},{4088, 2808},{7408, 5556},{3704, 2778},{8176, 4320},{4096, 2160},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_E3ISPM02100KPA        ] = { 0x1304, { "E3ISPM02100KPA              ", 0x0000000085042449, 2, 1, 0, 0, 0, 5.800000, 5.800000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_E3ISPM02100KPA_USB2   ] = { 0x1305, { "E3ISPM02100KPA(USB2.0)      ", 0x0000000085042509, 2, 1, 0, 0, 0, 5.800000, 5.800000, {{1920, 1080},}}},
    [TOUPCAM_MODEL_G3CMOS08300KPA        ] = { 0x1302, { "G3CMOS08300KPA              ", 0x0000000087692649, 2, 2, 0, 1, 0, 2.900000, 2.900000, {{3840, 2160},{1920, 1080},}}},
    [TOUPCAM_MODEL_G3CMOS08300KPA_USB2   ] = { 0x1303, { "G3CMOS08300KPA(USB2.0)      ", 0x0000000087692709, 2, 2, 0, 1, 0, 2.900000, 2.900000, {{3840, 2160},{1920, 1080},}}},
    [TOUPCAM_MODEL_BIGEYE4200KME         ] = { 0x12fc, { "BigEye4200KME               ", 0x0000020887182059, 3, 2, 0, 0, 4, 11.000000, 11.000000, {{2048, 2048},{1024, 1024},}}},
    [TOUPCAM_MODEL_BIGEYE4200KME_USB2    ] = { 0x12fd, { "BigEye4200KME(USB2.0)       ", 0x0000020887182119, 3, 2, 0, 0, 4, 11.000000, 11.000000, {{2048, 2048},{1024, 1024},}}},
    [TOUPCAM_MODEL_ITR3CMOS08300KPA      ] = { 0x1310, { "ITR3CMOS08300KPA            ", 0x00000400875f24c9, 2, 2, 2, 1, 4, 2.900000, 2.900000, {{3840, 2160},{1920, 1080},}}},
    [TOUPCAM_MODEL_ITR3CMOS08300KPA_USB2 ] = { 0x1311, { "ITR3CMOS08300KPA(USB2.0)    ", 0x00000400875f2589, 2, 2, 2, 1, 4, 2.900000, 2.900000, {{3840, 2160},{1920, 1080},}}},
    [TOUPCAM_MODEL_MAX04AM               ] = { 0x12ee, { "MAX04AM                     ", 0x00000088871b24d9, 3, 2, 0, 4, 4, 6.500000, 6.500000, {{2048, 2048},{1024, 1024},}}},
    [TOUPCAM_MODEL_MAX04AM_USB2          ] = { 0x12ef, { "MAX04AM(USB2.0)             ", 0x00000088871b2599, 3, 2, 0, 4, 4, 6.500000, 6.500000, {{2048, 2048},{1024, 1024},}}},
    [TOUPCAM_MODEL_G3M287C               ] = { 0x1176, { "G3M287C                     ", 0x0000001081482649, 2, 1, 0, 0, 0, 6.900000, 6.900000, {{720, 540},}}},
    [TOUPCAM_MODEL_G3M287C_USB2          ] = { 0x1177, { "G3M287C(USB2.0)             ", 0x0000001081482709, 2, 1, 0, 0, 0, 6.900000, 6.900000, {{720, 540},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KPB      ] = { 0x11e0, { "ATR3CMOS16000KPB            ", 0x00000000836b44e1, 2, 2, 0, 1, 0, 4.780000, 4.780000, {{4944, 3260},{1640, 1060},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KPB_USB2 ] = { 0x11e1, { "ATR3CMOS16000KPB(USB2.0)    ", 0x00000000836b45a1, 2, 2, 0, 1, 0, 4.780000, 4.780000, {{4944, 3260},{1640, 1060},}}},
    [TOUPCAM_MODEL_ATR3CMOS10300KPA_2    ] = { 0x121a, { "ATR3CMOS10300KPA            ", 0x00000000876b44c9, 2, 4, 0, 1, 0, 4.630000, 4.630000, {{4128, 2808},{4096, 2160},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_ATR3CMOS10300KPA_USB2_2] = { 0x121b, { "ATR3CMOS10300KPA(USB2.0)    ", 0x00000000876b4589, 2, 4, 0, 1, 0, 4.630000, 4.630000, {{4128, 2808},{4096, 2160},{2048, 1080},{1360, 720},}}},
    [TOUPCAM_MODEL_L3CMOS08500KPA        ] = { 0xc850, { "L3CMOS08500KPA              ", 0x0000000081000061, 3, 2, 2, 0, 0, 1.670000, 1.670000, {{3328, 2548},{1664, 1272},}}},
    [TOUPCAM_MODEL_L3CMOS08500KPA_USB2   ] = { 0xd850, { "L3CMOS08500KPA(USB2.0)      ", 0x0000000081000121, 3, 2, 2, 0, 0, 1.670000, 1.670000, {{3328, 2548},{1664, 1272},}}},
};
#endif

#define XP(x) Toupcam##x

struct toupcam_model_pid {
    uint16_t pid;
    XP(ModelV2) model;
};

enum {
    TOUPCAM_MODEL_ATR3CMOS16000KMA_2,
    TOUPCAM_MODEL_ATR3CMOS16000KMA_USB2_2,
    TOUPCAM_MODEL_ATR3CMOS16000KPA_2,
    TOUPCAM_MODEL_ATR3CMOS16000KPA_USB2_2,
    TOUPCAM_MODEL_ATR3CMOS16000KMA,
    TOUPCAM_MODEL_ATR3CMOS16000KMA_USB2,
    TOUPCAM_MODEL_ATR3CMOS16000KPA,
    TOUPCAM_MODEL_ATR3CMOS16000KPA_USB2,
    TOUPCAM_MODEL_G3M178M,
    TOUPCAM_MODEL_G3M178M_USB2,
    TOUPCAM_MODEL_G3M178C,
    TOUPCAM_MODEL_G3M178C_USB2,
    TOUPCAM_MODEL_G3M178M_2,
    TOUPCAM_MODEL_G3M178M_USB2_2,
    TOUPCAM_MODEL_G3M178C_2,
    TOUPCAM_MODEL_G3M178C_USB2_2,
    TOUPCAM_MODEL_GPCMOS01200KMB,
    TOUPCAM_MODEL_GPCMOS01200KPB,
};
static const struct toupcam_model_pid toupcam_models[] = {
    [TOUPCAM_MODEL_ATR3CMOS16000KMA_2    ] = { 0x11f6, { "DSI IV Mono", 0x00000000836b24d9, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KMA_USB2_2] = { 0x11f7, { "DSI IV Mono (USB2.0)", 0x00000000836b2599, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KPA_2    ] = { 0x11ea, { "DSI IV Color", 0x00000000836b24c9, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KPA_USB2_2] = { 0x11eb, { "DSI IV Color (USB2.0)", 0x00000000836b2589, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KMA      ] = { 0x106d, { "DSI IV Mono", 0x00000000816b24d9, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KMA_USB2 ] = { 0x1076, { "DSI IV Mono (USB2.0)", 0x00000000816b2599, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KPA      ] = { 0x106b, { "DSI IV Color", 0x00000000816b24c9, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_ATR3CMOS16000KPA_USB2 ] = { 0x1075, { "DSI IV Color (USB2.0)", 0x00000000816b2589, 3, 3, 0, 1, 0, 3.800000, 3.800000, {{4640, 3506},{2304, 1750},{1536, 1160},}}},
    [TOUPCAM_MODEL_G3M178M               ] = { 0x11cc, { "LPI-GM Advanced", 0x0000000081484659, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3M178M_USB2          ] = { 0x11cd, { "LPI-GM Advanced (USB2.0)", 0x0000000081484719, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3M178C               ] = { 0x11ca, { "LPI-GC Advanced", 0x0000000081484649, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3M178C_USB2          ] = { 0x11cb, { "LPI-GC Advanced (USB2.0)", 0x0000000081484709, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3M178M_2             ] = { 0x115c, { "LPI-GM Advanced", 0x0000000081484259, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3M178M_USB2_2        ] = { 0x115d, { "LPI-GM Advanced (USB2.0)", 0x0000000081484319, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3M178C_2             ] = { 0x115a, { "LPI-GC Advanced", 0x0000000081484249, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_G3M178C_USB2_2        ] = { 0x115b, { "LPI-GC Advanced (USB2.0)", 0x0000000081484309, 2, 2, 0, 0, 0, 2.400000, 2.400000, {{3040, 2048},{1520, 1024},}}},
    [TOUPCAM_MODEL_GPCMOS01200KMB        ] = { 0x1004, { "LPI-GM", 0x0000000080682219, 4, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
    [TOUPCAM_MODEL_GPCMOS01200KPB        ] = { 0x1003, { "LPI-GC", 0x0000000080682209, 4, 2, 0, 0, 0, 3.750000, 3.750000, {{1280, 960},{640, 480},}}},
};

struct oem_camera {
    const char *name; /* The OEM camera name */
    uint16_t vid; /* The OEM vendor ID */
    uint16_t pid; /* The OEM product ID */
    const struct toupcam_model_pid *toupcam; /* Pointer to the Toupcam equivalent */
};

static const struct oem_camera oem_cameras[] = {
    /* DSI IV */
    {
        .name = "Meade DSI IV Mono",
        .vid = 0x547,
        .pid = 0xe079,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_ATR3CMOS16000KMA_2],
    },
    {
        .name = "Meade DSI IV Mono (USB2.0)",
        .vid = 0x547,
        .pid = 0xe07a,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_ATR3CMOS16000KMA_USB2_2],
    },
    {
        .name = "Meade DSI IV Color",
        .vid = 0x547,
        .pid = 0xe077,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_ATR3CMOS16000KPA_2],
    },
    {
        .name = "Meade DSI IV Color (USB2.0)",
        .vid = 0x547,
        .pid = 0xe078,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_ATR3CMOS16000KPA_USB2_2],
    },
    /* DSI IV without DDR buffer */
    {
        .name = "Meade DSI IV Mono",
        .vid = 0x547,
        .pid = 0xe06d,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_ATR3CMOS16000KMA],
    },
    {
        .name = "Meade DSI IV Mono (USB2.0)",
        .vid = 0x547,
        .pid = 0xe076,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_ATR3CMOS16000KMA_USB2],
    },
    {
        .name = "Meade DSI IV Color",
        .vid = 0x547,
        .pid = 0xe06b,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_ATR3CMOS16000KPA],
    },
    {
        .name = "Meade DSI IV Color (USB2.0)",
        .vid = 0x547,
        .pid = 0xe075,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_ATR3CMOS16000KPA_USB2],
    },
    /* LPI-G Advanced */
    {
        .name = "Meade LPI-GM Advanced",
        .vid = 0x547,
        .pid = 0xe00d,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_G3M178M],
    },
    {
        .name = "Meade LPI-GM Advanced (USB2.0)",
        .vid = 0x547,
        .pid = 0xe00e,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_G3M178M_USB2],
    },
    {
        .name = "Meade LPI-GC Advanced",
        .vid = 0x547,
        .pid = 0xe00b,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_G3M178C],
    },
    {
        .name = "Meade LPI-GC Advanced (USB2.0)",
        .vid = 0x547,
        .pid = 0xe00e,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_G3M178C_USB2],
    },
    /* LPI-G Advanced with temperature sensor */
    {
        .name = "Meade LPI-GM Advanced",
        .vid = 0x547,
        .pid = 0xe009,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_G3M178M_2],
    },
    {
        .name = "Meade LPI-GM Advanced (USB2.0)",
        .vid = 0x547,
        .pid = 0xe00a,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_G3M178M_USB2_2],
    },
    {
        .name = "Meade LPI-GC Advanced",
        .vid = 0x547,
        .pid = 0xe007,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_G3M178C_2],
    },
    {
        .name = "Meade LPI-GC Advanced (USB2.0)",
        .vid = 0x547,
        .pid = 0xe008,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_G3M178C_USB2_2],
    },
    /* LPI-G */
    {
        .name = "Meade LPI-GM",
        .vid = 0x549,
        .pid = 0xe004,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_GPCMOS01200KMB],
    },
    {
        .name = "Meade LPI-GC",
        .vid = 0x549,
        .pid = 0xe003,
        .toupcam = &toupcam_models[TOUPCAM_MODEL_GPCMOS01200KPB],
    }
};
#ifndef countof
#define countof(x) (sizeof(x)/sizeof(x[0]))
#endif
static const struct oem_camera *vid_pid_to_oem_cam(int vid, int pid)
{
    unsigned i;
    for (i = 0; i < countof(oem_cameras); i++) {
        if ((oem_cameras[i].vid == vid) &&
            (oem_cameras[i].pid == pid)) {
            return &oem_cameras[i];
        }
    }

    return NULL;
}

int OEMCamEnum(XP(DeviceV2) *cam_infos, int cam_infos_count)
{
    int cnt = 0;
    libusb_device **list;
    ssize_t usb_cnt;
    ssize_t i = 0;

    libusb_init(NULL);
    usb_cnt = libusb_get_device_list(NULL, &list);
    if (usb_cnt < 0)
        return 0;

    for (i = 0; (i < usb_cnt) && (cnt < cam_infos_count); i++) {
        libusb_device *dev = list[i];
        const struct oem_camera *cam;
        struct libusb_device_descriptor desc;

        libusb_get_device_descriptor(dev, &desc);
        cam = vid_pid_to_oem_cam(desc.idVendor, desc.idProduct);
        if (!cam)
            continue;

        cam_infos[cnt].model = &cam->toupcam->model;
        strcpy(cam_infos[cnt].displayname, cam->name);
        sprintf(cam_infos[cnt].id, "tp-%d-%d-%d-%d", libusb_get_bus_number(dev),
            libusb_get_device_address(dev), 0x547, cam->toupcam->pid);
        cnt++;
    }
    libusb_free_device_list(list, 1);

    return cnt;
}

/**
 * This is the entire point of this library. We need to find the Meade-branded
 * toupcams and enumerate them.
 */
TOUPCAM_API(unsigned) Toupcam_EnumV2(ToupcamDeviceV2 arr[TOUPCAM_MAX])
{
    return OEMCamEnum(arr, TOUPCAM_MAX);
}
