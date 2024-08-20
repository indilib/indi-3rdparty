#include "AStarBox.h"

CAStarBoxPowerPorts::CAStarBoxPowerPorts()
{
    m_bLinked = false;
    m_bPortsOpen = false;

    m_bPwm1On = false;
    m_nPwm1DutyCycle = 0;

    m_bPwm2On = false;
    m_nPwm2DutyCycle = 0;

    m_mcp3421Present = false;

#ifdef PLUGIN_DEBUG
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/AStarBox-Log.txt";
    m_sLogFile.open(m_sLogfilePath, std::ios::out |std::ios::trunc);
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CAStarBoxPowerPorts] Version " << std::fixed << std::setprecision(2) << PLUGIN_VERSION << " build " << __DATE__ << " " << __TIME__ << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CAStarBoxPowerPorts] Constructor Called." << std::endl;
    m_sLogFile.flush();
#endif
    pcaCommandTimer.Reset();
}

CAStarBoxPowerPorts::~CAStarBoxPowerPorts()
{
    
}

int CAStarBoxPowerPorts::connect()
{
  // Open up ports initiates connection and
  // sets m_PortController.isPCA9685Present() if can connect.
  if(!m_bPortsOpen) {
    openAllPorts();
  }
  
  if (!m_PortController.isPCA9685Present()) {
    m_bLinked = false;
    return P_ERROR;
  }

  // Set linked state - need this set before getting PWM duty cycles
  m_bLinked = true;
  
  // Get PWM duty cycle and store it
  getPortPWM(PWM_1, m_nPwm1DutyCycle);
  getPortPWM(PWM_2, m_nPwm2DutyCycle);

  return PLUGIN_OK;
}

void CAStarBoxPowerPorts::disconnect()
{
    m_bLinked = false;
}


int CAStarBoxPowerPorts::openAllPorts()
{
    int nErr = PLUGIN_OK;

    m_PortController.init(1,0x40);   // bus id is 1 for /dev/i2c-1 ,  device address is 0x40

    m_mcp3421.setBusID(1); // /dev/i2c-1
    m_mcp3421Present = m_mcp3421.isMCP3421Present();
    if(m_mcp3421Present) {
        nErr = m_mcp3421.openMCP3421();
        if(nErr) {
            m_mcp3421Present = false;
            nErr = PLUGIN_OK;
        }
    }
    
	m_bPortsOpen = true;

    return nErr;
}

int CAStarBoxPowerPorts::getPortCount()
{
    return NB_PORTS;
}

void CAStarBoxPowerPorts::cmdWait()
{
    int dDelayMs;

    if(pcaCommandTimer.GetElapsedSeconds() < (INTER_COMMAND_WAIT_MS/1000)) {
        dDelayMs = INTER_COMMAND_WAIT_MS - int(pcaCommandTimer.GetElapsedSeconds() *1000);
        if(dDelayMs>0)
            std::this_thread::sleep_for(std::chrono::milliseconds(dDelayMs));
    }
    pcaCommandTimer.Reset();

}

int CAStarBoxPowerPorts::setPort(const int nPortID, const bool bOn)
{
    int nErr = PLUGIN_OK;
    int nTmp;

    if(!m_bLinked)
        return nErr;

    cmdWait();

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPort] Setting port " << nPortID << " to " << (bOn?"On":"Off") << std::endl;
        m_sLogFile.flush();
#endif

    switch(nPortID) {
        case POWER_1:
            if(bOn)
                nErr = m_PortController.setOn(PORT_1);
            else
                nErr = m_PortController.setOff(PORT_1);
            break;
        case POWER_2:
            if(bOn)
                nErr = m_PortController.setOn(PORT_2);
            else
                nErr = m_PortController.setOff(PORT_2);
            break;
        case POWER_3:
            if(bOn)
                nErr = m_PortController.setOn(PORT_3);
            else
                nErr = m_PortController.setOff(PORT_3);
            break;
        case POWER_4:
            if(bOn)
                nErr = m_PortController.setOn(PORT_4);
            else
                nErr = m_PortController.setOff(PORT_4);
            break;
        case PWM_1:
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPort] m_nPwm1DutyCycle " << m_nPwm1DutyCycle  << std::endl;
            m_sLogFile.flush();
