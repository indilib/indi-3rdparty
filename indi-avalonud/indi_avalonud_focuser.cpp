/*
    Avalon Unified Driver Focuser

    Copyright (C) 2020,2023

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "config.h"

#include "indicom.h"

#include <cstring>
#include <termios.h>
#include <stdarg.h>
#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif
#include <memory>
#include <chrono>
#include <pthread.h>
#include <zmq.h>

#include "indi_avalonud_focuser.h"


#define STEPMACHINE_DRIVER_NUM 2


using json = nlohmann::json;

const int IPport = 5450;

static char device_str[MAXINDIDEVICE] = "AvalonUD Focuser";


static std::unique_ptr<AUDFOCUSER> focuser(new AUDFOCUSER());

void ISGetProperties(const char *dev)
{
    focuser->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    focuser->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    focuser->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    focuser->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    focuser->ISSnoopDevice(root);
}

AUDFOCUSER::AUDFOCUSER()
{
    setVersion(AVALONUD_VERSION_MAJOR,AVALONUD_VERSION_MINOR);

    context = zmq_ctx_new();
    setSupportedConnections( CONNECTION_NONE );
    FI::SetCapability(FOCUSER_CAN_ABORT |
                      FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_SYNC);
}

AUDFOCUSER::~AUDFOCUSER()
{
//    zmq_ctx_term(context);
}

bool AUDFOCUSER::initProperties()
{
    INDI::Focuser::initProperties();

    ConfigTP[0].fill("ADDRESS", "Address", "127.0.0.1");
    ConfigTP.fill(getDeviceName(), "DEVICE_ADDRESS", "Server", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    // HW type
    HWTypeTP[0].fill("HW_TYPE", "Controller Type", "");
    HWTypeTP.fill(getDeviceName(), "HW_TYPE_INFO", "Type", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // HW identifier
    HWIdentifierTP[0].fill("HW_IDENTIFIER", "HW Identifier", "");
    HWIdentifierTP.fill(getDeviceName(), "HW_IDENTIFIER_INFO", "Identifier", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // low level SW info
    LowLevelSWTP[LLSW_NAME].fill("LLSW_NAME", "Name", "");
    LowLevelSWTP[LLSW_VERSION].fill("LLSW_VERSION", "Version", "--");
    LowLevelSWTP.fill(getDeviceName(), "LLSW_INFO", "LowLevel SW", INFO_TAB, IP_RO, 60, IPS_IDLE);

    addDebugControl();

    // Set limits as per documentation
    FocusAbsPosNP[0].setMin(0);
    FocusAbsPosNP[0].setMax(999999);
    FocusAbsPosNP[0].setStep(1000);

    FocusRelPosNP[0].setMin(0);
    FocusRelPosNP[0].setMax(999);
    FocusRelPosNP[0].setStep(100);

    FocusSpeedNP[0].setMin(1);
    FocusSpeedNP[0].setMax(254);
    FocusSpeedNP[0].setStep(10);

    pthread_mutex_init( &connectionmutex, NULL );

    return true;
}

void AUDFOCUSER::ISGetProperties(const char *dev)
{
    INDI::Focuser::ISGetProperties(dev);

    defineProperty(ConfigTP);
    loadConfig(true,ConfigTP.getName());
}

bool AUDFOCUSER::updateProperties()
{
    if (isConnected())
    {
        // Read these values before defining focuser interface properties
        readPosition();
    }

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        // Settings
        defineProperty(HWTypeTP);
        defineProperty(HWIdentifierTP);
        defineProperty(LowLevelSWTP);

        LOG_INFO("Focuser is ready");
    }
    else
    {
        deleteProperty(HWTypeTP);
        deleteProperty(HWIdentifierTP);
        deleteProperty(LowLevelSWTP);
    }

    return true;
}

bool AUDFOCUSER::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev,getDeviceName()))
    {
        // TCP Server settings
        if (ConfigTP.isNameMatch(name))
        {
            if ( isConnected() && strcmp(IPaddress,texts[0]) ) {
                DEBUG(INDI::Logger::DBG_WARNING, "Please Disconnect before changing IP address");
                return false;
            }
            ConfigTP.update(texts, names, n);
            ConfigTP.setState(IPS_OK);
            ConfigTP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewText(dev, name, texts, names, n);
}

bool AUDFOCUSER::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool AUDFOCUSER::Connect()
{
    char addr[1024], *answer;
    int timeout = 500;

    if (isConnected())
        return true;

    IPaddress = strdup(ConfigTP[0].text);

    DEBUGF(INDI::Logger::DBG_SESSION, "Attempting to connect %s focuser...",IPaddress);

    requester = zmq_socket(context, ZMQ_REQ);
    zmq_setsockopt(requester, ZMQ_RCVTIMEO, &timeout, sizeof(timeout) );
    snprintf( addr, sizeof(addr), "tcp://%s:%d", IPaddress, IPport );
    zmq_connect(requester, addr);

    answer = sendRequest("DISCOVER");
    if ( answer ) {
        if ( !strcmp(answer,"stepMachine") ) {
            free(answer);
            answer = sendRequest("INFOALL");
            if ( answer ) {
                json j;
                std::string sHWt,sHWi,sFWv;

                j = json::parse(answer,nullptr,false);
                free(answer);
                if ( j.is_discarded() ||
                        !j.contains("HWType") ||
                        !j.contains("HWFeatures") ||
                        !j.contains("HWIdentifier") ||
                        !j.contains("firmwareVersion") )
                {
                    zmq_close(requester);
                    DEBUGF(INDI::Logger::DBG_ERROR, "Communication with %s focuser failed",IPaddress);
                    free(IPaddress);
                    return false;
                }

                j["HWType"].get_to(sHWt);
                HWTypeTP[0].setText(sHWt);
                HWTypeTP.apply();
                j["HWFeatures"].get_to(features);
                j["HWIdentifier"].get_to(sHWi);
                HWIdentifierTP[0].setText(sHWi);
                HWIdentifierTP.apply();
                LowLevelSWTP[LLSW_NAME].setText("stepMachine");
                j["firmwareVersion"].get_to(sFWv);
                LowLevelSWTP[LLSW_VERSION].setText(sFWv);
                LowLevelSWTP.apply();
            }
            if ( !(features & 0x0100) ) {
                zmq_close(requester);
                DEBUGF(INDI::Logger::DBG_ERROR, "Focuser features not supported by %s hardware",IPaddress);
                free(IPaddress);
                return false;
            }
        } else {
            zmq_close(requester);
            DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect %s focuser",IPaddress);
            free(answer);
            free(IPaddress);
            return false;
        }
    } else {
        zmq_close(requester);
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect %s focuser",IPaddress);
        free(IPaddress);
        return false;
    }

    tid = SetTimer(getCurrentPollingPeriod());

    DEBUGF(INDI::Logger::DBG_SESSION, "Successfully connected %s focuser",IPaddress);
    return true;
}

bool AUDFOCUSER::Disconnect()
{
    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Disconnect called before driver connection");
        return true;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Attempting to disconnect focuser...");

    zmq_close(requester);

    RemoveTimer( tid );

    free(IPaddress);

    DEBUG(INDI::Logger::DBG_SESSION, "Successfully disconnected focuser");

    return true;
}

IPState AUDFOCUSER::MoveAbsFocuser(uint32_t targetTicks)
{
    char *answer;

    if ( !isConnected() ) {
        DEBUG(INDI::Logger::DBG_WARNING,"Positioning required before driver connection");
        return IPS_ALERT;
    }

    DEBUGF(INDI::Logger::DBG_SESSION,"Start positioning focuser at %ustep ...",targetTicks);
    answer = sendCommand("ABSOLUTE %d %d 0 0",STEPMACHINE_DRIVER_NUM,targetTicks);
    if ( !answer ) {
        DEBUGF(INDI::Logger::DBG_SESSION,"Start positioning focuser at %ustep completed",targetTicks);
        return IPS_BUSY;
    }
    DEBUGF(INDI::Logger::DBG_WARNING,"Start positioning focuser at %ustep failed due to %s",targetTicks,answer);
    free(answer);
    return IPS_ALERT;
}

IPState AUDFOCUSER::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    char *answer;

    if ( !isConnected() ) {
        DEBUG(INDI::Logger::DBG_WARNING,"Positioning required before driver connection");
        return IPS_OK;
    }

    DEBUGF(INDI::Logger::DBG_SESSION,"Start moving focuser of %ustep ...",ticks);
    if ( dir == FOCUS_INWARD )
        answer = sendCommand("RELATIVE %d -%lu 0 0",STEPMACHINE_DRIVER_NUM,ticks);
    else
        answer = sendCommand("RELATIVE %d %lu 0 0",STEPMACHINE_DRIVER_NUM,ticks);
    if ( !answer ) {
        DEBUGF(INDI::Logger::DBG_SESSION,"Start moving focuser of %ustep completed",ticks);
        return IPS_BUSY;
    }
    DEBUGF(INDI::Logger::DBG_WARNING,"Start moving focuser of %ustep failed due to %s",ticks,answer);
    free(answer);
    return IPS_ALERT;
}

bool AUDFOCUSER::SyncFocuser(uint32_t ticks)
{
    char *answer;

    if ( !isConnected() ) {
        DEBUG(INDI::Logger::DBG_WARNING,"Sync required before driver connection");
        return true;
    }

    DEBUGF(INDI::Logger::DBG_SESSION,"Sync focuser position to %ustep ...",ticks);
    answer = sendCommand("SYNC %d %ld",STEPMACHINE_DRIVER_NUM,ticks);
    if ( !answer ) {
        DEBUGF(INDI::Logger::DBG_SESSION,"Sync focuser position to %ustep completed",ticks);
        return true;
    }
    DEBUGF(INDI::Logger::DBG_SESSION,"Sync focuser position to %ustep failed due to %s",ticks,answer);
    free(answer);
    return false;
}

bool AUDFOCUSER::AbortFocuser()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Focuser abort ...");

    if (isConnected()) {
        char *answer;
        answer = sendCommand("STOP %d",STEPMACHINE_DRIVER_NUM);
        free(answer);
    } else {
        DEBUG(INDI::Logger::DBG_WARNING,"Abort required before driver connection");
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Focuser abort completed");

    return true;
}

void AUDFOCUSER::TimerHit()
{
    if (isConnected() == false)
        return;

    // Read the current position
    readPosition();

    // Check if we have a pending motion
    // if isMoving() is false, then we stopped, so we need to set the Focus Absolute
    // and relative properties to OK
    if ( (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY) && ( statusCode >= STILL ) )
    {
        FocusAbsPosNP.setState(IPS_OK);
        FocusAbsPosNP.apply();
        FocusRelPosNP.setState(IPS_OK);
        FocusRelPosNP.apply();
    }
    // If there was a different between last and current positions, let's update all clients
    else if (currentPosition != FocusAbsPosNP[0].getValue())
    {
        FocusAbsPosNP[0].setValue(currentPosition);
        FocusAbsPosNP.apply();
    }

    SetTimer(getCurrentPollingPeriod());
}

bool AUDFOCUSER::readPosition()
{
    char *answer;

    answer = sendRequest("STATUS %d",STEPMACHINE_DRIVER_NUM);
    if ( answer ) {
        json j;

        j = json::parse(answer,nullptr,false);
        free(answer);
        if ( j.is_discarded() ||
                !j.contains("position_step") ||
                !j.contains("statusCode") )
        {
            DEBUG(INDI::Logger::DBG_WARNING,"Status communication error");
            return false;
        }
        j["position_step"].get_to(currentPosition);
        j["statusCode"].get_to(statusCode);
        return true;
    }

    return false;
}

bool AUDFOCUSER::saveConfigItems(FILE *fp)
{
    // We need to reserve and save address mode
    // so that the next time the driver is loaded, it is remembered and applied.
    ConfigTP.save(fp);

    return INDI::Focuser::saveConfigItems(fp);
}

const char *AUDFOCUSER::getDefaultName()
{
    return device_str;
}

char* AUDFOCUSER::sendCommand(const char *fmt, ... )
{
    va_list ap;
    char buffer[4096], answer[4096], addr[1024];
    int rc,retries;
    zmq_pollitem_t item;

    va_start( ap, fmt );
    vsnprintf( buffer, sizeof(buffer), fmt, ap );
    va_end( ap );

    pthread_mutex_lock( &connectionmutex );
    retries = 3;
    do {
        zmq_send(requester, buffer, strlen(buffer), 0);
        item = { requester, 0, ZMQ_POLLIN, 0 };
        rc = zmq_poll( &item, 1, 500 ); // ms
        if ( ( rc >= 0 ) && ( item.revents & ZMQ_POLLIN ) ) {
            // communication succeeded
            rc = zmq_recv(requester, answer, sizeof(answer), 0);
            if ( rc >= 0 ) {
                pthread_mutex_unlock( &connectionmutex );
                answer[MIN(rc,(int)sizeof(answer)-1)] = '\0';
                if ( !strncmp(answer,"OK",2) )
                    return NULL;
                if ( !strncmp(answer,"ERROR:",6) )
                    return strdup(answer+6);
                return strdup("SYNTAXERROR");
            }
        }
        zmq_close(requester);
        requester = zmq_socket(context, ZMQ_REQ);
        snprintf( addr, sizeof(addr), "tcp://%s:%d", IPaddress, IPport );
        zmq_connect(requester, addr);
    } while ( --retries );
    pthread_mutex_unlock( &connectionmutex );
    DEBUG(INDI::Logger::DBG_WARNING, "No answer from driver");
    return strdup("COMMUNICATIONERROR");
}

char* AUDFOCUSER::sendRequest(const char *fmt, ... )
{
    va_list ap;
    char buffer[4096], answer[4096], addr[1024];
    int rc,retries;
    zmq_pollitem_t item;

    va_start( ap, fmt );
    vsnprintf( buffer, sizeof(buffer), fmt, ap );
    va_end( ap );

    pthread_mutex_lock( &connectionmutex );
    retries = 3;
    do {
        zmq_send(requester, buffer, strlen(buffer), 0);
        item = { requester, 0, ZMQ_POLLIN, 0 };
        rc = zmq_poll( &item, 1, 500 ); // ms
        if ( ( rc >= 0 ) && ( item.revents & ZMQ_POLLIN ) ) {
            // communication succeeded
            rc = zmq_recv(requester, answer, sizeof(answer), 0);
            if ( rc >= 0 ) {
                pthread_mutex_unlock( &connectionmutex );
                answer[MIN(rc,(int)sizeof(answer)-1)] = '\0';
                return strdup(answer);
            }
        }
        zmq_close(requester);
        requester = zmq_socket(context, ZMQ_REQ);
        snprintf( addr, sizeof(addr), "tcp://%s:%d", IPaddress, IPport );
        zmq_connect(requester, addr);
    } while ( --retries );
    pthread_mutex_unlock( &connectionmutex );
    DEBUG(INDI::Logger::DBG_WARNING, "No answer from driver");
    return strdup("COMMUNICATIONERROR");
}
