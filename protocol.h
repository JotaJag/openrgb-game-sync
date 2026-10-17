#pragma once
#include <stdint.h>

#define GS_PORT 6743
#define GS_MAGIC 0x3153474fU
#define GS_MAX_COLORS 132
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t version, kind, device, rows, cols, count;
    uint8_t session[16];
    char app[64];
    uint32_t colors[GS_MAX_COLORS];
} GameSyncPacket;
#pragma pack(pop)
#define GS_HEADER_SIZE 96
// kind 1: selected effect (also a heartbeat); kind 2: release session.
// device: 1 keyboard, 2 mouse, 3 headset, 4 mousepad, 5 keypad, 6 ChromaLink.
// Color format is 0x00BBGGRR, matching COLORREF and OpenRGB.
// Version 2 uses the upper byte of device for the source:
// 1 Chroma, 2 iCUE SDK 3, 3 Logitech Lightsync. Version 1 means Chroma.
