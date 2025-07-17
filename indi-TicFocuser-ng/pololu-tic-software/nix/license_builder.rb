# Make sure the user did not forget to run nix/build_installer.rb.
['commit', 'nixcrpkgs_commit', 'nixpkgs_commit'].each do |var|
  if ENV.fetch(var).empty?
    raise "Required environment variable $#{var} is empty."
  end
end

$license_names = []
$license_bodies = []

def add_license(name, src_file)
  $license_names << name
  body = "<pre>"
  body << File.read(ENV.fetch('src') + "/" + src_file)
  body << "</pre>"
  $license_bodies << body
end

add_license('distribution', 'LICENSE_GPLv3.txt')
add_license('pololu-tic-software', 'LICENSE.txt')
add_license('tinyxml2', 'LICENSE_tinyxml2.txt')
add_license('libyaml', 'LICENSE_libyaml.txt')

$license_names.concat ENV.fetch('license_names').split(' ')
$license_bodies.concat ENV.fetch('licenses').split(' ').map { |f| File.read(f) }

File.open(ENV.fetch('out'), 'w') do |f|
  f.puts <<EOF
<!DOCTYPE html>
<html lang="en">
<head>
<style>
pre {
  padding-left: 5em;
  padding-bottom: 1em;
  border-bottom: 1em solid #DDD;
}
</style>
<meta charset="utf-8">
<title>License Information</title>
<body>
<p>
  The Pololu Tic software distribution is licensed under the GPLv3,
  subject to the restrictions described below.
<p>
  Source code and instructions for building this software can be found in the
  <a href="https://github.com/pololu/pololu-tic-software">pololu-tic-software</a> repository.
<p>
  This release was built from
  <a href="https://github.com/pololu/pololu-tic-software/tree/#{ENV.fetch('commit')}">
  pololu-tic-software commit #{ENV.fetch('commit')[0, 7]}</a>,
  <a href="https://github.com/pololu/nixcrpkgs/tree/#{ENV.fetch('nixcrpkgs_commit')}">
  nixcrpkgs commit #{ENV.fetch('nixcrpkgs_commit')[0, 7]}</a>, and
  <a href="https://github.com/nixos/nixpkgs/tree/#{ENV.fetch('nixpkgs_commit')}">
  nixpkgs commit #{ENV.fetch('nixpkgs_commit')[0, 7]}</a>.
<p>
  Table of contents:
</p>
EOF

  f.puts "<ul>"
  $license_names.each do |name|
    f.puts "  <li><a href=\"##{name}\">#{name}</a>"
  end
  f.puts "</ul>"

  $license_names.zip($license_bodies).each do |name, body|
    f.puts "<section id=\"#{name}\">"
    f.puts body
    f.puts "</section>\n"
  end
end
