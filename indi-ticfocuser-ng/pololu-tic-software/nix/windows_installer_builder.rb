require 'fileutils'
require 'pathname'
include FileUtils

OutDir = Pathname(ENV.fetch('out'))
PayloadDir = Pathname(ENV.fetch('payload'))
LibusbpDir = Pathname(ENV.fetch('libusbp'))
SrcDir = Pathname(ENV.fetch('src'))
Version = File.read(PayloadDir + 'version.txt')

mkdir OutDir
cp_r PayloadDir, OutDir + 'payload'
cp_r SrcDir + 'drivers', OutDir
cp_r SrcDir + 'images', OutDir
cp LibusbpDir + 'bin' + 'libusbp-install-helper-1.dll', OutDir
cp ENV.fetch('license'), OutDir + 'LICENSE.html'

File.open(OutDir + 'app.wixproj', 'w') do |f|
  f.write <<EOF
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProductVersion>#{Version}</ProductVersion>
    <ProjectGuid>aedc86c8-1d6a-4d86-9b7a-59eddef268aa</ProjectGuid>
    <OutputName>pololu-tic-$(ProductVersion)-win</OutputName>
    <DefineConstants>
      ProductVersion=$(ProductVersion);
      DocumentationUrl=https://www.pololu.com/docs/0J71;
    </DefineConstants>
    <Configuration Condition=" '$(Configuration)' == '' ">Debug</Configuration>
    <Platform Condition=" '$(Platform)' == '' ">x86</Platform>
    <SchemaVersion>2.0</SchemaVersion>
    <OutputType>Package</OutputType>
    <DefineSolutionProperties>false</DefineSolutionProperties>
    <WixTargetsPath Condition=" '$(WixTargetsPath)' == '' ">$(MSBuildExtensionsPath)\\Microsoft\\WiX\\v3.x\\Wix.targets</WixTargetsPath>
  </PropertyGroup>
  <PropertyGroup Condition=" '$(Configuration)|$(Platform)' == 'Debug|x86' ">
    <OutputPath>bin\\$(Configuration)\\</OutputPath>
    <IntermediateOutputPath>obj\\$(Configuration)\\</IntermediateOutputPath>
    <DefineConstants>Debug;ProductVersion=$(ProductVersion)</DefineConstants>
  </PropertyGroup>
  <PropertyGroup Condition=" '$(Configuration)|$(Platform)' == 'Release|x86' ">
    <OutputPath>bin\\$(Configuration)\\</OutputPath>
    <IntermediateOutputPath>obj\\$(Configuration)\\</IntermediateOutputPath>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="app.wxs" />
    <Compile Include="ui.wxs" />
    <EmbeddedResource Include="en-us.wxl" />
    <WixExtension Include="WixUIExtension" />
    <WixExtension Include="WixUtilExtension" />
  </ItemGroup>
  <Import Project="$(WixTargetsPath)" />
</Project>
EOF
end

