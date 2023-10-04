/*
    ASI CCD Driver

    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2018 Leonard Bottleman (leonard@whiteweasel.net)
    Copyright (C) 2021 Pawel Soja (kernel32.pl@gmail.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once
#include <SVBCameraSDK.h>
#include <indibasetypes.h>

namespace Helpers
{

const char *toString(SVB_GUIDE_DIRECTION dir)
{
    switch (dir)
    {
    case SVB_GUIDE_NORTH: return "North";
    case SVB_GUIDE_SOUTH: return "South";
    case SVB_GUIDE_EAST:  return "East";
    case SVB_GUIDE_WEST:  return "West";
    default:              return "Unknown";
    }
}

const char *toString(SVB_BAYER_PATTERN pattern)
{
    switch (pattern)
    {
    case SVB_BAYER_BG: return "BGGR";
    case SVB_BAYER_GR: return "GRBG";
    case SVB_BAYER_GB: return "GBRG";
    case SVB_BAYER_RG: return "RGGB";
    default:           return "GRBG";  // default bayer pattern for SVBONY OSC camera.
    }
}


const char *toString(SVB_ERROR_CODE code)
{
    switch (code)
    {
    case SVB_SUCCESS:                    return "SVB_SUCCESS";
    case SVB_ERROR_INVALID_INDEX:        return "SVB_ERROR_INVALID_INDEX";
    case SVB_ERROR_INVALID_ID:           return "SVB_ERROR_INVALID_ID";
    case SVB_ERROR_INVALID_CONTROL_TYPE: return "SVB_ERROR_INVALID_CONTROL_TYPE";
    case SVB_ERROR_CAMERA_CLOSED:        return "SVB_ERROR_CAMERA_CLOSED";
    case SVB_ERROR_CAMERA_REMOVED:       return "SVB_ERROR_CAMERA_REMOVED";
    case SVB_ERROR_INVALID_PATH:         return "SVB_ERROR_INVALID_PATH";
    case SVB_ERROR_INVALID_FILEFORMAT:   return "SVB_ERROR_INVALID_FILEFORMAT";
    case SVB_ERROR_INVALID_SIZE:         return "SVB_ERROR_INVALID_SIZE";
    case SVB_ERROR_INVALID_IMGTYPE:      return "SVB_ERROR_INVALID_IMGTYPE";
    case SVB_ERROR_OUTOF_BOUNDARY:       return "SVB_ERROR_OUTOF_BOUNDARY";
    case SVB_ERROR_TIMEOUT:              return "SVB_ERROR_TIMEOUT";
    case SVB_ERROR_INVALID_SEQUENCE:     return "SVB_ERROR_INVALID_SEQUENCE";
    case SVB_ERROR_BUFFER_TOO_SMALL:     return "SVB_ERROR_BUFFER_TOO_SMALL";
    case SVB_ERROR_VIDEO_MODE_ACTIVE:    return "SVB_ERROR_VIDEO_MODE_ACTIVE";
    case SVB_ERROR_EXPOSURE_IN_PROGRESS: return "SVB_ERROR_EXPOSURE_IN_PROGRESS";
    case SVB_ERROR_GENERAL_ERROR:        return "SVB_ERROR_GENERAL_ERROR";
    case SVB_ERROR_INVALID_DIRECTION:    return "SVB_ERROR_INVALID_DIRECTION";
    case SVB_ERROR_UNKNOW_SENSOR_TYPE:   return "SVB_ERROR_UNKNOW_SENSOR_TYPE";
    case SVB_ERROR_INVALID_MODE:         return "SVB_ERROR_INVALID_MODE";
    case SVB_ERROR_END:                  return "SVB_ERROR_END";
    }
    return "UNKNOWN";
}

const char *toString(SVB_IMG_TYPE type)
{
    switch (type)
    {
    case SVB_IMG_RAW8:   return "SVB_IMG_RAW8";
	case SVB_IMG_RAW10:  return "SVB_IMG_RAW10";
	case SVB_IMG_RAW12:  return "SVB_IMG_RAW12";
	case SVB_IMG_RAW14:  return "SVB_IMG_RAW14";
    case SVB_IMG_RAW16:  return "SVB_IMG_RAW16";
    case SVB_IMG_RGB24:  return "SVB_IMG_RGB24";
	case SVB_IMG_RGB32:  return "SVB_IMG_RGB32";
    case SVB_IMG_Y8:     return "SVB_IMG_Y8";
    case SVB_IMG_Y16:    return "SVB_IMG_Y16";
    case SVB_IMG_END:    return "SVB_IMG_END";
    default:             return "UNKNOWN";
    }
}

const char *toPrettyString(SVB_IMG_TYPE type)
{
    switch (type)
    {
    case SVB_IMG_RAW8:   return "Raw 8 bit";
    case SVB_IMG_RAW10:  return "Raw 10 bit";
    case SVB_IMG_RAW12:  return "Raw 12 bit";
    case SVB_IMG_RAW14:  return "Raw 14 bit";
    case SVB_IMG_RAW16:  return "Raw 16 bit";
    case SVB_IMG_Y8:     return "Luma 8 bit";
    case SVB_IMG_Y16:    return "Luma 16 bit";
    case SVB_IMG_RGB24:  return "RGB 24";
    case SVB_IMG_RGB32:  return "RGB 32";
    case SVB_IMG_END:    return "END";
    default:             return "UNKNOWN";
    }
}

/*
    Determine the arguments in the following order:
        isColor: not color -> mono
        type: RGB* -> rgb, Y* -> mono 
        pattern: bayer pattern
        other -> mono
*/
INDI_PIXEL_FORMAT pixelFormat(SVB_IMG_TYPE type, SVB_BAYER_PATTERN pattern, bool isColor)
{
    if (isColor == false)
        return INDI_MONO;

    switch (type)
    {
    case SVB_IMG_RGB24: return INDI_RGB;
    case SVB_IMG_RGB32: return INDI_RGB;
    case SVB_IMG_Y8:    return INDI_MONO;
    case SVB_IMG_Y16:   return INDI_MONO;
    default:;           // see below
    }

    switch (pattern)
    {
    case SVB_BAYER_RG: return INDI_BAYER_RGGB;
    case SVB_BAYER_BG: return INDI_BAYER_BGGR;
    case SVB_BAYER_GR: return INDI_BAYER_GRBG;
    case SVB_BAYER_GB: return INDI_BAYER_GBRG;
    }

    return INDI_MONO;
}

