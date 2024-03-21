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
#include <ASICamera2.h>
#include <indibasetypes.h>

namespace Helpers
{

const char *toString(ASI_GUIDE_DIRECTION dir)
{
    switch (dir)
    {
    case ASI_GUIDE_NORTH: return "North";
    case ASI_GUIDE_SOUTH: return "South";
    case ASI_GUIDE_EAST:  return "East";
    case ASI_GUIDE_WEST:  return "West";
    default:              return "Unknown";
    }
}

const char *toString(ASI_BAYER_PATTERN pattern)
{
    switch (pattern)
    {
    case ASI_BAYER_BG: return "BGGR";
    case ASI_BAYER_GR: return "GRBG";
    case ASI_BAYER_GB: return "GBRG";
    default:           return "RGGB";
    }
}


const char *toString(ASI_ERROR_CODE code)
{
    switch (code)
    {
    case ASI_SUCCESS:                      return "ASI_SUCCESS";
    case ASI_ERROR_INVALID_INDEX:          return "ASI_ERROR_INVALID_INDEX";
    case ASI_ERROR_INVALID_ID:             return "ASI_ERROR_INVALID_ID";
    case ASI_ERROR_INVALID_CONTROL_TYPE:   return "ASI_ERROR_INVALID_CONTROL_TYPE";
    case ASI_ERROR_CAMERA_CLOSED:          return "ASI_ERROR_CAMERA_CLOSED";
    case ASI_ERROR_CAMERA_REMOVED:         return "ASI_ERROR_CAMERA_REMOVED";
    case ASI_ERROR_INVALID_PATH:           return "ASI_ERROR_INVALID_PATH";
    case ASI_ERROR_INVALID_FILEFORMAT:     return "ASI_ERROR_INVALID_FILEFORMAT";
    case ASI_ERROR_INVALID_SIZE:           return "ASI_ERROR_INVALID_SIZE";
    case ASI_ERROR_INVALID_IMGTYPE:        return "ASI_ERROR_INVALID_IMGTYPE";
    case ASI_ERROR_OUTOF_BOUNDARY:         return "ASI_ERROR_OUTOF_BOUNDARY";
    case ASI_ERROR_TIMEOUT:                return "ASI_ERROR_TIMEOUT";
    case ASI_ERROR_INVALID_SEQUENCE:       return "ASI_ERROR_INVALID_SEQUENCE";
    case ASI_ERROR_BUFFER_TOO_SMALL:       return "ASI_ERROR_BUFFER_TOO_SMALL";
    case ASI_ERROR_VIDEO_MODE_ACTIVE:      return "ASI_ERROR_VIDEO_MODE_ACTIVE";
    case ASI_ERROR_EXPOSURE_IN_PROGRESS:   return "ASI_ERROR_EXPOSURE_IN_PROGRESS";
    case ASI_ERROR_GENERAL_ERROR:          return "ASI_ERROR_GENERAL_ERROR";
    case ASI_ERROR_INVALID_MODE:           return "ASI_ERROR_INVALID_MODE";
    case ASI_ERROR_GPS_NOT_SUPPORTED:      return "ASI_ERROR_GPS_NOT_SUPPORTED";
    case ASI_ERROR_GPS_VER_ERR:            return "ASI_ERROR_GPS_VER_ERR";
    case ASI_ERROR_GPS_FPGA_ERR:           return "ASI_ERROR_GPS_FPGA_ERR";
    case ASI_ERROR_GPS_PARAM_OUT_OF_RANGE: return "ASI_ERROR_GPS_PARAM_OUT_OF_RANGE";
    case ASI_ERROR_GPS_DATA_INVALID:       return "ASI_ERROR_GPS_DATA_INVALID";
    case ASI_ERROR_END:                    return "ASI_ERROR_END";
    }
    return "UNKNOWN";
}

const char *toString(ASI_IMG_TYPE type)
{
    switch (type)
    {
    case ASI_IMG_RAW8:   return "ASI_IMG_RAW8";
    case ASI_IMG_RGB24:  return "ASI_IMG_RGB24";
    case ASI_IMG_RAW16:  return "ASI_IMG_RAW16";
    case ASI_IMG_Y8:     return "ASI_IMG_Y8";
    case ASI_IMG_END:    return "ASI_IMG_END";
    default:             return "UNKNOWN";
    }
}

const char *toPrettyString(ASI_IMG_TYPE type)
{
    switch (type)
    {
    case ASI_IMG_RAW8:   return "Raw 8 bit";
    case ASI_IMG_RGB24:  return "RGB 24";
    case ASI_IMG_RAW16:  return "Raw 16 bit";
    case ASI_IMG_Y8:     return "Luma";
    case ASI_IMG_END:    return "END";
    default:             return "UNKNOWN";
    }
}

INDI_PIXEL_FORMAT pixelFormat(ASI_IMG_TYPE type, ASI_BAYER_PATTERN pattern, bool isColor)
{
    if (isColor == false)
        return INDI_MONO;

    switch (type)
    {
    case ASI_IMG_RGB24: return INDI_RGB;
    case ASI_IMG_Y8:    return INDI_MONO;
    default:;           // see below
    }

    switch (pattern)
    {
    case ASI_BAYER_RG: return INDI_BAYER_RGGB;
    case ASI_BAYER_BG: return INDI_BAYER_BGGR;
    case ASI_BAYER_GR: return INDI_BAYER_GRBG;
    case ASI_BAYER_GB: return INDI_BAYER_GBRG;
    }

    return INDI_MONO;
}

}
