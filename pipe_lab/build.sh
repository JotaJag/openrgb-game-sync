#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
SDK_ROOT="$PWD/toolchain/usr/x86_64-w64-mingw32"
mkdir -p build/pipe-lab
clang++ --target=x86_64-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
 -std=c++17 -DCORSAIR_LIGHTING_SDK_DISABLE_DEPRECATION_WARNINGS -O1 -fno-exceptions -fno-rtti -fno-stack-protector -mno-stack-arg-probe -nostdlib \
 -I../openrgb-cue-sdk/src/include -Wl,--entry,mainCRTStartup -L"$SDK_ROOT/lib" \
 pipe_lab/original_probe.cpp -lkernel32 -lmsvcrt -o build/pipe-lab/original-probe.exe
clang --target=x86_64-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
 -std=c11 -O1 -fno-stack-protector -mno-stack-arg-probe -nostdlib \
 -Wl,--entry,mainCRTStartup -L"$SDK_ROOT/lib" pipe_lab/capture_server.c \
 -lkernel32 -lmsvcrt -o build/pipe-lab/capture-server.exe
bash pipe_service/build.sh
clang++ --target=x86_64-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
 -std=c++17 -O2 -Wno-deprecated-declarations -fno-exceptions -fno-rtti -fno-stack-protector -mno-stack-arg-probe -nostdlib \
 -I../openrgb-cue-sdk/src/include -Wl,--entry,mainCRTStartup -L"$SDK_ROOT/lib" \
 pipe_lab/latency_probe.cpp -lkernel32 -lmsvcrt -o build/pipe-lab/latency-probe.exe
