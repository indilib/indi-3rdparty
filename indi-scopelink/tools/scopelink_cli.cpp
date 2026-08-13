/*
    ScopeLink INDI driver - command line tool

    Everything the driver can read out of a controller, without libindi, a client or a running
    indiserver in the way. It exists mostly for support: "run this and send me what it prints" is a far
    shorter conversation than talking somebody through a graphical client over email.

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#include "scopelink/device.h"
#include "scopelink/faults.h"
#include "scopelink/parameters.h"
#include "scopelink/protocol.h"
#include "scopelink/transport.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{

void usage()
{
    printf("Usage: scopelink-cli [--port /dev/ttyACM0] [--verbose] <command>\n"
           "\n"
           "Commands:\n"
           "  info          Identification, capabilities and one status sample\n"
           "  status        One status sample\n"
           "  parameters    Every configuration identifier this controller holds\n"
           "  export FILE   Write the configuration to FILE, in the Windows editor's format\n"
           "  import FILE   Write the configuration in FILE to the controller\n"
           "  faults        The fault store with freeze frames\n"
           "  clear-faults  Clear every stored fault\n"
           "  eeprom        EEPROM wear counters\n");
}

const char *yesNo(bool value)
{
    return value ? "yes" : "no";
}

void printIdentification(scopelink::Device &device)
{
    const scopelink::Identification &identification = device.identification();
    const scopelink::Capabilities &capabilities     = device.capabilities();

    printf("Controller\n");
    printf("  Hardware            %d.%d\n", identification.hardwareMajor, identification.hardwareMinor);
    printf("  Interface           %d.%d\n", identification.interfaceMajor, identification.interfaceMinor);
    printf("  Unit                %s\n", identification.hardwareIdentifier.c_str());
    printf("  Firmware            %s\n", identification.softwareIdentifier.c_str());
    printf("\n");
    printf("Capabilities\n");
    printf("  Status frame        %zu bytes\n", capabilities.statusFrameLength);
    printf("  Freeze frame        %zu bytes\n", capabilities.dtcSnapshotLength);
    printf("  USB hub             %s\n", yesNo(capabilities.hasUsbHub));
    printf("  Smart switches      %s\n", yesNo(capabilities.hasSmartSwitchDiagnostics));
    printf("  Temperature sensor  %s\n", yesNo(capabilities.hasTemperatureSensor));
    printf("\n");
}

void printStatus(scopelink::Device &device)
{
    const scopelink::Status &status = device.refreshStatus();

    printf("Status\n");
    printf("  Supply              %.2f V\n", status.supplyVoltage / 1000.0);
    printf("  Rear fan            %.2f V%s\n", status.fanAVoltage / 1000.0,
           status.fanAManualOverrideEnabled ? " (manual)" : "");
    printf("  Side fan            %.2f V%s\n", status.fanBVoltage / 1000.0,
           status.fanBManualOverrideEnabled ? " (manual)" : "");
    printf("  Fan targets         %.1f K / %.1f K\n", status.fanATargetDT / 50.0, status.fanBTargetDT / 50.0);

    if (status.ambientTemperatureValid)
        printf("  Ambient             %.1f C\n", status.ambientTemperatureCelsius());
    else
        printf("  Ambient             not available\n");

    if (status.mirrorTemperatureValid)
        printf("  Mirror              %.1f C\n", status.mirrorTemperatureCelsius());
    else
        printf("  Mirror              not available\n");

    printf("  Focuser             %d steps, %s, load %u%%\n", status.motor1Position,
           status.motor1Moving ? "moving" : "stopped", status.motor1Load);

    printf("  Front flap          %d steps, %s, load %u%%\n", status.motor2Position,
           status.motor2Moving ? "moving" : "stopped", status.motor2Load);
    printf("  Flat panel          %d %%\n", status.flatboxDuty);
    printf("  Aux outputs         %s / %s\n", yesNo(status.powerSwitch1State), yesNo(status.powerSwitch2State));

    if (device.capabilities().hasUsbHub)
    {
        printf("  USB hub             port 1 %s%s, port 2 %s%s\n", yesNo(status.usb1PowerActive),
               status.usb1PowerFailure ? " FAULT" : "", yesNo(status.usb2PowerActive),
               status.usb2PowerFailure ? " FAULT" : "");
    }

    printf("  Faults              %d stored, %d active\n", status.storedFaultCount, status.activeFaultCount);
    printf("  Controller          %d C, CPU %d%% (peak %d%%), stack %d%%, I2C errors %d\n",
           status.controllerTemperature, status.cpuLoad, status.peakCpuLoad, status.stackUsage, status.i2cErrorCounter);
    printf("\n");
}

int readParameters(scopelink::Device &device, std::vector<scopelink::Did> &catalogue, bool quiet)
{
    int failed = 0;

    for (scopelink::Did &identifier : catalogue)
    {
        if (identifier.read(device))
        {
            if (!quiet)
                printf("  0x%04X  %-46s %d\n", identifier.id(), identifier.description().c_str(), identifier.value());
        }
        else
        {
            failed++;

            if (!quiet)
                printf("  0x%04X  %-46s not readable\n", identifier.id(), identifier.description().c_str());
        }
    }

    return failed;
}

void printFaults(scopelink::Device &device)
{
    const std::vector<scopelink::Fault> faults = scopelink::Fault::readAll(device);

    if (faults.empty())
    {
        printf("The fault store is empty.\n");
        return;
    }

    for (size_t index = 0; index < faults.size(); index++)
    {
        printf("Fault %zu: %s\n", index + 1, faults[index].summary().c_str());
        printf("  %s\n\n", faults[index].snapshotText().c_str());
    }
}

} // namespace

int main(int argc, char *argv[])
{
    std::string port = "/dev/ttyACM0";
    bool verbose     = false;
    std::vector<std::string> arguments;

    for (int index = 1; index < argc; index++)
    {
        const std::string argument = argv[index];

        if ((argument == "--port") && ((index + 1) < argc))
            port = argv[++index];
        else if (argument == "--verbose")
            verbose = true;
        else if ((argument == "--help") || (argument == "-h"))
        {
            usage();
            return 0;
        }
        else
        {
            arguments.push_back(argument);
        }
    }

    if (arguments.empty())
    {
        usage();
        return 1;
    }

    const std::string &action = arguments[0];

    scopelink::PosixSerialTransport transport(port, scopelink::Device::ReceiveTimeoutMs);
    scopelink::Protocol protocol(transport);
    scopelink::Device device(protocol);

    if (verbose)
    {
        const auto sink = [](const char *scope, const std::string &message)
        { fprintf(stderr, "[%s] %s\n", scope, message.c_str()); };

        protocol.setLogger(sink);
        device.setLogger(sink);
    }

    try
    {
        transport.open();
        device.open();

        if ((action == "info") || (action == "status"))
        {
            if (action == "info")
                printIdentification(device);

            printStatus(device);
        }
        else if (action == "parameters")
        {
            std::vector<scopelink::Did> catalogue = scopelink::buildDidCatalogue(device.capabilities());

            printf("Configuration\n");

            const int failed = readParameters(device, catalogue, false);

            printf("\n%zu identifiers, %d not readable.\n", catalogue.size(), failed);
        }
        else if ((action == "export") || (action == "import"))
        {
            if (arguments.size() < 2)
            {
                fprintf(stderr, "'%s' needs a file name.\n", action.c_str());
                return 1;
            }

            std::vector<scopelink::Did> catalogue = scopelink::buildDidCatalogue(device.capabilities());

            readParameters(device, catalogue, true);

            if (action == "export")
            {
                scopelink::did_file::write(arguments[1], device.identification().toString(), catalogue);
                printf("Exported %zu identifiers to '%s'.\n", catalogue.size(), arguments[1].c_str());
            }
            else
            {
                std::vector<scopelink::Did> loaded = catalogue;
                const size_t applied               = scopelink::did_file::read(arguments[1], loaded);

                int written = 0;
                int failed  = 0;

                for (size_t index = 0; index < catalogue.size(); index++)
                {
                    if (loaded[index].value() == catalogue[index].value())
                        continue;

                    catalogue[index].setValue(loaded[index].value());

                    if (catalogue[index].write(device))
                    {
                        written++;
                        printf("  %-46s %d\n", catalogue[index].description().c_str(), catalogue[index].value());
                    }
                    else
                    {
                        failed++;
                        fprintf(stderr, "  %-46s %s\n", catalogue[index].description().c_str(),
                                catalogue[index].lastError().c_str());
                    }
                }

                printf("Read %zu value(s) from '%s': %d written, %d failed.\n", applied, arguments[1].c_str(), written,
                       failed);
            }
        }
        else if (action == "faults")
        {
            printFaults(device);
        }
        else if (action == "clear-faults")
        {
            const std::vector<scopelink::Fault> faults = scopelink::Fault::readAll(device);

            for (const scopelink::Fault &fault : faults)
                fault.clear(device);

            printf("Cleared %zu stored fault(s).\n", faults.size());
            printFaults(device);
        }
        else if (action == "eeprom")
        {
            const scopelink::EepromStatistics statistics = scopelink::EepromStatistics::read(device);

            printf("EEPROM wear\n");
            printf("  Page erases         %d\n", statistics.pageEraseCounter);
            printf("  Dataset writes      %d\n", statistics.datasetCounter);
            printf("  Learnt data writes  %d\n", statistics.learntDataCounter);
            printf("  Fault block 1       %d\n", statistics.faultStoreBlock1Counter);
            printf("  Fault block 2       %d\n", statistics.faultStoreBlock2Counter);
            printf("  Fault block 3       %d\n", statistics.faultStoreBlock3Counter);
            printf("  Fault block 4       %d\n", statistics.faultStoreBlock4Counter);
        }
        else
        {
            fprintf(stderr, "Unknown command '%s'.\n\n", action.c_str());
            usage();
            return 1;
        }
    }
    catch (const std::exception &error)
    {
        fprintf(stderr, "%s\n", error.what());
        return 1;
    }

    return 0;
}
