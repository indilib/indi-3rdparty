#include "TestSerialImplementation.hpp"

using Testing::TestSerialImplementation;

TestSerialImplementation::TestSerialImplementation(std::string &portDevice, speed_t connectionSpeed = B9600) :
    mPortName(portDevice),
    mTtyFd(-1),
    mConnectionSpeed(connectionSpeed)
{
    memset(&mSerialSocked, 0, sizeof(mSerialSocked));
}

TestSerialImplementation::~TestSerialImplementation()
{
    if(IsOpen())
    {
        Close();
    }
}

//Opens the serial device, the acutal implementation has to deal with the handles!
bool TestSerialImplementation::Open()
{
    if(!IsOpen())
    {
        memset(&mSerialSocked, 0, sizeof(mSerialSocked));

        mSerialSocked.c_iflag = 0;
        mSerialSocked.c_oflag = 0;
        mSerialSocked.c_cflag = CS8 | CREAD | CLOCAL; // 8n1, see termios.h for more information
        mSerialSocked.c_lflag = 0;
        mSerialSocked.c_cc[VMIN] = 1;
        mSerialSocked.c_cc[VTIME] = 5;

        mTtyFd = open(mPortName.c_str(), O_RDWR | O_NONBLOCK);

        cfsetospeed(&mSerialSocked, mConnectionSpeed); // 9600 baud
        cfsetispeed(&mSerialSocked, mConnectionSpeed);

        tcsetattr(mTtyFd, TCSANOW, &mSerialSocked);
        return true;
    }
    return false;
}

//Closes the serial device, the actual implementation has to deal with the handles!
bool TestSerialImplementation::Close()
{
    if(IsOpen())
    {
        close(mTtyFd);

        mTtyFd = -1;

        return true;
    }
    return false;
}

//Returns true if the serial port is open and ready to receive or transmit data.
bool TestSerialImplementation::IsOpen()
{
    return (mTtyFd > -1);
}

//Returns the number of bytes to read available in the serial receiver queue.
size_t TestSerialImplementation::BytesToRead()
{
    if(IsOpen())
    {
        size_t chars_available = 0;
        int result = -1;

        result = ioctl(mTtyFd, FIONREAD, &chars_available);

        if(result > -1)
        {
            return chars_available;
        }
    }

    return 0;
}

//Reads a byte from the serial device. Can safely cast to uint8_t unless -1 is returned, corresponding to "stream end reached".
int16_t TestSerialImplementation::ReadByte()
{
    if(IsOpen())
    {
        uint8_t dataByte = 0x00;
        int result = read(mTtyFd, &dataByte, 1);

        if(result > 0)
        {
            return dataByte;
        }
    }

    return -1;
}

//writes the buffer to the serial interface.
//this function should handle all the quirks of various serial interfaces.
bool TestSerialImplementation::Write(uint8_t* buffer, size_t offset, size_t length)
{
    if(IsOpen() && buffer != nullptr && length > 0)
    {
        int result = write(mTtyFd, buffer, length);

        return true;
    }
    return false;
}

bool TestSerialImplementation::Flush()
{
    if(IsOpen())
    {
        int result = tcflush(mTtyFd, TCIOFLUSH);

        return true;
    }
    return false;
}
