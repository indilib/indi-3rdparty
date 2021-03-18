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

/*
const char *toString(ASI_ERROR_CODE code)
{
    switch (code)
    {
    case ASI_SUCCESS:                    return "ASI_SUCCESS";
	case ASI_ERROR_INVALID_INDEX:        return "ASI_ERROR_INVALID_INDEX";
	case ASI_ERROR_INVALID_ID:           return "ASI_ERROR_INVALID_ID";
	case ASI_ERROR_INVALID_CONTROL_TYPE: return "ASI_ERROR_INVALID_CONTROL_TYPE";
	case ASI_ERROR_CAMERA_CLOSED:        return "ASI_ERROR_CAMERA_CLOSED";
	case ASI_ERROR_CAMERA_REMOVED:       return "ASI_ERROR_CAMERA_REMOVED";
	case ASI_ERROR_INVALID_PATH:         return "ASI_ERROR_INVALID_PATH";
	case ASI_ERROR_INVALID_FILEFORMAT:   return "ASI_ERROR_INVALID_FILEFORMAT";
	case ASI_ERROR_INVALID_SIZE:         return "ASI_ERROR_INVALID_SIZE";
	case ASI_ERROR_INVALID_IMGTYPE:      return "ASI_ERROR_INVALID_IMGTYPE";
	case ASI_ERROR_OUTOF_BOUNDARY:       return "ASI_ERROR_OUTOF_BOUNDARY";
	case ASI_ERROR_TIMEOUT:              return "ASI_ERROR_TIMEOUT";
	case ASI_ERROR_INVALID_SEQUENCE:     return "ASI_ERROR_INVALID_SEQUENCE";
	case ASI_ERROR_BUFFER_TOO_SMALL:     return "ASI_ERROR_BUFFER_TOO_SMALL";
	case ASI_ERROR_VIDEO_MODE_ACTIVE:    return "ASI_ERROR_VIDEO_MODE_ACTIVE";
	case ASI_ERROR_EXPOSURE_IN_PROGRESS: return "ASI_ERROR_EXPOSURE_IN_PROGRESS";
	case ASI_ERROR_GENERAL_ERROR:        return "ASI_ERROR_GENERAL_ERROR";
	case ASI_ERROR_INVALID_MODE:         return "ASI_ERROR_INVALID_MODE";
	case ASI_ERROR_END:                  return "ASI_ERROR_END";
    }
}
*/

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
