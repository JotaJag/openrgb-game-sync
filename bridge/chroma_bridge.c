// Local Chroma SDK adapter for Wine/Proton. No game memory is inspected.
// The ABI follows the public Chroma SDK; only static/custom frames are supported.
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <objbase.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../protocol.h"

typedef struct { GUID id; GameSyncPacket packet; int used; } StoredEffect;
typedef struct { int type; DWORD connected; } DeviceInfo;
static SRWLOCK guard = SRWLOCK_INIT;
static SRWLOCK lifecycle = SRWLOCK_INIT;
static StoredEffect effects[2048];
static GameSyncPacket selected[6];
static int selected_valid[6];
static GUID session;
static char application[64];
static SOCKET connection;
static struct sockaddr_in destination;
static HANDLE worker, stop_event;
static int initialized;
static int allow_background;
__declspec(dllexport) LONG Init(void);

static int foreground(void)
{
    if (allow_background) return 1;
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

static void send_packet(GameSyncPacket* p)
{
    sendto(connection, (const char*)p, GS_HEADER_SIZE + p->count * 4, 0,
           (const struct sockaddr*)&destination, sizeof(destination));
}

static void header(GameSyncPacket* p, unsigned device, unsigned rows, unsigned cols)
{
    memset(p, 0, sizeof(*p));
    p->magic = GS_MAGIC; p->version = 1; p->kind = 1;
    p->device = (uint16_t)device; p->rows = (uint16_t)rows; p->cols = (uint16_t)cols;
    p->count = (uint16_t)(rows * cols);
    memcpy(p->session, &session, 16);
    memcpy(p->app, application, 64);
}

static DWORD WINAPI heartbeat(void* unused)
{
    (void)unused;
    while (WaitForSingleObject(stop_event, 200) == WAIT_TIMEOUT)
    {
        AcquireSRWLockShared(&guard);
        if (foreground())
        {
            for (int i = 0; i < 6; ++i) if (selected_valid[i]) send_packet(&selected[i]);
        }
        else
        {
            GameSyncPacket release;
            header(&release, 0, 0, 0); release.kind = 2;
            send_packet(&release);
        }
        ReleaseSRWLockShared(&guard);
    }
    return 0;
}

static LONG init_impl(void)
{
    AcquireSRWLockExclusive(&guard);
    if (initialized) { ReleaseSRWLockExclusive(&guard); return 0; }
    WSADATA data;
    if (WSAStartup(MAKEWORD(2,2), &data)) { ReleaseSRWLockExclusive(&guard); return 1062; }
    connection = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (connection == INVALID_SOCKET) { WSACleanup(); ReleaseSRWLockExclusive(&guard); return 1062; }
    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    char port_text[16] = {0}; GetEnvironmentVariableA("OPENRGB_GAME_SYNC_PORT", port_text, sizeof(port_text));
    unsigned long port = strtoul(port_text, NULL, 10);
    destination.sin_port = htons(port > 0 && port <= 65535 ? (u_short)port : GS_PORT);
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CoCreateGuid(&session);
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, path, sizeof(path));
    char* name = strrchr(path, '\\');
    strncpy(application, name ? name + 1 : path, sizeof(application) - 1);
    memset(effects, 0, sizeof(effects));
    memset(selected_valid, 0, sizeof(selected_valid));
    char background[8] = {0};
    GetEnvironmentVariableA("OPENRGB_GAME_SYNC_BACKGROUND", background, sizeof(background));
    allow_background = background[0] == '1';
    stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!stop_event) { closesocket(connection); WSACleanup(); ReleaseSRWLockExclusive(&guard); return 8; }
    // Keep code resident if a game unloads the DLL without calling UnInit.
    HMODULE module;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                      (LPCSTR)(uintptr_t)&Init, &module);
    initialized = 1;
    worker = CreateThread(NULL, 0, heartbeat, NULL, 0, NULL);
    if (!worker)
    {
        initialized = 0; CloseHandle(stop_event); closesocket(connection); WSACleanup();
        ReleaseSRWLockExclusive(&guard); return 8;
    }
    ReleaseSRWLockExclusive(&guard);
    return 0;
}

__declspec(dllexport) LONG Init(void)
{
    AcquireSRWLockExclusive(&lifecycle);
    LONG result = init_impl();
    ReleaseSRWLockExclusive(&lifecycle);
    return result;
}

__declspec(dllexport) LONG InitSDK(void* info) { (void)info; return Init(); }

static LONG uninit_impl(void)
{
    AcquireSRWLockExclusive(&guard);
    if (!initialized) { ReleaseSRWLockExclusive(&guard); return 0; }
    initialized = 0;
    SetEvent(stop_event);
    ReleaseSRWLockExclusive(&guard);
    WaitForSingleObject(worker, INFINITE);
    AcquireSRWLockExclusive(&guard);
    GameSyncPacket p;
    header(&p, 0, 0, 0); p.kind = 2;
    send_packet(&p);
    CloseHandle(worker); CloseHandle(stop_event); closesocket(connection); WSACleanup();
    memset(selected_valid, 0, sizeof(selected_valid));
    ReleaseSRWLockExclusive(&guard);
    return 0;
}

