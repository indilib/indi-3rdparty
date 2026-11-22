#include "playerone_wheel.h"
#include "playerone_wheel_hotplug_handler.h"

#include <eventloop.h>
#include <hotplugmanager.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{
std::vector<PWProperties> devices;
PWState state = PW_STATE_OPENED;
PWErrors positionResult = PW_OK;
PWErrors stateResult = PW_OK;
PWErrors closeResult = PW_OK;
int position = 0;
int openCount = 0;
int closeCount = 0;
int gotoCount = 0;
int failIndex = -1;
std::chrono::steady_clock::time_point lastClose;

PWProperties makeDevice(int handle, const char *name)
{
    PWProperties info {};
    info.Handle = handle;
    info.PositionCount = 7;
    std::strncpy(info.Name, name, sizeof(info.Name) - 1);
    return info;
}

class TestWheel : public POAWHEEL
{
    public:
        using POAWHEEL::POAWHEEL;
        using POAWHEEL::TimerHit;
        using POAWHEEL::SelectFilter;

        int slot() const
        {
            return CurrentFilter;
        }
        int target() const
        {
            return TargetFilter;
        }
        IPState slotState() const
        {
            return FilterSlotNP.getState();
        }
};

void connect(TestWheel &wheel)
{
    char connectName[] = "CONNECT";
    char *names[] = { connectName };
    ISState values[] = { ISS_ON };
    wheel.ISNewSwitch(wheel.getDeviceName(), "CONNECTION", values, names, 1);
}

void disconnect(TestWheel &wheel)
{
    char disconnectName[] = "DISCONNECT";
    char *names[] = { disconnectName };
    ISState values[] = { ISS_ON };
    wheel.ISNewSwitch(wheel.getDeviceName(), "CONNECTION", values, names, 1);
}

struct TimedTest
{
    TestWheel *wheel;
    std::chrono::steady_clock::time_point start;
    bool completeBeforeTimeout;
    bool responsive {false};
};

void checkResponsive(void *context)
{
    auto &test = *static_cast<TimedTest *>(context);
    assert(!test.wheel->isConnected());
    assert(closeCount == 0);
    test.responsive = true;
}

void nearTimeout(void *context)
{
    auto &test = *static_cast<TimedTest *>(context);
    assert(test.responsive);
    assert(closeCount == 0);
    assert(test.wheel->getSwitch("CONNECTION").getState() == IPS_BUSY);
    if (test.completeBeforeTimeout)
        state = PW_STATE_OPENED;
}

void finishTimedTest(void *context)
{
    auto &test = *static_cast<TimedTest *>(context);
    assert(std::chrono::steady_clock::now() - test.start >= std::chrono::seconds(20));
    if (test.completeBeforeTimeout)
    {
        assert(test.wheel->isConnected());
        assert(closeCount == 0);
    }
    else
    {
        assert(!test.wheel->isConnected());
        assert(closeCount == 1);
        assert(lastClose - test.start >= std::chrono::seconds(20));
        assert(test.wheel->getSwitch("CONNECTION").getState() == IPS_ALERT);
    }
    disconnect(*test.wheel);
    delete test.wheel;
    std::exit(0);
}

void attachWheel(void *)
{
    devices = {makeDevice(1, "Phoenix")};
}

void beginIncompleteDiscovery(void *context)
{
    auto &handler = *static_cast<PlayerOneWheelHotPlugHandler *>(context);
    assert(handler.getManagedDevices().count("1") == 1);
    assert(handler.getManagedDevices().at("1")->getSwitch("CONNECTION"));
    failIndex = 0;
    auto ids = handler.discoverConnectedDeviceIdentifiers();
    assert(ids.size() == 1 && ids[0] == "1");
    devices = {makeDevice(2, "Phoenix")};
}

void finishIncompleteDiscovery(void *context)
{
    auto &handler = *static_cast<PlayerOneWheelHotPlugHandler *>(context);
    assert(handler.getManagedDevices().count("1") == 1);
    assert(handler.getManagedDevices().count("2") == 0);
    failIndex = -1;
}

