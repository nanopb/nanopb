#!/bin/bash

# Run this from the top directory of nanopb tree.
# e.g. user@localhost:~/nanopb$ tools/set_version.sh nanopb-0.1.9-dev
# It sets the version number in pb.h and generator/nanopb_generator.py.

VERSION_NUMBER_ONLY=$(echo $1 | cut -d '-' -f 2)
sed -i -e 's/nanopb_version\s*=\s*"[^"]*"/nanopb_version = "'$1'"/' generator/nanopb_generator.py
sed -i -e 's/#define\s*NANOPB_VERSION\s*.*/#define NANOPB_VERSION "'$1'"/' pb.h
sed -i -e 's/project(\s*nanopb\s*VERSION\s*[^)]*\s*LANGUAGES\s*C\s*)/project(nanopb VERSION '$VERSION_NUMBER_ONLY' LANGUAGES C)/' CMakeLists.txt
sed -i -e 's/nanopb\@[^\s]*/nanopb\@'$VERSION_NUMBER_ONLY'/' zephyr/module.yml 
sed -i -e 's/cpe:2.3:a:nanopb_project:nanopb:[^:]*/cpe:2.3:a:nanopb_project:nanopb:'$VERSION_NUMBER_ONLY'/' zephyr/module.yml 
# Update the first occurrence of "version" in MODULE.bazel, which is the nanopb
# version. Use awk instead of sed because there is no sed approach that works on
# both Linux and MacOS (https://stackoverflow.com/q/148451/24291280).
awk '/version/ && !done { gsub(/version = ".*"/, "version = \"'$VERSION_NUMBER_ONLY'\""); done=1}; 1' MODULE.bazel > temp.MODULE.bazel && mv temp.MODULE.bazel MODULE.bazel

VERSION_ONLY=$(echo $1 | sed 's/nanopb-//')
if [[ $1 != *dev ]]
then sed -i -e 's/"version":\s*"[^"]*"/"version": "'$VERSION_ONLY'"/' library.json
fi

sed -i -e 's/version =.*/version = "'$VERSION_ONLY'"/' conanfile.py
sed -i -e 's/^version =.*/version = "'$VERSION_ONLY'"/' extra/poetry/pyproject.toml
