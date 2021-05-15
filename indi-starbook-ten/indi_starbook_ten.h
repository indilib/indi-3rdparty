#ifndef _INDI_STARBOOK_TEN_H_
#define _INDI_STARBOOK_TEN_H_

#include "inditelescope.h"
#include "indiguiderinterface.h"
#include "connectionhttp.h"
#include "starbook_ten.h"

class INDIStarbookTen : public INDI::Telescope, INDI::GuiderInterface {
public:
    INDIStarbookTen();
    ~INDIStarbookTen();

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

protected:
    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool Handshake() override;
    virtual bool saveConfigItems(FILE *fp) override;

    /***************************************************/
    /*** INDI Telescope interface                    ***/
    /***************************************************/

    virtual bool ReadScopeStatus() override;

    virtual bool Goto(double ra, double dec) override;
    virtual bool Sync(double ra, double dec) override;
    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
    virtual bool Park() override;
    virtual bool UnPark() override;
    virtual bool SetCurrentPark() override;
    virtual bool Abort() override;

    virtual bool SetTrackEnabled(bool enabled) override;

    virtual bool updateTime(ln_date *utc, double utc_offset) override;
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;

    virtual bool SetSlewRate(int index) override;

    /***************************************************/
    /*** INDI Guider interface                       ***/
    /***************************************************/

    void initGuiderProperties(const char *deviceName, const char *groupName);
    void processGuiderProperties(const char *name, double values[], char *names[], int n);

    virtual IPState GuideNorth(uint32_t ms) override;
    virtual IPState GuideSouth(uint32_t ms) override;
    virtual IPState GuideEast(uint32_t ms) override;
    virtual IPState GuideWest(uint32_t ms) override;

private:
    bool fetchStartupInfo();
    bool updateStarbookState(StarbookTen::MountStatus& stat);

    uint8_t DBG_SCOPE { INDI::Logger::DBG_IGNORE };

    /* Mount info */
    enum {
        MI_FW_VERSION,
        MI_LAST
    } MIProps;

    IText InfoT[MI_LAST] {};
    ITextVectorProperty InfoTP;

    /* Mount state */
    enum {
        MS_STATE,
        MS_LAST
    } MSProps;

    IText StateT[MS_LAST] {};
    ITextVectorProperty StateTP;

    /* Guide Rate */
    enum {
        GR_RA,
        GR_DE,
        GR_LAST
    } GuideProps;

    INumber GuideRateN[GR_LAST];
    INumberVectorProperty GuideRateNP;

    bool isPropGuidingRA = false;
    bool isPropGuidingDE = false;

    /* Homing */
    enum {
        HS_FIND_HOME,
        HS_LAST
    } HomeProps;

    ISwitch HomeS[HS_LAST];
    ISwitchVectorProperty HomeSP;

    Connection::HTTP *httpConnection = nullptr;

    StarbookTen *starbook;
};

#endif /* _INDI_STARBOOK_TEN_H_ */