#endif
            if(bOn) {
                nErr = m_PortController.setPWM(PORT_PWM1,m_nPwm1DutyCycle);
            }
            else {
                nErr = m_PortController.setPWM(PORT_PWM1,0);

            }
            m_bPwm1On = bOn;
            break;
        case PWM_2:
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPort] m_nPwm2DutyCycle " << m_nPwm2DutyCycle  << std::endl;
            m_sLogFile.flush();
#endif
            if(bOn) {
                nErr = m_PortController.setPWM(PORT_PWM2,m_nPwm2DutyCycle);
            }
            else {
                nErr = m_PortController.setPWM(PORT_PWM2,0);
            }
            m_bPwm2On = bOn;
            break;
        default:
            nErr = P_UKNOWN;
            return nErr;
            break;
    }
    if(nErr == -1)
      nErr = P_ERROR;

    return nErr;
}

int CAStarBoxPowerPorts::getPortStatus(const int nPortID, bool &bOn)
{
    int nErr = PLUGIN_OK;
    int nValue = 0;
	int dDelayMs;

    if(!m_bLinked)
        return nErr;

    cmdWait();

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPortStatus] Getting port " << nPortID << " status" << std::endl;
    m_sLogFile.flush();
#endif

    switch(nPortID) {
        case POWER_1:
            bOn = m_PortController.isPortOn(PORT_1);
            break;
        case POWER_2:
            bOn = m_PortController.isPortOn(PORT_2);
            break;
        case POWER_3:
            bOn = m_PortController.isPortOn(PORT_3);
            break;
        case POWER_4:
            bOn = m_PortController.isPortOn(PORT_4);
            break;
        case PWM_1:
            getPortPWM(PWM_1, nValue);
            bOn = (nValue!=0);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPortStatus] PWM1 nValue " << nValue << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPortStatus] PWM1 bOn " << (bOn?"On":"Off") << std::endl;
            m_sLogFile.flush();
#endif
            break;
        case PWM_2:
            getPortPWM(PWM_2, nValue);
            bOn = (nValue!=0);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPortStatus] PWM2 nValue " << nValue << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPortStatus] PWM2 bOn " << (bOn?"On":"Off") << std::endl;
            m_sLogFile.flush();
#endif
            break;
        default:
            nErr = P_INVALID;
            return nErr;
            break;
    }

    if(nErr == -1) {
      return P_ERROR;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPortStatus] Port " << nPortID << " status : " << nValue << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}


int CAStarBoxPowerPorts::setPortPWMDutyCyclePercent(const int nPortID, const int nDutyCiclePercent)
{
    int nErr = PLUGIN_OK;
    bool bOn = false;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPortPWMDutyCyclePercent] Setting port " << nPortID << " to " << std::fixed << std::setprecision(2) << nDutyCiclePercent << std::endl;
    m_sLogFile.flush();
#endif

    switch(nPortID) {
        case PWM_1:
            m_nPwm1DutyCycle = int(std::round((nDutyCiclePercent/100.0) * MAX_PCA_VALUE));
            if(m_bLinked) {
                if(m_bPwm1On)
                    setPortPWM(nPortID, m_nPwm1DutyCycle);
            }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPortPWMDutyCyclePercent] m_nPwm1DutyCycle :" << m_nPwm1DutyCycle << std::endl;
            m_sLogFile.flush();
#endif
            break;

        case PWM_2:
            m_nPwm2DutyCycle = int(std::round((nDutyCiclePercent/100.0) * MAX_PCA_VALUE));
            if(m_bLinked) {
                if(m_bPwm2On)
                    setPortPWM(nPortID, m_nPwm2DutyCycle);
            }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPortPWMDutyCyclePercent] m_nPwm2DutyCycle :" << m_nPwm2DutyCycle << std::endl;
            m_sLogFile.flush();
#endif
            break;

        default:
            nErr = P_INVALID;
            return nErr;
            break;
    }
    return PLUGIN_OK;
}




int CAStarBoxPowerPorts::getPortPWMDutyCyclePercent(const int nPortID, int &nDutyCiclePercent)
{
    int nDutyCicle;
    bool bOn;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPortPWMDutyCyclePercent] getting port " << nPortID << std::endl;
    m_sLogFile.flush();
