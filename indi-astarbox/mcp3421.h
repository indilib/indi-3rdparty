#include <linux/i2c-dev.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <sys/ioctl.h>

#define ADC_ADDR0 0x68
#define ADC_ADDR2 0x6a

// I2C definitions

#define I2C_SLAVE	0x0703
#define I2C_SMBUS	0x0720	/* SMBus-level access */

#define I2C_SMBUS_READ	1
#define I2C_SMBUS_WRITE	0

// SMBus transaction types

#define I2C_SMBUS_QUICK		    0
#define I2C_SMBUS_BYTE		    1
#define I2C_SMBUS_BYTE_DATA	    2
#define I2C_SMBUS_WORD_DATA	    3
#define I2C_SMBUS_PROC_CALL	    4
#define I2C_SMBUS_BLOCK_DATA	    5
#define I2C_SMBUS_I2C_BLOCK_BROKEN  6
#define I2C_SMBUS_BLOCK_PROC_CALL   7		/* SMBus 2.0 */
#define I2C_SMBUS_I2C_BLOCK_DATA    8

// SMBus messages

#define I2C_SMBUS_BLOCK_MAX	32	/* As specified in SMBus standard */
#define I2C_SMBUS_I2C_BLOCK_MAX	32	/* Not specified but we use same structure */


#define	MCP3422_SR_240	0
#define	MCP3422_SR_60	1
#define	MCP3422_SR_15	2
#define	MCP3422_SR_3_75	3

#define	MCP3422_GAIN_1	0
#define	MCP3422_GAIN_2	1
#define	MCP3422_GAIN_4	2
#define	MCP3422_GAIN_8	3

union i2c_smbus_data {
  uint8_t  byte ;
  uint16_t word ;
  uint8_t  block [I2C_SMBUS_BLOCK_MAX + 2] ;	// block [0] is used for length + one more for PEC
} ;


class mcp3421
{
public:
    mcp3421();
    ~mcp3421();
    void setBusID(int nBus);
    bool isMCP3421Present();
    int openMCP3421();
    int closeMCP3421();
    double getVoltValue();

private:
    int m_nADCAdress;

    int m_fd;
    double m_value;
    double m_dVperDiv;
    int	m_resistorDividerRatio;
    std::string m_sDevPath;
    
    static inline int i2c_smbus_access (int fd, char rw, uint8_t command, int size, union i2c_smbus_data *data);
    int openDevice(const char* devPath, int devAddr);
    void waitForConversion (int fd, unsigned char *buffer, int n);
    int readValue(int fd, int channel, int sampleRate, int gain);

};
