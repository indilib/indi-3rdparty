require 'fileutils'
require 'pathname'
include FileUtils

ENV['PATH'] = ENV.fetch('_PATH')

ConfigName = ENV.fetch('config_name')
OutDir = Pathname(ENV.fetch('out'))
PayloadDir = Pathname(ENV.fetch('payload'))
SrcDir = Pathname(ENV.fetch('src'))
Version = File.read(PayloadDir + 'version.txt')

StagingDir = Pathname('pololu-tic-macos-files')
OutTar = OutDir + "#{StagingDir}.tar"
AppName = 'Pololu Tic Stepper Motor Controller'
PkgFile = "pololu-tic-#{Version}-#{ConfigName}.pkg"
PkgId = 'com.pololu.tic'
AppExe = 'ticgui'
CliExe = 'ticcmd'

ReleaseDir = Pathname('release')
AppDir = ReleaseDir + "#{AppName}.app"
ContentsDir = AppDir + 'Contents'
BinDir = ContentsDir + 'MacOS'
AppResDir = ContentsDir + 'Resources'
PathDir = Pathname('path')
ResDir = Pathname('resources')

mkdir_p StagingDir
cd StagingDir
mkdir_p BinDir
mkdir_p AppResDir
mkdir_p PathDir
mkdir_p ResDir

cp_r Dir.glob(PayloadDir + 'bin' + '*'), BinDir
cp ENV.fetch('license'), ContentsDir + 'LICENSE.html'

File.open(ContentsDir + 'Info.plist', 'w') do |f|
  f.puts <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>en</string>
  <key>CFBundleExecutable</key>
  <string>#{AppExe}</string>
  <key>CFBundleIconFile</key>
  <string>app.icns</string>
  <key>CFBundleIdentifier</key>
  <string>#{PkgId}.app</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundleName</key>
  <string>#{AppName}</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleShortVersionString</key>
  <string>#{Version}</string>
  <key>CFBundleSignature</key>
  <string>????</string>
  <key>CFBundleVersion</key>
  <string>#{Version}</string>
  <key>NSHumanReadableCopyright</key>
  <string>Copyright (C) #{Time.now.year} Pololu Corporation</string>
</dict>
</plist>
EOF
end

cp SrcDir + 'images' + 'app.icns', AppResDir

File.open(PathDir + '99-pololu-tic', 'w') do |f|
  f.puts "/Applications/#{AppName}.app/Contents/MacOS"
end

File.open(ResDir + 'welcome.html', 'w') do |f|
  f.puts <<EOF
<!DOCTYPE html>
<html>
<style>
body { font-family: sans-serif; }
</style>
<title>Welcome</title>
<p>
This package installs the configuration software for the Pololu Tic stepper
motor controllers on your computer.
<p>
This package will install two programs:
<ul>
<li>Pololu Tic Control Center (ticgui)
<li>Pololu Tic Command-line Utility (ticcmd)
</ul>
EOF
end

File.open('distribution.xml', 'w') do |f|
  f.puts <<EOF
<?xml version="1.0" encoding="utf-8" standalone="no"?>
<installer-gui-script minSpecVersion="2">
  <title>#{AppName} #{Version}</title>
  <welcome file="welcome.html" />
  <pkg-ref id="app">app.pkg</pkg-ref>
  <pkg-ref id="path">path.pkg</pkg-ref>
  <options customize="allow" require-scripts="false" />
  <domain enable_anywhere="false" enable_currentUserHome="false"
    enable_localSystem="true" />
  <choices-outline>
    <line choice="app" />
    <line choice="path" />
  </choices-outline>
  <choice id="app" visible="false">
    <pkg-ref id="app" />
  </choice>
  <choice id="path" title="Add binary directory to the PATH"
    description="Adds an entry to /etc/paths.d/ so you can easily run #{CliExe} from a terminal.">
    <pkg-ref id="path" />
  </choice>
  <volume-check>
    <allowed-os-versions>
      <os-version min="10.11" />
    </allowed-os-versions>
  </volume-check>
</installer-gui-script>
EOF
end

File.open('build.sh', 'w') do |f|
  f.puts <<EOF
set -ue

pkgbuild --analyze --root zzz nocomponents.plist

pkgbuild \\
  --identifier #{PkgId}.app \\
  --version "#{Version}" \\
  --root "#{ReleaseDir}" \\
  --install-location /Applications \\
  --component-plist nocomponents.plist \\
 app.pkg

pkgbuild \\
  --identifier #{PkgId}.path \\
  --version "#{Version}" \\
  --root "#{PathDir}" \\
  --install-location /etc/paths.d \\
  --component-plist nocomponents.plist \\
  path.pkg

productbuild \\
  --identifier #{PkgId} \\
  --version "#{Version}" \\
  --resources "#{ResDir}" \\
  --distribution distribution.xml \\
  "#{PkgFile}"
EOF
end

mkdir_p OutDir
chmod_R 'u+w', '.'
chmod 'u+x', 'build.sh'
cd '..'
success = system("tar cfv #{OutTar} #{StagingDir}")
raise "tar failed: error #{$?.exitstatus}" if !success

