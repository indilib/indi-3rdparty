/**
   This file is part of the AAG Cloud Watcher INDI Driver.
   A driver for the AAG Cloud Watcher (AAGware - http : //www.aagware.eu/)

   Copyright (C) 2012 - 2015 Sergio Alonso (zerjioi@ugr.es)
   Copyright (C) 2019 Adrián Pardini - Universidad Nacional de La Plata (github@tangopardo.com.ar)
   Copyright (C) 2021 Jasem Mutlaq

   AAG Cloud Watcher INDI Driver is free software : you can redistribute it
   and / or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   AAG Cloud Watcher INDI Driver is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with AAG Cloud Watcher INDI Driver.  If not, see
   < http : //www.gnu.org/licenses/>.

   Anemometer code contributed by Joao Bento.
*/

#include <HeaterPID.h>
#include <time.h>
#include <algorithm>

HeaterPID::HeaterPID(double kp, double ki, double kd, double minoutput, double maxoutput) :
	Kp(kp),
	Ki(ki),
	Kd(kd),
	minOutput(minoutput),
	maxOutput(maxoutput)
{
}

HeaterPID::~HeaterPID()
{
}

void HeaterPID::setParameters(double kp, double ki, double kd, double minoutput, double maxoutput)
{
	Kp = kp;
	Ki = ki;
	Kd = kd;
	minOutput = minoutput;
	maxOutput = maxoutput;
}

double HeaterPID::calculate(double setPoint, double currValue)
{
	double currError;
	double correctionP;
	double correctionI;
	double correctionD;
	double output;
	double newSumError;
	time_t currTime;

	currError = setPoint - currValue;
	currTime = time(nullptr);

	if ( lastTime == 0 )
	{
		lastTime = currTime;
		lastError = 0;
	
		return(lastOutput);
	}

	newSumError = sumError + (currTime - lastTime) * (lastError + currError)/2.0;

	correctionP = Kp * currError;
	correctionI = Ki * newSumError;
	correctionD = Kd * (currError - lastError) / (currTime - lastTime);

	output = lastOutput + correctionP + correctionI + correctionD;

	// Anti-windup, only integrate error, if both last and current outputs are within limits
	if (output>minOutput && output<maxOutput && lastOutput>minOutput && lastOutput<maxOutput) 
	{
		sumError = newSumError;
	}

	// Clamp output
	output = std::min(maxOutput,std::max(output,minOutput));

	lastCorrectionP = correctionP;
	lastCorrectionI = correctionI;
	lastCorrectionD = correctionD;

	lastOutput = output;
	lastError = currError;
	lastTime = currTime;

	return(output);
}

