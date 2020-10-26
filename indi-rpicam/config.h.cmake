#ifndef CONFIG_H
#define CONFIG_H

/* Define INDI Data Dir */
#cmakedefine INDI_DATA_DIR "@INDI_DATA_DIR@"

/* Define Driver version */
#define INDI_RPICAM_VERSION_MAJOR @INDI_RPICAM_VERSION_MAJOR@
#define INDI_RPICAM_VERSION_MINOR @INDI_RPICAM_VERSION_MINOR@

#cmakedefine USE_ISO

#endif // CONFIG_H