__declspec(dllexport) LONG UnInit(void)
{
    AcquireSRWLockExclusive(&lifecycle);
    LONG result = uninit_impl();
    ReleaseSRWLockExclusive(&lifecycle);
    return result;
}

static LONG create(unsigned device, int effect, const void* param, GUID* id)
{
    AcquireSRWLockExclusive(&guard);
    if (!initialized) { ReleaseSRWLockExclusive(&guard); return 1062; }
    GameSyncPacket p;
    unsigned rows = 1, cols = 1;
    int is_static = 0, is_custom = 0, custom_key = 0;
    switch (device)
    {
        case 1: is_static = effect == 4; is_custom = effect == 2 || effect == 8;
                custom_key = effect == 8; rows = 6; cols = 22; break;
        case 2: is_static = effect == 6; is_custom = effect == 3 || effect == 8;
                rows = effect == 8 ? 9 : 1; cols = effect == 8 ? 7 : 30; break;
        case 3: is_static = effect == 1; is_custom = effect == 4; cols = 5; break;
        case 4: is_static = effect == 4; is_custom = effect == 2; cols = 15; break;
        case 5: is_static = effect == 5; is_custom = effect == 2; rows = 4; cols = 5; break;
        case 6: is_static = effect == 2; is_custom = effect == 1; cols = 5; break;
        default: ReleaseSRWLockExclusive(&guard); return 87;
    }
    if (effect != 0 && !is_static && !is_custom) { ReleaseSRWLockExclusive(&guard); return 50; }
    if (effect != 0 && !param) { ReleaseSRWLockExclusive(&guard); return 87; }
    header(&p, device, rows, cols);
    const uint32_t* input = (const uint32_t*)param;
    if (is_static)
    {
        uint32_t color = device == 2 ? input[1] : input[0];
        for (unsigned i = 0; i < p.count; ++i) p.colors[i] = color & 0xffffff;
    }
    if (is_custom)
    {
        for (unsigned i = 0; i < p.count; ++i)
        {
            uint32_t color = input[i];
            if (custom_key && (input[p.count + i] & 0x01000000)) color = input[p.count + i];
            p.colors[i] = color & 0xffffff;
        }
    }
    if (id)
    {
        unsigned slot;
        for (slot = 0; slot < 2048 && effects[slot].used; ++slot) {}
        if (slot == 2048) { ReleaseSRWLockExclusive(&guard); return 8; }
        CoCreateGuid(id);
        effects[slot].id = *id;
        effects[slot].packet = p;
        effects[slot].used = 1;
    }
    else
    {
        selected[device - 1] = p; selected_valid[device - 1] = 1;
        if (foreground()) send_packet(&p);
    }
    ReleaseSRWLockExclusive(&guard);
    return 0;
}

#define DEVICE_API(Name, Role) __declspec(dllexport) LONG Name(int effect, void* param, GUID* id) { return create(Role, effect, param, id); }
DEVICE_API(CreateKeyboardEffect, 1)
DEVICE_API(CreateMouseEffect, 2)
DEVICE_API(CreateHeadsetEffect, 3)
DEVICE_API(CreateMousepadEffect, 4)
DEVICE_API(CreateKeypadEffect, 5)
DEVICE_API(CreateChromaLinkEffect, 6)

__declspec(dllexport) LONG SetEffect(GUID id)
{
    AcquireSRWLockExclusive(&guard);
    if (!initialized) { ReleaseSRWLockExclusive(&guard); return 1062; }
    for (unsigned i = 0; i < 2048; ++i) if (effects[i].used && !memcmp(&effects[i].id, &id, 16))
    {
        GameSyncPacket* p = &effects[i].packet;
        selected[p->device - 1] = *p; selected_valid[p->device - 1] = 1;
        if (foreground()) send_packet(p);
        ReleaseSRWLockExclusive(&guard); return 0;
    }
    ReleaseSRWLockExclusive(&guard); return 87;
}

__declspec(dllexport) LONG DeleteEffect(GUID id)
{
    AcquireSRWLockExclusive(&guard);
    for (unsigned i = 0; i < 2048; ++i) if (effects[i].used && !memcmp(&effects[i].id, &id, 16))
    { effects[i].used = 0; ReleaseSRWLockExclusive(&guard); return 0; }
    ReleaseSRWLockExclusive(&guard); return 87;
}

__declspec(dllexport) LONG CreateEffect(GUID device, int effect, void* param, GUID* id)
{ (void)device; (void)effect; (void)param; (void)id; return 50; }
__declspec(dllexport) LONG QueryDevice(GUID device, DeviceInfo* info)
{
    (void)device;
    if (!info) return 87;
    info->type = 1; info->connected = 1;
    return initialized ? 0 : 1062;
}
__declspec(dllexport) LONG RegisterEventNotification(HWND window) { (void)window; return 50; }
__declspec(dllexport) LONG UnregisterEventNotification(void) { return 0; }
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{ (void)instance; (void)reason; (void)reserved; return TRUE; }
