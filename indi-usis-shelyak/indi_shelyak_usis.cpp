/**
 * @file indi_shelyak_usis.cpp
 * @author Etienne Cochard
 * @copyright (c) 2022 R-libre ingenierie, all rights reserved.
 *
 * @description shelyak universal usis driver
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <map>
#include <math.h>
#include <memory>

#include <indicom.h>

#include "config.h"
#include "indi_shelyak_usis.h"

#include "cJSON/cJSON.h"

#define PORT_SETTING _text_settings[0]
#define PORT_LINE _text_line[0]

double _atof( const char* in )
{
	char* e;
	return strtof( in, &e );
}



ShelyakUsis::ShelyakUsis( )
{
	_props = nullptr;
	_serialPort = -1;
	setVersion( SHELYAK_USIS_VERSION_MAJOR, SHELYAK_USIS_VERSION_MINOR );
}

ShelyakUsis::~ShelyakUsis( )
{
	if ( _serialPort != -1 ) {
		tty_disconnect( _serialPort );
	}
}

/* Returns the name of the device. */
const char* ShelyakUsis::getDefaultName( )
{
	return "Shelyak Usis driver";
}

/* */
bool ShelyakUsis::Connect( )
{
	int rc;
	if ( ( rc = tty_connect( PORT_SETTING.text, 2400, 8, 0, 1, &_serialPort ) ) != TTY_OK ) {
		char errMsg[MAXRBUF];
		tty_error_msg( rc, errMsg, MAXRBUF );
		LOGF_ERROR( "Failed to connect to port %s. Error: %s", PORT_SETTING.text, errMsg );
		return false;
	}

	LOGF_INFO( "%s is online.", getDeviceName( ) );
	return true;
}

bool ShelyakUsis::Disconnect( )
{
	if ( _serialPort != -1 ) {
		tty_disconnect( _serialPort );
		LOGF_INFO( "%s is offline.", getDeviceName( ) );
	}

	return true;
}

/* Initialize and setup all properties on startup. */
bool ShelyakUsis::initProperties( )
{
	INDI::DefaultDevice::initProperties( );

	//--------------------------------------------------------------------------------
	// Options
	//--------------------------------------------------------------------------------

	// setup the text input for the serial port
	IUFillText( &PORT_SETTING, "PORT", "Port", "/dev/ttyACM0" );
	IUFillTextVector( &PORT_LINE, &PORT_SETTING, 1, getDeviceName( ), "DEVICE_PORT", "Ports", COMMUNICATION_TAB, IP_RW, 60, IPS_IDLE );

	//--------------------------------------------------------------------------------
	// Spectrograph Settings
	//--------------------------------------------------------------------------------

	// IUFillNumber(&_settings_num[0], "GRATING", "Grating [lines/mm]", "%.2f", 0, 1000, 0, 79);
	// IUFillNumber(&_settings_num[1], "INCIDENCE_ANGLE_ALPHA", "Incidence angle alpha [degrees]", "%.2f", 0, 90, 0, 62.2);
	// IUFillNumberVector(&SettingsNP, SettingsN, 5, getDeviceName(), "SPECTROGRAPH_SETTINGS", "Settings", "Spectrograph settings", IP_RW, 60, IPS_IDLE);

	setDriverInterface( SPECTROGRAPH_INTERFACE );
	return true;
}

void ShelyakUsis::ISGetProperties( const char* dev )
{
	INDI::DefaultDevice::ISGetProperties( dev );
	defineProperty( &PORT_LINE );
	// defineProperty(&SettingsNP);
	loadConfig( true, PORT_SETTING.name );
}

