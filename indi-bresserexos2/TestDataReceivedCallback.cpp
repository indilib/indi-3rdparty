#include "TestDataReceivedCallback.hpp"

using namespace Testing;


TestDataReceivedCallback::TestDataReceivedCallback()
{

}

TestDataReceivedCallback::~TestDataReceivedCallback()
{

}

void TestDataReceivedCallback::OnPointingCoordinatesReceived(float right_ascension, float declination)
{
    std::cout << "Received data : RA: " << right_ascension << " DEC:" << declination << std::endl;
}
