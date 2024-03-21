/*
    QHY QFocuser

    Copyright (C) 2024 Chen Jiaqi (cjq@qhyccd.com)

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

#pragma once

#include "indifocuser.h"
#include <indipropertynumber.h>

#include <memory>
#include <cstring>
#include <termios.h>
#include <fstream>

#define USB_CDC_RX_LEN      128

class QFocuser : public INDI::Focuser
{
public:
    QFocuser();
    virtual ~QFocuser() = default;

    virtual const char *getDefaultName() override;
    
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual void TimerHit() override;

private:
  int SendCommand(char *cmd_line);
  int ReadResponse(char *buf);
  void GetFocusParams();

  int updatePosition(double *value);
  int updateTemperature();
  int updatePositionRelativeInward(double value);
  int updatePositionRelativeOutward(double value);
  int updatePositionAbsolute(double value);

  int updateSetPosition(int value);

  int updateSetReverse(int value);

  int updateSetSpeed(int value);

  int timerID { -1 };
  bool initTargetPos = true;
  double targetPos{ 0 };
  bool isReboot = false;
  int RebootTimes = 0;

  int FocusSpeedMin = 0;
  int FocusSpeedMax = 8;

  uint8_t buff[USB_CDC_RX_LEN];

  int32_t cmd_version;
  int32_t cmd_version_board;
  int32_t cmd_position;
  int32_t cmd_out_temp;
  int32_t cmd_chip_temp;
  int32_t cmd_voltage;

  INDI::PropertyNumber TemperatureNP{1};

  INDI::PropertyNumber TemperatureChipNP{1};

  INDI::PropertyNumber VoltageNP{1};

  INDI::PropertyNumber FOCUSVersionNP{1};

  INDI::PropertyNumber BOARDVersionNP{1};

  INDI::PropertyNumber FocusSpeedNP{1};

protected:
    virtual bool Handshake() override;
    virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
    virtual bool SetFocuserSpeed(int speed) override;
    virtual bool SyncFocuser(uint32_t ticks) override;
    virtual bool AbortFocuser() override;
    virtual bool ReverseFocuser(bool enabled) override;
};