void ShelyakUsis::scanProperties( )
{
	UsisResponse rsp;

	if ( sendCmd( &rsp, "INFO;PROPERTY_COUNT;" ) ) {
		int nprops = atoi( rsp.parts[4] );
		// LOGF_INFO("driver has %d properties.", nprops );

		for ( int i = 0; i < nprops; i++ ) {
			// if the value is readonly, skip it.
			sendCmd( &rsp, "INFO;PROPERTY_ATTR_MODE;%d;0", i );
			if ( strcmp( rsp.parts[4], "RO" ) == 0 ) {
				continue;
			}

			UsisProperty* p = new UsisProperty;
			p->type = _undefined;
			p->next = _props;
			_props = p;

			sendCmd( &rsp, "INFO;PROPERTY_NAME;%d", i );
			strncpy( p->name, rsp.parts[4], MAX_NAME_LENGTH + 1 );

			sendCmd( &rsp, "INFO;PROPERTY_TYPE;%d", i );
			if ( strcmp( rsp.parts[4], "TEXT" ) == 0 ) {
				p->type = _text;
			}
			else if ( strcmp( rsp.parts[4], "ENUM" ) == 0 ) {
				p->type = _enum;
				p->enm._evals = nullptr;
			}
			else if ( strcmp( rsp.parts[4], "FLOAT" ) == 0 ) {
				p->type = _number;
				p->num.minVal = -9999.0;
				p->num.maxVal = +9999.0;
			}
			else {
				// unknown type
				delete p;
				continue;
			}

			if ( sendCmd( &rsp, "INFO;PROPERTY_ATTR_COUNT;%d", i ) ) {
				int nattr = atoi( rsp.parts[4] );
				// fprintf( stderr, "property %s has %d attrs.\n", p->name, nattr );
				// LOGF_INFO("property %s has %d attrs.", p->name, nattr );

				for ( int j = 0; j < nattr; j++ ) {
					sendCmd( &rsp, "INFO;PROPERTY_ATTR_NAME;%d;%d", i, j );

					if ( p->type == _number ) { // float

						if ( strcmp( rsp.parts[4], "MIN" ) == 0 ) {
							sendCmd( &rsp, "GET;%s;MIN", p->name );
							p->num.minVal = _atof( rsp.parts[4] );
						}
						else if ( strcmp( rsp.parts[4], "MAX" ) == 0 ) {
							sendCmd( &rsp, "GET;%s;MAX", p->name );
							p->num.maxVal = _atof( rsp.parts[4] );
						}
					}
					else if ( p->type == _enum ) {
						sendCmd( &rsp, "INFO;PROPERTY_ATTR_ENUM_COUNT;%d;%d", i, j );
						int nenum = atoi( rsp.parts[4] );

						
						

						for( int k=0; k<nenum; k++ ) {
							sendCmd( &rsp, "INFO;PROPERTY_ATTR_ENUM_VALUE;%d;%d", i, k );

							UsisEnum* e = new UsisEnum;
							strncpy( e->name, rsp.parts[4], sizeof(e->name) );
							e->value = k;
							e->next = p->enm._evals;
							p->enm._evals = e; 
						}
					}
				}
			}

			// get value
			sendCmd( &rsp, "GET;%s;VALUE", p->name );

			if ( p->type == _text ) {
				strncpy( p->text.value, rsp.parts[4], sizeof(p->text.value) );
			}
			else if ( p->type == _enum ) {
				strncpy( p->enm.value, rsp.parts[4], sizeof(p->enm.value) );
			}
			else if ( p->type == _number ) {
				p->num.value = _atof( rsp.parts[4] );
			}

			createProperty( p );
		}
	}
}

void ShelyakUsis::createProperty( UsisProperty* prop )
{
	if ( prop->type == _text ) {
		IUFillText( &prop->text._val, prop->name, prop->name, prop->text.value );
		IUFillTextVector( &prop->text._vec, &prop->text._val, 1, getDeviceName( ), prop->name, prop->name, "USIS", IP_RW, 60, IPS_IDLE );
		defineProperty( &prop->text._vec );
	}
	else if( prop->type==_enum ) {
		ISwitch switches[32];
		UsisEnum* e = prop->enm._evals;
		int nsw = 0;
		
		while( nsw<32 && e ) {
			IUFillSwitch( &e->_val, e->name, e->name, ISS_ON );
			switches[nsw] = e->_val;
			nsw++;
			e = e->next;
		}

		IUFillSwitchVector( &prop->enm._vec, switches, nsw, getDeviceName( ), prop->name, prop->name, "USIS", IP_RW, ISR_1OFMANY, 60, IPS_IDLE );
		defineProperty( &prop->enm._vec );
	}
	else if ( prop->type == _number ) {
		IUFillNumber( &prop->num._val, prop->name, prop->name, "%.2f", prop->num.minVal, prop->num.maxVal, 0.01, prop->num.value );
		IUFillNumberVector( &prop->num._vec, &prop->num._val, 1, getDeviceName( ), prop->name, prop->name, "USIS", IP_RW, 60, IPS_IDLE );
		defineProperty( &prop->num._vec );
	}
}


void ShelyakUsis::releaseProperty( UsisProperty* p ) {

	if( p->type==_enum ) {
		UsisEnum* e = p->enm._evals;
		while( e ) {
			UsisEnum* n = e->next;
			deleteProperty( e->name );
			delete e;
			e = n;
		}
	}

	deleteProperty( p->name );
}



bool ShelyakUsis::sendCmd( UsisResponse* rsp, const char* text, ... )
{
	va_list lst;
	va_start( lst, text );

	bool rc = _send( text, lst ) && _receive( rsp );

	va_end( lst );
	return rc;
}

