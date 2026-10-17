#define CUESDK_EXPORTS
#include "CUESDK.h"
#include "transport.h"
#include "keyboard_map.h"
#include <string.h>

static SRWLOCK guard = SRWLOCK_INIT;
static int initialized;
static uint32_t keyboard[126], mouse[4];
static bool dirty[2];
static CorsairLedPosition positions[126], mousePositions[4];
static CorsairLedPositions layouts[2];
static CorsairDeviceInfo devices[2];
static bool result(bool ok, CorsairError fail = CE_InvalidArguments) { SetLastError(ok ? CE_Success : fail); return ok; }
struct CallbackData { void(*callback)(void*,bool,CorsairError); void* context; bool success; CorsairError error; };
static DWORD WINAPI deliver_callback(void* value)
{
    auto* data = static_cast<CallbackData*>(value);
    data->callback(data->context,data->success,data->error);
    HeapFree(GetProcessHeap(),0,data); return 0;
}
static bool complete_async(bool ok, void(*callback)(void*,bool,CorsairError), void* context)
{
    CorsairError saved = static_cast<CorsairError>(GetLastError());
    if (!callback) return ok;
    auto* data = static_cast<CallbackData*>(HeapAlloc(GetProcessHeap(),0,sizeof(CallbackData)));
    if (!data) return result(false,CE_ServerNotFound);
    *data = {callback,context,ok,saved};
    if (!QueueUserWorkItem(deliver_callback,data,WT_EXECUTEDEFAULT))
    { HeapFree(GetProcessHeap(),0,data); return result(false,CE_ServerNotFound); }
    SetLastError(saved); return ok;
}
static int index_for(int device, CorsairLedId id)
{
    if (device == 1) return id >= CLM_1 && id <= CLM_4 ? int(id) - CLM_1 : -1;
    for (int i = 0; i < 126; ++i) if (cue_keys[i] && cue_keys[i] == id) return i;
    return -1;
}
static bool buffer(int device, int size, CorsairLedColor* colors)
{
    if (!initialized) return result(false, CE_ProtocolHandshakeMissing);
    if (device < 0 || device > 1 || size < 0 || size > 4096 || (size && !colors)) return result(false);
    for (int i = 0; i < size; ++i)
    {
        if (colors[i].ledId <= CLI_Invalid || colors[i].ledId > CLI_Last ||
            colors[i].r < 0 || colors[i].r > 255 || colors[i].g < 0 || colors[i].g > 255 ||
            colors[i].b < 0 || colors[i].b > 255) return result(false);
        for(int j=0;j<i;++j)if(colors[j].ledId==colors[i].ledId)return result(false);
    }
    for (int i = 0; i < size; ++i)
    {
        auto c = colors[i];
        int index=index_for(device,c.ledId);
        if(index>=0){
            (device == 0 ? keyboard : mouse)[index] = uint32_t(c.r | (c.g << 8) | (c.b << 16));
            dirty[device]=true;
        }
    }
    return result(true);
}
static bool flush()
{
    if (!initialized) return result(false, CE_ProtocolHandshakeMissing);
    bool ok = true;
    if (dirty[0]) { ok &= gs_frame(1, 6, 21, keyboard) != 0; dirty[0] = false; }
    if (dirty[1]) { ok &= gs_frame(2, 1, 4, mouse) != 0; dirty[1] = false; }
    return result(ok, CE_ServerNotFound);
}
extern "C" {
CorsairProtocolDetails CorsairPerformProtocolHandshake()
{
    AcquireSRWLockExclusive(&guard);
    if (!initialized && gs_init(2, reinterpret_cast<void*>(&CorsairPerformProtocolHandshake)))
    {
        initialized = 1;
        memset(devices, 0, sizeof(devices));
        devices[0].type = CDT_Keyboard; devices[0].model = "OpenRGB Virtual Keyboard";
        devices[0].physicalLayout = CPL_US; devices[0].logicalLayout = CLL_US_Int; devices[0].capsMask = CDC_Lighting;
        strcpy(devices[0].deviceId, "openrgb-keyboard");
        int n = 0;
        for (int i = 0; i < 126; ++i) if (cue_keys[i])
            positions[n++] = {static_cast<CorsairLedId>(cue_keys[i]), double(i / 21 * 20), double(i % 21 * 20), 18., 18.};
        layouts[0] = {n, positions}; devices[0].ledsCount = n;
        devices[1].type = CDT_Mouse; devices[1].model = "OpenRGB Virtual Mouse";
        devices[1].physicalLayout = CPL_Zones4; devices[1].capsMask = CDC_Lighting; devices[1].ledsCount = 4;
        strcpy(devices[1].deviceId, "openrgb-mouse");
        for (int i = 0; i < 4; ++i) mousePositions[i] = {static_cast<CorsairLedId>(CLM_1 + i), double(i * 20), 0., 18., 18.};
        layouts[1] = {4, mousePositions};
    }
    CorsairProtocolDetails details = {"3.0.464", initialized ? "3.0.464" : nullptr, 16, initialized ? 16 : 0, false};
    bool ok = initialized;
    ReleaseSRWLockExclusive(&guard); result(ok,CE_ServerNotFound); return details;
}
CorsairError CorsairGetLastError() { return static_cast<CorsairError>(GetLastError()); }
int CorsairGetDeviceCount() { return initialized ? 2 : 0; }
CorsairDeviceInfo* CorsairGetDeviceInfo(int index)
{
    AcquireSRWLockShared(&guard); bool ok = initialized && index >= 0 && index < 2;
    auto* device = ok ? &devices[index] : nullptr;
    ReleaseSRWLockShared(&guard); result(ok); return device;
}
CorsairLedPositions* CorsairGetLedPositionsByDeviceIndex(int index)
{
    AcquireSRWLockShared(&guard); bool ok = initialized && index >= 0 && index < 2;
    auto* layout = ok ? &layouts[index] : nullptr;
    ReleaseSRWLockShared(&guard); result(ok); return layout;
}
CorsairLedPositions* CorsairGetLedPositions() { return CorsairGetLedPositionsByDeviceIndex(0); }
bool CorsairSetLedsColorsBufferByDeviceIndex(int index, int size, CorsairLedColor* colors)
{
    AcquireSRWLockExclusive(&guard); bool ok = buffer(index, size, colors); ReleaseSRWLockExclusive(&guard); return ok;
}
bool CorsairSetLedsColorsFlushBuffer()
{
    AcquireSRWLockExclusive(&guard); bool ok = flush(); ReleaseSRWLockExclusive(&guard); return ok;
}
bool CorsairSetLedsColors(int size, CorsairLedColor* colors)
{
    AcquireSRWLockExclusive(&guard); bool ok = buffer(0, size, colors) && flush(); ReleaseSRWLockExclusive(&guard); return ok;
}
bool CorsairSetLedsColorsAsync(int size, CorsairLedColor* colors, void(*callback)(void*, bool, CorsairError), void* context)
{
    bool ok = CorsairSetLedsColors(size, colors);
    return complete_async(ok,callback,context);
}
bool CorsairSetLedsColorsFlushBufferAsync(void(*callback)(void*, bool, CorsairError), void* context)
{
    bool ok = CorsairSetLedsColorsFlushBuffer();
    return complete_async(ok,callback,context);
}
bool CorsairGetLedsColorsByDeviceIndex(int device, int size, CorsairLedColor* colors)
{
    AcquireSRWLockExclusive(&guard);
    bool ok = initialized && device >= 0 && device < 2 && size >= 0 && size <= 4096 && (!size || colors);
    if (ok) for (int i = 0; i < size; ++i) {
        if(colors[i].ledId<=CLI_Invalid||colors[i].ledId>CLI_Last)ok=false;
        for(int j=0;j<i;++j)if(colors[j].ledId==colors[i].ledId)ok=false;
    }
    if (ok) for (int i = 0; i < size; ++i)
    {
        int index=index_for(device,colors[i].ledId);
        if(index<0)continue;
        auto c = (device == 0 ? keyboard : mouse)[index];
        colors[i].r = c & 255; colors[i].g = (c >> 8) & 255; colors[i].b = (c >> 16) & 255;
    }
    result(ok); ReleaseSRWLockExclusive(&guard); return ok;
}
bool CorsairGetLedsColors(int size, CorsairLedColor* colors) { return CorsairGetLedsColorsByDeviceIndex(0, size, colors); }
CorsairLedId CorsairGetLedIdForKeyName(char key)
{
    int hid = key >= 'a' && key <= 'z' ? key - 'a' + 4 : key >= 'A' && key <= 'Z' ? key - 'A' + 4 :
              key >= '1' && key <= '9' ? key - '1' + 30 : key == '0' ? 39 : key == ' ' ? 44 : 0;
    for (int i = 0; i < 126; ++i) if (hid && hid_keys[i] == hid) return static_cast<CorsairLedId>(cue_keys[i]);
    return CLI_Invalid;
}
bool CorsairRequestControl(CorsairAccessMode mode) { return result(initialized && mode == CAM_ExclusiveLightingControl); }
bool CorsairReleaseControl(CorsairAccessMode mode)
{
    if (!initialized || mode != CAM_ExclusiveLightingControl) return result(false);
    AcquireSRWLockExclusive(&guard); dirty[0] = dirty[1] = false; gs_suspend(); ReleaseSRWLockExclusive(&guard);
    return result(true);
}
bool CorsairSetLayerPriority(int priority) { return result(initialized && priority >= 0 && priority <= 255); }
bool CorsairRegisterKeypressCallback(void (*)(void*, CorsairKeyId, bool), void*) { return result(false, CE_IncompatibleProtocol); }
bool CorsairGetBoolPropertyValue(int, CorsairDevicePropertyId, bool*) { return result(false); }
bool CorsairGetInt32PropertyValue(int, CorsairDevicePropertyId, int*) { return result(false); }
bool CorsairSubscribeForEvents(CorsairEventHandler, void*) { return result(false, CE_IncompatibleProtocol); }
bool CorsairUnsubscribeFromEvents() { return result(true); }
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
}
