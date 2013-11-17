#!/bin/bash

# Run this script in the top nanopb directory to create a binary package
# for Linux users.

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

# Package the Python libraries
( cd $DEST/generator; bbfreeze nanopb_generator.py )
mv $DEST/generator/dist $DEST/generator-bin

# Package the protoc compiler
cp `which protoc` $DEST/generator-bin/protoc.bin
cat > $DEST/generator-bin/protoc << EOF
#!/bin/bash
SCRIPTDIR=\$(dirname \$(readlink -f \$0))
export LD_LIBRARY_PATH=\$SCRIPTDIR
export PATH=\$SCRIPTDIR:\$PATH
exec \$SCRIPTDIR/protoc.bin "\$@"
EOF
chmod +x $DEST/generator-bin/protoc

# Make the nanopb generator available as a protoc plugin
ln -s nanopb-generator $DEST/generator-bin/protoc-gen-nanopb

