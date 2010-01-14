#!/bin/bash
# This scripts builds a self-contained executable file for ThreadSanitizer.
# Usage:
#   ./mk-self-contained-tsan.sh            \
#      /pin/root                           \
#      /dir/where/tsan-pin-files/reside    \
#      resulting_binary

# take Pin from here:
PIN_ROOT="$1"
# Our .so files are here:
IN_DIR="$2"
# Put the result here:
OUT="$3"
# The files/dirs to take:
IN_FILES="tsan_pin.sh bin/*ts_pin.so"

rm -rf $OUT           # remove the old one
touch  $OUT           # create the new one
chmod +x $OUT

# Create the header.
cat << 'EOF' >> $OUT
#!/bin/bash
# This is a self-extracting executable of ThreadSanitizerPin.
# This file is autogenerated by mk-self-contained-tsan-pin.sh.

# We extract the temporary files to $TSAN_EXTRACT_DIR/tsan_pin.XXXXXX
TSAN_EXTRACT_DIR=${TSAN_EXTRACT_DIR:-/tmp}
EXTRACT_DIR="$(mktemp -d $TSAN_EXTRACT_DIR/tsan_pin.XXXXXX)"

cleanup() {
  rm -rf $EXTRACT_DIR
}
# We will cleanup on exit.
trap cleanup EXIT

mkdir -p $EXTRACT_DIR
chmod +rwx $EXTRACT_DIR
EOF
# end of header

# Create the self-extractor

# Exclude unneeded binaries.
TAR_EXCLUDE="$TAR_EXCLUDE --exclude=*/doc/*       \
                          --exclude=*/include/*   \
                          --exclude=*/examples/*   \
                          "
# Create the running part.

cat << 'EOF' >> $OUT
# Extract:
echo Extracting ThreadSanitizerPin to $EXTRACT_DIR
sed '1,/^__COMPRESSED_DATA_BELOW__$/d' $0 | tar xz -C $EXTRACT_DIR

export PIN_ROOT=$EXTRACT_DIR
$EXTRACT_DIR/tsan_pin.sh "$@"
EXIT_STATUS=$?
cleanup # the trap above will handle the cleanup only if we are in bash 3.x
exit $EXIT_STATUS # make sure to return the exit code from the tool.

__COMPRESSED_DATA_BELOW__
EOF

# Dump the compressed binary at the very end of the file.
echo tar zcvh -C $IN_DIR $TAR_EXCLUDE $IN_FILES
tar zcvh -C $IN_DIR $TAR_EXCLUDE $IN_FILES -C $PIN_ROOT ./{extras,ia32,intel64,pin}  >> $OUT

echo "File $OUT successfully created"

