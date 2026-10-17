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
#include "lightsync_effects.h"
API BOOL LogiLedInit(void)
{
    AcquireSRWLockExclusive(&animation_lifecycle);AcquireSRWLockExclusive(&guard);
    if(initialized){ReleaseSRWLockExclusive(&guard);ReleaseSRWLockExclusive(&animation_lifecycle);return TRUE;}
    int ok=gs_init(3,(const void*)(uintptr_t)&LogiLedInit);
    if(ok){
        memset(keys,0,sizeof(keys));memset(key_effects,0,sizeof(key_effects));
        memset(excluded,0,sizeof(excluded));memset(saved_valid,0,sizeof(saved_valid));
        global_effect.mode=0;ambient=0;target=7;has_saved=0;
        animation_stop=CreateEventA(NULL,TRUE,FALSE,NULL);
        if(animation_stop){initialized=1;animation_thread=CreateThread(NULL,0,animate_lighting,NULL,0,NULL);}
        if(!animation_stop||!animation_thread){
            initialized=0;if(animation_stop)CloseHandle(animation_stop);gs_shutdown();ok=0;
        }
    }
    ReleaseSRWLockExclusive(&guard);ReleaseSRWLockExclusive(&animation_lifecycle);return ok;
}
API BOOL LogiLedInitWithName(const wchar_t* name) { (void)name; return LogiLedInit(); }
API void LogiLedShutdown(void)
{
    AcquireSRWLockExclusive(&animation_lifecycle);AcquireSRWLockExclusive(&guard);
    if(!initialized){ReleaseSRWLockExclusive(&guard);ReleaseSRWLockExclusive(&animation_lifecycle);return;}
    initialized=0;SetEvent(animation_stop);ReleaseSRWLockExclusive(&guard);
    WaitForSingleObject(animation_thread,INFINITE);
    CloseHandle(animation_thread);CloseHandle(animation_stop);animation_thread=NULL;animation_stop=NULL;
    gs_shutdown();ReleaseSRWLockExclusive(&animation_lifecycle);
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
    int cancelled=ok?cancel_effects():0;
    if (ok && (target & 4)) { for (int i = 0; i < 126; ++i) keys[i] = color; ok &= gs_frame(1,6,21,keys); }
    if (ok && (target & 3)) { ambient = color; ok &= gs_frame(6,1,1,&ambient); }
    if (initialized)emit_restored_except(cancelled,(target&4?1:0)|(target&3?2:0));
    ReleaseSRWLockExclusive(&guard); return ok;
}
API BOOL LogiLedSetLightingFromBitmap(const unsigned char* bitmap)
{
    if (!bitmap) return FALSE;
    AcquireSRWLockExclusive(&guard);
    int ok = initialized;
    if (ok) for (int i = 0; i < 126; ++i) if (!excluded[i] && bitmap[i*4+3]) {
        cancel_key(i);effect_skip[i]=1;
        keys[i]=(uint32_t)(bitmap[i*4+2] | (bitmap[i*4+1] << 8) | (bitmap[i*4] << 16));
    }
    if (ok) ok = gs_frame(1,6,21,keys);
    ReleaseSRWLockExclusive(&guard); return ok;
}
static BOOL set_key(const int* map, int code, int r, int g, int b)
{
    int index = find_key(map,code); uint32_t color;
    if (index < 0 || !color_value(r,g,b,&color)) return FALSE;
    AcquireSRWLockExclusive(&guard);
    int ok = initialized;
    if (ok) { cancel_key(index);effect_skip[index]=1;keys[index] = color; ok = gs_frame(1,6,21,keys); }
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
    int cancelled=ok?cancel_effects():0;
    if (ok && role == 1) { for (int i = 0; i < 126; ++i) keys[i] = color; ok = gs_frame(1,6,21,keys); }
    else if (ok) ok = gs_frame(role,1,1,&color);
    if(initialized)emit_restored_except(cancelled,role==1?1:role==6?2:0);
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
    if (initialized) { cancel_effects();if (has_saved) { memcpy(keys,saved,sizeof(keys)); ambient = saved_ambient; } gs_suspend(); }
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
    if (ok) { cancel_key(i);effect_skip[i]=1;keys[i] = saved_key[i]; ok = gs_frame(1,6,21,keys); }
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
API BOOL LogiLedFlashLighting(int r,int g,int b,int duration,int interval)
{return start_global_effect(1,r,g,b,duration,interval);}
API BOOL LogiLedPulseLighting(int r,int g,int b,int duration,int interval)
{return start_global_effect(2,r,g,b,duration,interval);}
API BOOL LogiLedStopEffects(void)
{
    AcquireSRWLockExclusive(&guard);int ok=initialized;
    if(ok){
        int keyboard_changed=global_effect.mode&&(effect_target&4),ambient_changed=global_effect.mode&&(effect_target&3);
        for(int i=0;i<126;++i)keyboard_changed|=key_effects[i].mode!=0;
        cancel_effects();
        if(keyboard_changed)gs_frame(1,6,21,keys);
        if(ambient_changed)gs_frame(6,1,1,&ambient);
    }
    ReleaseSRWLockExclusive(&guard);return ok;
}
API BOOL LogiLedFlashSingleKey(int key,int r,int g,int b,int duration,int interval)
{
    uint32_t color;int index=find_key(scan_keys,key);
    if(index<0||!color_value(r,g,b,&color)||duration<0||interval<=0)return FALSE;
    AcquireSRWLockExclusive(&guard);int ok=initialized;
    if(ok){cancel_key(index);effect_skip[index]=1;uint32_t base=keys[index];
        key_effects[index]=(TimedEffect){1,GetTickCount64(),(unsigned)duration,(unsigned)interval,base,color,base};
        animation_tick(GetTickCount64());}
    ReleaseSRWLockExclusive(&guard);return ok;
}
API BOOL LogiLedPulseSingleKey(int key,int sr,int sg,int sb,int er,int eg,int eb,int duration,BOOL infinite)
{
    uint32_t from,to;int index=find_key(scan_keys,key);
    if(index<0||!color_value(sr,sg,sb,&from)||!color_value(er,eg,eb,&to)||duration<=0)return FALSE;
    AcquireSRWLockExclusive(&guard);int ok=initialized;
    if(ok){cancel_key(index);effect_skip[index]=1;
        key_effects[index]=(TimedEffect){2,GetTickCount64(),infinite?0:(unsigned)duration,(unsigned)duration,from,to,keys[index]};
        animation_tick(GetTickCount64());}
    ReleaseSRWLockExclusive(&guard);return ok;
}
API BOOL LogiLedStopEffectsOnKey(int key)
{
    int index=find_key(scan_keys,key);if(index<0)return FALSE;
    AcquireSRWLockExclusive(&guard);int ok=initialized;
    if(ok){int changed=key_effects[index].mode||(global_effect.mode&&(effect_target&4)&&!effect_skip[index]);
        cancel_key(index);if(global_effect.mode&&(effect_target&4)&&!effect_skip[index])keys[index]=effect_base[index];
        effect_skip[index]=1;if(changed)gs_frame(1,6,21,keys);}
    ReleaseSRWLockExclusive(&guard);return ok;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) { (void)module;(void)reason;(void)reserved;return TRUE; }
