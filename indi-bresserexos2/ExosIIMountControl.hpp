/*
 * ExosIIMountControl.hpp
 *
 * Copyright 2020 Kevin Krüger <kkevin@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#ifndef _EXOSIIMOUNTCONTROL_H_INCLUDED_
#define _EXOSIIMOUNTCONTROL_H_INCLUDED_

#include <cstdint>
#include <vector>
#include <string>
#include <limits>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "Config.hpp"

#include "StateMachine.hpp"
#include "SerialDeviceControl/CriticalData.hpp"
#include "SerialDeviceControl/SerialCommand.hpp"
#include "SerialDeviceControl/SerialCommandTransceiver.hpp"
#include "SerialDeviceControl/INotifyPointingCoordinatesReceived.hpp"

//The manual states a tracking speed for 0.004°/s everything above is considered slewing.
#define TRACK_SLEW_THRESHOLD (0.0045)

#define EXPR_TO_STRING(x) #x

namespace TelescopeMountControl
{
//enum representing the telescope mount state
enum TelescopeMountState
{
    //intial state, no serial connection established
    Disconnected = 0,
    //intial state if the serial connection was established,
    //the mount did not report any pointing coordinates yet.
    //if an error occurs, the telescope also will be in this state.
    Unknown = 1,
    //The driver received at least one response from the mount,
    //indicating the communication is working, and the mount accepts commands.
    Connected = 2,
    //If the user issues the park command the telescope likely needs to slew to the parking position,
    //this is determined by the differentials of the positions information send by the controller.
    ParkingIssued = 3,
    //If the status report messages arrive and the telescope is not moving, this is the assumed state.
    //the telescope is assumed in park/initial position according to manual
    //or if the "park" command is issued.
    Parked = 4,
    //The controller autonomously desides the motion speeds, and does not report any "state".
    //This state is assumed if the motion speeds exceed a certain threshold.
    Slewing = 5,
    //This state is assumed if the telescope is moves below the slewing threshold.
    //its also the default when a "goto" is issued, since the telescope controller automatically tracks the issued coordinates.
    Tracking = 6,
    //when tracking an object, move to a direction.
    MoveWhileTracking = 7,
    //This state is reached when issuing the stop command while slewing or tracking.
    Idle = 8,
    //The error state of the telescope any none defined transition will reset to this state.
    FailSafe = 9,
};

//Enum reqresenting the virtuals signal the telescope issues.
enum TelescopeSignals
{
    //connect to the telescope.
    Connect = TelescopeMountState::FailSafe + 1, //need to be exclusive from states so there is no hazzling with communtativity of the xor operator of hashing functions.
    //disconnect from the telescope.
    Disconnect,
    //request the geolocation initially.
    RequestedGeoLocationReceived,
    //when the geolocation is requested this signal gets issued if the controller reports the pointing coordinates.
    InitialPointingCoordinatesReceived,
    //stop any motion of the telescope.
    Stop,
    //goto a position and track the position when reached.
    GoTo,
    //park the telescope.
    Park,
    //issued when the parking position is reached.
    ParkingPositionReached,
    //motion above tracking threshold.
    Slew,
    //motion below tracking threshold.
    Track,
    //motion to tracking target, when reached this is signaled.
    TrackingTargetReached,
    //move a certain direction while tracking.
    StartMotion,
    //stop moving in a certain direction while tracking.
    StopMotion,
    //used as a token to represent an initialized yet invalid signal.s
    INVALID,
};

//type definition of the state machine type for convinience.
typedef StateMachine<TelescopeMountState, TelescopeSignals, IStateNotification<TelescopeMountState, TelescopeSignals>>
        MountStateMachine;

//store the motion state while tracking.
struct MotionState
{
    //move in what direction
    SerialDeviceControl::SerialCommandID MotionDirection;
    //how many messages per second
    uint16_t CommandsPerSecond;
};

//These types have to inherit/implement:
//-The ISerialInterface.hpp as Interface type.
template<class InterfaceType>
class ExosIIMountControl :
    public SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>,
            public SerialDeviceControl::INotifyPointingCoordinatesReceived,
            public IStateNotification<TelescopeMountState, TelescopeSignals>
{
    public:
        //create a exos controller using a reference of a particular serial implementation.
        ExosIIMountControl(InterfaceType &interfaceImplementation) :
            SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>
                    (interfaceImplementation, *this),
                    mIsMotionControlThreadRunning(false),
                    mIsMotionControlRunning(false),
                    mMountStateMachine(*this, TelescopeMountState::Disconnected, TelescopeMountState::FailSafe)
        {
            SerialDeviceControl::EquatorialCoordinates initialCoordinates;
            initialCoordinates.RightAscension = std::numeric_limits<float>::quiet_NaN();
            initialCoordinates.Declination    = std::numeric_limits<float>::quiet_NaN();

            mCurrentPointingCoordinates.Set(initialCoordinates);
            mSiteLocationCoordinates.Set(initialCoordinates);

            MotionState initialState;
            initialState.MotionDirection = SerialDeviceControl::SerialCommandID::NULL_COMMAND_ID;
            initialState.CommandsPerSecond = 0;

            mMotionState.Set(initialState);

            //initialize statemachine:
            mMountStateMachine.AddFinalState(TelescopeMountState::Disconnected);

            //build transition table:
            mMountStateMachine.AddTransition(TelescopeMountState::Disconnected, TelescopeSignals::Disconnect,
                                             TelescopeMountState::Disconnected);
            mMountStateMachine.AddTransition(TelescopeMountState::Disconnected, TelescopeSignals::Connect,
                                             TelescopeMountState::Unknown);

            mMountStateMachine.AddTransition(TelescopeMountState::Unknown, TelescopeSignals::Disconnect,
                                             TelescopeMountState::Disconnected);
            mMountStateMachine.AddTransition(TelescopeMountState::Unknown, TelescopeSignals::InitialPointingCoordinatesReceived,
                                             TelescopeMountState::Connected);
            mMountStateMachine.AddTransition(TelescopeMountState::Unknown, TelescopeSignals::RequestedGeoLocationReceived,
                                             TelescopeMountState::Connected);

            mMountStateMachine.AddTransition(TelescopeMountState::Connected, TelescopeSignals::Connect, TelescopeMountState::Connected);
            mMountStateMachine.AddTransition(TelescopeMountState::Connected, TelescopeSignals::RequestedGeoLocationReceived,
                                             TelescopeMountState::Parked);
            mMountStateMachine.AddTransition(TelescopeMountState::Connected, TelescopeSignals::InitialPointingCoordinatesReceived,
                                             TelescopeMountState::Parked);
            mMountStateMachine.AddTransition(TelescopeMountState::Connected, TelescopeSignals::Track, TelescopeMountState::Tracking);
            mMountStateMachine.AddTransition(TelescopeMountState::Connected, TelescopeSignals::Slew, TelescopeMountState::Slewing);
            mMountStateMachine.AddTransition(TelescopeMountState::Connected, TelescopeSignals::Disconnect,
                                             TelescopeMountState::Disconnected);

            mMountStateMachine.AddTransition(TelescopeMountState::Parked, TelescopeSignals::Park, TelescopeMountState::Parked);
            mMountStateMachine.AddTransition(TelescopeMountState::Parked, TelescopeSignals::Disconnect,
                                             TelescopeMountState::Disconnected);
            mMountStateMachine.AddTransition(TelescopeMountState::Parked, TelescopeSignals::Stop, TelescopeMountState::Parked);
            mMountStateMachine.AddTransition(TelescopeMountState::Parked, TelescopeSignals::GoTo, TelescopeMountState::Slewing);

            mMountStateMachine.AddTransition(TelescopeMountState::Idle, TelescopeSignals::Stop, TelescopeMountState::Idle);
            mMountStateMachine.AddTransition(TelescopeMountState::Idle, TelescopeSignals::GoTo, TelescopeMountState::Slewing);
            mMountStateMachine.AddTransition(TelescopeMountState::Idle, TelescopeSignals::Park, TelescopeMountState::ParkingIssued);
            mMountStateMachine.AddTransition(TelescopeMountState::Idle, TelescopeSignals::Disconnect,
                                             TelescopeMountState::Disconnected);

            mMountStateMachine.AddTransition(TelescopeMountState::ParkingIssued, TelescopeSignals::Park,
                                             TelescopeMountState::ParkingIssued);
            mMountStateMachine.AddTransition(TelescopeMountState::ParkingIssued, TelescopeSignals::Slew,
                                             TelescopeMountState::ParkingIssued);
            mMountStateMachine.AddTransition(TelescopeMountState::ParkingIssued, TelescopeSignals::Track,
                                             TelescopeMountState::ParkingIssued);
            mMountStateMachine.AddTransition(TelescopeMountState::ParkingIssued, TelescopeSignals::ParkingPositionReached,
                                             TelescopeMountState::Parked);
            mMountStateMachine.AddTransition(TelescopeMountState::ParkingIssued, TelescopeSignals::Stop, TelescopeMountState::Idle);
            mMountStateMachine.AddTransition(TelescopeMountState::ParkingIssued, TelescopeSignals::Disconnect,
                                             TelescopeMountState::Disconnected);

            mMountStateMachine.AddTransition(TelescopeMountState::Slewing, TelescopeSignals::Stop, TelescopeMountState::Idle);
            mMountStateMachine.AddTransition(TelescopeMountState::Slewing, TelescopeSignals::GoTo, TelescopeMountState::Slewing);
            mMountStateMachine.AddTransition(TelescopeMountState::Slewing, TelescopeSignals::Track, TelescopeMountState::Tracking);
            mMountStateMachine.AddTransition(TelescopeMountState::Slewing, TelescopeSignals::Slew, TelescopeMountState::Slewing);
            mMountStateMachine.AddTransition(TelescopeMountState::Slewing, TelescopeSignals::Park, TelescopeMountState::ParkingIssued);
            mMountStateMachine.AddTransition(TelescopeMountState::Slewing, TelescopeSignals::Disconnect,
                                             TelescopeMountState::Disconnected);

            mMountStateMachine.AddTransition(TelescopeMountState::Tracking, TelescopeSignals::Track, TelescopeMountState::Tracking);
            mMountStateMachine.AddTransition(TelescopeMountState::Tracking, TelescopeSignals::Slew, TelescopeMountState::Slewing);
            mMountStateMachine.AddTransition(TelescopeMountState::Tracking, TelescopeSignals::GoTo, TelescopeMountState::Slewing);
            mMountStateMachine.AddTransition(TelescopeMountState::Tracking, TelescopeSignals::Stop, TelescopeMountState::Idle);
            mMountStateMachine.AddTransition(TelescopeMountState::Tracking, TelescopeSignals::StartMotion,
                                             TelescopeMountState::MoveWhileTracking);
            mMountStateMachine.AddTransition(TelescopeMountState::Tracking, TelescopeSignals::Park, TelescopeMountState::ParkingIssued);
            mMountStateMachine.AddTransition(TelescopeMountState::Tracking, TelescopeSignals::Disconnect,
                                             TelescopeMountState::Disconnected);

            mMountStateMachine.AddTransition(TelescopeMountState::MoveWhileTracking, TelescopeSignals::StopMotion,
                                             TelescopeMountState::Tracking);
            mMountStateMachine.AddTransition(TelescopeMountState::MoveWhileTracking, TelescopeSignals::StartMotion,
                                             TelescopeMountState::MoveWhileTracking);
            mMountStateMachine.AddTransition(TelescopeMountState::MoveWhileTracking, TelescopeSignals::Stop, TelescopeMountState::Idle);
            mMountStateMachine.AddTransition(TelescopeMountState::MoveWhileTracking, TelescopeSignals::StopMotion,
                                             TelescopeMountState::Tracking);
            mMountStateMachine.AddTransition(TelescopeMountState::MoveWhileTracking, TelescopeSignals::GoTo,
                                             TelescopeMountState::Slewing);
            mMountStateMachine.AddTransition(TelescopeMountState::MoveWhileTracking, TelescopeSignals::Track,
                                             TelescopeMountState::MoveWhileTracking);
            mMountStateMachine.AddTransition(TelescopeMountState::MoveWhileTracking, TelescopeSignals::Slew,
                                             TelescopeMountState::MoveWhileTracking);
            mMountStateMachine.AddTransition(TelescopeMountState::MoveWhileTracking, TelescopeSignals::Park,
                                             TelescopeMountState::ParkingIssued);
            mMountStateMachine.AddTransition(TelescopeMountState::MoveWhileTracking, TelescopeSignals::Disconnect,
                                             TelescopeMountState::Disconnected);

            mMountStateMachine.Reset();
        }

        virtual ~ExosIIMountControl()
        {
            SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>::Stop();
        }

        //open the serial connection and start the serial reporting.
        virtual bool Start()
        {
            mMountStateMachine.Reset();

            mMountStateMachine.DoTransition(TelescopeSignals::Connect);

            SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>::Start();

            mMotionCommandThread = std::thread(&ExosIIMountControl<InterfaceType>::MotionControlThreadFunction, this);

            return true;
        }

        //Stop the serial reporting and close the serial port.
        virtual bool Stop()
        {
            if(mIsMotionControlThreadRunning.Get())
            {
                mIsMotionControlThreadRunning.Set(false);
                StopMotionToDirection();
                mMotionCommandThread.join();
            }

            bool rc = DisconnectSerial();

            rc |= SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>::Stop();

            return rc;
        }

        bool StartMotionToDirection(SerialDeviceControl::SerialCommandID direction, uint16_t commandsPerSecond)
        {
            //this only works while tracking a target
            {
                std::lock_guard<std::mutex> notifyLock(mMotionCommandControlMutex);

                MotionState initialState;
                initialState.MotionDirection = direction;
                initialState.CommandsPerSecond = commandsPerSecond;

                mMotionState.Set(initialState);

                mIsMotionControlRunning.Set(true);
            }

            mMotionControlCondition.notify_all();

            return mMountStateMachine.DoTransition(TelescopeSignals::StartMotion);
        }

        bool StopMotionToDirection()
        {
            //this changes back to tracking
            MotionState stopState;
            stopState.MotionDirection = SerialDeviceControl::SerialCommandID::NULL_COMMAND_ID;
            stopState.CommandsPerSecond = 0;

            mMotionState.Set(stopState);

            mIsMotionControlRunning.Set(false);

            mMotionControlCondition.notify_all();

            if(mMountStateMachine.CurrentState() != TelescopeMountState::MoveWhileTracking)
            {
                std::cerr << "INFO: motion already disabled." << std::endl;
                return true;
            }
            else
            {
                return mMountStateMachine.DoTransition(TelescopeSignals::StopMotion);
            }
        }

        //Disconnect from the mount.
        bool DisconnectSerial()
        {
            std::vector<uint8_t> messageBuffer;
            if(SerialDeviceControl::SerialCommand::GetDisconnectCommandMessage(messageBuffer))
            {
                bool rc = SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>::SendMessageBuffer(
                              &messageBuffer[0], 0, messageBuffer.size());

                return rc && mMountStateMachine.DoTransition(TelescopeSignals::Disconnect);
            }
            else
            {
                std::cerr << "DisconnectSerial: Failed!" << std::endl;
                return false;
            }
        }

        //stop any motion of the telescope.
        bool StopMotion()
        {
            std::vector<uint8_t> messageBuffer;
            if(SerialDeviceControl::SerialCommand::GetStopMotionCommandMessage(messageBuffer))
            {
                bool rc = SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>::SendMessageBuffer(
                              &messageBuffer[0], 0, messageBuffer.size());

                return rc && mMountStateMachine.DoTransition(TelescopeSignals::Stop);
            }
            else
            {
                std::cerr << "StopMotion: Failed!" << std::endl;
                return false;
            }
        }

        //Order to telescope to go to the parking state.
        bool ParkPosition()
        {

            std::vector<uint8_t> messageBuffer;
            if(SerialDeviceControl::SerialCommand::GetParkCommandMessage(messageBuffer))
            {
                bool rc = SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>::SendMessageBuffer(
                              &messageBuffer[0], 0, messageBuffer.size());


                return rc && mMountStateMachine.DoTransition(TelescopeSignals::Park);
            }
            else
            {
                std::cerr << "ParkPosition: Failed!" << std::endl;
                return false;
            }
        }

        //GoTo and track the sky position represented by the equatorial coordinates.
        bool GoTo(float rightAscension, float declination)
        {
            std::vector<uint8_t> messageBuffer;
            if(SerialDeviceControl::SerialCommand::GetGotoCommandMessage(messageBuffer, rightAscension, declination))
            {
                bool rc = SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>::SendMessageBuffer(
                              &messageBuffer[0], 0, messageBuffer.size());

                return rc && mMountStateMachine.DoTransition(TelescopeSignals::GoTo);
            }
            else
            {
                //TODO: error message
                std::cerr << "GoTo: Failed!" << std::endl;
                return false;
            }
        }

        //GoTo and track the sky position represented by the equatorial coordinates.
        bool Sync(float rightAscension, float declination)
        {
            std::vector<uint8_t> messageBuffer;
            if(SerialDeviceControl::SerialCommand::GetSyncCommandMessage(messageBuffer, rightAscension, declination))
            {
                std::cerr << "Sent Sync command!" << std::endl;
                return SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>::SendMessageBuffer(
                           &messageBuffer[0], 0, messageBuffer.size());
                //return rc && mMountStateMachine.DoTransition(TelescopeSignals::GoTo);
            }
            else
            {
                //TODO: error message
                std::cerr << "Sync: Failed!" << std::endl;
                return false;
            }
        }

        //Set the location of the telesope, using decimal latitude and longitude parameters.
        //This does not change to state of the telescope.
        bool SetSiteLocation(float latitude, float longitude)
        {
            std::vector<uint8_t> messageBuffer;
            if(SerialDeviceControl::SerialCommand::GetSetSiteLocationCommandMessage(messageBuffer, latitude, longitude))
            {
                return SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>::SendMessageBuffer(
                           &messageBuffer[0], 0, messageBuffer.size());
            }
            else
            {
                std::cerr << "SetSiteLocation: Failed!" << std::endl;
                return false;
            }
        }

        //Set the location of the telesope, using decimal latitude and longitude parameters.
        //This does not change to state of the telescope.
        bool RequestSiteLocation()
        {
            std::vector<uint8_t> messageBuffer;
            if(SerialDeviceControl::SerialCommand::GetGetSiteLocationCommandMessage(messageBuffer))
            {
                //std::cout << "Message sent!" << std::endl;
                return SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>::SendMessageBuffer(
                           &messageBuffer[0], 0, messageBuffer.size());
            }
            else
            {
                //TODO: error message
                std::cerr << "RequestSiteLocation: Failed!" << std::endl;
                return false;
            }
        }

        //issue the set time command, using date and time parameters. This does not change the state of the telescope.
        bool SetDateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
        {
            std::vector<uint8_t> messageBuffer;
            if(SerialDeviceControl::SerialCommand::GetSetDateTimeCommandMessage(messageBuffer, year, month, day, hour, minute, second))
            {
                return SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>::SendMessageBuffer(
                           &messageBuffer[0], 0, messageBuffer.size());
            }
            else
            {
                //TODO:error message.
                std::cerr << "SetDateTime: Failed!" << std::endl;
                return false;
            }
        }

        template<SerialDeviceControl::SerialCommandID Direction>
        bool MoveDirection()
        {
            std::vector<uint8_t> messageBuffer;
            if(SerialDeviceControl::SerialCommand::GetMoveWhileTrackingCommandMessage(messageBuffer, Direction))
            {
                bool rc = SerialDeviceControl::SerialCommandTransceiver<InterfaceType, TelescopeMountControl::ExosIIMountControl<InterfaceType>>::SendMessageBuffer(
                              &messageBuffer[0], 0, messageBuffer.size());
                return rc && mMountStateMachine.DoTransition(TelescopeSignals::StartMotion);
            }
            else
            {
                //TODO:error message.
                return false;
            }
        }

        bool MoveNorth()
        {
            return MoveDirection<SerialDeviceControl::SerialCommandID::MOVE_NORTH_COMMAND_ID>();
        }

        bool MoveSouth()
        {
            return MoveDirection<SerialDeviceControl::SerialCommandID::MOVE_SOUTH_COMMAND_ID>();
        }

        bool MoveEast()
        {
            return MoveDirection<SerialDeviceControl::SerialCommandID::MOVE_EAST_COMMAND_ID>();
        }

        bool MoveWest()
        {
            return MoveDirection<SerialDeviceControl::SerialCommandID::MOVE_WEST_COMMAND_ID>();
        }


        //Called each time a pair of coordinates was received from the serial interface.
        virtual void OnPointingCoordinatesReceived(float right_ascension, float declination)
        {
            //std::cerr << "Received data : RA: " << right_ascension << " DEC:" << declination << std::endl;

            SerialDeviceControl::EquatorialCoordinates lastCoordinates = GetPointingCoordinates();

            SerialDeviceControl::EquatorialCoordinates coordinatesReceived;
            coordinatesReceived.RightAscension = right_ascension;
            coordinatesReceived.Declination = declination;

            bool coordinatesNotNan = !std::isnan(right_ascension) && !std::isnan(declination);

            mCurrentPointingCoordinates.Set(coordinatesReceived);

            SerialDeviceControl::EquatorialCoordinates delta = SerialDeviceControl::EquatorialCoordinates::Delta(lastCoordinates,
                    coordinatesReceived);
            float absDelta = SerialDeviceControl::EquatorialCoordinates::Absolute(delta);

            TelescopeMountState currentState = mMountStateMachine.CurrentState();
            TelescopeSignals signal = TelescopeSignals::INVALID;

            switch(currentState)
            {
                case TelescopeMountState::Unknown:
                    if(coordinatesNotNan)
                    {
                        signal = TelescopeSignals::InitialPointingCoordinatesReceived;
                    }
                    break;

                case TelescopeMountState::Connected:
                    if(std::isnan(absDelta))
                    {
                        break;
                    }

                    if(!std::isnan(absDelta) && (absDelta > 0.0f))
                    {
                        //see if the telescope is moving initially -> previous/externally triggered motion.
                        if(absDelta > TRACK_SLEW_THRESHOLD)
                        {
                            signal = TelescopeSignals::Slew;
                        }
                        else
                        {
                            signal = TelescopeSignals::Track;
                        }
                    }
                    else
                    {
                        //assume parked otherwise.
                        signal = TelescopeSignals::InitialPointingCoordinatesReceived;
                    }
                    break;

                case TelescopeMountState::ParkingIssued:
                    if(absDelta > 0.0f)
                    {
                        if(absDelta > TRACK_SLEW_THRESHOLD)
                        {
                            signal = TelescopeSignals::Slew;
                        }
                        else
                        {
                            signal = TelescopeSignals::Track;
                        }
                    }
                    else
                    {
                        signal = TelescopeSignals::ParkingPositionReached;
                    }
                    break;

                case TelescopeMountState::Idle:

                    break;

                case TelescopeMountState::Parked:

                    break;

                case TelescopeMountState::Tracking:
                    if(absDelta > 0.0f)
                    {
                        //may be externally triggered motion
                        if(absDelta > TRACK_SLEW_THRESHOLD)
                        {
                            signal = TelescopeSignals::Slew;
                        }
                        else
                        {
                            signal = TelescopeSignals::Track;
                        }
                    }
                    break;

                case TelescopeMountState::Slewing:
                    if(absDelta > 0.0f)
                    {
                        //may be externally triggered motion
                        if(absDelta > TRACK_SLEW_THRESHOLD)
                        {
                            signal = TelescopeSignals::Slew;
                        }
                        else
                        {
                            signal = TelescopeSignals::Track;
                        }
                    }

                default:

                    break;
            }

            if(signal != TelescopeSignals::INVALID)
            {
                mMountStateMachine.DoTransition(signal);
            }
        }

        //Called each time a pair of geo coordinates was received from the serial inferface. This happends only by active request (GET_SITE_LOCATION_COMMAND_ID)
        virtual void OnSiteLocationCoordinatesReceived(float latitude, float longitude)
        {
            std::cerr << "Received data : LAT: " << latitude << " LON:" << longitude << std::endl;

            SerialDeviceControl::EquatorialCoordinates coordinatesReceived;
            coordinatesReceived.RightAscension = latitude;
            coordinatesReceived.Declination = longitude;

            mSiteLocationCoordinates.Set(coordinatesReceived);

            mMountStateMachine.DoTransition(TelescopeSignals::RequestedGeoLocationReceived);
        }

        virtual void OnTransitionChanged(TelescopeMountState fromState, TelescopeSignals signal, TelescopeMountState toState)
        {
            if(fromState != toState)
            {
                std::cerr << "Transition : (" << StateToString(fromState) << "," << SignalToString(signal) << ") -> " << StateToString(
                              toState) << std::endl;
            }
        }

        virtual void OnErrorStateReached(TelescopeMountState fromState, TelescopeSignals signal)
        {
            std::cerr << "Reached Error/Fail Safe State: most likly an undefined transition occured!" << std::endl;
            std::cerr << "Transition : (" << StateToString(fromState) << "," << SignalToString(signal) << ") -> ??? tripped this error!"
                      << std::endl;
        }

        //return the current telescope state.
        TelescopeMountState GetTelescopeState()
        {
            return mMountStateMachine.CurrentState();
        }

        //return the current pointing coordinates.
        SerialDeviceControl::EquatorialCoordinates GetPointingCoordinates()
        {
            return mCurrentPointingCoordinates.Get();
        }

        //return the current pointing coordinates.
        SerialDeviceControl::EquatorialCoordinates GetSiteLocation()
        {
            return mSiteLocationCoordinates.Get();
        }

    private:
        //mutex protected container for the current coordinates the telescope is pointing at.
        SerialDeviceControl::CriticalData<SerialDeviceControl::EquatorialCoordinates> mCurrentPointingCoordinates;

        //mutex protected container for the current site location set in the telescope.
        SerialDeviceControl::CriticalData<SerialDeviceControl::EquatorialCoordinates> mSiteLocationCoordinates;

        //mutex protected container for the current telescope state.
        //SerialDeviceControl::CriticalData<TelescopeMountState> mTelescopeState;

        //mutex protected state variable of the motion thread.
        SerialDeviceControl::CriticalData<bool> mIsMotionControlThreadRunning;

        //mutex protected state variable of the motion thread to indicate if a motion is started.
        SerialDeviceControl::CriticalData<bool> mIsMotionControlRunning;

        //holds the state of motion, direction and rate.
        SerialDeviceControl::CriticalData<MotionState> mMotionState;

        //motion control thread structure, to periodically sent direction commands.
        std::thread mMotionCommandThread;

        //mutex used for signaling the thread to start motion command sending,
        //this is also used to halt the thread when the motion should be stopped.
        std::mutex mMotionCommandControlMutex;

        //Condition variable to signal start and stop of motion command sending.
        std::condition_variable mMotionControlCondition;

        //state machine of the the telescope hardware
        MountStateMachine mMountStateMachine;

        //Thread function for the motion thread.
        void MotionControlThreadFunction()
        {
            bool isThreadRunning = mIsMotionControlThreadRunning.Get();
            bool isMotionRunning;// = mIsMotionControlRunning.Get();

            if(!isThreadRunning)
            {
                mIsMotionControlThreadRunning.Set(true);

                std::cerr << "Motion Control Thread started!" << std::endl;

                do
                {
                    do
                    {
                        std::unique_lock<std::mutex> motionLock(mMotionCommandControlMutex);
                        isMotionRunning = mIsMotionControlRunning.Get();

                        if(!isMotionRunning)
                        {
                            //initially no motion commands are send, so wait until a motion in either direction is started by the start call.
                            mMotionControlCondition.wait(motionLock);

                            isMotionRunning = mIsMotionControlRunning.Get();
                            if(!isMotionRunning)
                            {
                                break;
                            }
                        }

                        MotionState motionState = mMotionState.Get();

                        //check if motion state is valid.
                        if
                        (
                            motionState.MotionDirection <= SerialDeviceControl::SerialCommandID::NULL_COMMAND_ID ||
                            motionState.MotionDirection >= SerialDeviceControl::SerialCommandID::STOP_MOTION_COMMAND_ID ||
                            motionState.CommandsPerSecond == 0
                        )
                        {
                            //motion is tripped but no values are provided -> disable motion and wait again.
                            mIsMotionControlRunning.Set(false);

                            motionState.MotionDirection = SerialDeviceControl::SerialCommandID::NULL_COMMAND_ID;
                            motionState.CommandsPerSecond = 0;

                            mMotionState.Set(motionState);
                            break;
                        }

                        int waitTime = 1000 / motionState.CommandsPerSecond;

                        //send command to move to direction.
                        switch(motionState.MotionDirection)
                        {
                            case SerialDeviceControl::SerialCommandID::MOVE_EAST_COMMAND_ID:
                                MoveEast();
                                break;

                            case SerialDeviceControl::SerialCommandID::MOVE_WEST_COMMAND_ID:
                                MoveWest();
                                break;

                            case SerialDeviceControl::SerialCommandID::MOVE_NORTH_COMMAND_ID:
                                MoveNorth();
                                break;

                            case SerialDeviceControl::SerialCommandID::MOVE_SOUTH_COMMAND_ID:
                                MoveSouth();
                                break;

                            default:
                                break;
                        }

                        //wait before next loop.
                        std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::milliseconds(waitTime));

                        isMotionRunning = mIsMotionControlRunning.Get();
                    }
                    while(isMotionRunning);

                    isThreadRunning = mIsMotionControlThreadRunning.Get();
                }
                while(isThreadRunning);

                std::cerr << "Motion Control Thread stopped!" << std::endl;
            }
        }

    public:
        static std::string SignalToString(TelescopeSignals signal)
        {
            switch(signal)
            {
                case TelescopeSignals::Connect:
                    return EXPR_TO_STRING(TelescopeSignals::Connect);

                case TelescopeSignals::Disconnect:
                    return EXPR_TO_STRING(TelescopeSignals::Disconnect);

                case TelescopeSignals::GoTo:
                    return EXPR_TO_STRING(TelescopeSignals::GoTo);

                case TelescopeSignals::InitialPointingCoordinatesReceived:
                    return EXPR_TO_STRING(TelescopeSignals::InitialPointingCoordinatesReceived);

                case TelescopeSignals::ParkingPositionReached:
                    return EXPR_TO_STRING(TelescopeSignals::ParkingPositionReached);

                case TelescopeSignals::RequestedGeoLocationReceived:
                    return EXPR_TO_STRING(TelescopeSignals::RequestedGeoLocationReceived);

                case TelescopeSignals::Slew:
                    return EXPR_TO_STRING(TelescopeSignals::Slew);

                case TelescopeSignals::Track:
                    return EXPR_TO_STRING(TelescopeSignals::Track);

                case TelescopeSignals::TrackingTargetReached:
                    return EXPR_TO_STRING(TelescopeSignals::TrackingTargetReached);

                case TelescopeSignals::StartMotion:
                    return EXPR_TO_STRING(TelescopeSignals::StartMotion);

                case TelescopeSignals::StopMotion:
                    return EXPR_TO_STRING(TelescopeSignals::StopMotion);

                case TelescopeSignals::Park:
                    return EXPR_TO_STRING(TelescopeSignals::Park);

                case TelescopeSignals::Stop:
                    return EXPR_TO_STRING(TelescopeSignals::Stop);

                default:
                    return "Invalid Signal!";

            }
        }

        static std::string StateToString(TelescopeMountState state)
        {
            switch(state)
            {
                case TelescopeMountState::Disconnected:
                    return EXPR_TO_STRING(TelescopeMountState::Disconnected);

                case TelescopeMountState::Connected:
                    return EXPR_TO_STRING(TelescopeMountState::Connected);

                case TelescopeMountState::Parked:
                    return EXPR_TO_STRING(TelescopeMountState::Parked);

                case TelescopeMountState::Idle:
                    return EXPR_TO_STRING(TelescopeMountState::Idle);

                case TelescopeMountState::Unknown:
                    return EXPR_TO_STRING(TelescopeMountState::Unknown);

                case TelescopeMountState::ParkingIssued:
                    return EXPR_TO_STRING(TelescopeMountState::ParkingIssued);

                case TelescopeMountState::Tracking:
                    return EXPR_TO_STRING(TelescopeMountState::Tracking);

                case TelescopeMountState::Slewing:
                    return EXPR_TO_STRING(TelescopeMountState::Slewing);

                case TelescopeMountState::MoveWhileTracking:
                    return EXPR_TO_STRING(TelescopeMountState::MoveWhileTracking);

                case TelescopeMountState::FailSafe:
                    return EXPR_TO_STRING(TelescopeMountState::FailSafe);

                default:
                    return "Invalid State";
            }
        }
};
}
#endif
