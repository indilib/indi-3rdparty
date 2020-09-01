<?xml version="1.0" encoding="UTF-8"?>
<driversList>
<devGroup group="Auxiliary">
        <device label="Arduino Simple Switcher" skel="simple_switcher_sk.xml" manufacturer="DIY">
                <driver name="Arduino Simple Switcher">indi_duino</driver>
                <version>@DUINO_VERSION_MAJOR@.@DUINO_VERSION_MINOR@</version>
        </device>
        <device label="Arduino Switcher" skel="switcher_sk.xml" manufacturer="DIY">
                <driver name="Arduino Switcher">indi_duino</driver>
                <version>@DUINO_VERSION_MAJOR@.@DUINO_VERSION_MINOR@</version>
        </device>
	<device label="Arduino Digital Inputs" skel="digital_inputs_sk.xml" manufacturer="DIY">
                <driver name="Arduino Demo">indi_duino</driver>
                <version>@DUINO_VERSION_MAJOR@.@DUINO_VERSION_MINOR@</version>
        </device>
	<device label="Arduino Demo" skel="various_sk.xml" manufacturer="DIY">
                <driver name="Arduino Demo">indi_duino</driver>
                <version>@DUINO_VERSION_MAJOR@.@DUINO_VERSION_MINOR@</version>
        </device>
	<device label="Arduino Stepper" skel="stepper_sk.xml" manufacturer="DIY">
                <driver name="Arduino Stepper">indi_duino</driver>
                <version>@DUINO_VERSION_MAJOR@.@DUINO_VERSION_MINOR@</version>
        </device>
	<device label="Arduino Focuser" skel="focuser_sk.xml" manufacturer="DIY">
                <driver name="Arduino Focuser">indi_duino</driver>
                <version>@DUINO_VERSION_MAJOR@.@DUINO_VERSION_MINOR@</version>
        </device>
	<device label="Arduino Cosmos" skel="cosmos_sk.xml" manufacturer="DIY">
                <driver name="Arduino Cosmos">indi_duino</driver>
                <version>@DUINO_VERSION_MAJOR@.@DUINO_VERSION_MINOR@</version>
        </device>
	<device label="Arduino Servo" skel="servo_sk.xml" manufacturer="DIY">
                <driver name="Arduino Servo">indi_duino</driver>
                <version>@DUINO_VERSION_MAJOR@.@DUINO_VERSION_MINOR@</version>
        </device>
	<device label="Arduino Roof" skel="roof_sk.xml" manufacturer="DIY">
                <driver name="Arduino Roof">indi_duino</driver>
                <version>@DUINO_VERSION_MAJOR@.@DUINO_VERSION_MINOR@</version>
        </device>
</devGroup>
<devGroup group="Weather">
    <device label="Arduino MeteoStation" skel="meteostation_sk.xml" manufacturer="DIY">
            <driver name="Arduino MeteoStation">indi_duino</driver>
            <version>@DUINO_VERSION_MAJOR@.@DUINO_VERSION_MINOR@</version>
    </device>
    <device label="Arduino MeteoStation SQM" skel="meteostationSQM_sk.xml" manufacturer="DIY">
            <driver name="Arduino MeteoStation SQM">indi_duino</driver>
            <version>@DUINO_VERSION_MAJOR@.@DUINO_VERSION_MINOR@</version>
    </device>
</devGroup>
</driversList>
