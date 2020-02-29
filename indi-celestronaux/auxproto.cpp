#include "auxproto.h"

#include <algorithm>
#include <math.h>
#include <queue>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <stdio.h>

#define READ_TIMEOUT 1 		// s
#define CTS_TIMEOUT 100		// ms
#define RTS_DELAY 50		// ms

#define BUFFER_SIZE 10240
int MAX_CMD_LEN = 32;
bool DEBUG      = false;

//////////////////////////////////////////////////
/////// Utility functions
//////////////////////////////////////////////////


void prnBytes(unsigned char *b, int n)
{
    fprintf(stderr, "[");
    for (int i = 0; i < n; i++)
        fprintf(stderr, "%02x ", b[i]);
    fprintf(stderr, "]\n");
}

void dumpMsg(buffer buf)
{
    fprintf(stderr, "MSG: ");
    for (unsigned int i = 0; i < buf.size(); i++)
    {
        fprintf(stderr, "%02x ", buf[i]);
    }
    fprintf(stderr, "\n");
}

////////////////////////////////////////////////
//////  AUXCommand class
////////////////////////////////////////////////

AUXCommand::AUXCommand()
{
    data.reserve(MAX_CMD_LEN);
}

AUXCommand::AUXCommand(buffer buf)
{
    data.reserve(MAX_CMD_LEN);
    parseBuf(buf);
}

AUXCommand::AUXCommand(AUXCommands c, AUXtargets s, AUXtargets d, buffer dat)
{
    cmd = c;
    src = s;
    dst = d;
    data.reserve(MAX_CMD_LEN);
    data = dat;
    len  = 3 + data.size();
}

AUXCommand::AUXCommand(AUXCommands c, AUXtargets s, AUXtargets d)
{
    cmd = c;
    src = s;
    dst = d;
    data.reserve(MAX_CMD_LEN);
    len = 3 + data.size();
}

void AUXCommand::dumpCmd()
{
    if (DEBUG)
    {
        fprintf(stderr, "(%02x) %02x -> %02x: ", cmd, src, dst);
        for (unsigned int i = 0; i < data.size(); i++)
        {
            fprintf(stderr, "%02x ", data[i]);
        }
        fprintf(stderr, "\n");
    }
}

void AUXCommand::fillBuf(buffer &buf)
{
    buf.resize(len + 3);
    buf[0] = 0x3b;
    buf[1] = (unsigned char)len;
    buf[2] = (unsigned char)src;
    buf[3] = (unsigned char)dst;
    buf[4] = (unsigned char)cmd;
    for (unsigned int i = 0; i < data.size(); i++)
    {
        buf[i + 5] = data[i];
    }
    buf.back() = checksum(buf);
    //dumpMsg(buf);
}

void AUXCommand::parseBuf(buffer buf)
{
    len   = buf[1];
    src   = (AUXtargets)buf[2];
    dst   = (AUXtargets)buf[3];
    cmd   = (AUXCommands)buf[4];
    data  = buffer(buf.begin() + 5, buf.end() - 1);
    valid = (checksum(buf) == buf.back());
    if (not valid)
    {
        fprintf(stderr, "Checksum error: %02x vs. %02x", checksum(buf), buf.back());
        dumpMsg(buf);
    };
}

unsigned char AUXCommand::checksum(buffer buf)
{
    int l  = buf[1];
    int cs = 0;
    for (int i = 1; i < l + 2; i++)
    {
        cs += buf[i];
    }
    //fprintf(stderr,"CS: %x  %x  %x\n", cs, ~cs, ~cs+1);
    return (unsigned char)(((~cs) + 1) & 0xFF);
    //return ((~sum([ord(c) for c in msg]) + 1) ) & 0xFF
}

// One definition rule (ODR) constants
// AUX commands use 24bit integer as a representation of angle in units of
// fractional revolutions. Thus 2^24 steps makes full revolution.
const long STEPS_PER_REVOLUTION = 16777216;
const double STEPS_PER_DEGREE   = STEPS_PER_REVOLUTION / 360.0;

long AUXCommand::getPosition()
{
    if (data.size() == 3)
    {
        unsigned int a = (unsigned int)data[0] << 16 | (unsigned int)data[1] << 8 | (unsigned int)data[2];
        //fprintf(stderr,"Angle: %d %08x = %d => %f\n", sizeof(a), a, a, a/pow(2,24));
        return (long)a % STEPS_PER_REVOLUTION;
    }
    else
    {
        return 0;
    }
}

void AUXCommand::setPosition(double p)
{
    setPosition(long(p * STEPS_PER_DEGREE));
}

void AUXCommand::setPosition(long p)
{
    //int a=(int)p; fprintf(stderr,"Angle: %08x = %d => %f\n", a, a, a/pow(2,24));
    data.resize(3);
    // Fold the value to 0-STEPS_PER_REVOLUTION range
    if (p < 0)
    {
        p += STEPS_PER_REVOLUTION;
    }
    p = p % STEPS_PER_REVOLUTION;
    for (int i = 2; i > -1; i--)
    {
        data[i] = (unsigned char)(p & 0xff);
        p >>= 8;
    }
    len = 6;
}

void AUXCommand::setRate(unsigned char r)
{
    data.resize(1);
    len     = 4;
    data[0] = r;
}
