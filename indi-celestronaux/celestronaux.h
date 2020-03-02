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
    // uint8_t cauxConnection { CONNECTION_SERIAL | CONNECTION_TCP };

    unsigned int DBG_CAUX;
    unsigned int DBG_AUXMOUNT;

    void initScope(char const *ip, int port);
    void initScope();
    bool detectScope();
    void closeConnection();
    void emulateGPS(AUXCommand &m);
    void serial_readMsgs();
    bool tcp_readMsgs_tty();
    bool tcp_readMsgs_net();
    void readMsgs();
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

    // FP
    int modem_ctrl;
    void setRTS(bool rts);
    bool waitCTS(float timeout);
    int nevo_tty_read(int PortFD,char *buf,int bufsiz,int timeout,int *n);
    int nevo_tty_write (int PortFD,char *buf,int bufsiz,float timeout,int *n);

};
