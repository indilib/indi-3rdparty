<?xml version="1.0" encoding="UTF-8"?>
<driversList>
<devGroup group="CCDs">
        <device label="ZWO CCD" mdpd="true" manufacturer="ZWO">
                <driver name="ZWO CCD">indi_asi_ccd</driver>
                <version>@ASI_VERSION_MAJOR@.@ASI_VERSION_MINOR@</version>
        </device>
</devGroup>
<devGroup group="Filter Wheels">
        <device label="ZWO EFW" mdpd="true" manufacturer="ZWO">
                <driver name="ZWO EFW">indi_asi_wheel</driver>
                <version>@ASI_VERSION_MAJOR@.@ASI_VERSION_MINOR@</version>
        </device>
</devGroup>
<devGroup group="Focusers">
        <device label="ZWO EAF" manufacturer="ZWO">
                <driver name="ZWO EAF">indi_asi_focuser</driver>
                <version>@ASI_VERSION_MAJOR@.@ASI_VERSION_MINOR@</version>
        </device>
        <device label="ZWO CAA" mdpd="true" manufacturer="ZWO">
                <driver name="ZWO CAA">indi_asi_rotator</driver>
                <version>@ASI_VERSION_MAJOR@.@ASI_VERSION_MINOR@</version>
        </device>
</devGroup>
<devGroup group="Auxiliary">
        <device label="ZWO ST4" manufacturer="ZWO">
                <driver name="ZWO ST4">indi_asi_st4</driver>
                <version>@ASI_VERSION_MAJOR@.@ASI_VERSION_MINOR@</version>
        </device>
</devGroup>
</driversList>
