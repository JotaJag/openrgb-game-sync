#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
SDK_ROOT="$PWD/toolchain/usr/x86_64-w64-mingw32"
clang --target=x86_64-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
 -std=c11 -O2 -fno-stack-protector -mno-stack-arg-probe -nostdlib \
 -Wl,--entry,mainCRTStartup -L"$SDK_ROOT/lib" pipe_service/host.c \
 -lws2_32 -lkernel32 -luser32 -lmsvcrt -o build/cue-pipe-host.exe