File.open(OutDir + 'app.wxs', 'w') do |f|
  f.write <<EOF
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi"
     xmlns:util="http://schemas.microsoft.com/wix/UtilExtension">
  <Product Name="Pololu Tic Stepper Motor Controller"
           Version="$(var.ProductVersion)"
           Manufacturer="Pololu"
           Language="1033"
           UpgradeCode="fcb7ef44-ed3c-4778-b599-b64f6b556145"
           Id="*">

    <Package Description="Pololu Tic Stepper Motor Controller Installer"
             Manufacturer="Pololu"
             Compressed="yes"
             InstallerVersion="400"
             InstallScope="perMachine" />

    <MajorUpgrade AllowDowngrades="no"
                  DowngradeErrorMessage="A newer version of this software is already installed."
                  AllowSameVersionUpgrades="no" />

    <Media Id="1" Cabinet="cabinet.cab" EmbedCab="yes" />

    <Property Id="ARPCOMMENTS">
      Software and drivers for the Pololu Tic Stepper Motor Controller.
    </Property>
    <Property Id="ARPCONTACT">Pololu</Property>
    <Property Id="ARPURLINFOABOUT">$(var.DocumentationUrl)</Property>
    <Property Id="ARPHELPTELEPHONE">702-262-6648</Property>

    <Icon Id="app.ico" SourceFile="images/app.ico" />
    <Property Id="ARPPRODUCTICON" Value="app.ico" />

    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFilesFolder" Name="PFiles">
        <Directory Id="Pololu" Name="Pololu">
          <Directory Id="INSTALLDIR" Name="Tic">
            <Directory Id="BinDir" Name="bin" FileSource="payload/bin/">
              <Component Id="CliExe">
                <File Id="CliExe" Name="ticcmd.exe" />
              </Component>
              <Component Id="GuiExe">
                <File Id="GuiExe" Name="ticgui.exe" KeyPath="yes">
                  <Shortcut Id="GuiShortcut" Directory="PololuMenuFolder"
                            Name="Tic Control Center"
                            Icon="app.ico" Advertise="yes"
                            WorkingDirectory="BinDir" />
                </File>
              </Component>
            </Directory>
            <Component Id="ModifyPath">
              <RegistryValue KeyPath="yes" Root="HKLM"
                           Key="Software\\[Manufacturer]\\[ProductName]"
                           Name="ModifyPath" Type="integer" Value="1" />
              <Environment Id="ModifyPath" Name="PATH" Value="[INSTALLDIR]bin"
                           Permanent="no" Part="last" Action="set" System="yes" />
              <Condition>MODIFYPATH=1</Condition>
            </Component>
            <Directory Id="DriversDir" Name="drivers" FileSource="drivers/">
              <Component Id="MainInf">
                <File Id="MainInf" Name="tic.inf" />
              </Component>
              <Component Id="BootloaderInf">
                <File Id="BootloaderInf" Name="tic-bootloader.inf" />
              </Component>
              <Component Id="PololuCat">
                <File Id="PololuCat" Name="pololu.cat" />
              </Component>
            </Directory>
            <Component Id="DocumentationUrl" Guid="8dbfa4f9-a0e7-4c97-b4c3-baaaf3da337c">
              <util:InternetShortcut Id="DocumentationUrl" Type="url"
                                     Name="Tic online documentation"
                                     Target="$(var.DocumentationUrl)" />
            </Component>
            <Component Id="LicenseHtml">
              <File Id="LicenseHtml" Source="LICENSE.html" />
            </Component>
          </Directory>
        </Directory>
      </Directory>
      <Directory Id="ProgramMenuFolder">
        <Directory Id="PololuMenuFolder" Name="Pololu">
          <Component Id="PololuMenuFolder">
            <RemoveFolder Id="PololuMenuFolderRemove" On="uninstall" />
            <RegistryValue KeyPath="yes" Root="HKCU"
                           Key="Software\\[Manufacturer]\\[ProductName]"
                           Name="PololuMenuFolder" Type="integer" Value="1" />
          </Component>
          <Component Id="DocumentationUrlInMenu">
            <util:InternetShortcut Id="DocumentationUrlInMenu" Type="url"
                                   Name="Tic online documentation"
                                   Target="$(var.DocumentationUrl)" />
            <RegistryValue KeyPath="yes" Root="HKCU"
                           Key="Software\\[Manufacturer]\\[ProductName]"
                           Name="DocumentationUrlInMenu" Type="integer" Value="1" />
          </Component>
        </Directory>
      </Directory>
    </Directory>

    <Binary Id="InstallHelper"
            SourceFile="libusbp-install-helper-1.dll" />

    <Feature Id="Software"
             Title="Pololu Tic Stepper Motor Controller Software"
             AllowAdvertise="no"
             ConfigurableDirectory="INSTALLDIR">
      <ComponentRef Id="CliExe" />
      <ComponentRef Id="GuiExe" />
      <ComponentRef Id="ModifyPath" />
      <ComponentRef Id="PololuMenuFolder" />
      <ComponentRef Id="DocumentationUrl" />
      <ComponentRef Id="DocumentationUrlInMenu" />
      <ComponentRef Id="LicenseHtml" />
    </Feature>
    <Feature Id="Drivers">
      <ComponentRef Id="MainInf" />
      <ComponentRef Id="BootloaderInf" />
      <ComponentRef Id="PololuCat" />
    </Feature>

    <CustomAction Id="BroadcastSettingChange" BinaryKey="InstallHelper"
                  DllEntry="libusbp_broadcast_setting_change"
                  Impersonate="yes" Execute="deferred" Return="check" />

    <CustomAction Id="InstallInf1" Impersonate="no" Execute="deferred" Return="check"
                  BinaryKey="InstallHelper" DllEntry="libusbp_install_inf" />
    <CustomAction Id="InstallInf1.SetProperty"
                  Property="InstallInf1" Value="[INSTALLDIR]drivers\\tic.inf" />

    <CustomAction Id="InstallInf2" Impersonate="no" Execute="deferred" Return="check"
                  BinaryKey="InstallHelper" DllEntry="libusbp_install_inf" />
    <CustomAction Id="InstallInf2.SetProperty"
                  Property="InstallInf2" Value="[INSTALLDIR]drivers\\tic-bootloader.inf" />

    <InstallExecuteSequence>
      <Custom Action="InstallInf1.SetProperty" After="InstallFiles" />
      <Custom Action="InstallInf1" After="InstallInf1.SetProperty">(NOT Installed) OR REINSTALLMODE</Custom>
      <Custom Action="InstallInf2.SetProperty" After="InstallFiles" />
      <Custom Action="InstallInf2" After="InstallInf2.SetProperty">(NOT Installed) OR REINSTALLMODE</Custom>
      <Custom Action="BroadcastSettingChange" Before="InstallFinalize" />
    </InstallExecuteSequence>

    <WixVariable Id="WixUIBannerBmp"
                 Value="images\\setup_banner_wix.bmp" />
    <WixVariable Id="WixUIDialogBmp"
                 Value="images\\setup_welcome_wix.bmp" />

    <UIRef Id="ui" />
  </Product>
