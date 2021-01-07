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

///AHP_XC_VERSION This library version
#define AHP_XC_VERSION 0x010010

///AHP_XC_LIVE_AUTOCORRELATOR indicates if the correlator can do live spectrum analysis
#define AHP_XC_LIVE_AUTOCORRELATOR (1<<0)
///AHP_XC_LIVE_CROSSCORRELATOR indicates if the correlator can do live cross-correlation
#define AHP_XC_LIVE_CROSSCORRELATOR (1<<1)
///AHP_XC_HAS_LED_FLAGS indicates if the correlator has led lines available to drive
#define AHP_XC_HAS_LED_FLAGS (1<<2)
///AHP_XC_HAS_CROSSCORRELATOR indicates if the correlator can cross-correlate or can autocorrelate only
#define AHP_XC_HAS_CROSSCORRELATOR (1<<3)

/**
 * \defgroup DSP_Defines DSP API defines
*/
/*@{*/

///XC_BASE_RATE is the base baud rate of the XC cross-correlators
#define XC_BASE_RATE ((int)57600)

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
} ahp_xc_correlation;

typedef struct {
    unsigned long jitter_size;
    ahp_xc_correlation *correlations;
} ahp_xc_sample;

typedef struct {
    unsigned long n_lines;
    unsigned long n_baselines;
    unsigned long tau;
    unsigned long bps;
    unsigned long cross_lag;
    unsigned long auto_lag;
    unsigned long* counts;
    ahp_xc_sample* autocorrelations;
    ahp_xc_sample* crosscorrelations;
} ahp_xc_packet;

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
* \sa ahp_xc_disconnect
*/
DLL_EXPORT int ahp_xc_connect(const char *port);

/**
* \brief Connect to a serial port or other stream with the given file descriptor
* \param fd The file descriptor of the stream
*/
DLL_EXPORT void ahp_xc_connect_fd(int fd);

/**
* \brief Disconnect from the serial port opened with ahp_xc_connect()
* \sa ahp_xc_connect
*/
DLL_EXPORT void ahp_xc_disconnect(void);

/**
* \brief Obtain the current baud rate
* \return Returns the baud rate
*/
DLL_EXPORT int ahp_xc_get_baudrate(void);

/**
* \brief Obtain the current baud rate
* \param rate The new baud rate index
* \param setterm Change the termios settings of the current fd or port opened
*/
DLL_EXPORT void ahp_xc_set_baudrate(baud_rate rate);

/*@}*/
/**
 * \defgroup Features of the correlator
*/
/*@{*/

/**
* \brief Obtain the current baud rate
* \return Returns 0 on success, -1 if any error was encountered
*/
DLL_EXPORT int ahp_xc_get_properties(void);

/**
* \brief Obtain the correlator bits per sample
* \return Returns the bits per sample value
*/
DLL_EXPORT int ahp_xc_get_bps(void);

/**
* \brief Obtain the correlator number of lines
* \return Returns the number of lines
*/
DLL_EXPORT int ahp_xc_get_nlines(void);

/**
* \brief Obtain the correlator total baselines
* \return Returns the baselines quantity
*/
DLL_EXPORT int ahp_xc_get_nbaselines(void);

/**
* \brief Obtain the correlator maximum delay value
* \return Returns the delay size
*/
DLL_EXPORT int ahp_xc_get_delaysize(void);

/**
* \brief Obtain the correlator jitter buffer size for autocorrelations
* \return Returns the jitter size
*/
DLL_EXPORT int ahp_xc_get_autocorrelator_jittersize(void);

/**
* \brief Obtain the correlator jitter buffer size for crosscorrelations
* \return Returns the jitter size
*/
DLL_EXPORT int ahp_xc_get_crosscorrelator_jittersize(void);

/**
* \brief Obtain the correlator maximum readout frequency
* \return Returns the maximum readout frequency
*/
DLL_EXPORT int ahp_xc_get_frequency(void);

/**
* \brief Obtain the correlator maximum readout frequency
* \return Returns the maximum readout frequency
*/
DLL_EXPORT int ahp_xc_get_frequency_divider(void);

/**
* \brief Obtain the serial packet transmission time in microseconds
* \return Returns the packet transmission time
*/
DLL_EXPORT unsigned int ahp_xc_get_packettime(void);

