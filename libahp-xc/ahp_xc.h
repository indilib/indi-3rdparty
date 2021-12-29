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
* \mainpage AHP® XC Crosscorrelators driver library
* \section Introduction
*
* The AHP XC correlators are devices that permit cross-correlation and auto-correlation
* counting from quantum detectors, ranging from radio to photon counters to geiger-mode detectors
* or noise-regime, light scattering counters, the XC series offer a scientific grade solution for
* laboratory testing and measurement in quantum resoluting detection environments.
*
* This software is meant to work with the XC series cross-correlators
* visit http://www.iliaplatone.com/xc for more informations and purchase options.
*
* \author Ilia Platone
*/

/**
 * \defgroup XC_API AHP® XC Correlators API
 *
 * This library contains functions for direct low-level usage of the AHP cross-correlators.<br>
 * This documentation describes utility, applicative and hardware control functions included into the library.<br>
 * Each section and component is documented for general usage.
*/
/**@{*/

/**
 * \defgroup Defs Defines
 */
 /**@{*/

 ///AHP_XC_VERSION This library version
#define AHP_XC_VERSION 0x112
///XC_BASE_RATE is the base baud rate of the XC cross-correlators
#define XC_BASE_RATE ((int)57600)
///XC_HIGH_RATE is the base baud rate for big packet XC cross-correlators
#define XC_HIGH_RATE ((int)500000)
///AHP_XC_PLL_FREQUENCY is the PLL frequency of the XC cross-correlators
#define AHP_XC_PLL_FREQUENCY 400000000

/**@}*/
/**
 * \defgroup Enumerations
 */
 /**@{*/

 /**
 * \brief AHP XC header flags
 */
typedef enum {
    HAS_CROSSCORRELATOR = 1, ///AHP_XC_HAS_CROSSCORRELATOR indicates if the correlator can cross-correlate or can autocorrelate only
    HAS_LEDS = 2, ///AHP_XC_HAS_LEDS indicates if the correlator has led lines available to drive
    HAS_PSU = 4, ///AHP_XC_HAS_PSU indicates if the correlator has an internal PSU PWM driver on 2nd flag bit
    HAS_CUMULATIVE_ONLY = 8 ///HAS_CUMULATIVE_ONLY indicates if the correlator has cumulative readout available only
} xc_header_flags;

/**
* \brief These are the baud rates supported
*/
typedef enum {
    R_BASE = 0,
    R_BASEX2 = 1,
    R_BASEX4 = 2,
    R_BASEX8 = 3,
} baud_rate;

/**
* \brief The XC firmare commands
*/
typedef enum {
    CLEAR = 0, ///Clear autocorrelation and crosscorrelation delays
    SET_INDEX = 1, ///Set the current input line index for following commands
    SET_LEDS = 2, /// witch on or off current line leds requires HAS_LEDS
    SET_BAUD_RATE = 3, ///Set the readout and command baud rate
    SET_DELAY = 4, ///Set the autocorrelator or crosscorrelator delay
    SET_FREQ_DIV = 8, ///Set the frequency divider in powers of two
    SET_VOLTAGE = 9, ///Set the indexed input voltage, requires HAS_PSU in header
    ENABLE_TEST = 12, ///Enables tests on currently indexed input
    ENABLE_CAPTURE = 13 ///Enable capture flags
} xc_cmd;

/**
* \brief The XC capture flags
*/
typedef enum {
    CAP_ENABLE = 0, ///Enable capture
    CAP_EXT_CLK = 1, ///Enable external clock
} xc_capture_flags;

/**
* \brief The XC firmare commands
*/
typedef enum {
    TEST_NONE = 0, ///No extra signals or functions
    TEST_SIGNAL = 1, ///Apply PLL clock on voltage led
    SCAN_AUTO = 2, ///Autocorrelator continuum scan
    SCAN_CROSS = 4, ///Crosscorrelator continuum scan
    TEST_BCM = 8, ///BCM modulation on voltage led
    TEST_ALL = 0xf, ///All tests enabled
} xc_test;

