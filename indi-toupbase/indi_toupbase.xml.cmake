<?xml version="1.0" encoding="UTF-8"?>
<driversList>
  <devGroup group="CCDs">
    <device label="@LABEL@" mdpd="true" manufacturer="@MANUFACTURER@">
      <driver name="@DRIVER_NAME@">indi_@DRIVER_LIB_NAME@_ccd</driver>
      <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
    </device>
  </devGroup>
  <devGroup group="Filter Wheels">
    <device label="@LABEL@" mdpd="true" manufacturer="@MANUFACTURER@">
      <driver name="@DRIVER_NAME@">indi_@DRIVER_LIB_NAME@_wheel</driver>
      <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
    </device>
  </devGroup>
  <devGroup group="Focusers">
    <device label="@LABEL@" mdpd="true" manufacturer="@MANUFACTURER@">
      <driver name="@DRIVER_NAME@">indi_@DRIVER_LIB_NAME@_focuser</driver>
      <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
    </device>
  </devGroup>
</driversList>
