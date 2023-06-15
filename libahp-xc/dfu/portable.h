
#ifndef PORTABLE_H
#define PORTABLE_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#else
# define PACKAGE "dfu-util"
# define PACKAGE_VERSION "0.10-msvc"
# define PACKAGE_STRING "dfu-util 0.10-msvc"
# define PACKAGE_BUGREPORT "http://sourceforge.net/p/dfu-util/tickets/"
/*#ifdef _WIN32
# include <io.h>
#else
# include <sys/io.h>
#endif*/
/* FIXME if off_t is a typedef it is not a define */
# ifndef off_t
#  define off_t long int
# endif
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if defined HAVE_WINDOWS_H
# define milli_sleep(msec) do {\
  if (msec != 0) {\
    Sleep(msec);\
  } } while (0)
#else
# include <time.h>
# define milli_sleep(msec) do {\
  if (msec != 0) {\
    struct timespec nanosleepDelay = { (msec) / 1000, ((msec) % 1000) * 1000000 };\
    nanosleep(&nanosleepDelay, NULL);\
  } } while (0)
#endif /* HAVE_NANOSLEEP */

#ifdef HAVE_ERR
# include <err.h>
#else
# include <errno.h>
# include <string.h>
# define warnx(...) do {\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n"); } while (0)
# define errx(eval, ...) do {\
    warnx(__VA_ARGS__);\
    exit(eval); } while (0)
# define warn(...) do {\
    fprintf(stderr, "%s: ", strerror(errno));\
    warnx(__VA_ARGS__); } while (0)
# define err(eval, ...) do {\
    warn(__VA_ARGS__);\
    exit(eval); } while (0)
#endif /* HAVE_ERR */

#ifdef HAVE_SYSEXITS_H
# include <sysexits.h>
#else
# define EX_OK		0	/* successful termination */
# define EX_USAGE	64	/* command line usage error */
# define EX_SOFTWARE	70	/* internal software error */
# define EX_IOERR	74	/* input/output error */
#endif /* HAVE_SYSEXITS_H */

#ifndef O_BINARY
# define O_BINARY   0
#endif

#endif /* PORTABLE_H */
