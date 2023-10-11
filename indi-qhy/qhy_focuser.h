#pragma once

#include "libindi/indifocuser.h"
#define USB_CDC_RX_LEN      64

// namespace Connection
// {
//     class Serial;
// }

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
  // Connection::Serial *serialConnection{nullptr};
  int SendCommand(char *cmd_line);
  int ReadResponse(char *buf);
  void GetFocusParams();

  int updatePosition(double *value);
  int updateTemperature(double *value);
  int updateBacklash(double *value);
  int updateMotorSettings(double *duty, double *delay, double *ticks);
  int updatePositionRelativeInward(double value);
  int updatePositionRelativeOutward(double value);
  int updatePositionAbsolute(double value);

  int updateMaxPosition(double *value);
  int updateSetPosition(int value);

  int updateSetSpeed(int value);

  int ReadUntilComplete(char *buf, int timeout);

  int timerID { -1 };
  bool initTargetPos = true;
  double targetPos{ 0 };
  double simulatedTemperature{ 0 };
  double simulatedPosition{ 0 };

  uint8_t buff[USB_CDC_RX_LEN];

  int32_t cmd_position;
  int32_t cmd_out_temp;
  int32_t cmd_chip_temp;

  ISwitch ResetPosS[1];
  ISwitchVectorProperty ResetPosSP;
  ISwitch RevertDirS[2];
  ISwitchVectorProperty RevertDirSP;

  INumber TemperatureChip[1];
  INumberVectorProperty TemperatureChipVP;
  INumber TemperatureN[1];
  INumberVectorProperty TemperatureNP;

  INumber SettingsN[3];
  INumberVectorProperty SettingsNP;

  INumber MinMaxPositionN[2];
  INumberVectorProperty MinMaxPositionNP;

  INumber MaxTravelN[1];
  INumberVectorProperty MaxTravelNP;

  INumber SetRegisterPositionN[1];
  INumberVectorProperty SetRegisterPositionNP;

  INumber RelMovementN[1];
  INumberVectorProperty RelMovementNP;

  INumber AbsMovementN[1];
  INumberVectorProperty AbsMovementNP;

  INumber FocusSpeedN[1];
  INumberVectorProperty FocusSpeedNP;
        

protected:
    virtual bool saveConfigItems(FILE *fp) override;

    virtual bool Handshake() override;

    virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);
    virtual IPState MoveAbsFocuser(uint32_t targetTicks);
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
    virtual bool SetFocuserSpeed(int speed);
    virtual bool SyncFocuser(uint32_t ticks);
    virtual bool AbortFocuser();
};

