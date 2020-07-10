#pragma once

#include <indicom.h>
#include <inditelescope.h>
#include <connectionplugins/connectionserial.h>
#include <connectionplugins/connectiontcp.h>
#include <alignment/AlignmentSubsystemForDrivers.h>

#include "auxproto.h"

class CelestronAUX : 
    public INDI::Telescope, 
    public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers
{
  public:
    CelestronAUX();
    ~CelestronAUX();

    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                           char *formats[], char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

  protected:
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool saveConfigItems(FILE *fp) override;
    //virtual bool Connect() override;
    virtual bool Handshake() override;
    virtual bool Disconnect() override;

    virtual const char *getDefaultName() override;
    ln_hrz_posn AltAzFromRaDec(double ra, double dec, double ts);

    virtual bool Sync(double ra, double dec) override;
    virtual bool Goto(double ra, double dec) override;
    virtual bool Abort() override;
    virtual bool Park() override;
    virtual bool UnPark() override;

    // TODO: Switch to AltAz from N-S/W-E
    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

    virtual bool ReadScopeStatus() override;
    virtual void TimerHit() override;

    virtual bool updateLocation(double latitude, double longitude, double elevation) override;

    bool trackingRequested();

    long GetALT();
    long GetAZ();
    bool slewing();
    bool Slew(AUXtargets trg, int rate);
    bool SlewALT(int rate);
    bool SlewAZ(int rate);
    bool GoToFast(long alt, long az, bool track);
    bool GoToSlow(long alt, long az, bool track);
    bool setCordwrap(bool enable);
    bool getCordwrap();
    bool setCordwrapPos(long pos);
    long getCordwrapPos();
    bool getVersion(AUXtargets trg);
    void getVersions();
    bool Track(long altRate, long azRate);
    bool TimerTick(double dt);

  private:
    static const long STEPS_PER_REVOLUTION;
    static const double STEPS_PER_DEGREE;
    static const double DEFAULT_SLEW_RATE;
    static const double TRACK_SCALE;
    static const long MAX_ALT;
    static const long MIN_ALT;
    
    enum ScopeStatus_t
    {
        IDLE,
        SLEWING_FAST,
        APPROACH,
        SLEWING_SLOW,
        SLEWING_MANUAL,
        TRACKING
    };
    ScopeStatus_t ScopeStatus;

    enum AxisStatus
    {
        STOPPED,
        SLEWING
    };

    enum AxisDirection
    {
        FORWARD,
        REVERSE
    };

    AxisStatus AxisStatusALT;
    AxisDirection AxisDirectionALT;

    AxisStatus AxisStatusAZ;
    AxisDirection AxisDirectionAZ;

    double Approach; // approach distance

    // Previous motion direction
    // TODO: Switch to AltAz from N-S/W-E
    typedef enum {
        PREVIOUS_NS_MOTION_NORTH   = DIRECTION_NORTH,
        PREVIOUS_NS_MOTION_SOUTH   = DIRECTION_SOUTH,
        PREVIOUS_NS_MOTION_UNKNOWN = -1
    } PreviousNSMotion_t;
    typedef enum {
        PREVIOUS_WE_MOTION_WEST    = DIRECTION_WEST,
        PREVIOUS_WE_MOTION_EAST    = DIRECTION_EAST,
        PREVIOUS_WE_MOTION_UNKNOWN = -1
    } PreviousWEMotion_t;

    // GoTo
    ln_equ_posn GoToTarget;
    int slewTicks, maxSlewTicks;

    // Tracking
    ln_equ_posn CurrentTrackingTarget;
    ln_equ_posn NewTrackingTarget;

    // Tracing in timer tick
    int TraceThisTickCount;
    bool TraceThisTick;
    bool simulator = false;

    // connection
    bool isRTSCTS;

    unsigned int DBG_CAUX;
    unsigned int DBG_AUXMOUNT;

    void initScope(char const *ip, int port);
    void initScope();
    bool detectNetScope(bool set_ip);
    bool detectNetScope();
    void closeConnection();
    void emulateGPS(AUXCommand &m);
    bool serial_readMsgs(AUXCommand c);
    bool tcp_readMsgs();
    bool readMsgs(AUXCommand c);
    void processCmd(AUXCommand &cmd);
    void querryStatus();
    int sendBuffer(int PortFD, buffer buf);
    bool sendCmd(AUXCommand &c);

    double Lat, Lon, Elv;
    long Alt;
    long Az;
    long AltRate;
    long AzRate;
    long targetAlt;
    long targetAz;
    long slewRate;
    bool tracking;
    bool slewingAlt, slewingAz;
    bool gpsemu;
    bool cordwrap;
    long cordwrapPos;
    unsigned mb_ver_maj, mb_ver_min;
    unsigned alt_ver_maj, alt_ver_min;
    unsigned azm_ver_maj, azm_ver_min;

    // FP
    int modem_ctrl;
    void setRTS(bool rts);
    bool waitCTS(float timeout);
    bool detectRTSCTS();
    int response_data_size;
    int aux_tty_read(int PortFD,char *buf,int bufsiz,int timeout,int *n);
    int aux_tty_write (int PortFD,char *buf,int bufsiz,float timeout,int *n);
    bool tty_set_speed(int PortFD, speed_t speed);
    void hex_dump(char *buf, buffer data, size_t size);


    // Additional interface elements specific to Celestron Scopes
    private:
    // Firmware 
    IText FirmwareT[9];
    ITextVectorProperty FirmwareTP;
    enum {FW_HC, FW_HCp, FW_AZM, FW_ALT, FW_WiFi, FW_BAT, FW_CHG, FW_LIGHT, FW_GPS};
    // Networked Mount autodetect
    ISwitch NetDetectS[1];
    ISwitchVectorProperty NetDetectSP;
    // Mount Cordwrap 
    ISwitch CordWrapS[2];
    ISwitchVectorProperty CordWrapSP;
    enum { CORDWRAP_OFF, CORDWRAP_ON };
    ISwitch CWPosS[4];
    ISwitchVectorProperty CWPosSP;
    enum { CORDWRAP_N, CORDWRAP_E, CORDWRAP_S, CORDWRAP_W};
    // GPS emulator
    ISwitch GPSEmuS[2];
    ISwitchVectorProperty GPSEmuSP;
    enum { GPSEMU_OFF, GPSEMU_ON };
};