#endif

    switch(nPortID) {
        case PWM_1:
            if(m_bLinked) {
                if(m_bPwm1On) {
                    getPortPWM(PWM_1, nDutyCicle);
                    m_nPwm1DutyCycle = nDutyCicle ; // port is on so we get the actual value
                }
            }
            nDutyCiclePercent = int(std::round((float(m_nPwm1DutyCycle)/float(MAX_PCA_VALUE)) * 100.0));
            break;
        case PWM_2:
            if(m_bLinked) {
                if(m_bPwm2On) {
                    getPortPWM(PWM_2, nDutyCicle);
                    m_nPwm2DutyCycle = nDutyCicle ; // port is on so we get the actual value
                }
            }
            nDutyCiclePercent = int(std::round((float(m_nPwm2DutyCycle)/float(MAX_PCA_VALUE)) * 100.0));
            break;
        default:
            return P_INVALID;
            break;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPortPWMDutyCyclePercent] Port " << nPortID << " is at " << std::fixed << std::setprecision(2) << nDutyCiclePercent << std::endl;
    m_sLogFile.flush();
#endif

    return PLUGIN_OK;
}

int CAStarBoxPowerPorts::setPortPWM(const int nPortID, const int nDutyCycle)
{
    int nErr = PLUGIN_OK;
	int dDelayMs;

    if(!m_bLinked)
        return nErr;

    cmdWait();
    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPortPWM] Port " << nPortID << " to " << nDutyCycle << std::endl;
    m_sLogFile.flush();
#endif

    switch(nPortID) {
        case PWM_1:
            m_nPwm1DutyCycle = nDutyCycle;
            m_PortController.setPWM(PORT_PWM1 ,m_nPwm1DutyCycle);
            break;
        case PWM_2:
            m_nPwm2DutyCycle = nDutyCycle;
            m_PortController.setPWM(PORT_PWM2 ,m_nPwm2DutyCycle);
            break;
    }
   return nErr;
}

int CAStarBoxPowerPorts::getPortPWM(const int nPortID,int &nDutyCycle)
{
    int nErr = PLUGIN_OK;
    int nOffValue, nOnValue;

    if(!m_bLinked)
        return nErr;

    cmdWait();

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPortPWM] Getting port " << nPortID << std::endl;
    m_sLogFile.flush();
#endif

    switch(nPortID) {
        case PWM_1:
            m_PortController.getPWM(PORT_PWM1, nDutyCycle);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPortPWM] PWM_1 nDutyCycle " << nDutyCycle << std::endl;
            m_sLogFile.flush();
#endif
            if (nDutyCycle == 4096) {
                nDutyCycle = 0;
            }
            else if (nDutyCycle == 0)
                nDutyCycle = 4096;

            m_nPwm1DutyCycle = nDutyCycle;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPortPWM] PWM_1 m_nPwm1DutyCycle " << m_nPwm1DutyCycle << std::endl;
            m_sLogFile.flush();
#endif
            break;
        case PWM_2:
            m_PortController.getPWM(PORT_PWM2, nDutyCycle);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPortPWM] PWM_2 nDutyCycle " << nDutyCycle << std::endl;
            m_sLogFile.flush();
#endif
            if (nDutyCycle == 4096) {
                nDutyCycle = 0;
            }
            else if (nDutyCycle == 0)
                nDutyCycle = 4096;

            m_nPwm2DutyCycle = nDutyCycle;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPortPWM] PWM_2 m_nPwm2DutyCycle " << m_nPwm2DutyCycle << std::endl;
            m_sLogFile.flush();
#endif
            break;
    }

    return nErr;
}

int CAStarBoxPowerPorts::loadBootStates(std::vector<int> &bootStates)
{
    int nErr = PLUGIN_OK;
    std::vector<std::string> sbootStates;
    int i;
    std::ifstream   Datafile;
    std::string sTmp;
    std::string sConfig;
    
    Datafile.open("/etc/astarbox.conf", std::ifstream::in);
    if(Datafile.is_open()) {
        while (std::getline(Datafile, sTmp)) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [loadBootStates] sTmp : " << sTmp << std::endl;
    m_sLogFile.flush();
#endif
            sConfig.append(sTmp);
            break;
        }
        Datafile.close();
    }
    else
        return ERR_FILE;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [loadBootStates] Port config : " << sConfig << std::endl;
    m_sLogFile.flush();
