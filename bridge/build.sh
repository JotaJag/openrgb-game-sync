#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
SDK_ROOT="$PWD/toolchain/usr/x86_64-w64-mingw32"
mkdir -p build
clang --target=x86_64-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
  -std=c11 -O2 -fno-stack-protector -mno-stack-arg-probe -nostdlib -shared \
  -Wl,--entry,DllMain -L"$SDK_ROOT/lib" bridge/chroma_bridge.c \
  -lws2_32 -lkernel32 -luser32 -lole32 -lmsvcrt -o build/RzChromaSDK64.dll
clang --target=x86_64-w64-windows-gnu --sysroot="$SDK_ROOT" -std=c11 -O2 \
  -fno-stack-protector -mno-stack-arg-probe -c bridge/transport.c -o build/transport.o
clang++ --target=x86_64-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
  -std=c++17 -O2 -fno-exceptions -fno-rtti -fno-stack-protector -mno-stack-arg-probe -nostdlib -shared \
  -I../openrgb-cue-sdk/src/include -Wl,--entry,DllMain -L"$SDK_ROOT/lib" \
  bridge/cue_bridge.cpp build/transport.o -lws2_32 -lkernel32 -luser32 -lole32 -lmsvcrt -o build/CUESDK.x64_2015.dll
clang --target=x86_64-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
  -std=c11 -O2 -fno-stack-protector -mno-stack-arg-probe -nostdlib -shared \
  -Wl,--entry,DllMain -L"$SDK_ROOT/lib" bridge/lightsync_bridge.c build/transport.o \
  -lws2_32 -lkernel32 -luser32 -lole32 -lmsvcrt -o build/LogitechLed.dll
SDK_ROOT="$PWD/toolchain/usr/i686-w64-mingw32"
clang --target=i686-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
  -std=c11 -O2 -fno-stack-protector -mno-stack-arg-probe -nostdlib -shared \
  -Wl,--entry,DllMain@12 -L"$SDK_ROOT/lib" bridge/chroma_bridge.c \
  -lws2_32 -lkernel32 -luser32 -lole32 -lmsvcrt -o build/RzChromaSDK.dll
clang --target=i686-w64-windows-gnu --sysroot="$SDK_ROOT" -std=c11 -O2 \
  -fno-stack-protector -mno-stack-arg-probe -c bridge/transport.c -o build/transport32.o
clang++ --target=i686-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
  -std=c++17 -O2 -fno-exceptions -fno-rtti -fno-stack-protector -mno-stack-arg-probe -nostdlib -shared \
  -I../openrgb-cue-sdk/src/include -Wl,--entry,DllMain@12 -L"$SDK_ROOT/lib" \
  bridge/cue_bridge.cpp build/transport32.o -lws2_32 -lkernel32 -luser32 -lole32 -lmsvcrt -o build/CUESDK_2015.dll
clang --target=i686-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
  -std=c11 -O2 -fno-stack-protector -mno-stack-arg-probe -nostdlib -shared \
  -Wl,--entry,DllMain@12 -L"$SDK_ROOT/lib" bridge/lightsync_bridge.c build/transport32.o \
  -lws2_32 -lkernel32 -luser32 -lole32 -lmsvcrt -o build/LogitechLed32.dll
SDK_ROOT="$PWD/toolchain/usr/x86_64-w64-mingw32"
clang++ --target=x86_64-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
 -std=c++17 -O2 -fno-exceptions -fno-rtti -fno-stack-protector -mno-stack-arg-probe -nostdlib -shared \
 -Wl,--entry,DllMain -L"$SDK_ROOT/lib" bridge/cue4_bridge.cpp build/transport.o \
 -lws2_32 -lkernel32 -luser32 -lole32 -lmsvcrt -o build/iCUESDK.x64.dll
