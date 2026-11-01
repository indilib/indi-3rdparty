#ifndef CONFIG_H
#define CONFIG_H

/* Define INDI Data Dir */
#cmakedefine INDI_DATA_DIR "@INDI_DATA_DIR@"
/* Define Driver version */
#define LIBCAMERA_VERSION_MAJOR @LIBCAMERA_VERSION_MAJOR@
#define LIBCAMERA_VERSION_MINOR @LIBCAMERA_VERSION_MINOR@
/* libcamera names the sensor size property PixelArrayArea instead of PixelArraySize */
#cmakedefine HAVE_LIBCAMERA_PIXEL_ARRAY_AREA

#endif // CONFIG_H