#endif

    nErr = parseFields(sConfig, sbootStates, ':');
    if(nErr)
        return nErr;

    bootStates.clear();
    for(i=0; i< sbootStates.size(); i++)
        bootStates.push_back(std::stoi(sbootStates[i]));

    if(bootStates.size() < 6) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [loadBootStates] Boot config  size incorrect : " << bootStates.size() << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_PARSE;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [loadBootStates] bootStates[PORT_1] : " << bootStates[PORT_1] << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [loadBootStates] bootStates[PORT_2] : " << bootStates[PORT_2] << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [loadBootStates] bootStates[PORT_3] : " << bootStates[PORT_3] << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [loadBootStates] bootStates[PORT_4] : " << bootStates[PORT_4] << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [loadBootStates] bootStates[PORT_PWM1] : " << bootStates[PORT_PWM1] << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [loadBootStates] bootStates[PORT_PWM2] : " << bootStates[PORT_PWM2] << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}

int CAStarBoxPowerPorts::saveBootStates(std::vector<int> &bootStates)
{
    int nErr = PLUGIN_OK;
    std::stringstream ssConfig;
    std::ofstream Datafile;

    if(bootStates.size() < 6)
        return ERR_PARSE;

    ssConfig << bootStates[PORT_1] << ":" <<
            bootStates[PORT_2] << ":" <<
            bootStates[PORT_3] << ":" <<
            bootStates[PORT_4] << ":" <<
            ((bootStates[PORT_PWM1] == 1)?m_nPwm1DutyCycle:0) << ":" <<
            ((bootStates[PORT_PWM2] == 1)?m_nPwm2DutyCycle:0);
        
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [saveBootStates] bootStates[PORT_1] : " << bootStates[PORT_1] << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [saveBootStates] bootStates[PORT_2] : " << bootStates[PORT_2] << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [saveBootStates] bootStates[PORT_3] : " << bootStates[PORT_3] << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [saveBootStates] bootStates[PORT_4] : " << bootStates[PORT_4] << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [saveBootStates] bootStates[PORT_PWM1] : " << ((bootStates[PORT_PWM1] == 1)?m_nPwm1DutyCycle:0) << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [saveBootStates] m_nPwm1DutyCycle : " << m_nPwm1DutyCycle << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [saveBootStates] bootStates[PORT_PWM2] : " << ((bootStates[PORT_PWM2] == 1)?m_nPwm2DutyCycle:0) << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [saveBootStates] m_nPwm2DutyCycle : " << m_nPwm2DutyCycle << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [saveBootStates] port config        : " << ssConfig.str() << std::endl;
    m_sLogFile.flush();
#endif

    Datafile.open("/etc/astarbox.conf", std::ios::out |std::ios::trunc);
    if(Datafile.is_open()) {
        Datafile << ssConfig.str();
        Datafile.close();
    } else {
        return ERR_FILE;
    }

    return nErr;
}

bool  CAStarBoxPowerPorts::isMCP3421Present()
{
    return  m_mcp3421Present;
}

int CAStarBoxPowerPorts::openMCP3421()
{
    return m_mcp3421.openMCP3421();
}

int CAStarBoxPowerPorts::closeMCP3421()
{
    return m_mcp3421.closeMCP3421();
}

double CAStarBoxPowerPorts::getVoltage()
{
    if(m_mcp3421Present)
        return m_mcp3421.getVoltValue();
    return 0;
}


int CAStarBoxPowerPorts::parseFields(const std::string sResp, std::vector<std::string> &svFields, char cSeparator)
{
    int nErr = PLUGIN_OK;
    std::string sSegment;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseFields] sResp = " << sResp << std::endl;
    m_sLogFile.flush();
#endif

    if(sResp.size()==0) {
        return ERR_PARSE;
    }

    std::stringstream ssTmp(sResp);

    svFields.clear();
    // split the string into vector elements
    while(std::getline(ssTmp, sSegment, cSeparator))
    {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseFields] sSegment = " << sSegment << std::endl;
        m_sLogFile.flush();
#endif
        svFields.push_back(sSegment);
    }

    if(svFields.size()==0) {
        nErr = ERR_PARSE;
    }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseFields] Done all good." << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}


std::string& CAStarBoxPowerPorts::trim(std::string &str, const std::string& filter )
{
    return ltrim(rtrim(str, filter), filter);
}

std::string& CAStarBoxPowerPorts::ltrim(std::string& str, const std::string& filter)
{
    str.erase(0, str.find_first_not_of(filter));
    return str;
}

std::string& CAStarBoxPowerPorts::rtrim(std::string& str, const std::string& filter)
{
    str.erase(str.find_last_not_of(filter) + 1);
    return str;
}


#ifdef PLUGIN_DEBUG
const std::string CAStarBoxPowerPorts::getTimeStamp()
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}
#endif