/**
* \brief Correlations structure
*/
typedef struct {
    unsigned long correlations; ///Correlations count
    unsigned long counts; ///Pulses count
    double coherence; ///Coherence ratio given by correlations/counts
} ahp_xc_correlation;

/**
* \brief Sample structure
*/
typedef struct {
    unsigned long lag_size; ///Maximum lag in a single shot
    ahp_xc_correlation *correlations; ///Correlations array, of size lag_size in an ahp_xc_packet
} ahp_xc_sample;

/**
* \brief Packet structure
*/
typedef struct {
    unsigned long n_lines; ///Number of lines in this correlator
    unsigned long n_baselines; ///Total number of baselines obtainable
    unsigned long tau; ///Bandwidth inverse frequency
    unsigned long bps; ///Bits capacity in each sample
    unsigned long cross_lag; ///Maximum crosscorrelation lag in a single shot
    unsigned long auto_lag; ///Maximum autocorrelation lag in a single shot
    unsigned long* counts; ///Counts in the current shot
    ahp_xc_sample* autocorrelations; ///Autocorrelations in the current shot
    ahp_xc_sample* crosscorrelations; ///Crosscorrelations in the current shot
} ahp_xc_packet;

/**@}*/
/**
 * \defgroup Utilities Utility functions
*/
/**@{*/

/**
* \brief Get 2d projection for intensity interferometry
* \param alt The altitude coordinate
* \param az The azimuth coordinate
* \param baseline The reference baseline in meters
* \return Returns a 3-element double vector containing the 2d perspective coordinates and the z-offset
*/
DLL_EXPORT double* ahp_xc_get_2d_projection(double alt, double az, double *baseline);

/**@}*/
/**
 * \defgroup Comm Communication
*/
/**@{*/

/**
* \brief Connect to a serial port
* \param port The serial port name or filename
* \return Returns 0 on success, -1 if any error was encountered
* \sa ahp_xc_disconnect
*/
DLL_EXPORT int ahp_xc_connect(const char *port, int high_rate);

/**
* \brief Connect to a serial port or other stream with the given file descriptor
* \param fd The file descriptor of the stream
*/
DLL_EXPORT int ahp_xc_connect_fd(int fd);

/**
* \brief Disconnect from the serial port opened with ahp_xc_connect
* \sa ahp_xc_connect
*/
DLL_EXPORT void ahp_xc_disconnect(void);

/**
* \brief Report connection status
* \sa ahp_xc_connect
* \sa ahp_xc_connect_fd
* \sa ahp_xc_disconnect
*/
DLL_EXPORT unsigned int ahp_xc_is_connected(void);

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

/**@}*/
/**
 * \defgroup Feat Features of the correlator
*/
/**@{*/

/**
* \brief Obtain the current baud rate
* \return Returns 0 on success, -1 if any error was encountered
*/
DLL_EXPORT int ahp_xc_get_properties(void);

/**
* \brief Obtain the correlator header
* \return Returns a string containing the header and device identifier
*/
DLL_EXPORT char* ahp_xc_get_header(void);

/**
* \brief Obtain the correlator bits per sample
* \return Returns the bits per sample value
*/
DLL_EXPORT unsigned int ahp_xc_get_bps(void);

/**
* \brief Obtain the correlator number of lines
* \return Returns the number of lines
*/
DLL_EXPORT unsigned int ahp_xc_get_nlines(void);

/**
* \brief Obtain the correlator total baselines
* \return Returns the baselines quantity
*/
DLL_EXPORT unsigned int ahp_xc_get_nbaselines(void);

/**
* \brief Obtain the correlator maximum delay value
* \return Returns the delay size
*/
DLL_EXPORT unsigned int ahp_xc_get_delaysize(void);

/**
* \brief Obtain the correlator lag buffer size for autocorrelations
* \return Returns the lag size
*/
DLL_EXPORT unsigned int ahp_xc_get_autocorrelator_lagsize(void);

/**
* \brief Obtain the correlator lag buffer size for crosscorrelations
* \return Returns the lag size
*/
DLL_EXPORT unsigned int ahp_xc_get_crosscorrelator_lagsize(void);

