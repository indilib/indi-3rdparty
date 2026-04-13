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

#pragma once

#include <time.h>

class HeaterPID
{
	public:
		HeaterPID(double kp, double ki, double kd, double minoutput, double maxoutput);
		~HeaterPID();
		void setParameters(double kp, double ki, double kd, double minoutput, double maxoutput);
		double calculate(double setPoint, double currValue);

		double getLastCorrectionP() { return(lastCorrectionP); };
		double getLastCorrectionI() { return(lastCorrectionI); };
		double getLastCorrectionD() { return(lastCorrectionD); };

		double getSumError() { return(sumError); };
		double getLastOutput() { return(lastOutput); };
		double getMinOutput() { return(minOutput); };
		double getMaxOutput() { return(maxOutput); };

	private:
		double Kp {0};
		double Ki {0};
		double Kd {0};
		double minOutput {0};
		double maxOutput {0};

		double sumError {0};
		double lastError {0};
		double lastOutput {0};
		time_t lastTime {0};

		double lastCorrectionP {0};
		double lastCorrectionI {0};
		double lastCorrectionD {0};
};
