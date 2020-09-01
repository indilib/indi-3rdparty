<?xml version="1.0" encoding="UTF-8"?>
<driversList>
    <devGroup group="Focusers">
        <device label="Armadillo focuser" manufacturer="Lunatico">
            <driver name="Armadillo focuser">indi_armadillo_focus</driver>
            <version>@LUNATICO_VERSION_MAJOR@.@LUNATICO_VERSION_MINOR@</version>
        </device>
        <device label="Platypus focuser" manufacturer="Lunatico">
            <driver name="Platypus focuser">indi_platypus_focus</driver>
            <version>@LUNATICO_VERSION_MAJOR@.@LUNATICO_VERSION_MINOR@</version>
        </device>
    </devGroup>
    <devGroup group="Auxiliary">
        <device label="Seletek Rotator" manufacturer="Lunatico">
            <driver name="Seletek Rotator">indi_seletek_rotator</driver>
            <version>@LUNATICO_VERSION_MAJOR@.@LUNATICO_VERSION_MINOR@</version>
        </device>
    </devGroup>
    <devGroup group="Domes">
        <device label="DragonFly Dome" manufacturer="Lunatico">
            <driver name="DragonFly Dome">indi_dragonfly_dome</driver>
            <version>@LUNATICO_VERSION_MAJOR@.@LUNATICO_VERSION_MINOR@</version>
        </device>
    </devGroup>
</driversList>
