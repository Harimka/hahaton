#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR=${BUILD_DIR:-/tmp/team04-c-firmware-build}
CC=${CC:-clang}
CXX=${CXX:-clang++}
INC="$ROOT/Drivers/bsp/Inc"
SRC="$ROOT/Drivers/bsp/Src"
TESTS="$ROOT/tests"

mkdir -p "$BUILD_DIR"
"$CC" -std=c11 -Wall -Wextra -Werror -pedantic -I"$INC" \
    "$SRC/lsm6ds3_c.c" "$SRC/fifocapture_c.c" "$TESTS/lsm6ds3_c_test.c" \
    -o "$BUILD_DIR/lsm6ds3_c_test"
"$BUILD_DIR/lsm6ds3_c_test"

"$CC" -std=c11 -Wall -Wextra -Werror -pedantic -I"$INC" \
    "$SRC/lsm6ds3_c.c" "$SRC/fifocapture_c.c" "$TESTS/fifocapture_c_test.c" \
    -o "$BUILD_DIR/fifocapture_c_test"
"$BUILD_DIR/fifocapture_c_test"

"$CC" -std=c11 -Wall -Wextra -Werror -pedantic -I"$INC" \
    -c "$SRC/lsm6ds3_c.c" -o "$BUILD_DIR/lsm6ds3_c.o"
"$CC" -std=c11 -Wall -Wextra -Werror -pedantic -I"$INC" \
    -c "$SRC/fifocapture_c.c" -o "$BUILD_DIR/fifocapture_c.o"
"$CXX" -std=c++17 -Wall -Wextra -Werror -pedantic -I"$INC" \
    "$SRC/lsm6ds3.cpp" "$SRC/fifocapture.cpp" "$TESTS/fifocapture_test.cpp" \
    "$BUILD_DIR/lsm6ds3_c.o" "$BUILD_DIR/fifocapture_c.o" -o "$BUILD_DIR/fifocapture_test"
"$BUILD_DIR/fifocapture_test"
"$CC" -std=c11 -Wall -Wextra -Werror -pedantic -I"$INC" \
    -c "$SRC/sdcard_c.c" -o "$BUILD_DIR/sdcard_c.o"
"$CXX" -std=c++17 -Wall -Wextra -Werror -pedantic -I"$INC" \
    "$SRC/sdcard.cpp" "$TESTS/sdcard_test.cpp" "$BUILD_DIR/sdcard_c.o" \
    -o "$BUILD_DIR/sdcard_test"
"$BUILD_DIR/sdcard_test"
printf '%s\n' 'All host driver tests passed.'
