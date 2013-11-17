#!/bin/bash

# Run this script in the top nanopb directory to create a binary package
# for Windows users. This script is designed to run under MingW/MSYS bash
# and requires the following tools: git, make, zip, unix2dos

set -e
set -x

VERSION=`git describe --always`
DEST=dist/$VERSION

rm -rf $DEST
mkdir -p $DEST

# Export the files from newest commit
git archive HEAD | tar x -C $DEST

# Rebuild the Python .proto files
make -BC $DEST/generator/proto

# Make the nanopb generator available as a protoc plugin
cp $DEST/generator/nanopb_generator.py $DEST/generator/protoc-gen-nanopb.py

# Package the Python libraries
( cd $DEST/generator; bbfreeze nanopb_generator.py protoc-gen-nanopb.py )
mv $DEST/generator/dist $DEST/generator-bin

# The python interpreter requires MSVCR90.dll.
# FIXME: Find a way around hardcoding this path
cp /c/windows/winsxs/x86_microsoft.vc90.crt_1fc8b3b9a1e18e3b_9.0.30729.4974_none_50940634bcb759cb/MSVCR90.DLL $DEST/generator-bin/

# Remove temp file
rm $DEST/generator/protoc-gen-nanopb.py

# Package the protoc compiler
cp `which protoc.exe` $DEST/generator-bin/
cp `which MSVCR100.DLL` $DEST/generator-bin/
cp `which MSVCP100.DLL` $DEST/generator-bin/

# Convert line breaks for convenience
find $DEST -name '*.c' -o -name '*.h' -o -name '*.txt' \
    -o -name '*.proto' -o -name '*.py' -o -name '*.options' \
    -exec unix2dos '{}' \;

# Zip it all up
( cd dist; zip -r $VERSION.zip $VERSION )
