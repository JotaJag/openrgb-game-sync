#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
python tests/generate_interfaces.py
g++ -std=c++17 -fPIC -Wno-unused-parameter \
  -I. -Ibuild -I../openrgb-sequence-fix -I../openrgb-sequence-fix/build \
  -I../openrgb-sequence-fix/Effects -I../openrgb-sequence-fix/OpenRGB \
  -I../openrgb-sequence-fix/OpenRGB/qt -I../openrgb-sequence-fix/OpenRGB/RGBController \
  -I../openrgb-sequence-fix/OpenRGB/dependencies/json tests/integration.cpp \
  -Lbuild -L../openrgb-sequence-fix/build -lOpenRGBGameSyncPlugin -lOpenRGBEffectsPlugin \
  -Wl,-rpath,"$PWD/build" -Wl,-rpath,"$PWD/../openrgb-sequence-fix/build" \
  -o build/integration-test $(pkg-config --cflags --libs Qt6Widgets Qt6Network)
SDK_ROOT="$PWD/toolchain/usr/x86_64-w64-mingw32"
clang --target=x86_64-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
  -std=c11 -O2 -fno-stack-protector -mno-stack-arg-probe -nostdlib \
  -Wl,--entry,mainCRTStartup -L"$SDK_ROOT/lib" tests/native_smoke.c \
  -lkernel32 -o build/native-smoke.exe
for SDK in CUE LIGHTSYNC; do
  if [ "$SDK" = CUE ]; then SDK_NAME=cue; else SDK_NAME=lightsync; fi
  clang++ --target=x86_64-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
    -std=c++17 -O2 -fno-exceptions -fno-rtti -fno-stack-protector -mno-stack-arg-probe -nostdlib \
    -DTEST_"$SDK" -I../openrgb-cue-sdk/src/include -Wl,--entry,mainCRTStartup \
    -L"$SDK_ROOT/lib" tests/sdk_smoke.cpp -lkernel32 -o "build/$SDK_NAME-smoke.exe"
done

SDK_ROOT="$PWD/toolchain/usr/i686-w64-mingw32"
clang --target=i686-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
 -std=c11 -O2 -fno-stack-protector -mno-stack-arg-probe -nostdlib -DTEST_32 \
 -Wl,--entry,mainCRTStartup -L"$SDK_ROOT/lib" tests/native_smoke.c -lkernel32 -o build/native-smoke32.exe
for SDK in CUE LIGHTSYNC; do
 if [ "$SDK" = CUE ]; then SDK_NAME=cue; else SDK_NAME=lightsync; fi
 clang++ --target=i686-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
 -std=c++17 -O2 -fno-exceptions -fno-rtti -fno-stack-protector -mno-stack-arg-probe -nostdlib \
 -DTEST_32 -DTEST_"$SDK" -I../openrgb-cue-sdk/src/include -Wl,--entry,mainCRTStartup \
 -L"$SDK_ROOT/lib" tests/sdk_smoke.cpp -lkernel32 -o "build/$SDK_NAME-smoke32.exe"
done
SDK_ROOT="$PWD/toolchain/usr/x86_64-w64-mingw32"
clang++ --target=x86_64-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
 -std=c++17 -O2 -fno-exceptions -fno-rtti -fno-stack-protector -mno-stack-arg-probe -nostdlib \
 -Wl,--entry,mainCRTStartup -L"$SDK_ROOT/lib" tests/cue4_smoke.cpp -lkernel32 -o build/cue4-smoke.exe
clang++ --target=x86_64-w64-windows-gnu --sysroot="$SDK_ROOT" -fuse-ld=lld \
 -std=c++17 -O2 -fno-exceptions -fno-rtti -fno-stack-protector -mno-stack-arg-probe -nostdlib \
 -Wl,--entry,mainCRTStartup -L"$SDK_ROOT/lib" tests/extended_smoke.cpp -lkernel32 -o build/extended-smoke.exe
