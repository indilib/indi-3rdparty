#include "IndiSerialWrapper.hpp"

using namespace GoToDriver;

#define UNUSED(x) (void)(x)

IndiSerialWrapper::IndiSerialWrapper() :
    mTtyFd(-1)
{

}

IndiSerialWrapper::~IndiSerialWrapper()
{

}

int IndiSerialWrapper::GetFD()
{
    return mTtyFd;
}

void IndiSerialWrapper::SetFD(int fd)
{
    mTtyFd = fd;
}

//Opens the serial device, the acutal implementation has to deal with the handles!
bool IndiSerialWrapper::Open()
{
    //indi opens the serial interface for us, so assume this worked.
    return true;
}

//Closes the serial device, the actual implementation has to deal with the handles!
bool IndiSerialWrapper::Close()
{
    //indi closes the serial interface for us so assume this worked.
    return true;
}

//Returns true if the serial port is open and ready to receive or transmit data.
bool IndiSerialWrapper::IsOpen()
{
    return (mTtyFd > -1);
}

//Returns the number of bytes to read available in the serial receiver queue.
size_t IndiSerialWrapper::BytesToRead()
{
    if(IsOpen())
    {
        size_t chars_available = 0;
        int result = -1;

        result = ioctl(mTtyFd, FIONREAD, &chars_available);

        //std::cerr << "Bytes Available: FD " << std::dec << mTtyFd << " data: " << chars_available << std::endl;

        if(result > -1)
        {
            return chars_available;
        }
    }

    return 0;
}

//Reads a byte from the serial device. Can safely cast to uint8_t unless -1 is returned, corresponding to "stream end reached".
int16_t IndiSerialWrapper::ReadByte()
{
    if(IsOpen())
    {
        int bytesRead = 0;
        char dataByte = 0x00;
        int result = tty_read(mTtyFd, &dataByte, 1, 0, &bytesRead);

        if(result == TTY_OK)
        {
            //std::cerr << "Read OK: FD " << std::dec << mTtyFd << " data: " << std::hex << dataByte << std::dec << std::endl;
            return dataByte;
        }
        /*else
        {
        	//TODO: log error;
        	//LOGF_ERROR("BresserExosIIDriver::IndiSerialWrapper::ReadByte: error reading from serial device...");
        	std::cerr << "Error while reading!" << std::endl;
        }*/
    }

    return -1;
}

//writes the buffer to the serial interface.
//this function should handle all the quirks of various serial interfaces.
bool IndiSerialWrapper::Write(uint8_t* buffer, size_t offset, size_t length)
{
    UNUSED(offset);
    
    {
        std::lock_guard<std::mutex> guard(mMutex);

        if(IsOpen() && buffer != nullptr && length > 0)
        {
            int nbytes_written;
            int result = tty_write(mTtyFd, (char*)buffer, length, &nbytes_written);

            if(result != TTY_OK)
            {
                std::cerr << "Error while writing!" << std::endl;
                //TODO log error:
                //LOGF_ERROR("BresserExosIIDriver::IndiSerialWrapper::Write: error writing to serial device...");
                return false;
            }
            return true;
        }
    }
    return false;
}

//flush the buffer.
bool IndiSerialWrapper::Flush()
{
    if(IsOpen())
    {
        int result = tcflush(mTtyFd, TCIOFLUSH);

        if(result != TTY_OK)
        {
            std::cerr << "Error while flushing!" << std::endl;
            return false;
        }
        return true;
    }
    return false;
}
