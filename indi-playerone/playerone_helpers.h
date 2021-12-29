/*
    PlayerOne CCD Driver

    Copyright (C) 2021 Hiroshi Saito (hiro3110g@gmail.com)

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
#include <PlayerOneCamera.h>
#include <indibasetypes.h>

namespace Helpers
{
const char *toString(POAConfig dir)
{
    switch (dir)
    {
    case POA_GUIDE_NORTH: return "North";
    case POA_GUIDE_SOUTH: return "South";
    case POA_GUIDE_EAST:  return "East";
    case POA_GUIDE_WEST:  return "West";
    default:              return "Unknown";
    }
}

const char *toString(POABayerPattern pattern)
{
    switch (pattern)
    {
    case POA_BAYER_BG: return "BGGR";
    case POA_BAYER_GR: return "GRBG";
    case POA_BAYER_GB: return "GBRG";
    default:           return "RGGB";
    }
}


const char *toString(POAErrors code)
{
    switch (code)
    {
    case POA_OK:                      return "POA_OK";
    case POA_ERROR_INVALID_INDEX:     return "POA_ERROR_INVALID_INDEX";
    case POA_ERROR_INVALID_ID:        return "POA_ERROR_INVALID_ID";
    case POA_ERROR_INVALID_CONFIG:    return "POA_ERROR_INVALID_CONFIG";
    case POA_ERROR_INVALID_ARGU:      return "POA_ERROR_INVALID_ARGU";
    case POA_ERROR_NOT_OPENED:        return "POA_ERROR_NOT_OPENED";
    case POA_ERROR_DEVICE_NOT_FOUND:  return "POA_ERROR_DEVICE_NOT_FOUND";
    case POA_ERROR_OUT_OF_LIMIT:      return "POA_ERROR_OUT_OF_LIMIT";
    case POA_ERROR_EXPOSURE_FAILED:   return "POA_ERROR_EXPOSURE_FAILED";
    case POA_ERROR_TIMEOUT:           return "POA_ERROR_TIMEOUT";
    case POA_ERROR_SIZE_LESS:         return "POA_ERROR_SIZE_LESS";
    case POA_ERROR_EXPOSING:          return "POA_ERROR_EXPOSING";
    case POA_ERROR_POINTER:           return "POA_ERROR_POINTER";
    case POA_ERROR_CONF_CANNOT_WRITE: return "POA_ERROR_CONF_CANNOT_WRITE";
    case POA_ERROR_CONF_CANNOT_READ:  return "POA_ERROR_CONF_CANNOT_READ";
    case POA_ERROR_ACCESS_DENIED:     return "POA_ERROR_ACCESS_DENIED";
    case POA_ERROR_OPERATION_FAILED:  return "POA_ERROR_OPERATION_FAILED";
    case POA_ERROR_MEMORY_FAILED:     return "POA_ERROR_MEMORY_FAILED";
    }
    return "UNKNOWN";
}

const char *toString(POAImgFormat type)
{
    switch (type)
    {
    case POA_RAW8:   return "POA_RAW8";
    case POA_RGB24:  return "POA_RGB24";
    case POA_RAW16:  return "POA_RAW16";
    case POA_MONO8:  return "POA_MONO8";
    case POA_END:    return "POA_END";
    default:         return "UNKNOWN";
    }
}

const char *toPrettyString(POAImgFormat type)
{
    switch (type)
    {
    case POA_RAW8:   return "Raw 8 bit";
    case POA_RGB24:  return "RGB 24";
    case POA_RAW16:  return "Raw 16 bit";
    case POA_MONO8:  return "Luma";
    case POA_END:    return "END";
    default:         return "UNKNOWN";
    }
}

INDI_PIXEL_FORMAT pixelFormat(POAImgFormat type, POABayerPattern pattern, bool isColor)
{
    if (isColor == false)
        return INDI_MONO;

    switch (type)
    {
    case POA_RGB24: return INDI_RGB;
    case POA_MONO8: return INDI_MONO;
    default:;           // see below
    }

    switch (pattern)
    {
    case POA_BAYER_RG: return INDI_BAYER_RGGB;
    case POA_BAYER_BG: return INDI_BAYER_BGGR;
    case POA_BAYER_GR: return INDI_BAYER_GRBG;
    case POA_BAYER_GB: return INDI_BAYER_GBRG;
    default:;
    }

    return INDI_MONO;
}

}