void finishHotplugTest(void *context)
{
    auto &handler = *static_cast<PlayerOneWheelHotPlugHandler *>(context);
    assert(handler.getManagedDevices().count("1") == 0);
    assert(handler.getManagedDevices().count("2") == 1);
    assert(handler.getManagedDevices().at("2")->getSwitch("CONNECTION"));
    std::exit(0);
}
}

extern "C"
{
    int POAGetPWCount()
    {
        return static_cast<int>(devices.size());
    }
    PWErrors POAGetPWProperties(int index, PWProperties *info)
    {
        if (index == failIndex || index < 0 || static_cast<size_t>(index) >= devices.size())
            return PW_ERROR_INVALID_INDEX;
        *info = devices[index];
        return PW_OK;
    }
    PWErrors POAGetPWPropertiesByHandle(int handle, PWProperties *info)
    {
        for (const auto &device : devices)
            if (device.Handle == handle)
            {
                *info = device;
                return PW_OK;
            }
        return PW_ERROR_INVALID_HANDLE;
    }
    PWErrors POAOpenPW(int)
    {
        ++openCount;
        return PW_OK;
    }
    PWErrors POAClosePW(int)
    {
        ++closeCount;
        lastClose = std::chrono::steady_clock::now();
        return closeResult;
    }
    PWErrors POAGetPWState(int, PWState *value)
    {
        *value = state;
        return stateResult;
    }
    PWErrors POAGetCurrentPosition(int, int *value)
    {
        *value = position;
        return positionResult;
    }
    PWErrors POAGotoPosition(int, int)
    {
        ++gotoCount;
        return PW_OK;
    }
    PWErrors POAGetOneWay(int, int *value)
    {
        *value = 0;
        return PW_OK;
    }
    PWErrors POASetOneWay(int, int)
    {
        return PW_OK;
    }
    PWErrors POAResetPW(int)
    {
        return PW_OK;
    }
}

