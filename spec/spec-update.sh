#!/bin/bash
set -eE -o pipefail

function help() {
  printf "\n%s\n%s\n" "usage: $0 <old version> <new version>" "eg. $0 1.8.8 1.8.9"
  printf "\n%s\n\n" "Please execute from the spec folder as all paths are relative to there"
}

if [ "$#" -ne "2" ]; then
  help
  exit 1
fi

if [ $(basename $PWD) != "spec" ]; then
  help
  exit 1
fi

OLDSPEC="$1"
NEWSPEC="$2"

function updatespec() {
for filename in `find ../*/*.spec`
  do
  printf "updating %s to %s in %s\n" "$OLDSPEC" "$NEWSPEC" "$filename"
  sed -i "s/Version:$OLDSPEC/Version:$NEWSPEC/g" ${filename}
done
printf "\n%s\n\n" "Complete. Please verify with \"git diff\""
}

function gitcommands () {
git diff
git add ../*/*.spec
git commit -m "spec version update: from $1 to $2"
}


updatespec
#gitcommands # uncomment if desired. This will git add the changes and commit with the old and new versions
