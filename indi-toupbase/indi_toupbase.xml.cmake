<?xml version="1.0" encoding="UTF-8"?>
<driversList>
<devGroup group="CCDs">
        <device label="Toupcam" mdpd="true" manufacturer="Toupcam">
                <driver name="Touptek">indi_toupcam_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Altair" mdpd="true" manufacturer="Altair">
                <driver name="Altair">indi_altair_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Bresser" mdpd="true" manufacturer="Bresser">
                <driver name="Bresser">indi_bressercam_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Mallincam" mdpd="true" manufacturer="Mallincam">
                <driver name="MALLINCAM">indi_mallincam_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Nncam" mdpd="true" manufacturer="Nn">
                <driver name="Nn">indi_nncam_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Ogmacam" mdpd="true" manufacturer="OGMAVision">
                <driver name="OGMAVision">indi_ogmacam_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="OmegonPro" mdpd="true" manufacturer="Omegon">
                <driver name="Astroshop">indi_omegonprocam_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="StartshootG" mdpd="true" manufacturer="Orion">
                <driver name="Orion">indi_starshootg_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Tscam" mdpd="true" manufacturer="Teleskop">
                <driver name="Teleskop">indi_tscam_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Meadecam" mdpd="true" manufacturer="Meade">
                <driver name="Meade">indi_meadecam_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Svbonycam" mdpd="true" manufacturer="SVBONY2">
                <driver name="SVBONY2">indi_svbonycam_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
</devGroup>
<devGroup group="Filter Wheels">
        <device label="Toupcam EFW" mdpd="true" manufacturer="Toupcam">
                <driver name="Touptek">indi_toupcam_wheel</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Altair EFW" mdpd="true" manufacturer="Altair">
                <driver name="Altair">indi_altair_wheel</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Bresser EFW" mdpd="true" manufacturer="Bresser">
                <driver name="Bresser">indi_bressercam_wheel</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Mallincam EFW" mdpd="true" manufacturer="Mallincam">
                <driver name="MALLINCAM">indi_mallincam_wheel</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Nncam EFW" mdpd="true" manufacturer="Nn">
                <driver name="Nn">indi_nncam_wheel</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Ogmacam EFW" mdpd="true" manufacturer="OGMAVision">
                <driver name="OGMAVision">indi_ogmacam_wheel</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="OmegonPro EFW" mdpd="true" manufacturer="Omegon">
                <driver name="Astroshop">indi_omegonprocam_wheel</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="StartshootG EFW" mdpd="true" manufacturer="Orion">
                <driver name="Orion">indi_starshootg_wheel</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Tscam EFW" mdpd="true" manufacturer="Teleskop">
                <driver name="Teleskop">indi_tscam_wheel</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Meadecam EFW" mdpd="true" manufacturer="Meade">
                <driver name="Meade">indi_meadecam_wheel</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Svbonycam EFW" mdpd="true" manufacturer="SVBONY2">
                <driver name="SVBONY2">indi_svbonycam_wheel</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
</devGroup>
<devGroup group="Focusers">
        <device label="Toupcam AAF" mdpd="true" manufacturer="Toupcam">
                <driver name="Touptek">indi_toupcam_focuser</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Altair AAF" mdpd="true" manufacturer="Altair">
                <driver name="Altair">indi_altair_focuser</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Bresser AAF" mdpd="true" manufacturer="Bresser">
                <driver name="Bresser">indi_bressercam_focuser</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Mallincam AAF" mdpd="true" manufacturer="Mallincam">
                <driver name="MALLINCAM">indi_mallincam_focuser</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Nncam AAF" mdpd="true" manufacturer="Nn">
                <driver name="Nn">indi_nncam_focuser</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Ogmacam AAF" mdpd="true" manufacturer="OGMAVision">
                <driver name="OGMAVision">indi_ogmacam_focuser</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="OmegonPro AAF" mdpd="true" manufacturer="Omegon">
                <driver name="Astroshop">indi_omegonprocam_focuser</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="StartshootG AAF" mdpd="true" manufacturer="Orion">
                <driver name="Orion">indi_starshootg_focuser</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Tscam AAF" mdpd="true" manufacturer="Teleskop">
                <driver name="Teleskop">indi_tscam_focuser</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Meadecam AAF" mdpd="true" manufacturer="Meade">
                <driver name="Meade">indi_meadecam_focuser</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Svbonycam AAF" mdpd="true" manufacturer="SVBONY">
                <driver name="SVBONY2">indi_svbonycam_focuser</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
</devGroup>
</driversList>
