#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// C++ includes
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <cmath>

#include "mcp3421.h"
#include "PCA9685.h"
#include "StopWatch.h"

#define NB_PORTS    6
#define ON true
#define OFF false

#define PORT_1  0
#define PORT_2  1
#define PORT_3  2
#define PORT_4  3
#define PORT_PWM1  4
#define PORT_PWM2  5

#define POWER_1 1
#define POWER_2 2
#define POWER_3 3
#define POWER_4 4
#define PWM_1 5
#define PWM_2 6


enum portErrors {PLUGIN_OK, P_ERROR, P_UKNOWN, P_INVALID};
enum methodErrors {OK, PORT_OPEN_ERROR, ERR_PARSE, ERR_FILE};

// #define PLUGIN_DEBUG	3
#define PLUGIN_VERSION	1.01

#define INTER_COMMAND_WAIT_MS 500	// ms

class CAStarBoxPowerPorts
{
public:
	CAStarBoxPowerPorts();
	~CAStarBoxPowerPorts();

    
    virtual int connect();
    virtual void disconnect();
    
	virtual int openAllPorts();
	virtual int getPortCount();
	virtual int setPort(const int nPortID, const bool bOn);
	virtual int getPortStatus(const int nPortID, bool &bOn);

	virtual int setPortPWMDutyCyclePercent(const int nPortID, const int nDutyCiclePercent);
	virtual int getPortPWMDutyCyclePercent(const int nPortID, int &nDutyCiclePercent);

	virtual int loadBootStates(std::vector<int> &states);
	virtual int saveBootStates(std::vector<int> &states);

    virtual bool	isMCP3421Present();
    virtual int		openMCP3421();
    virtual int		closeMCP3421();
    virtual double	getVoltage();
    
private:
    int             setPortPWM(const int nPortID, const int nDutyCycle);
    int             getPortPWM(const int nPortID, int &nDutyCycle);
    void            cmdWait();
    int				parseFields(const std::string sResp, std::vector<std::string> &svFields, char cSeparator);
	std::string&    trim(std::string &str, const std::string &filter );
	std::string&    ltrim(std::string &str, const std::string &filter);
	std::string&    rtrim(std::string &str, const std::string &filter);

	CStopWatch pcaCommandTimer;
    bool    m_bLinked;
    
	bool    m_bPortsOpen;
	// pwm ports
    bool    m_bPwm1On;
	int     m_nPwm1DutyCycle;

    bool    m_bPwm2On;
	int	    m_nPwm2DutyCycle;

	PCA9685 m_PortController;
    mcp3421 m_mcp3421;
    bool    m_mcp3421Present;
    
#ifdef PLUGIN_DEBUG
    // timestamp for logs
    const std::string getTimeStamp();
    std::ofstream m_sLogFile;
    std::string m_sLogfilePath;
#endif

};