</Wix>
EOF
end

File.open(OutDir + 'en-us.wxl', 'w') do |f|
  f.write <<EOF
<?xml version="1.0" encoding="utf-8"?>
<WixLocalization Culture="en-us" xmlns="http://schemas.microsoft.com/wix/2006/localization">
  <String Id="WelcomeDlgDescription">The Setup Wizard will install the software and drivers for the Tic Stepper Motor Controller on your computer (version [ProductVersion]).  Click Next to continue or Cancel to exit the Setup Wizard.</String>
  <String Id="ModifyPathDlgTitle">{\\WixUI_Font_Title}Installation Options</String>
  <String Id="ModifyPathDlgDescription">Click Next to use the default options or change the options below.</String>
  <String Id="ModifyPathDlgOption">Add the bin directory to the PATH environment variable.</String>
  <String Id="ModifyPathDlgNote">This is not required but will make it easier to run ticcmd from the Command Prompt.</String>
</WixLocalization>
EOF
end

File.open(OutDir + 'ui.wxs', 'w') do |f|
  f.write <<EOF
<!-- This is a UI based on WixUI_InstallDir that provides a checkbox where
     the user can choose whether they want to modify the PATH or not. -->
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Fragment Id="ui">
    <UI Id="ui">
      <Property Id="MODIFYPATH" Value="1" Secure="yes" />
      <Property Id="WIXUI_INSTALLDIR" Value="INSTALLDIR" />

      <UIRef Id="WixUI_ErrorProgressText" />
      <UIRef Id="WixUI_Common" />

      <TextStyle Id="WixUI_Font_Normal" FaceName="Tahoma" Size="8" />
      <TextStyle Id="WixUI_Font_Bigger" FaceName="Tahoma" Size="12" />
      <TextStyle Id="WixUI_Font_Title" FaceName="Tahoma" Size="9" Bold="yes" />

      <Property Id="DefaultUIFont" Value="WixUI_Font_Normal" />
      <Property Id="WixUI_Mode" Value="InstallDir" />

      <Dialog Id="ModifyPathDlg" Width="370" Height="270" Title="!(loc.InstallDirDlg_Title)">
        <Control Id="Next" Type="PushButton" X="236" Y="243" Width="56" Height="17" Default="yes" Text="!(loc.WixUINext)" />
        <Control Id="Back" Type="PushButton" X="180" Y="243" Width="56" Height="17" Text="!(loc.WixUIBack)" />
        <Control Id="Cancel" Type="PushButton" X="304" Y="243" Width="56" Height="17" Cancel="yes" Text="!(loc.WixUICancel)">
          <Publish Event="SpawnDialog" Value="CancelDlg">1</Publish>
        </Control>

        <Control Id="Title" Type="Text" X="15" Y="6" Width="200" Height="15" Transparent="yes" NoPrefix="yes" Text="!(loc.ModifyPathDlgTitle)" />
        <Control Id="Description" Type="Text" X="25" Y="23" Width="280" Height="15" Transparent="yes" NoPrefix="yes" Text="!(loc.ModifyPathDlgDescription)" />
        <Control Id="BannerBitmap" Type="Bitmap" X="0" Y="0" Width="370" Height="44" TabSkip="no" Text="!(loc.InstallDirDlgBannerBitmap)" />
        <Control Id="BannerLine" Type="Line" X="0" Y="44" Width="370" Height="0" />
        <Control Id="BottomLine" Type="Line" X="0" Y="234" Width="370" Height="0" />

        <Control Id="ModifyPath" Type="CheckBox" X="20" Y="100" Width="300" Height="30" Property="MODIFYPATH" CheckBoxValue="1"
                 Text="!(loc.ModifyPathDlgOption)" />
        <Control Id="FolderLabel" Type="Text" X="40" Y="130" Width="290" Height="30" NoPrefix="yes" Text="!(loc.ModifyPathDlgNote)" />
      </Dialog>

      <DialogRef Id="BrowseDlg" />
      <DialogRef Id="DiskCostDlg" />
      <DialogRef Id="ErrorDlg" />
      <DialogRef Id="FatalError" />
      <DialogRef Id="FilesInUse" />
      <DialogRef Id="MsiRMFilesInUse" />
      <DialogRef Id="PrepareDlg" />
      <DialogRef Id="ProgressDlg" />
      <DialogRef Id="ResumeDlg" />
      <DialogRef Id="UserExit" />

      <Publish Dialog="BrowseDlg" Control="OK" Event="DoAction" Value="WixUIValidatePath" Order="3">1</Publish>
      <Publish Dialog="BrowseDlg" Control="OK" Event="SpawnDialog" Value="InvalidDirDlg" Order="4"><![CDATA[WIXUI_INSTALLDIR_VALID<>"1"]]></Publish>

      <Publish Dialog="ExitDialog" Control="Finish" Event="EndDialog" Value="Return" Order="999">1</Publish>

      <Publish Dialog="WelcomeDlg" Control="Next" Event="NewDialog" Value="InstallDirDlg">NOT Installed</Publish>

      <Publish Dialog="InstallDirDlg" Control="Back" Event="NewDialog" Value="WelcomeDlg">1</Publish>
      <Publish Dialog="InstallDirDlg" Control="Next" Event="SetTargetPath" Value="[WIXUI_INSTALLDIR]" Order="1">1</Publish>
      <Publish Dialog="InstallDirDlg" Control="Next" Event="DoAction" Value="WixUIValidatePath" Order="2">NOT WIXUI_DONTVALIDATEPATH</Publish>
      <Publish Dialog="InstallDirDlg" Control="Next" Event="SpawnDialog" Value="InvalidDirDlg" Order="3"><![CDATA[NOT WIXUI_DONTVALIDATEPATH AND WIXUI_INSTALLDIR_VALID<>"1"]]></Publish>
      <Publish Dialog="InstallDirDlg" Control="Next" Event="NewDialog" Value="ModifyPathDlg" Order="4">WIXUI_DONTVALIDATEPATH OR WIXUI_INSTALLDIR_VALID="1"</Publish>
      <Publish Dialog="InstallDirDlg" Control="ChangeFolder" Property="_BrowseProperty" Value="[WIXUI_INSTALLDIR]" Order="1">1</Publish>
      <Publish Dialog="InstallDirDlg" Control="ChangeFolder" Event="SpawnDialog" Value="BrowseDlg" Order="2">1</Publish>

      <Publish Dialog="ModifyPathDlg" Control="Back" Event="NewDialog" Value="InstallDirDlg">1</Publish>
      <Publish Dialog="ModifyPathDlg" Control="Next" Event="NewDialog" Value="VerifyReadyDlg">1</Publish>

      <Publish Dialog="VerifyReadyDlg" Control="Back" Event="NewDialog" Value="ModifyPathDlg" Order="1">NOT Installed</Publish>
      <Publish Dialog="VerifyReadyDlg" Control="Back" Event="NewDialog" Value="MaintenanceTypeDlg" Order="2">Installed AND NOT PATCH</Publish>

      <Publish Dialog="MaintenanceWelcomeDlg" Control="Next" Event="NewDialog" Value="MaintenanceTypeDlg">1</Publish>

      <Publish Dialog="MaintenanceTypeDlg" Control="RepairButton" Event="NewDialog" Value="VerifyReadyDlg">1</Publish>
      <Publish Dialog="MaintenanceTypeDlg" Control="RemoveButton" Event="NewDialog" Value="VerifyReadyDlg">1</Publish>
      <Publish Dialog="MaintenanceTypeDlg" Control="Back" Event="NewDialog" Value="MaintenanceWelcomeDlg">1</Publish>
      <Property Id="ARPNOMODIFY" Value="1" />
    </UI>
  </Fragment>
</Wix>
EOF
end

File.open(OutDir + 'build.sh', 'w') do |f|
  f.write <<EOF
#!/bin/bash

# This is a bash script that we use at Pololu to generate the official
# installer for Windows.  It is meant to be run in MSYS2.

set -ue

. $POLOLU_SIGN_ENV

"$MSBUILD" -t:rebuild -p:Configuration=Release -p:TreatWarningsAsErrors=True \\
  app.wixproj

cp bin/Release/en-us/*.msi .

"$SIGNTOOL" sign $SIGNFLAGS -d "Pololu Tic Stepper Motor Controller Setup" *.msi
EOF
end