int main(int argc, char **argv)
{
    devices = { makeDevice(1, "Phoenix") };
    if (argc == 2 && std::strcmp(argv[1], "hotplug") == 0)
    {
        devices.clear();
        auto handler = std::make_shared<PlayerOneWheelHotPlugHandler>();
        auto &manager = INDI::HotPlugManager::getInstance();
        manager.registerHandler(handler);
        manager.setNonUdevPollingDuration(0);
        manager.start(1000);
        addTimer(1200, attachWheel, nullptr);
        addTimer(2500, beginIncompleteDiscovery, handler.get());
        addTimer(4800, finishIncompleteDiscovery, handler.get());
        addTimer(7200, finishHotplugTest, handler.get());
        eventLoop();
        return 1;
    }
    if (argc == 2)
    {
        auto *wheel = new TestWheel(devices[0], "Timed Test Wheel");
        wheel->ISGetProperties(nullptr);
        state = PW_STATE_MOVING;
        const auto start = std::chrono::steady_clock::now();
        connect(*wheel);
        TimedTest test {wheel, start, std::strcmp(argv[1], "ready") == 0};
        addTimer(2000, checkResponsive, &test);
        addTimer(19500, nearTimeout, &test);
        addTimer(20500, finishTimedTest, &test);
        eventLoop();
        return 1;
    }
    TestWheel wheel(devices[0], "Test Wheel");
    wheel.ISGetProperties(nullptr);

    state = PW_STATE_MOVING;
    connect(wheel);
    assert(!wheel.isConnected());
    assert(wheel.getSwitch("CONNECTION").getState() == IPS_BUSY);
    assert(!wheel.getNumber("FILTER_SLOT"));
    for (int i = 0; i < 79; ++i)
        wheel.TimerHit();
    assert(closeCount == 0);
    state = PW_STATE_OPENED;
    position = 3;
    wheel.TimerHit();
    assert(wheel.isConnected());
    assert(wheel.slot() == 4 && wheel.target() == 4);
    assert(gotoCount == 0);

    assert(wheel.SelectFilter(6));
    positionResult = PW_ERROR_IS_MOVING;
    wheel.TimerHit();
    assert(wheel.slot() == 4);
    double requested[] = {2};
    char slotName[] = "FILTER_SLOT_VALUE";
    char *slotNames[] = {slotName};
    wheel.ISNewNumber(wheel.getDeviceName(), "FILTER_SLOT", requested, slotNames, 1);
    assert(wheel.target() == 6);
    positionResult = PW_OK;
    position = 5;
    wheel.TimerHit();
    assert(wheel.slot() == 6 && wheel.slotState() == IPS_OK);

    char calibrateName[] = "CALIBRATE";
    char *calibrateNames[] = {calibrateName};
    ISState calibrateValues[] = {ISS_ON};
    wheel.ISNewSwitch(wheel.getDeviceName(), "FILTER_CALIBRATION", calibrateValues, calibrateNames, 1);
    positionResult = PW_ERROR_IS_MOVING;
    wheel.TimerHit();
    assert(wheel.getSwitch("FILTER_CALIBRATION").getState() == IPS_BUSY);
    positionResult = PW_OK;
    position = 0;
    wheel.TimerHit();
    assert(wheel.getSwitch("FILTER_CALIBRATION").getState() == IPS_OK);
    assert(wheel.slot() == 1 && wheel.target() == 1);

    disconnect(wheel);
    assert(closeCount == 1);

    state = PW_STATE_MOVING;
    connect(wheel);
    for (int i = 0; i < 79; ++i)
        wheel.TimerHit();
    assert(closeCount == 1);
    wheel.TimerHit();
    assert(closeCount == 2);
    assert(wheel.getSwitch("CONNECTION").getState() == IPS_ALERT);

    connect(wheel);
    disconnect(wheel);
    assert(closeCount == 3);
    wheel.TimerHit();
    assert(closeCount == 3);

    connect(wheel);
    stateResult = PW_ERROR_OPERATION_FAILED;
    wheel.TimerHit();
    stateResult = PW_OK;
    assert(closeCount == 4);
    assert(wheel.getSwitch("CONNECTION").getState() == IPS_ALERT);

    connect(wheel);
    closeResult = PW_ERROR_OPERATION_FAILED;
    disconnect(wheel);
    assert(closeCount == 5);
    closeResult = PW_OK;
    disconnect(wheel);
    assert(closeCount == 6);

    state = PW_STATE_OPENED;
    connect(wheel);
    wheel.TimerHit();
    assert(wheel.isConnected());
    wheel.ISNewSwitch(wheel.getDeviceName(), "FILTER_CALIBRATION", calibrateValues, calibrateNames, 1);
    positionResult = PW_ERROR_IS_MOVING;
    wheel.TimerHit();
    wheel.unplug();
    assert(closeCount == 7);
    wheel.TimerHit();
    assert(closeCount == 7);
    positionResult = PW_OK;

    PlayerOneWheelHotPlugHandler handler;
    auto ids = handler.discoverConnectedDeviceIdentifiers();
    assert(ids.size() == 1 && ids[0] == "1");
    auto first = handler.createDevice("1");
    assert(first);
    devices.push_back(makeDevice(2, "Phoenix"));
    auto second = handler.createDevice("2");
    assert(second && std::strcmp(first->getDeviceName(), second->getDeviceName()) != 0);
    failIndex = 0;
    ids = handler.discoverConnectedDeviceIdentifiers();
    assert(ids.size() == 2);
    failIndex = -1;
    handler.destroyDevice(first);
    assert(handler.getManagedDevices().size() == 1);
    devices.erase(devices.begin());
    ids = handler.discoverConnectedDeviceIdentifiers();
    assert(ids.size() == 1 && ids[0] == "2");
    auto replacement = handler.createDevice("2");
    assert(replacement == second);
    assert(openCount == 6);

    devices = {makeDevice(1, "Phoenix"), makeDevice(2, "Phoenix")};
    PlayerOneWheelHotPlugHandler initialHandler;
    auto initialFirst = initialHandler.createDevice("1");
    auto initialSecond = initialHandler.createDevice("2");
    assert(initialFirst && initialSecond);
    assert(std::strcmp(initialFirst->getDeviceName(), "PlayerOne Phoenix 0") == 0);
    assert(std::strcmp(initialSecond->getDeviceName(), "PlayerOne Phoenix 1") == 0);
}
