# We run this Bash script to update our internal version of libyaml.

set -ue pipefail

version=0.1.7
url=http://pyyaml.org/download/libyaml/yaml-$version.tar.gz

if [ ! -d yaml ]; then
  wget $url
  tar -xf yaml-$version.tar.gz
  mv yaml-$version yaml
fi

cp yaml/LICENSE LICENSE_yaml.txt

cp yaml/include/yaml.h lib/libyaml/

# Turn the library into a drop-in library so there is only one C file and header
# file we have to worry about.
{
  echo "// Single-file version of libyaml, auto-generated from"
  echo "// $url"
  echo "#if 0"
  cat yaml/LICENSE
  echo "#endif"
  echo '#pragma GCC diagnostic ignored "-Wunused-but-set-variable"'
  echo '#pragma GCC diagnostic ignored "-Wunused-value"'
  echo '#pragma GCC diagnostic ignored "-Wunused-parameter"'
  cat yaml/win32/config.h  # only has version numbers
  cat yaml/src/yaml_private.h
  cat yaml/src/*.c
} | sed -r 's/#include .(yaml_private|config)\.h.//' > lib/libyaml/yaml.c
