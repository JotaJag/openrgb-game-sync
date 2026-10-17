# OpenRGB Game Sync (local prototype)

Version 0.5 adds a standalone protocol-10 service for the unchanged Corsair
3.0.234 DLL shipped with the installed Diablo IV build. The previous DLL
replacement approach caused a user-reported game crash and remains blocked
for Diablo/Battle.net. Automatic library preparation is currently paused.


Native OpenRGB 1.0 / Qt6 plugin with an on/off switch and separate Chroma, iCUE
and Lightsync switches. Receives supported
game lighting through a Windows SDK adapter in Wine/Proton, takes priority over
OpenRGB Effects, and returns control when disabled, unloaded, unfocused, or when
the game's heartbeat stops. Applications that never send lighting do not take
control. No screen capture or game-memory inspection is used.

## Current scope

- Chroma SDK static, off, custom keyboard (6×22), custom-key, mouse (30 LEDs or
  9×7), headset, mousepad, keypad and ChromaLink frames.
- Keyboard mapping uses the OpenRGB device matrix, scaled to the Chroma grid;
  exact key alignment varies by keyboard layout.
- Other devices use their category's colors where available; internal lighting
  follows ChromaLink, with a keyboard color average as fallback. Black pixels
  are excluded from the average unless every pixel is black.
- First valid sending session owns priority until focus loss, release or timeout.
  The bridge normally emits frames only while its game has foreground focus.
- Saved effect assignments and configuration are preserved. Sequence pauses its
  progression during takeover. Static devices have their colors/mode restored.
