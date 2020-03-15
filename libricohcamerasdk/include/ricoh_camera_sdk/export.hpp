// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_EXPORT_HPP_
#define RICOH_CAMERA_SDK_EXPORT_HPP_

#ifdef _WIN32
#ifndef _NOT_USE_DLL
#pragma warning(disable:4251)
#ifdef WINDLL_EXPORTS
#define RCSDK_API __declspec(dllexport)
#else  // WINDLL_EXPORTS
#define RCSDK_API __declspec(dllimport)
#endif
#else  // _NOT_USE_DLL
#define RCSDK_API
#endif
#else  // _WIN32
#define RCSDK_API
#endif

#endif // RICOH_CAMERA_SDK_EXPORT_HPP_
