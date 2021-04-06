<?xml version="1.0" encoding="UTF-8"?>
<driversList>
<devGroup group="CCDs">
	<device label="FLI CCD" manufacturer="Finger Lakes Instruments">
		<driver name="FLI CCD">indi_fli_ccd</driver>
                <version>@FLI_CCD_VERSION_MAJOR@.@FLI_CCD_VERSION_MINOR@</version>
	</device>
</devGroup>
<devGroup group="Focusers">
	<device label="FLI PDF" manufacturer="Finger Lakes Instruments">
                <driver name="FLI PDF">indi_fli_focus</driver>
                <version>@FLI_CCD_VERSION_MAJOR@.@FLI_CCD_VERSION_MINOR@</version>
	</device>
</devGroup>
<devGroup group="Filter Wheels">
        <device label="FLI CFW" manufacturer="Finger Lakes Instruments">
                <driver name="FLI CFW">indi_fli_wheel</driver>
                <version>@FLI_CCD_VERSION_MAJOR@.@FLI_CCD_VERSION_MINOR@</version>
        </device>
</devGroup>
</driversList>