int getBPP(SVB_IMG_TYPE type)
{
    switch (type)
    {
    case SVB_IMG_RAW8:   return 8;
	case SVB_IMG_RAW10:  return 16;
	case SVB_IMG_RAW12:  return 16;
	case SVB_IMG_RAW14:  return 16;
    case SVB_IMG_RAW16:  return 16;
    case SVB_IMG_RGB24:  return 8;
	case SVB_IMG_RGB32:  return 8;
    case SVB_IMG_Y8:     return 8;
    case SVB_IMG_Y16:    return 16;
    case SVB_IMG_END:    return 8;
    default:             return 8;
    }
}

int getNChannels(SVB_IMG_TYPE type)
{
    switch (type)
    {
    case SVB_IMG_RAW8:   return 1;
	case SVB_IMG_RAW10:  return 1;
	case SVB_IMG_RAW12:  return 1;
	case SVB_IMG_RAW14:  return 1;
    case SVB_IMG_RAW16:  return 1;
    case SVB_IMG_RGB24:  return 3;
	case SVB_IMG_RGB32:  return 4;
    case SVB_IMG_Y8:     return 1;
    case SVB_IMG_Y16:    return 1;
    case SVB_IMG_END:    return 1;
    default:             return 1;
    }
}

int getNAxis(SVB_IMG_TYPE type)
{
    switch (type)
    {
    case SVB_IMG_RAW8:   return 2;
	case SVB_IMG_RAW10:  return 2;
	case SVB_IMG_RAW12:  return 2;
	case SVB_IMG_RAW14:  return 2;
    case SVB_IMG_RAW16:  return 2;
    case SVB_IMG_RGB24:  return 3;
	case SVB_IMG_RGB32:  return 3;
    case SVB_IMG_Y8:     return 2;
    case SVB_IMG_Y16:    return 2;
    case SVB_IMG_END:    return 2;
    default:             return 2;
    }
}

bool isRGB(SVB_IMG_TYPE type)
{
    return (type == SVB_IMG_RGB24) || (type == SVB_IMG_RGB32);
}

bool isColor(SVB_IMG_TYPE type)
{
    switch (type)
    {
    case SVB_IMG_RAW8:
	case SVB_IMG_RAW10:
	case SVB_IMG_RAW12:
	case SVB_IMG_RAW14:
    case SVB_IMG_RAW16:
    case SVB_IMG_RGB24:
	case SVB_IMG_RGB32:
        return true;
    case SVB_IMG_Y8:
    case SVB_IMG_Y16:
    default:
        return false;
    }
}

bool hasBayer(SVB_IMG_TYPE type)
{
    switch (type)
    {
    case SVB_IMG_RAW8:
	case SVB_IMG_RAW10:
	case SVB_IMG_RAW12:
	case SVB_IMG_RAW14:
    case SVB_IMG_RAW16:
        return true;
    case SVB_IMG_RGB24:
	case SVB_IMG_RGB32:
    case SVB_IMG_Y8:
    case SVB_IMG_Y16:
    default:
        return false;
    }
}

}

