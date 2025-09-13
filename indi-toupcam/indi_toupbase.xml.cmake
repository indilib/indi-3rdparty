<?xml version="1.0" encoding="UTF-8"?>
<driversList>
<devGroup group="CCDs">
        <device label="Toupcam" mdpd="true" manufacturer="Toupcam">
                <driver name="Touptek">indi_toupcam_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
</devGroup>
<devGroup group="Filter Wheels">
        <device label="Toupcam EFW" mdpd="true" manufacturer="Toupcam">
                <driver name="Touptek">indi_toupcam_wheel</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
</devGroup>
<devGroup group="Focusers">
        <device label="Toupcam AAF" mdpd="true" manufacturer="Toupcam">
                <driver name="Touptek">indi_toupcam_focuser</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
</devGroup>
</driversList>
