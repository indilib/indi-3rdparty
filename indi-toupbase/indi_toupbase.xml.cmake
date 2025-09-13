<?xml version="1.0" encoding="UTF-8"?>
<driversList>
  <devGroup group="CCDs">
    <device label="@MAKER@" mdpd="true" manufacturer="@MAKER@">
      <driver name="@DRIVER@">indi_@DRIVER_NAME@_ccd</driver>
      <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
    </device>
  </devGroup>
  <devGroup group="Filter Wheels">
    <device label="@MAKER@" mdpd="true" manufacturer="@MAKER@">
      <driver name="@DRIVER@">indi_@DRIVER_NAME@_wheel</driver>
      <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
    </device>
  </devGroup>
  <devGroup group="Focusers">
    <device label="@MAKER@" mdpd="true" manufacturer="@MAKER@">
      <driver name="@DRIVER@">indi_@DRIVER_NAME@_focuser</driver>
      <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
    </device>
  </devGroup>
</driversList>
