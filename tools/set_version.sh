#!/bin/bash

# Run this from the top directory of nanopb tree.
# e.g. user@localhost:~/nanopb$ tools/set_version.sh 1.0.0-dev
# It sets the version number in pb.h and generator/nanopb_generator.py.
#
# VERSION_NUMBER is e.g. "1.0.0-dev" (valid semver)
# WITHOUT_SUFFIX is just "1.0.0" (only numbers)
# SUFFIX is "dev" or empty
# MAJOR_VERSION is "1"

VERSION_NUMBER=$1
WITHOUT_SUFFIX=$(echo $1 | cut -d '-' -f 1)
MAJOR_VERSION=$(echo $1 | cut -d '.' -f 1)
SUFFIX=$(echo $1- | cut -d '-' -f 2)
SEMVER_VALID=$(echo $1 | sed -nE '/^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-([0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*))?(\+([0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*))?$/p')

if [[ "x$1" != "x$SEMVER_VALID" ]] || [[ "x$1" == "x" ]]
then echo "Error: version number must follow semantic version format"
exit 1
fi

# Use semantic version where supported
sed -i -e 's/nanopb_version\s*=\s*"[^"]*"/nanopb_version = "nanopb-'$VERSION_NUMBER'"/' generator/nanopb_generator.py
sed -i -e 's/#define\s*NANOPB_VERSION\s*.*/#define NANOPB_VERSION "nanopb-'$VERSION_NUMBER'"/' pb.h
sed -i -e "0,/version: '[^']*'/s/version: '[^']*'/version: '$VERSION_NUMBER'/" meson.build
sed -i -e 's/nanopb\@[^\s]*/nanopb\@'$VERSION_NUMBER'/' zephyr/module.yml
sed -i -e 's/cpe:2.3:a:nanopb_project:nanopb:[^:]*:[^:]*/cpe:2.3:a:nanopb_project:nanopb:'$WITHOUT_SUFFIX':'$SUFFIX'/' zephyr/module.yml
sed -i -e 's/"version":\s*"[^"]*"/"version": "'$VERSION_NUMBER'"/' library.json
sed -i -e 's/version =.*/version = "'$VERSION_NUMBER'"/' conanfile.py

# CMake doesn't support semver suffixes, but there is separate VERSION_STRING where we can have them.
sed -i -e 's/project(\s*nanopb \s*VERSION \s*[^)]*\s*LANGUAGES\s*C\s*)/project(nanopb VERSION '$WITHOUT_SUFFIX' LANGUAGES C)/' CMakeLists.txt
sed -i -e 's/set(\s*nanopb_VERSION_STRING \s*[^)]*)/set(nanopb_VERSION_STRING "nanopb-'$VERSION_NUMBER'")/' CMakeLists.txt
sed -i -e 's/set(\s*nanopb_VERSION \s*[^)]*)/set(nanopb_VERSION "'$VERSION_NUMBER'")/' CMakeLists.txt
sed -i -e 's/set(\s*nanopb_SOVERSION \s*[^)]*)/set(nanopb_SOVERSION '$MAJOR_VERSION')/' CMakeLists.txt

# Python versioning does not support semantic version suffixes,
# but supports .devN suffix.
if [[ $VERSION_NUMBER != $WITHOUT_SUFFIX ]]
then sed -i -e 's/^version =.*/version = "'$WITHOUT_SUFFIX.dev0'"/' extra/poetry/pyproject.toml
else sed -i -e 's/^version =.*/version = "'$WITHOUT_SUFFIX'"/' extra/poetry/pyproject.toml
fi

# Update the first occurrence of "version" in MODULE.bazel, which is the nanopb
# version. Use awk instead of sed because there is no sed approach that works on
# both Linux and MacOS (https://stackoverflow.com/q/148451/24291280).
awk '/version/ && !done { gsub(/version = ".*"/, "version = \"'$VERSION_NUMBER'\""); done=1}; 1' MODULE.bazel > temp.MODULE.bazel && mv temp.MODULE.bazel MODULE.bazel