/**
* \brief Obtain the correlator maximum readout frequency
* \param index The line index.
* \return Returns the maximum readout frequency
*/
DLL_EXPORT unsigned int ahp_xc_get_frequency(void);

/**
* \brief Obtain the correlator maximum readout frequency
* \return Returns the maximum readout frequency
*/
DLL_EXPORT unsigned int ahp_xc_get_frequency_divider(void);

/**
* \brief Obtain the serial packet transmission time in microseconds
* \return Returns the packet transmission time
*/
DLL_EXPORT unsigned int ahp_xc_get_packettime(void);

/**
* \brief Obtain the serial packet size
* \return Returns the packet size
*/
DLL_EXPORT unsigned int ahp_xc_get_packetsize(void);

/**
* \brief Returns the cross-correlation capability of the device
* \return Returns 1 if crosscorrelation is possible
*/
DLL_EXPORT int ahp_xc_has_crosscorrelator(void);

/**
* \brief Returns if the device offers internal PSU line
* \return Returns 1 if PSU is available
*/
DLL_EXPORT int ahp_xc_has_psu(void);

/**
* \brief Returns if the device has led lines to drive
* \return Returns 1 if leds are available
*/
DLL_EXPORT int ahp_xc_has_leds(void);

/**
* \brief Returns if the device has cumulative readout only
* \return Returns 1 if edge readout is not available
*/
DLL_EXPORT int ahp_xc_has_cumulative_only();

/**@}*/
/**
 * \defgroup Data Data and streaming
*/
/**@{*/

/**
* \brief Allocate and return a packet structure
* \return Returns the packet structure
*/
DLL_EXPORT ahp_xc_packet *ahp_xc_alloc_packet(void);

/**
* \brief Free a previously allocated packet structure
* \param packet the packet structure to be freed
*/
DLL_EXPORT void ahp_xc_free_packet(ahp_xc_packet *packet);

/**
* \brief Allocate and return a samples array
* \param nlines The Number of samples to be allocated.
* \param len The lag_size and correlations field size of each sample.
* \return Returns the samples array
* \sa ahp_xc_free_samples
* \sa ahp_xc_sample
* \sa ahp_xc_packet
*/
DLL_EXPORT ahp_xc_sample *ahp_xc_alloc_samples(unsigned long nlines, unsigned long len);

/**
* \brief Free a previously allocated samples array
* \param packet the samples array to be freed
* \sa ahp_xc_alloc_samples
* \sa ahp_xc_sample
* \sa ahp_xc_packet
*/
DLL_EXPORT void ahp_xc_free_samples(unsigned long nlines, ahp_xc_sample *samples);

/**
* \brief Grab a data packet
* \param packet The xc_packet structure to be filled.
* \sa ahp_xc_set_lag_auto
* \sa ahp_xc_set_lag_cross
* \sa ahp_xc_alloc_packet
* \sa ahp_xc_free_packet
* \sa ahp_xc_packet
*/
DLL_EXPORT int ahp_xc_get_packet(ahp_xc_packet *packet);

/**
* \brief Initiate an autocorrelation scan
* \param index The line index.
* \param start the starting channel for this scan.
*/
DLL_EXPORT void ahp_xc_start_autocorrelation_scan(unsigned int index, off_t start);

/**
* \brief End an autocorrelation scan
* \param index The line index.
*/
DLL_EXPORT void ahp_xc_end_autocorrelation_scan(unsigned int index);

/**
* \brief Scan all available delay channels and get autocorrelations of each input
* \param index the index of the input chosen for autocorrelation.
* \param autocorrelations An ahp_xc_sample array pointer that this function will allocate, will be filled with the autocorrelated values and will be of size ahp_xc_delaysize.
* \param percent A pointer to a double which, during scanning, will be updated with the percent of completion.
* \param interrupt A pointer to an integer whose value, during execution, if turns into 1 will abort scanning.
* \sa ahp_xc_get_delaysize
* \sa ahp_xc_sample
*/
DLL_EXPORT int ahp_xc_scan_autocorrelations(unsigned int index, ahp_xc_sample **autocorrelations, off_t start, unsigned int len, int *interrupt, double *percent);

