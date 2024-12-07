#include "mcp3421.h"

mcp3421::mcp3421()
{
    m_dVperDiv = 0.000015625;
    m_resistorDividerRatio = 7;
    m_fd = -1;
    m_nADCAdress = ADC_ADDR0;
}

mcp3421::~mcp3421()
{
    if(m_fd>0)
        close(m_fd);
    m_fd = -1;
}

void mcp3421::setBusID(int nBus)
{
    m_sDevPath = "/dev/i2c-" + std::to_string(nBus);
}

bool mcp3421::isMCP3421Present()
{
    m_nADCAdress = ADC_ADDR0;
    if(m_fd <0) {
        m_fd = openDevice(m_sDevPath.c_str(), m_nADCAdress);
        if(m_fd<0) {
            m_nADCAdress = ADC_ADDR2;
            m_fd = openDevice(m_sDevPath.c_str(), m_nADCAdress);
            if(m_fd<0) {
                return false;
            }
        }
        close(m_fd);
        m_fd = -1;
    }
    return true;
}

int mcp3421::openMCP3421()
{
    int nErr = 0;
    
    m_fd = openDevice(m_sDevPath.c_str(), m_nADCAdress);
    if(m_fd<0) {
        if(m_nADCAdress == ADC_ADDR0)
            m_nADCAdress = ADC_ADDR2;
        else
            m_nADCAdress = ADC_ADDR0;
        m_fd = openDevice(m_sDevPath.c_str(), m_nADCAdress);
        if(m_fd<0) {
            return m_fd;
        }
    }
    else
        return nErr;

    return nErr;

}

int mcp3421::closeMCP3421()
{
    int nErr = 0;

    if(m_fd>0)
        nErr = close(m_fd);
    m_fd = -1;
    return nErr;
}

double mcp3421::getVoltValue()
{
    int nValue;
    nValue = readValue(m_fd, 0, MCP3422_SR_3_75, MCP3422_GAIN_1);
    m_value = float(nValue) * m_dVperDiv * m_resistorDividerRatio;
    return m_value;
}

int mcp3421::i2c_smbus_access (int fd, char rw, uint8_t command, int size, union i2c_smbus_data *data)
{
  struct i2c_smbus_ioctl_data args ;

  args.read_write = rw ;
  args.command    = command ;
  args.size       = size ;
  args.data       = data ;
  return ioctl (fd, I2C_SMBUS, &args) ;
}

int mcp3421::openDevice(const char* devPath, int devAddr)
{
    int fd;
    int r;

    fd = open(devPath, O_RDWR);
    if(fd <= 0) {
		return -1;
	}

    if( ( r = ioctl(fd, I2C_SLAVE, devAddr)) < 0) {
		return -1;
	}

    // in case the above return 0 even if the device is not present.. yes this does indeed happen
    if( (r = readValue(fd, 0, MCP3422_SR_3_75, MCP3422_GAIN_1)) < 0) {
        close(fd);
        fd = -1;
    }

    return fd;
}

void mcp3421::waitForConversion (int fd, unsigned char *buffer, int n)
{
  for (;;)
  {
    read (fd, buffer, n) ;
    if ((buffer [n-1] & 0x80) == 0)
      break ;
    usleep (1000) ;
  }
}

int mcp3421::readValue(int fd, int channel, int sampleRate, int gain)
{
    unsigned char config ;
    unsigned char buffer [4] ;
    int value = 0 ;
    int r;
    
    config = 0x80 | (channel << 5) | (sampleRate << 2) | (gain);

    r = i2c_smbus_access (fd, I2C_SMBUS_WRITE, config, I2C_SMBUS_BYTE, NULL);
    if(r<0)
      return r;

  switch (sampleRate)	// Sample rate
  {
    case MCP3422_SR_3_75:			// 18 bits
      waitForConversion (fd, &buffer [0], 4) ;
      value = ((buffer [0] & 3) << 16) | (buffer [1] << 8) | buffer [2] ;
      break ;

    case MCP3422_SR_15:				// 16 bits
      waitForConversion (fd, buffer, 3) ;
      value = (buffer [0] << 8) | buffer [1] ;
      break ;

    case MCP3422_SR_60:				// 14 bits
      waitForConversion (fd, buffer, 3) ;
      value = ((buffer [0] & 0x3F) << 8) | buffer [1] ;
      break ;

    case MCP3422_SR_240:			// 12 bits - default
      waitForConversion (fd, buffer, 3) ;
      value = ((buffer [0] & 0x0F) << 8) | buffer [1] ;
      break ;
  }

    return value;
}
