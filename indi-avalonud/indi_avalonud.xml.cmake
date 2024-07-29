<?xml version="1.0" encoding="UTF-8"?>
<driversList>
    <devGroup group="Telescopes">
        <device label="AvalonUD Telescope" manufacturer="Avalon-Instruments">
            <driver name="AvalonUD Telescope">indi_avalonud_telescope</driver>
            <version>@AVALONUD_VERSION_MAJOR@.@AVALONUD_VERSION_MINOR@</version>
        </device>
    </devGroup>
    <devGroup group="Focusers">
        <device label="AvalonUD Focuser" manufacturer="Avalon-Instruments">
            <driver name="AvalonUD Focuser">indi_avalonud_focuser</driver>
            <version>@AVALONUD_VERSION_MAJOR@.@AVALONUD_VERSION_MINOR@</version>
        </device>
    </devGroup>
    <devGroup group="Auxiliary">
        <device label="AvalonUD Aux" manufacturer="Avalon-Instruments">
            <driver name="AvalonUD Aux">indi_avalonud_aux</driver>
            <version>@AVALONUD_VERSION_MAJOR@.@AVALONUD_VERSION_MINOR@</version>
        </device>
    </devGroup>
</driversList>
