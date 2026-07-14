#!/bin/bash
set -e

if ! ls -d wasi-sdk* >/dev/null 2>&1; then
    echo "wasi-sdk not found in CWD. Fetching archive..."
    curl -sL https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-34-rc.1/wasi-sdk-34.0-rc.1-x86_64-linux.tar.gz | tar -xz
else
    echo "wasi-sdk directory already exists. Skipping download."
fi

if [ ! -d "quickjs" ]; then
    echo "quickjs directory not found in CWD. Cloning repository..."
    git clone https://github.com/quickjs-ng/quickjs
    cp qjs.c quickjs
else
    echo "quickjs repository already exists. Skipping clone."
fi


WASI_SDK_PATH="$PWD/wasi-sdk-34.0-rc.1-x86_64-linux"
CC="$WASI_SDK_PATH/bin/clang"
SYSROOT="$WASI_SDK_PATH/share/wasi-sysroot"

mkdir build

OPTIMIZATIONS="-Oz -flto"

DEFINES="-DCONFIG_VERSION=\"0.14.0\" -D_GNU_SOURCE -D_WASI_EMULATED_SIGNAL"

CFLAGS="-std=c23 --target=wasm32-wasip1 --sysroot=$SYSROOT $OPTIMIZATIONS $DEFINES -I."

echo "Compiling objects..."

$CC $CFLAGS -c ./quickjs/quickjs.c -o build/quickjs.o
$CC $CFLAGS -c ./quickjs/qjs.c -o build/qjs.o
$CC $CFLAGS -c ./quickjs/dtoa.c -o build/dtoa.o
$CC $CFLAGS -c ./quickjs/libunicode.c -o build/libunicode.o
$CC $CFLAGS -c ./quickjs/libregexp.c -o build/libregexp.o
$CC $CFLAGS -Denviron=__wasilibc_environ -c ./quickjs/quickjs-libc.c -o build/quickjs-libc.o

echo "Linking..."

$CC --target=wasm32-wasip1 --sysroot=$SYSROOT $OPTIMIZATIONS \
  -DCONFIG_WASI=y \
  -Wl,--strip-all \
  -Wl,--gc-sections \
  build/quickjs.o build/quickjs-libc.o build/qjs.o build/dtoa.o build/libunicode.o build/libregexp.o \
  -lwasi-emulated-signal \
  -lwasi-emulated-mman \
  -o qjs-wasi.wasm

# /home/user/bin/wasm-opt -Oz -s 5 --vacuum --strip-debug qjs.wasm -o qjs-wasi.wasm
rm -rf "$PWD/build"

echo "Done compiling qjs-wasi.wasm with index.js embedded."