/**
* \brief Obtain the serial packet size
* \return Returns the packet size
*/
DLL_EXPORT int ahp_xc_get_packetsize(void);
/*@}*/
/**
 * \defgroup Data and streaming
*/
/*@{*/

/**
* \brief Allocate and return a packet structure
* \return Returns the packet structure
*/
DLL_EXPORT ahp_xc_packet *ahp_xc_alloc_packet();

/**
* \brief Free a previously allocated packet structure
* \param packet the packet structure to be freed
*/
DLL_EXPORT void ahp_xc_free_packet(ahp_xc_packet *packet);

/**
* \brief Allocate and return a samples array
* \return Returns the samples array
*/
DLL_EXPORT ahp_xc_sample *ahp_xc_alloc_samples(unsigned long nlines, unsigned long len);

/**
* \brief Free a previously allocated samples array
* \param packet the samples array to be freed
*/
DLL_EXPORT void ahp_xc_free_samples(unsigned long nlines, ahp_xc_sample *samples);

/**
* \brief Grab a data packet
* \param counts The counts of each input pulses within the packet time
* \param autocorrelations The autocorrelations counts of each input pulses with itself delayed by the clock cycles defined with ahp_xc_set_lag_auto.
* \param crosscorrelations The crosscorrelations counts of each input's with others' pulses.
* \sa ahp_xc_set_lag_auto
* \sa ahp_xc_set_lag_cross
*/
DLL_EXPORT int ahp_xc_get_packet(ahp_xc_packet *packet);

/**
* \brief Scan all available delay channels and get autocorrelations of each input
* \param autocorrelations A pre-allocated correlation array of size delay_size*num_lines which will be filled with the autocorrelated values.
* \param percent A pointer to a double which, during scanning, will be updated with the percent of completion.
* \param interrupt A pointer to an integer whose value, during execution, if turns into 1 will abort scanning.
*/
DLL_EXPORT void ahp_xc_scan_autocorrelations(ahp_xc_sample *autocorrelations, int stacksize, double *percent, int *interrupt);

/**
* \brief Scan all available delay channels and get crosscorrelations of each input with others
* \param autocorrelations A pre-allocated correlation array of size (delay_size*2+1)*num_baselines which will be filled with the crosscorrelated values.
* \param percent A pointer to a double which, during scanning, will be updated with the percent of completion.
* \param interrupt A pointer to an integer whose value, during execution, if turns into 1 will abort scanning.
*/
DLL_EXPORT void ahp_xc_scan_crosscorrelations(ahp_xc_sample *crosscorrelations, int stacksize, double *percent, int *interrupt);

/*@}*/
/**
 * \defgroup Commands and setup of the correlator
*/
/*@{*/

/**
* \brief Enable capture by starting serial transmission from the correlator
* \param enable 1 to enable capture, 0 to stop capturing.
*/
DLL_EXPORT void ahp_xc_enable_capture(int enable);

/**
* \brief Switch on or off the led lines of the correlator
* \param index The input line index starting from 0
* \param leds The enable mask of the leds
*/
DLL_EXPORT void ahp_xc_set_leds(int index, int leds);

/**
* \brief Set the lag of the selected input in clock cycles (for cross-correlation)
* \param index The input line index starting from 0
* \param value The lag amount in clock cycles
*/
DLL_EXPORT void ahp_xc_set_lag_cross(int index, int value);

/**
* \brief Set the lag of the selected input in clock cycles (for auto-correlation)
* \param index The input line index starting from 0
* \param value The lag amount in clock cycles
*/
DLL_EXPORT void ahp_xc_set_lag_auto(int index, int value);

/**
* \brief Set the clock divider for autocorrelation and crosscorrelation
* \param value The clock divider power of 2
*/
DLL_EXPORT void ahp_xc_set_frequency_divider(unsigned char value);

/**
* \brief Send an arbitrary command to the AHP xc device
* \param cmd The command
* \param value The command parameter
*/
DLL_EXPORT ssize_t ahp_xc_send_command(it_cmd cmd, unsigned char value);

/**
* \brief Obtain the current libahp-xc version
*/
DLL_EXPORT inline unsigned int ahp_xc_get_version() { return AHP_XC_VERSION; }

/*@}*/
/*@}*/
/*@}*/
#ifdef __cplusplus
} // extern "C"
#endif

#endif //_AHP_XC_H


