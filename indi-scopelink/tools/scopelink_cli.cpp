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
#include "scopelink/firmware.h"
#include "scopelink/parameters.h"
#include "scopelink/protocol.h"
#include "scopelink/transport.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

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
           "  eeprom        EEPROM wear counters\n"
           "  flash FILE    Write the firmware in FILE to the controller\n"
           "\n"
           "'flash' takes the encrypted firmware file the vendor supplies for this particular unit, which\n"
           "is named after it. The controller restarts into its boot loader, is written and restarts\n"
           "again, all on this one port; nothing else has to be installed. Leave it plugged in until the\n"
           "command returns. An interrupted write leaves the unit in its boot loader with no firmware,\n"
           "which this same command recovers from - run it again.\n");
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
    printf("  Motors              %d\n", capabilities.motorCount);
    printf("  Motor assignment    %s\n", yesNo(capabilities.hasConfigurableMotorRoles));
    printf("  USB hub             %s\n",
           capabilities.hasUsbHub ? ((capabilities.usbDownstreamPortCount == 2) ? "yes, 2 ports" : "yes, 6 ports") :
                                    "no");
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

    // Numbered rather than named after a job. What each motor drives is the assignment, which a
    // controller on interface 1.1 holds and which this tool does not read yet, so naming the first two
    // "focuser" and "flap" here would be printing a guess as a fact.
    for (int index = 0; index < status.motorCount(); index++)
    {
        const scopelink::MotorReading reading = status.motor(index);
        char label[24];

        snprintf(label, sizeof(label), "Motor %d", index + 1);

        printf("  %-18s  %d steps, %s, load %u%%\n", label, reading.position, reading.moving ? "moving" : "stopped",
               reading.load);
    }

    printf("  Flat panel          %d %%\n", status.flatboxDuty);
    printf("  Aux outputs         %s / %s\n", yesNo(status.powerSwitch1State), yesNo(status.powerSwitch2State));

    if (device.capabilities().hasUsbHub)
    {
        // Two ports on generation 3 and six on generation 4, so the row is built rather than written out.
        // Only generation 3 reports a fault per port; generation 4 spent those two bytes on four more
        // ports, and its firmware derived each fault flag from the active flag beside it anyway.
        std::string ports;

        for (int port = 0; port < status.usbDownstreamPortCount(); port++)
        {
            const bool faulted = (port == 0) ? status.usb1PowerFailure : ((port == 1) && status.usb2PowerFailure);

            if (!ports.empty())
                ports += ", ";

            ports += "port " + std::to_string(port + 1) + " " + yesNo(status.usbPowerActive(port));

            if (faulted && device.capabilities().hasUsbPowerFailureReporting)
                ports += " FAULT";
        }

        printf("  USB hub             %s\n", ports.c_str());
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
            // Shown the way the firmware describes it - in its own units, and by name where it is one of a
            // set of settings - because this is the output that gets pasted into a support mail.
            if (!quiet)
            {
                printf("  0x%04X  %-46s %s\n", identifier.id(), identifier.description().c_str(),
                       identifier.descriptor().formatValue(identifier.value()).c_str());
            }
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

/**
 * @brief Writes a firmware file to the controller and reports what happens.
 * @return Process exit status
 *
 * The device has already been identified by the time this runs, which is what supplies the unit
 * identifier the file is checked against. The controller is then asked to restart into its boot loader
 * and the port is handed to the update, which owns it until the new firmware is answering: the device
 * leaves the USB bus twice on the way and the descriptor opened before it went is dead each time.
 */
int flash(scopelink::PosixSerialTransport &transport, scopelink::Protocol &protocol, scopelink::Device &device,
          const std::string &fileName, bool firmwareRunning, bool verbose)
{
    std::string identifier;
    std::string running;

    if (firmwareRunning)
    {
        identifier = device.identification().hardwareIdentifier;
        running    = device.identification().softwareIdentifier;
    }
    else
    {
        // The boot loader answers the unit identifier and its own name and nothing else, which is all
        // that is needed here: the identifier names the file, and the file is what gets it running again.
        const scopelink::Identification identification = scopelink::Identification::readCommon(protocol);

        identifier = identification.hardwareIdentifier;
        running    = identification.softwareIdentifier + " (boot loader, no firmware to run)";
    }

    scopelink::FirmwareFile file = scopelink::FirmwareFile::open(fileName, identifier);

    printf("Unit          %s\n", identifier.c_str());
    printf("Running now   %s\n", running.c_str());
    printf("File          %s, %zu bytes\n\n", file.name().c_str(), file.container().size());

    if (firmwareRunning && !device.requestJumpToBootloader())
    {
        // Not fatal on its own: a controller that restarted before its answer got out looks exactly like
        // one that never heard the request, and only the wait that follows can tell them apart.
        printf("The controller did not acknowledge the restart request; waiting to see whether it "
               "restarts anyway.\n");
    }

    transport.close();

    scopelink::FirmwareUpdate update(transport, file);

    if (verbose)
    {
        update.setLogger([](const char *scope, const std::string &message)
                         { fprintf(stderr, "[%s] %s\n", scope, message.c_str()); });
    }

    std::string shown;

    while (update.step())
    {
        if (update.message() != shown)
        {
            shown = update.message();
            printf("%3d%%  %s\n", update.percent(), shown.c_str());
            fflush(stdout);
        }

        // The update advances by one bounded piece of work per call and leaves the pacing to its caller,
        // which in a driver is the event loop's timer and here is simply this.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (update.stage() != scopelink::FirmwareUpdate::Stage::Complete)
    {
        fprintf(stderr, "\n%s\n", update.failure().c_str());
        return 1;
    }

    printf("100%%  %s\n\nThe firmware has been replaced.\n", update.message().c_str());

    // The configuration lives outside the firmware slot, so it is expected to have come through, but it
    // is worth saying so rather than leaving the user to wonder.
    printf("The controller's stored configuration is outside the area that was erased. Check it with "
           "'scopelink-cli parameters'.\n");

    return 0;
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

        // 'flash' is the one command that has to work on a controller which answers almost nothing. A
        // unit whose previous update was interrupted is sitting in its boot loader with no firmware to
        // identify itself with, and putting firmware back into it is precisely what this command is for.
        bool firmwareRunning = true;

        if (action == "flash")
        {
            try
            {
                device.open();
            }
            catch (const std::exception &error)
            {
                firmwareRunning = false;

                fprintf(stderr, "No firmware answered on %s; checking for a boot loader. (%s)\n", port.c_str(),
                        error.what());
            }
        }
        else
        {
            device.open();
        }

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
                        printf("  %-46s %s\n", catalogue[index].description().c_str(),
                               catalogue[index].descriptor().formatValue(catalogue[index].value()).c_str());
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
        else if (action == "flash")
        {
            if (arguments.size() < 2)
            {
                fprintf(stderr, "'flash' needs a file name.\n");
                return 1;
            }

            return flash(transport, protocol, device, arguments[1], firmwareRunning, verbose);
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
