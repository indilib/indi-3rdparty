/*
    pkTriggerCord
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2011-2019 Andras Salamon <andras.salamon@melda.info>

    based on:

    pslr-shoot

    Command line remote control of Pentax DSLR cameras.
    Copyright (C) 2009 Ramiro Barreiro <ramiro_barreiro69@yahoo.es>
    With fragments of code from PK-Remote by Pontus Lidman.
    <https://sourceforge.net/projects/pkremote>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License
    and GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <math.h>
#include <sys/time.h>

#include "pslr.h"
#include "pktriggercord-servermode.h"

#ifdef WIN32
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC | O_BINARY
#else
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC
#endif

bool debug = false;
bool warnings = false;

const char *shortopts = "m:q:a:r:d:t:o:i:F:fghvsSw";

pslr_settings settings;
bool bulb_timer_before=false;
bool astrotracer_before=false;
bool need_bulb_new_cleanup=false;
bool need_one_push_bracketing_cleanup=false;

static struct option const longopts[] = {
    {"exposure_mode", required_argument, NULL, 'm'},
    {"resolution", required_argument, NULL, 'r'},
    {"quality", required_argument, NULL, 'q'},
    {"aperture", required_argument, NULL, 'a'},
    {"shutter_speed", required_argument, NULL, 't'},
    {"iso", required_argument, NULL, 'i'},
    {"file_format", required_argument, NULL, 1},
    {"output_file", required_argument, NULL, 'o'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"status", no_argument, NULL, 's'},
    {"status_hex", no_argument, NULL, 2},
    {"frames", required_argument, NULL, 'F'},
    {"delay", required_argument, NULL, 'd'},
    {"auto_focus", no_argument, NULL, 'f'},
    {"green", no_argument, NULL, 'g'},
    {"warnings", no_argument, NULL, 'w'},
    {"exposure_compensation", required_argument, NULL, 3},
    {"flash_exposure_compensation", required_argument, NULL, 5},
    {"debug", no_argument, NULL, 4},
    {"dust_removal", no_argument, NULL, 6},
    {"color_space", required_argument, NULL, 7},
    {"af_mode", required_argument, NULL, 8},
    {"ae_metering", required_argument, NULL, 9},
    {"flash_mode", required_argument, NULL, 10},
    {"drive_mode", required_argument, NULL, 11},
    {"select_af_point", required_argument, NULL, 12},
    {"jpeg_image_tone", required_argument, NULL, 13},
    {"white_balance_mode", required_argument, NULL, 14},
    {"white_balance_adjustment", required_argument, NULL, 15},
    {"model", required_argument, NULL, 16},
    {"nowarnings", no_argument, NULL, 17},
    {"device", required_argument, NULL, 18},
    {"reconnect", no_argument, NULL, 19},
    {"timeout", required_argument, NULL, 20},
    {"noshutter", no_argument, NULL, 21},
    {"servermode", no_argument, NULL, 22},
    {"servermode_timeout", required_argument, NULL, 23},
    {"pentax_debug_mode", required_argument, NULL,24},
    {"dangerous", no_argument, NULL, 25},
    {"read_datetime", no_argument, NULL, 26},
    {"read_firmware_version", no_argument, NULL, 27},
    {"settings_hex", no_argument, NULL, 28},
    {"dump_memory", required_argument, NULL, 29},
    {"settings", no_argument, NULL, 'S'},
    { NULL, 0, NULL, 0}
};

int save_buffer(pslr_handle_t camhandle, int bufno, int fd, pslr_status *status, user_file_format filefmt, int jpeg_stars) {
    pslr_buffer_type imagetype;
    uint8_t buf[65536];
    uint32_t length;
    uint32_t current;

    if (filefmt == USER_FILE_FORMAT_PEF) {
        imagetype = PSLR_BUF_PEF;
    } else if (filefmt == USER_FILE_FORMAT_DNG) {
        imagetype = PSLR_BUF_DNG;
    } else {
        imagetype = pslr_get_jpeg_buffer_type( camhandle, jpeg_stars );
    }

    DPRINT("get buffer %d type %d res %d\n", bufno, imagetype, status->jpeg_resolution);

    if (pslr_buffer_open(camhandle, bufno, imagetype, status->jpeg_resolution) != PSLR_OK) {
        return 1;
    }

    length = pslr_buffer_get_size(camhandle);
    DPRINT("Buffer length: %d\n", length);
    current = 0;

    while (true) {
        uint32_t bytes;
        bytes = pslr_buffer_read(camhandle, buf, sizeof (buf));
        if (bytes == 0) {
            break;
        }
        ssize_t r = write(fd, buf, bytes);
        if (r == 0) {
            DPRINT("write(buf): Nothing has been written to buf.\n");
        } else if (r == -1) {
            perror("write(buf)");
        } else if (r < bytes) {
            DPRINT("write(buf): only write %zu bytes, should be %d bytes.\n", r, bytes);
        }
        current += bytes;
    }
    pslr_buffer_close(camhandle);
    return 0;
}

void save_memory(pslr_handle_t camhandle, int fd, uint32_t length) {
    uint8_t buf[65536];
    uint32_t current;

    DPRINT("save memory %d\n", length);

    current = 0;

    while (current<length) {
        uint32_t bytes;
        int readsize=length-current>65536 ? 65536 : length-current;
        bytes = pslr_fullmemory_read(camhandle, buf, current, readsize);
        if (bytes == 0) {
            break;
        }
        ssize_t r = write(fd, buf, bytes);
        if (r == 0) {
            DPRINT("write(buf): Nothing has been written to buf.\n");
        } else if (r == -1) {
            perror("write(buf)");
        } else if (r < bytes) {
            DPRINT("write(buf): only write %zu bytes, should be %d bytes.\n", r, bytes);
        }
        current += bytes;
    }
}


void print_status_info( pslr_handle_t h, pslr_status status ) {
    printf("\n");
    printf( "%s", collect_status_info( h, status ) );
}

void print_settings_info( pslr_handle_t h, pslr_settings settings ) {
    printf("\n");
    printf( "%s", collect_settings_info( h, settings ) );
}

void usage(char *name) {
    printf("\nUsage: %s [OPTIONS]\n\n\
\n\
      --model=CAMERA_MODEL              valid values are: K20d, K10d, GX10, GX20, K-x, K200D, K-7, K-r, K-5, K-2000, K-m, K-30, K100D, K110D, K-01, K-3, K-3II, K-500\n\
      --device=DEVICE                   valid values for Linux: sg0, sg1, ..., for Windows: C, D, E, ...\n\
      --timeout=SECONDS                 timeout for camera connection ( 0 means forever )\n\
  -w, --warnings                        warning mode on\n\
      --nowarnings                      warning mode off\n\
  -m, --exposure_mode=MODE              valid values are GREEN, P, SV, TV, AV, TAV, M and X\n\
      --exposure_compensation=VALUE     exposure compensation value\n\
      --drive_mode=DRIVE_MODE           valid values are: Single, Continuous-HI, SelfTimer-12, SelfTimer-2, Remote, Remote-3, Continuous-LO\n\
  -i, --iso=ISO                         single value (400) or interval (200-800)\n\
      --color_space=COLOR_SPACE         valid values are: sRGB, AdobeRGB\n\
      --af_mode=AF_MODE                 valid values are: AF.S, AF.C, AF.A\n\
      --select_af_point=AF_SELECT_MODE  valid values are: Auto-5, Auto-11, Spot, Select, or numerical value\n\
      --ae_metering=AE_METERING         valid values are: Multi, Center, Spot\n\
      --flash_mode=FLASH_MODE           valid values are: Manual, Manual-RedEye, Slow, Slow-RedEye, TrailingCurtain, Auto, Auto-RedEye, Wireless\n\
      --flash_exposure_compensation=VAL flash exposure compensation value\n\
  -a, --aperture=APERTURE\n\
  -t, --shutter_speed=SHUTTER SPEED     values can be given in rational form (eg. 1/90) or decimal form (eg. 0.8)\n\
  -r, --resolution=RESOLUTION           resolution in megapixels\n\
  -q, --quality=QUALITY                 valid values are 1, 2, 3 and 4\n\
      --jpeg_image_tone=IMAGE_TONE      valid values are: Auto, Natural, Bright, Portrait, Landscape, Vibrant, Monochrome, Muted, ReversalFilm, BleachBypass, Radiant, CrossProcessing, Flat\n\
      --white_balance_mode=WB_MODE      valid values are: Auto, Daylight, Shade, Cloudy, Fluorescent_D, Fluorescent_N, Fluorescent_W, Fluorescent_L, Tungsten, Flash, Manual, Manual2, Manual3, Kelvin1, Kelvin2, Kelvin3, CTE, MultiAuto\n\
      --white_balance_adjustment=WB_ADJ valid values like: G5B2, G3A5, B5, A3, G5, M4...\n\
  -f, --auto_focus                      autofocus\n\
      --reconnect                       reconnect between shots\n\
      --servermode                      start in server mode and wait for commands\n\
      --servermode_timeout=SECONDS      servermode timeout\n\
  -g, --green                           green button\n\
  -s, --status                          print status info\n\
      --status_hex                      print status hex info\n\
  -S, --settings                        print settings info\n\
      --settings_hex                    print settings hex info\n\
      --read_datetime                   print the camera date and time\n\
      --read_firmware_version           print the firmware version of the camera\n\
      --dump_memory SIZE                dumps the internal memory of the camera to pentax_dump.dat file. Size is in bytes, but can be specified using K, M, and G modifiers.\n\
      --dust_removal                    dust removal\n\
  -F, --frames=NUMBER                   number of frames\n\
  -d, --delay=SECONDS                   delay between the frames (seconds)\n\
      --file_format=FORMAT              valid values: PEF, DNG, JPEG\n\
  -o, --output_file=FILE                send output to FILE\n\
      --debug                           turn on debug messages\n\
      --noshutter                       do not send shutter command, just wait for new photo, download and delete from camera\n\
  -v, --version                         display version information and exit\n\
  -h, --help                            display this help and exit\n\
      --pentax_debug_mode={0|1}		enable or disable camera debug mode and exit (DANGEROUS). Valid values are: 0, 1\n\
\n", name);
}

int open_file(char* output_file, int frameNo, user_file_format_t ufft) {
    int ofd;
    char fileName[256];

    if (!output_file) {
        ofd = 1;
    } else {
        char *dot = strrchr(output_file, '.');
        int prefix_length;
        if (dot && !strcmp(dot+1, ufft.extension)) {
            prefix_length = dot - output_file;
        } else {
            prefix_length = strlen(output_file);
        }
        snprintf(fileName, 256, "%.*s-%04d.%s", prefix_length, output_file, frameNo, ufft.extension);
        ofd = open(fileName, FILE_ACCESS, 0664);
        if (ofd == -1) {
            fprintf(stderr, "Could not open %s\n", output_file);
            return -1;
        }
    }
    return ofd;
}

void warning_message( const char* message, ... ) {
    if ( warnings ) {
        // Write to stderr
        //
        va_list argp;
        va_start(argp, message);
        vfprintf( stderr, message, argp );
        va_end(argp);
    }
}

void process_wbadj( const char* argv0, const char chr, uint32_t adj, uint32_t *wbadj_mg, uint32_t *wbadj_ba ) {
    if ( chr == 'M' ) {
        *wbadj_mg = 7 - adj;
    } else if ( chr == 'G' )  {
        *wbadj_mg = 7 + adj;
    } else if ( chr == 'B' ) {
        *wbadj_ba = 7 - adj;
    } else if ( chr == 'A' ) {
        *wbadj_ba = 7 + adj;
    } else {
        warning_message("%s: Invalid white_balance_adjustment\n", argv0);
    }
}

char *copyright_version(char *name, char *version) {
    char *ret = malloc(sizeof(char)*1024);
    sprintf(ret, "%s %s\n\n\%s\
License LGPLv3: GNU LGPL version 3 <http://gnu.org/licenses/lgpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n", name, version, copyright() );
    return ret;
}

char *command_line(int argc, char **argv) {
    int len = 0;
    int i;
    for (i=0; i<argc; ++i) {
        len += strlen(argv[i])+4;
    }
    char *ret = malloc(sizeof(char)*len);
    memset(ret, 0, sizeof(char)*len);
    for (i=0; i<argc; ++i) {
        strcat(ret, argv[i]);
        strcat(ret, " ");
    }
    return ret;
}

void bulb_old(pslr_handle_t camhandle, pslr_rational_t shutter_speed, struct timeval prev_time) {
    DPRINT("bulb oldstyle\n");
    struct timeval current_time;
    pslr_bulb( camhandle, true );
    pslr_shutter(camhandle);
    gettimeofday(&current_time, NULL);
    double waitsec = 1.0 * shutter_speed.nom / shutter_speed.denom - timeval_diff_sec(&current_time, &prev_time);
    if ( waitsec < 0 ) {
        waitsec = 0;
    }
    sleep_sec( waitsec  );
    pslr_bulb( camhandle, false );
}

void bulb_new(pslr_handle_t camhandle, pslr_rational_t shutter_speed) {
    if (pslr_has_setting_by_name(camhandle, "bulb_timer")) {
        pslr_write_setting_by_name(camhandle, "bulb_timer", 1);
    } else if (pslr_has_setting_by_name(camhandle, "astrotracer")) {
        pslr_write_setting_by_name(camhandle, "astrotracer", 1);
    } else {
        fprintf(stderr, "New bulb mode is not supported for this camera model\n");
    }
    int bulb_sec = (int)(shutter_speed.nom / shutter_speed.denom);
    if (pslr_has_setting_by_name(camhandle, "bulb_timer_sec")) {
        pslr_write_setting_by_name(camhandle, "bulb_timer_sec", bulb_sec);
    } else if (pslr_has_setting_by_name(camhandle, "astrotracer_timer_sec")) {
        pslr_write_setting_by_name(camhandle, "astrotracer_timer_sec", bulb_sec);
    } else {
        fprintf(stderr, "New bulb mode is not supported for this camera model\n");
    }
    pslr_shutter(camhandle);
}

void bulb_new_cleanup(pslr_handle_t camhandle) {
    if (pslr_has_setting_by_name(camhandle, "bulb_timer")) {
        if (!bulb_timer_before) {
            pslr_write_setting_by_name(camhandle, "bulb_timer", bulb_timer_before);
        }
    } else if (pslr_has_setting_by_name(camhandle, "astrotracer")) {
        if (!astrotracer_before) {
            pslr_write_setting_by_name(camhandle, "astrotracer", astrotracer_before);
        }
    }
}

