#pragma once

// #include "libindi/indifocuser.h"
#include "indifocuser.h"
#include <indipropertynumber.h>

#include <memory>
#include <cstring>
#include <termios.h>
#include <fstream>
#include <libindi/json.h>

#define USB_CDC_RX_LEN      128

using json = nlohmann::json;

class QFocuser : public INDI::Focuser
{
public:
    QFocuser();
    virtual ~QFocuser() = default;

    // You must override this method in your class.
    virtual const char *getDefaultName() override;
    
    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISSnoopDevice(XMLEle *root) override;

    virtual void TimerHit() override;

private:
  int SendCommand(char *cmd_line);
  int ReadResponse(char *buf);
  void GetFocusParams();

  int updatePosition(double *value);
  int updateTemperature(double *value);
  int updatePositionRelativeInward(double value);
  int updatePositionRelativeOutward(double value);
  int updatePositionAbsolute(double value);

  int updateSetPosition(int value);

  int updateSetReverse(int value);

  int updateSetSpeed(int value);

  int timerID { -1 };
  bool initTargetPos = true;
  double targetPos{ 0 };
  double simulatedTemperature{ 0 };
  double simulatedPosition{ 0 };
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
    virtual bool saveConfigItems(FILE *fp) override;

    virtual bool Handshake() override;

    virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);
    virtual IPState MoveAbsFocuser(uint32_t targetTicks);
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
    virtual bool SetFocuserSpeed(int speed);
    virtual bool SyncFocuser(uint32_t ticks);
    virtual bool AbortFocuser();
    virtual bool ReverseFocuser(bool enabled);
};