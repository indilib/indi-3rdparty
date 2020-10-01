/*
    libahp_xc library to drive the AHP XC correlators
    Copyright (C) 2020  Ilia Platone

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _AHP_XC_H
#define _AHP_XC_H

#ifdef  __cplusplus
extern "C" {
#endif
#ifdef _WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT extern
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * \defgroup AHP XC correlators driving library
*
* The AHP XC correlators series permit intensity cross-correlation and auto-correlation
* counting from pulse detectors ranging from radio to photon counters to geiger-mode detectors
* or noise-regime or light scattering counters.
* This software is meant to work with the XC series cross-correlator FPGA, programmed with the
* Verilog firmware available online at https://github.com/ahp-electronics/xc-firmware
*
* \author Ilia Platone
*/
/*@{*/

/**
 * \defgroup DSP_Defines DSP API defines
*/
/*@{*/
///if min() is not present you can use this one
#ifndef min
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (a) _b = (b); \
     _a < _b ? _a : _b; })
#endif
///if max() is not present you can use this one
#ifndef max
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (a) _b = (b); \
     _a > _b ? _a : _b; })
#endif

///XC_BASE_RATE is the base baud rate of the XC cross-correlators
#define XC_BASE_RATE 57600

/*@}*/

/**
 * \defgroup Enumerations
*/
/*@{*/
/**
* \brief These are the baud rates supported
*/
typedef enum {
    R_57600 = 0,
    R_115200 = 1,
    R_230400 = 2,
    R_460800 = 3,
} baud_rate;

/**
* \brief The XC firmare commands
*/
typedef enum {
    CLEAR = 0,
    SET_INDEX = 1,
    SET_LEDS = 2,
    SET_BAUD_RATE = 3,
    SET_DELAY = 4,
    SET_FREQ_DIV = 8,
    ENABLE_CAPTURE = 13
} it_cmd;

/**
* \brief This type is used for correlations
*/
typedef struct {
    unsigned long correlations;
    unsigned long counts;
    double coherence;
} correlation;

/*@}*/

/**
 * \defgroup XC Correlators API
*/
/*@{*/
/**
 * \defgroup Communication
*/
/*@{*/

/**
* \brief Connect to a serial port
* \param port The serial port name or filename
* \return Returns 0 on success, -1 if any error was encountered
* \sa xc_disconnect
*/
DLL_EXPORT int xc_connect(const char *port);

/**
* \brief Connect to a serial port or other stream with the given file descriptor
* \param fd The file descriptor of the stream
*/
DLL_EXPORT void xc_connect_fd(int fd);

/**
* \brief Disconnect from the serial port opened with xc_connect()
* \sa xc_connect
*/
DLL_EXPORT void xc_disconnect();

/**
* \brief Obtain the current baud rate
* \return Returns the baud rate
*/
DLL_EXPORT int xc_get_baudrate();

/**
* \brief Obtain the current baud rate
* \param rate The new baud rate index
* \param setterm Change the termios settings of the current fd or port opened
*/
DLL_EXPORT void xc_set_baudrate(baud_rate rate, int setterm);

/*@}*/
/**
 * \defgroup Features of the correlator
*/
/*@{*/

/**
* \brief Obtain the current baud rate
* \return Returns 0 on success, -1 if any error was encountered
*/
DLL_EXPORT int xc_get_properties();

/**
* \brief Obtain the correlator bits per sample
* \return Returns the bits per sample value
*/
DLL_EXPORT int xc_get_bps();

/**
* \brief Obtain the correlator number of lines
* \return Returns the number of lines
*/
DLL_EXPORT int xc_get_nlines();

/**
* \brief Obtain the correlator total baselines
* \return Returns the baselines quantity
*/
DLL_EXPORT int xc_get_nbaselines();

/**
* \brief Obtain the correlator maximum delay value
* \return Returns the delay size
*/
DLL_EXPORT int xc_get_delaysize();

/**
* \brief Obtain the correlator maximum readout frequency
* \return Returns the maximum readout frequency
*/
DLL_EXPORT int xc_get_frequency();

/**
* \brief Obtain the serial packet time
* \return Returns the packet transmission time
*/
DLL_EXPORT unsigned int xc_get_packettime();

/**
* \brief Obtain the serial packet size
* \return Returns the packet size
*/
DLL_EXPORT int xc_get_packetsize();
/*@}*/
/**
 * \defgroup Data and streaming
*/
/*@{*/

/**
* \brief Grab a data packet
* \param counts The counts of each input pulses within the packet time
* \param autocorrelations The autocorrelations counts of each input pulses with itself delayed by the clock cycles defined with xc_set_line.
* \param crosscorrelations The crosscorrelations counts of each input's with others' pulses.
* \sa xc_set_line
* \sa xc_set_delay
*/
DLL_EXPORT void xc_get_packet(unsigned long *counts, unsigned long *autocorrelations, unsigned long *crosscorrelations);

/**
* \brief Scan all available delay channels and get autocorrelations of each input
* \param autocorrelations A pre-allocated correlation array of size delay_size*num_lines which will be filled with the autocorrelated values.
* \param percent A pointer to a double which, during scanning, will be updated with the percent of completion.
* \param interrupt A pointer to an integer whose value, during execution, if turns into 1 will abort scanning.
*/
DLL_EXPORT void xc_scan_autocorrelations(correlation *autocorrelations, double *percent, int *interrupt);

/**
* \brief Scan all available delay channels and get crosscorrelations of each input with others
* \param autocorrelations A pre-allocated correlation array of size (delay_size*2+1)*num_baselines which will be filled with the crosscorrelated values.
* \param percent A pointer to a double which, during scanning, will be updated with the percent of completion.
* \param interrupt A pointer to an integer whose value, during execution, if turns into 1 will abort scanning.
*/
DLL_EXPORT void xc_scan_crosscorrelations(correlation *crosscorrelations, double *percent, int *interrupt);

/*@}*/
/**
 * \defgroup Commands and setup of the correlator
*/
/*@{*/
DLL_EXPORT void xc_enable_capture(int enable);
DLL_EXPORT void xc_set_power(int index, int lv, int hv);
DLL_EXPORT void xc_set_delay(int index, int value);
DLL_EXPORT void xc_set_line(int index, int value);
DLL_EXPORT void xc_set_frequency_divider(unsigned char value);
DLL_EXPORT ssize_t xc_send_command(it_cmd c, unsigned char value);
/*@}*/
/*@}*/
/*@}*/
#ifdef __cplusplus
} // extern "C"
#endif

#endif //_AHP_XC_H


