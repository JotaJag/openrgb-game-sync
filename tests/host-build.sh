#!/usr/bin/env bash
set -Eeuo pipefail
cd -- "$(dirname -- "$0")/.."
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror tests/chroma_frame_test.c -o build/chroma-frame-test
build/chroma-frame-test
c++ -std=c++17 -Wall -Wextra -Wno-misleading-indentation -Itests/winshim \
  tests/cue4_host_test.cpp -pthread -o build/cue4-host-test
build/cue4-host-test
sdk="${CUE_SDK_DIR:-../openrgb-cue-sdk}"
if [[ -f "$sdk/src/include/CUESDK.h" ]]; then
  c++ -std=c++17 -Wall -Wextra -Wno-deprecated-declarations -Itests/winshim -I"$sdk/src/include" \
    tests/cue3_host_test.cpp -pthread -o build/cue3-host-test
  build/cue3-host-test
fi
cc -std=c11 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Itests/winshim \
  tests/lightsync_host_test.c -pthread -o build/lightsync-host-test
build/lightsync-host-test
echo 'PASS: Chroma frame ABI, iCUE 3/4 adapter calls and Lightsync timing through a host Windows/transport shim.'
if command -v pkg-config >/dev/null && pkg-config --exists Qt6Core Qt6Network; then
  moc=$(pkg-config --variable=libexecdir Qt6Core)/moc
  "$moc" GameReceiver.h -o build/moc_GameReceiver_test.cpp
  c++ -std=c++17 -fPIC tests/receiver_packet_test.cpp GameReceiver.cpp \
    build/moc_GameReceiver_test.cpp $(pkg-config --cflags --libs Qt6Core Qt6Network) -o build/receiver-packet-test
  build/receiver-packet-test
  echo 'PASS: real Qt receiver validates extended frames, legacy packets and per-device release without binding a socket.'
  c++ -std=c++17 -fPIC tests/helper_process_test.cpp HelperProcess.cpp \
    $(pkg-config --cflags --libs Qt6Core) -o build/helper-process-test
  build/helper-process-test
  if [[ -f build/libOpenRGBGameSyncPlugin.so ]]; then
    c++ -std=c++17 -fPIC tests/plugin_metadata_test.cpp $(pkg-config --cflags --libs Qt6Core) -o build/plugin-metadata-test
    build/plugin-metadata-test "$PWD/build/libOpenRGBGameSyncPlugin.so" "$PWD/plugin.json"
    echo 'PASS: compiled plugin metadata matches the development version.'
    if pkg-config --exists Qt6Widgets; then
      python3 tests/generate_interfaces.py
      sdk="${OPENRGB_SOURCE_DIR:-../openrgb-sequence-fix/OpenRGB}"
      c++ -std=c++17 -fPIC -Wno-unused-parameter -Ibuild -I"$sdk" -I"$sdk/RGBController" -I"$sdk/dependencies/json" \
        tests/render_efficiency_test.cpp -Lbuild -lOpenRGBGameSyncPlugin -Wl,-rpath,"$PWD/build" \
        $(pkg-config --cflags --libs Qt6Widgets Qt6Network) -o build/render-efficiency-test
      QT_QPA_PLATFORM=offscreen build/render-efficiency-test
    fi
  fi
fi