bool ShelyakUsis::_send( const char* text, va_list lst )
{
	char buf[MAX_FRAME_LENGTH+1];
	vsnprintf( buf, MAX_FRAME_LENGTH, text, lst );

	fprintf( stderr, "> sending %s\n", buf );
	LOGF_INFO("> sending %s", buf );

	strcat( buf, "\n" );

	int rc, nbytes_written;
	if ( ( rc = tty_write( _serialPort, buf, strlen( buf ), &nbytes_written ) ) != TTY_OK ) // send the bytes to the spectrograph
	{
		char errmsg[MAXRBUF];
		tty_error_msg( rc, errmsg, MAXRBUF );
		LOGF_ERROR( "error: %s.", errmsg );
		return false;
	}

	return true;
}

bool ShelyakUsis::_receive( UsisResponse* rsp )
{
	int rc, nread;
	if ( ( rc = tty_nread_section( _serialPort, rsp->buffer, sizeof( rsp->buffer ), '\n', 100, &nread ) ) != TTY_OK ) // send the bytes to the spectrograph
	{
		char errmsg[MAXRBUF];
		tty_error_msg( rc, errmsg, MAXRBUF );
		LOGF_ERROR( "error: %s.", errmsg );
		return false;
	}

	rsp->buffer[nread] = 0;

	fprintf( stderr, "< received %s\n", rsp->buffer );
	LOGF_INFO("< received %s", rsp->buffer );

	char* p = rsp->buffer;
	char* e = p + nread;

	// trim '\n' & ' '
	while ( e >= p && ( e[-1] == '\n' || e[-1] == ' ' ) ) {
		*e--;
		*e = 0;
	}

	rsp->pcount = 0;
	rsp->parts[rsp->pcount++] = p;

	while ( p < e && rsp->pcount < 5 ) {
		if ( *p == ';' ) {
			*p = 0;
			rsp->parts[rsp->pcount++] = p + 1;
		}

		p++;
	}

	if ( strcmp( rsp->parts[0], "M00" ) != 0 ) {
		return false;
	}

	return true;
}

/**
 *
 */

bool ShelyakUsis::updateProperties( )
{
	INDI::DefaultDevice::updateProperties( );
	if ( isConnected( ) ) {
		scanProperties( );
	}
	else {
		UsisProperty* p = _props;
		while ( p ) {
			UsisProperty* n = p->next;
			releaseProperty( p );
			delete p;
			p = n;
		}

		_props = nullptr;
	}

	return true;
}

/**
 * Handle a request to change a switch. 
 **/

bool ShelyakUsis::ISNewSwitch( const char* dev, const char* name, ISState* states, char* names[], int n )
{
	//for( int i=0; i<n; i++ ) {
	//	fprintf( stderr, "set switch: %s = %d\n", names[i], states[i] ) ;
	//}

	if ( n==1 && dev && strcmp( dev, getDeviceName( ) ) == 0 ) {
		for ( UsisProperty* p = _props; p; p = p->next ) {
			if ( p->type == _enum && strcmp( p->name, name ) == 0 ) {

				for( UsisEnum* e = p->enm._evals; e; e=e->next ) {

					if( strcmp( e->name, names[0] )==0 ) {
						UsisResponse rsp;
						sendCmd( &rsp, "SET;%s;VALUE;%s", name, names[0] );
						return true;
					}
				}
			}
		}
	}

	return INDI::DefaultDevice::ISNewSwitch( dev, name, states, names, n ); // send it to the parent classes
}

/**
 * Handle a request to change text.
 **/

bool ShelyakUsis::ISNewText( const char* dev, const char* name, char* texts[], char* names[], int n )
{
	if ( dev && strcmp( dev, getDeviceName( ) ) == 0 ) {
		if ( strcmp( PORT_SETTING.name, name ) == 0 ) {
			IUUpdateText( &PORT_LINE, texts, names, n );
			PORT_LINE.s = IPS_OK;
			IDSetText( &PORT_LINE, nullptr );
			return true;
		}
	}

	return INDI::DefaultDevice::ISNewText( dev, name, texts, names, n );
}

/**
 *
 */

bool ShelyakUsis::ISNewNumber( const char* dev, const char* name, double values[], char* names[], int n )
{
	if ( dev && strcmp( dev, getDeviceName( ) ) == 0 && n == 1 ) {

		for ( UsisProperty* p = _props; p; p = p->next ) {
			if ( p->type == _number && strcmp( p->name, names[0] ) == 0 ) {
				UsisResponse rsp;
				sendCmd( &rsp, "SET;%s;VALUE;%lf", names[0], values[0] );
				return true;
			}
		}
	}

	return INDI::DefaultDevice::ISNewNumber( dev, name, values, names, n );
}


ShelyakUsis* usis = new ShelyakUsis( );
