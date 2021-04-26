/*
 * StateMachine.hpp
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

#ifndef _STATEMACHINE_H_INCLUDED_
#define _STATEMACHINE_H_INCLUDED_

#include <cstdint>
#include <map>
#include <unordered_set>
#include <tuple>
#include <limits>
#include <mutex>
#include "Config.hpp"

namespace TelescopeMountControl
{
//interface template for statemachine transition notification.
template<typename StateType, typename SignalType>
class IStateNotification
{
    public:
        //every time a state change occurs this function is called.
        virtual void OnTransitionChanged(StateType fromState, SignalType signal, StateType toState) = 0;

        //If the error state is triped this callback is called.
        virtual void OnErrorStateReached(StateType fromState, SignalType signal) = 0;
};

//state machine with thread safe state transition.
template<typename StateType, typename SignalType, class NotificationInterfaceImplementation>
class StateMachine
{
        //tuple used for state signal relationship.
        typedef std::tuple<StateType, SignalType> StateSignal;

    private:
        //implementation object of the notification inferface
        NotificationInterfaceImplementation &mStateMachineNotification;

        //transition table of the state changes
        std::map<StateSignal, StateType> mTransitionTable;

        //set of states which are concidered final.
        std::unordered_set<StateType> mFinalStates;

        //start state of the state machine.
        StateType mStartState;

        //any undefined transition causes this state to be active
        StateType mErrorState;

        //the pointer of the current state.
        StateType mCurrentState;

        //make this whole thing thread safe.
        std::mutex mMutex;

    public:
        StateMachine(NotificationInterfaceImplementation &interfaceImplementation, StateType startState, StateType errorState) :
            mStateMachineNotification(interfaceImplementation),
            mStartState(startState),
            mErrorState(errorState),
            mCurrentState(mStartState)
        {

        }

        virtual ~StateMachine()
        {

        }

        //Reset the machine so it can simply restart.
        bool Reset()
        {
            std::lock_guard<std::mutex> guard(mMutex); //lock this function call to avoid concurrent modification.

            mCurrentState = mStartState;

            return true;
        }

        //mark a state as final.
        bool AddFinalState(StateType state)
        {
            std::lock_guard<std::mutex> guard(mMutex); //lock this function call to avoid concurrent modification.

            mFinalStates.insert(state);

            return false;
        }

        //Add a transition from a state to a state tripped by a signal. All types should be in range of the state and signal types.
        bool AddTransition(StateType fromState, SignalType signal, StateType toState)
        {
            std::lock_guard<std::mutex> guard(mMutex); //lock this function call to avoid concurrent modification.

            StateSignal stateSignalTupel = std::make_tuple(fromState, signal);

            if(mTransitionTable.count(stateSignalTupel) == 0) //only deterministic state machines alowed.
            {
                mTransitionTable[stateSignalTupel] = toState;
                return true;

            }

            return false;
        }

        //submit a signal to the state machine and do a transistion.
        //the notify interface gets called when a transition or indefined transition occured.
        bool DoTransition(SignalType signal)
        {
            std::lock_guard<std::mutex> guard(mMutex); //lock this function call to avoid concurrent modification.

            StateSignal stateSignalTupel = std::make_tuple(mCurrentState, signal);

            StateType fromState = mCurrentState;

            if(mTransitionTable.count(stateSignalTupel) > 0) //transition defined.
            {
                StateType toState = mTransitionTable[stateSignalTupel];

                mCurrentState = toState;

                mStateMachineNotification.OnTransitionChanged(fromState, signal, toState);

                return true;
            }
            else //undefined transition.
            {
                mCurrentState = mErrorState;

                mStateMachineNotification.OnErrorStateReached(fromState, signal);
            }

            return false;
        }

        //returns true if the current state is a final state.
        bool IsFinalized()
        {
            std::lock_guard<std::mutex> guard(mMutex);

            if(mFinalStates.count(mCurrentState) > 0)
            {
                return true;
            }

            return false;
        }

        // Returns true if the state machine is in the error state.
        bool IsInErrorState()
        {
            std::lock_guard<std::mutex> guard(mMutex);

            return mCurrentState == mErrorState;
        }

        //returns the current state of the machine.
        StateType CurrentState()
        {
            std::lock_guard<std::mutex> guard(mMutex);

            return mCurrentState;
        }
};
}


#endif
