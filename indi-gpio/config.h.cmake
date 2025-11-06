#ifndef CONFIG_H
#define CONFIG_H

/* Define INDI Data Dir */
#cmakedefine INDI_DATA_DIR "@INDI_DATA_DIR@"

/* Define Driver version */
#define VERSION_MAJOR @VERSION_MAJOR@
#define VERSION_MINOR @VERSION_MINOR@

/* Define if libgpiod v2 is present */
#cmakedefine HAVE_LIBGPIOD_V2

#endif // CONFIG_H