- Receiver binds only to IPv4 localhost UDP port 6743; it is not remotely exposed.
- iCUE SDK 3 (the family used by Diablo IV's `CUESDK.x64_2015.dll`): LED discovery,
  virtual keyboard/mouse, per-LED colors, buffering/flushing, asynchronous
  completion, color readback and releasing control. Virtual devices are mapped
  to physical OpenRGB devices; iCUE itself is not installed. iCUE SDK 4 is provided by a separate
  64-bit adapter: virtual keyboard discovery, session callback, color buffering,
  asynchronous completion, alpha blending, readback, priority and release.
  Hardware properties and key interception report unsupported; virtual mouse
  support is currently limited to SDK 3.
- Logitech LED/Lightsync: global and per-key colors, scan/HID key codes, 6×21 BGRA
  keyboard bitmaps, exclusions, target categories/zones, save/restore and
  shutdown. Registry discovery is prepared in the selected Wine prefix. Timed
  built-in pulse/flash commands and Quartz key codes currently return unsupported.
- All adapters are currently **64-bit**. Other game prefixes need preparation.
- Chroma hardware animation
  commands such as wave/reactive, generic `CreateEffect`, and the newer 8×24
  keyboard custom2 API return unsupported. They are not silently treated as a
  working integration. Compatibility with each real game still needs validation.

## Components

`GameSyncPlugin.*`: OpenRGB plugin, UI and rendering.
`GameReceiver.*` and `protocol.h`: bounded frame receiver with session validation.
`PriorityLease.h`: cooperative, expiring shared-memory lock used by both plugins.
`bridge/chroma_bridge.c`: local replacement Chroma SDK adapter for 64-bit games.
`bridge/cue_bridge.cpp`, `bridge/lightsync_bridge.c`, `bridge/transport.c`:
native iCUE 3 and Lightsync adapters with shared foreground-aware transport.
The adapter is pinned while loaded, has a bounded effect cache and releases on
UnInit; a 1.5-second receiver timeout handles processes exiting without UnInit.

The companion Effects source in `../openrgb-sequence-fix` contains both the
Sequence flash correction and the cooperative rendering gate. Replacing that
plugin with an upstream build removes coordination and can cause conflicting
writers. Keep these two local plugin builds together.

## Build

Requires Qt6 Core/Widgets/Network, qmake6, a C++17 compiler, clang/lld, and MinGW
headers/libraries under `toolchain/usr/x86_64-w64-mingw32`. The OpenRGB API 5
headers are taken from `../openrgb-sequence-fix/OpenRGB` (OpenRGB release 1.0).
The iCUE ABI headers are referenced directly from `../openrgb-cue-sdk`, checked
out at the official Corsair `v3.0.464` tag; they are not bundled here.

```sh
mkdir -p build
cd build
qmake6 ../GameSync.pro
make -j4
cd ..
bash bridge/build.sh
bash tests/build.sh
QT_QPA_PLATFORM=offscreen build/integration-test
python tests/wine_smoke.py
python tests/sdk_wine_smoke.py
```

The integration test runs the actual compiled Sequence and Game Sync renderers
against simulated controllers. It verifies ordinary operation, invalid packets,
exclusive takeover, disabling, static restoration, timeout and unloading. The
Wine test executes the real Windows DLL in a separate temporary Wine prefix,
checks effect creation/selection, actual network frames, heartbeat, release, and
foreground suppression. It never launches or modifies a game.

## Installation and use

Copy `build/libOpenRGBGameSyncPlugin.so` into `~/.config/OpenRGB/plugins/` and
install the matching patched Effects plugin. Restart OpenRGB. The **Game Sync**
tab has the enable switch and reports the current game or waiting state.

Version 0.4 adds Chroma REST and iCUE SDK 4 lighting; version 0.3 introduced automatic discovery/preparation for Linux launchers: Steam/Proton,
Lutris (including Battle.net, Epic, Ubisoft, EA and GOG prefixes), Heroic, Bottles,
and the standard Wine prefix. Both 32- and 64-bit adapters are built.

Run `python3 auto_prepare.py --plan` for a read-only inventory and
`python3 auto_prepare.py --enable` to enable preparation. OpenRGB subsequently
checks registered libraries every minute while Game Sync is enabled. Proton's
user_settings callback prepares the prefix after creation and before the game.
Lutris/Heroic use their pre-launch task. Existing custom tasks are preserved and
reported rather than overwritten. Bottles uses periodic discovery; a new bottle
must exist before it can be prepared. Unknown custom Wine locations require the
launcher to register them or a pre-launch invocation with WINEPREFIX set.

Only recognized Chroma, iCUE SDK 3/4 and Logitech LED DLLs are replaced, and
only if the replacement provides every exported function from the original.
This checks symbol availability, not complete behavior of every API.
The Proton preparation worker has a 6-second total deadline and cleans up its
process group on timeout; the shared lock times out instead of blocking launch.
Lutris/Heroic hooks also have a 6-second budget and always return success.
Preparation errors are displayed in the Game Sync tab (details on hover). Every
replacement is journaled, with original contents under
`~/.local/share/openrgb-game-sync/automation/originals/`. Read-only runners,
active prefixes and unsupported libraries are reported in `status.json` there.
System-installed read-only Proton runners cannot receive the prelaunch callback;
existing prefixes still receive periodic preparation while stopped. Updated
SDKs inside a running shared launcher are deferred until that launcher stops.
This is not a guarantee for every game or SDK revision: iCUE hardware properties, Quartz,
hardware-generated effects and some animation APIs remain unsupported.

Uncheck Game Sync to stop priority and automatic preparation, or disable the
plugin in OpenRGB. Installed adapters remain on disk but cannot change lighting
without an enabled receiver. To undo file changes, stop the affected launcher,
then restore the corresponding original from the automation journal. Earlier
manual installation backups remain under `~/.local/share/openrgb-backups/`.

Tests cover simulated launcher inventories, preserving user hooks, SDK updates,
backups, idempotency, disabled preparation, and the Proton before-game callback.
Actual SDK DLLs are exercised under isolated Wine in both architectures; the
native plugin/Effects priority test uses fake devices. Real game/launcher
end-to-end behavior still requires user validation. No game memory is inspected.

`OPENRGB_GAME_SYNC_BACKGROUND=1` bypasses focus suppression for isolated headless
tests; it is not set for the user's games.
`OPENRGB_GAME_SYNC_PORT` and `OPENRGB_GAME_SYNC_LEASE_KEY` isolate test sockets
and priority leases from the real OpenRGB instance; they are not set for games.

## Chroma REST

OpenRGB owns a local-only helper at `http://127.0.0.1:54235/razer/chromasdk`.
Supports session creation/deletion, 15-second session expiry, heartbeat,
static/off/custom/custom-key effects, effect creation/selection/deletion and
keyboard/mouse/headset/mousepad/keypad/ChromaLink categories. It uses the same
priority receiver as the DLLs. The helper starts/stops with the plugin switch.
Desktop HTTP clients are supported; HTTPS, browser cross-origin clients,
desktop matrices and hardware animation APIs are not implemented. REST has no
process-focus information: a client must stop/release when appropriate; a live
heartbeat retains its last frame. Bind failures are shown in the plugin.

Protocol references: [Razer REST](https://assets.razerzone.com/dev_portal/REST/html/index.html)
and [Corsair SDK](https://github.com/CorsairOfficial/cue-sdk).
SDK 4 headers in `bridge/sdk4` come from official commit
`890bc3e27e651f19ff73eff87bfa6497bb93dac5` (v4.0.84).

This project integrates supported RGB APIs; it cannot provide native game events
for software that doesn't emit them. It is developed and installed for this
Linux Wine/Proton setup, not a tested Windows/macOS plugin release.

## License

New local plugin/adapter code: GPL-2.0-or-later. The build uses the public OpenRGB
plugin interface and Qt. Source protocol/ABI reference material was inspected
from OpenRGB and the public Artemis Chroma adapter; no Artemis implementation is
bundled into this adapter.

## Original Corsair DLL service (0.5)

`pipe_service/host.c` runs as a separate Wine process. It supplies the original
SDK's fixed bootstrap pipes and per-client input/output/callback pipes. The
private protocol observed with the original DLL uses length-prefixed UTF-8 JSON.
`pipe_service/backend.py` implements protocol 10 handshake, one virtual keyboard,
LED positions, synchronous/asynchronous color submission, buffered flushing,
color readback, ASCII key-name lookup, priority/release and disconnect handling.
Unknown functions return explicit SDK errors. This is a protocol-10 implementation,
not a complete emulator of every iCUE version or hardware function.

The host and Python backend communicate over a random localhost TCP port with a
per-run authentication token. Production uses UMU `runinprefix` with the same
Proton Experimental as Battle.net. No game DLL, registry, game executable or
in-process hook is installed. The plugin controls the service's lifecycle through
`pipe_services` entries in its local configuration. The library-replacement
block remains in force and independent of this service.

Windows focus events filter outgoing frames. Closing a client releases priority;
disabling the plugin/source stops the service. Keyboard frames are rendered on
keyboards using their matrix; other Direct-mode devices receive an average
color. This makes internal PC lighting participate without claiming that each
LED reproduces the original Windows iCUE hardware layout.

Tests: `python3 tests/pipe_service_test.py`,
`bash pipe_lab/build.sh`, `python3 pipe_lab/probe.py` (creates isolated fixture),
`python3 pipe_lab/service_test.py --umu`, and the native integration executable.
The original-DLL test checks negotiation, exact device type, LED positions,
sync/async colors, buffering, readback, callback and release against the actual
unmodified vendor binary. Native integration checks the path through the
plugin-managed service to fake controllers and service shutdown. Real Diablo
behavior remains a separate user validation.
