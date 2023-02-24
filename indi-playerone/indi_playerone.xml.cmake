<?xml version="1.0" encoding="UTF-8"?>
<driversList>
<devGroup group="CCDs">
        <device label="PlayerOne CCD" mdpd="true" manufacturer="PlayerOne">
                <driver name="PlayerOne CCD">indi_playerone_ccd</driver>
                <version>@PLAYERONE_VERSION_MAJOR@.@PLAYERONE_VERSION_MINOR@</version>
	    </device>
	    <device label="PlayerOne Camera 1" manufacturer="PlayerOne">
                <driver name="PlayerOne Camera">indi_playerone_single_ccd</driver>
                <version>@PLAYERONE_VERSION_MAJOR@.@PLAYERONE_VERSION_MINOR@</version>
        </device>
        <device label="PlayerOne Camera 2" manufacturer="PlayerOne">
                <driver name="PlayerOne Camera">indi_playerone_single_ccd</driver>
                <version>@PLAYERONE_VERSION_MAJOR@.@PLAYERONE_VERSION_MINOR@</version>
        </device>
        <device label="PlayerOne Camera 3" manufacturer="PlayerOne">
                <driver name="PlayerOne Camera">indi_playerone_single_ccd</driver>
                <version>@PLAYERONE_VERSION_MAJOR@.@PLAYERONE_VERSION_MINOR@</version>
        </device>
</devGroup>
<devGroup group="Filter Wheels">
        <device label="PlayerOne EFW" mdpd="true" manufacturer="PlayerOne">
                <driver name="PlayerOne EFW">indi_playerone_wheel</driver>
                <version>@PLAYERONE_VERSION_MAJOR@.@PLAYERONE_VERSION_MINOR@</version>
        </device>
</devGroup>
</driversList>
