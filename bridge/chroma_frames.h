#pragma once
#include "../protocol.h"

// Public Chroma layouts and enum values, including Keyboard v2 (8x24 + 6x22 keys).
// No Windows state here: the DLL and the host-side tests use the same decoder.
static int chroma_frame(unsigned device, int effect, const uint32_t* input, GameSyncPacket* p)
{
    unsigned rows = 1, cols = 1;
    int is_static = 0, is_custom = 0, keyed = 0, extended = 0;
    switch (device)
    {
        case 1: is_static = effect == 4; is_custom = effect == 2 || effect == 8 || effect == 9;
                keyed = effect == 8 || effect == 9; extended = effect == 9;
                rows = extended ? 8 : 6; cols = extended ? 24 : 22; break;
        case 2: is_static = effect == 6 || effect == 9; is_custom = effect == 3 || effect == 8;
                rows = effect == 8 ? 9 : 1; cols = effect == 8 ? 7 : 30; break;
        case 3: is_static = effect == 1; is_custom = effect == 4; cols = 5; break;
        case 4: is_static = effect == 4; is_custom = effect == 2 || effect == 6;
                cols = effect == 6 ? 20 : 15; break;
        case 5: is_static = effect == 5; is_custom = effect == 2; rows = 4; cols = 5; break;
        case 6: is_static = effect == 2; is_custom = effect == 1; cols = 5; break;
        default: return 87;
    }
    if (effect != 0 && !is_static && !is_custom) return 50;
    if (effect != 0 && !input) return 87;
    p->device = device; p->rows = rows; p->cols = cols; p->count = rows * cols;
    for (unsigned i = 0; i < p->count; ++i)
    {
        uint32_t color = 0;
        if (is_static) color = input[device == 2 && effect == 6 ? 1 : 0];
        if (is_custom)
        {
            color = input[i];
            if (keyed)
            {
                // The key override remains the standard 6x22 layout for CUSTOM2.
                // Scale it consistently with the normalized device-grid renderer.
                unsigned key = extended ? ((i / cols) * 6 / rows) * 22 + ((i % cols) * 22 / cols) : i;
                uint32_t overlay = input[p->count + key];
                if (overlay & 0x01000000) color = overlay;
            }
        }
        p->colors[i] = color & 0xffffff;
    }
    return 0;
}