/**
* \brief Initiate a crosscorrelation scan
* \param index The line index.
* \param start the starting channel for this scan.
*/
DLL_EXPORT void ahp_xc_start_crosscorrelation_scan(unsigned int index, off_t start);

/**
* \brief End a crosscorrelation scan
* \param index The line index.
*/
DLL_EXPORT void ahp_xc_end_crosscorrelation_scan(unsigned int index);

/**
* \brief Scan all available delay channels and get crosscorrelations of each input with others
* \param index1 the index of the first input in this baseline.
* \param index2 the index of the second input in this baseline.
* \param crosscorrelations An ahp_xc_sample array pointer that this function will allocate, will be filled with the crosscorrelated values and will be of size ahp_xc_delaysize*2-1.
* \param percent A pointer to a double which, during scanning, will be updated with the percent of completion.
* \param interrupt A pointer to an integer whose value, during execution, if turns into 1 will abort scanning.
* \sa ahp_xc_get_delaysize
* \sa ahp_xc_sample
*/
DLL_EXPORT int ahp_xc_scan_crosscorrelations(unsigned int index1, unsigned int index2, ahp_xc_sample **crosscorrelations, off_t start1, off_t start2, unsigned int size, int *interrupt, double *percent);

/**@}*/
/**
 * \defgroup Cmds Commands and setup of the correlator
*/
/**@{*/

/**
* \brief Set integration flags
* \param flag the flag to be set.
*/
DLL_EXPORT int ahp_xc_set_capture_flag(xc_capture_flags flag);

/**
* \brief Clear integration flags
* \param flag the flag to be clear.
*/
DLL_EXPORT int ahp_xc_clear_capture_flag(xc_capture_flags flag);

/**
* \brief Switch on or off the led lines of the correlator
* \param index The input line index starting from 0
* \param leds The enable mask of the leds
*/
DLL_EXPORT void ahp_xc_set_leds(unsigned int index, int leds);

/**
* \brief Set the lag of the selected input in clock cycles (for cross-correlation)
* \param index The input line index starting from 0
* \param value The lag amount in clock cycles
*/
DLL_EXPORT void ahp_xc_set_lag_cross(unsigned int index, off_t value);

/**
* \brief Set the lag of the selected input in clock cycles (for auto-correlation)
* \param index The input line index starting from 0
* \param value The lag amount in clock cycles
*/
DLL_EXPORT void ahp_xc_set_lag_auto(unsigned int index, off_t value);

/**
* \brief Set the clock divider for autocorrelation and crosscorrelation
* \param value The clock divider power of 2
*/
DLL_EXPORT void ahp_xc_set_frequency_divider(unsigned char value);

/**
* \brief Set the supply voltage on the current line
* \param index The input line index starting from 0
* \param value The voltage level
*/
DLL_EXPORT void ahp_xc_set_voltage(unsigned int index, unsigned char value);

/**
* \brief Enable tests on the current line
* \param index The input line index starting from 0
* \param value The test type
*/
DLL_EXPORT void ahp_xc_set_test(unsigned int index, xc_test value);

/**
* \brief Disable tests on the current line
* \param index The input line index starting from 0
* \param value The test type
*/
DLL_EXPORT void ahp_xc_clear_test(unsigned int index, xc_test value);

/**
* \brief Get the current status of the test features
* \param index The line index starting from 0
*/
DLL_EXPORT unsigned char ahp_xc_get_test(unsigned int index);

/**
* \brief Get the current status of the leds on line
* \param index The line index starting from 0
*/
DLL_EXPORT unsigned char ahp_xc_get_leds(unsigned int index);

/**
* \brief Send an arbitrary command to the AHP xc device
* \param cmd The command
* \param value The command parameter
*/
DLL_EXPORT int ahp_xc_send_command(xc_cmd cmd, unsigned char value);

/**
* \brief Obtain the current libahp-xc version
*/
DLL_EXPORT inline unsigned int ahp_xc_get_version(void) { return AHP_XC_VERSION; }

/**@}*/
/**@}*/
#ifdef __cplusplus
} // extern "C"
#endif

#endif //_AHP_XC_H


