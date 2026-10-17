#include "transport.h"
#include "keyboard_map.h"
#include <string.h>
#define API __declspec(dllexport)
static SRWLOCK guard = SRWLOCK_INIT;
static int initialized, target = 7, has_saved;
static uint32_t keys[126], saved[126], saved_key[126];
static unsigned char excluded[126], saved_valid[126];
static uint32_t ambient, saved_ambient;

static int color_value(int r, int g, int b, uint32_t* color)
{
    if (r < 0 || r > 100 || g < 0 || g > 100 || b < 0 || b > 100) return 0;
    *color = (uint32_t)((r * 255 / 100) | ((g * 255 / 100) << 8) | ((b * 255 / 100) << 16)); return 1;
}
static int find_key(const int* map, int code)
{
    for (int i = 0; i < 126; ++i) if (code && map[i] == code) return i;
    return -1;
}
API BOOL LogiLedInit(void)
{
    AcquireSRWLockExclusive(&guard);
    initialized = gs_init(3, (const void*)(uintptr_t)&LogiLedInit);
    ReleaseSRWLockExclusive(&guard); return initialized;
}
API BOOL LogiLedInitWithName(const wchar_t* name) { (void)name; return LogiLedInit(); }
API void LogiLedShutdown(void)
{
    AcquireSRWLockExclusive(&guard); initialized = 0; ReleaseSRWLockExclusive(&guard); gs_shutdown();
}
API BOOL LogiLedGetSdkVersion(int* major, int* minor, int* build)
{
    if (!major || !minor || !build) return FALSE;
    *major = 9; *minor = 0; *build = 0; return TRUE;
}
API BOOL LogiLedSetTargetDevice(int value)
{
    if (value < 0 || value > 7) return FALSE;
    AcquireSRWLockExclusive(&guard); target = value; ReleaseSRWLockExclusive(&guard); return TRUE;
}
API BOOL LogiLedSetLighting(int r, int g, int b)
{
    uint32_t color; if (!color_value(r,g,b,&color)) return FALSE;
    AcquireSRWLockExclusive(&guard);
    int ok = initialized;
    if (ok && (target & 4)) { for (int i = 0; i < 126; ++i) keys[i] = color; ok &= gs_frame(1,6,21,keys); }
    if (ok && (target & 3)) { ambient = color; ok &= gs_frame(6,1,1,&ambient); }
    ReleaseSRWLockExclusive(&guard); return ok;
}
API BOOL LogiLedSetLightingFromBitmap(const unsigned char* bitmap)
{
    if (!bitmap) return FALSE;
    AcquireSRWLockExclusive(&guard);
    int ok = initialized;
    if (ok) for (int i = 0; i < 126; ++i) if (!excluded[i] && bitmap[i*4+3])
        keys[i] = (uint32_t)(bitmap[i*4+2] | (bitmap[i*4+1] << 8) | (bitmap[i*4] << 16));
    if (ok) ok = gs_frame(1,6,21,keys);
    ReleaseSRWLockExclusive(&guard); return ok;
}
static BOOL set_key(const int* map, int code, int r, int g, int b)
{
    int index = find_key(map,code); uint32_t color;
    if (index < 0 || !color_value(r,g,b,&color)) return FALSE;
    AcquireSRWLockExclusive(&guard);
    int ok = initialized;
    if (ok) { keys[index] = color; ok = gs_frame(1,6,21,keys); }
    ReleaseSRWLockExclusive(&guard); return ok;
}
API BOOL LogiLedSetLightingForKeyWithKeyName(int key,int r,int g,int b) { return set_key(scan_keys,key,r,g,b); }
API BOOL LogiLedSetLightingForKeyWithScanCode(int key,int r,int g,int b) { return set_key(scan_keys,key,r,g,b); }
API BOOL LogiLedSetLightingForKeyWithHidCode(int key,int r,int g,int b) { return set_key(hid_keys,key,r,g,b); }
API BOOL LogiLedSetLightingForKeyWithQuartzCode(int key,int r,int g,int b) { (void)key;(void)r;(void)g;(void)b;return FALSE; }
API BOOL LogiLedSetLightingForTargetZone(int device,int zone,int r,int g,int b)
{
    uint32_t color; if (zone < 0 || zone > 4 || !color_value(r,g,b,&color)) return FALSE;
    unsigned role = device == 0 ? 1 : device == 3 ? 2 : device == 4 ? 4 : device == 8 ? 3 : device == 14 ? 6 : 0;
    if (!role) return FALSE;
    AcquireSRWLockExclusive(&guard);
    int ok = initialized;
    if (ok && role == 1) { for (int i = 0; i < 126; ++i) keys[i] = color; ok = gs_frame(1,6,21,keys); }
    else if (ok) ok = gs_frame(role,1,1,&color);
    ReleaseSRWLockExclusive(&guard); return ok;
}
API BOOL LogiLedSaveCurrentLighting(void)
{
    AcquireSRWLockExclusive(&guard);
    if (initialized) { memcpy(saved,keys,sizeof(keys)); saved_ambient = ambient; has_saved = 1; }
    ReleaseSRWLockExclusive(&guard); return initialized;
}
API BOOL LogiLedRestoreLighting(void)
{
    AcquireSRWLockExclusive(&guard);
    if (initialized) { if (has_saved) { memcpy(keys,saved,sizeof(keys)); ambient = saved_ambient; } gs_suspend(); }
    ReleaseSRWLockExclusive(&guard); return initialized;
}
API BOOL LogiLedSaveLightingForKey(int key)
{
    int i = find_key(scan_keys,key); if (i < 0) return FALSE;
    AcquireSRWLockExclusive(&guard);
    if (initialized) { saved_key[i] = keys[i]; saved_valid[i] = 1; }
    ReleaseSRWLockExclusive(&guard); return initialized;
}
API BOOL LogiLedRestoreLightingForKey(int key)
{
    int i = find_key(scan_keys,key); if (i < 0) return FALSE;
    AcquireSRWLockExclusive(&guard); int ok = initialized && saved_valid[i];
    if (ok) { keys[i] = saved_key[i]; ok = gs_frame(1,6,21,keys); }
    ReleaseSRWLockExclusive(&guard); return ok;
}
API BOOL LogiLedExcludeKeysFromBitmap(const int* list,int count)
{
    if (count < 0 || count > 126 || (count && !list)) return FALSE;
    for (int i = 0; i < count; ++i) if (find_key(scan_keys,list[i]) < 0) return FALSE;
    AcquireSRWLockExclusive(&guard); memset(excluded,0,sizeof(excluded));
    for (int i = 0; i < count; ++i) excluded[find_key(scan_keys,list[i])] = 1;
    ReleaseSRWLockExclusive(&guard); return TRUE;
}
// Built-in timed hardware animations are not emulated yet.
API BOOL LogiLedFlashLighting(int r,int g,int b,int duration,int interval) { (void)r;(void)g;(void)b;(void)duration;(void)interval;return FALSE; }
API BOOL LogiLedPulseLighting(int r,int g,int b,int duration,int interval) { return LogiLedFlashLighting(r,g,b,duration,interval); }
API BOOL LogiLedStopEffects(void) { return initialized; }
API BOOL LogiLedFlashSingleKey(int key,int r,int g,int b,int duration,int interval) { (void)key;return LogiLedFlashLighting(r,g,b,duration,interval); }
API BOOL LogiLedPulseSingleKey(int key,int sr,int sg,int sb,int er,int eg,int eb,int duration,BOOL infinite)
{ (void)key;(void)sr;(void)sg;(void)sb;(void)er;(void)eg;(void)eb;(void)duration;(void)infinite;return FALSE; }
API BOOL LogiLedStopEffectsOnKey(int key) { return find_key(scan_keys,key) >= 0 && initialized; }
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) { (void)module;(void)reason;(void)reserved;return TRUE; }
