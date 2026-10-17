#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <objbase.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "transport.h"
#include "../protocol.h"
static SRWLOCK guard = SRWLOCK_INIT, lifecycle = SRWLOCK_INIT;
static SOCKET connection;
static HANDLE stop_event, worker;
static struct sockaddr_in destination;
static GameSyncPacket frames[6];
static int valid[6], initialized, allow_background;
static unsigned source_id;
static GUID session;
static char app[64];

static int foreground(void)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return allow_background || pid == GetCurrentProcessId();
}
static void send_packet(GameSyncPacket* p)
{
    sendto(connection, (const char*)p, GS_HEADER_SIZE + p->count * 4, 0,
           (const struct sockaddr*)&destination, sizeof(destination));
}
static void make_header(GameSyncPacket* p)
{
    memset(p, 0, sizeof(*p));
    p->magic = GS_MAGIC; p->version = 2; p->kind = 1;
    memcpy(p->session, &session, 16); memcpy(p->app, app, 64);
}
static void release_session(void)
{
    GameSyncPacket p; make_header(&p); p.kind = 2; send_packet(&p);
}
static DWORD WINAPI heartbeat(void* unused)
{
    (void)unused;
    while (WaitForSingleObject(stop_event, 200) == WAIT_TIMEOUT)
    {
        AcquireSRWLockShared(&guard);
        if (foreground()) { for (int i = 0; i < 6; ++i) if (valid[i]) send_packet(&frames[i]); }
        else release_session();
        ReleaseSRWLockShared(&guard);
    }
    return 0;
}
int gs_init(unsigned source, const void* address)
{
    AcquireSRWLockExclusive(&lifecycle); AcquireSRWLockExclusive(&guard);
    if (initialized) { ReleaseSRWLockExclusive(&guard); ReleaseSRWLockExclusive(&lifecycle); return 1; }
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa)) goto fail;
    connection = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (connection == INVALID_SOCKET) { WSACleanup(); goto fail; }
    memset(&destination, 0, sizeof(destination)); destination.sin_family = AF_INET;
    char port_text[16] = {0}; GetEnvironmentVariableA("OPENRGB_GAME_SYNC_PORT", port_text, sizeof(port_text));
    unsigned long port = strtoul(port_text, NULL, 10);
    destination.sin_port = htons(port > 0 && port <= 65535 ? (u_short)port : GS_PORT);
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (CoCreateGuid(&session) != S_OK) { closesocket(connection); WSACleanup(); goto fail; }
    source_id = source;
    char path[MAX_PATH] = {0}; GetModuleFileNameA(NULL, path, sizeof(path));
    char* name = strrchr(path, '\\'); memset(app, 0, sizeof(app));
    strncpy(app, name ? name + 1 : path, sizeof(app) - 1);
    char background[8] = {0}; GetEnvironmentVariableA("OPENRGB_GAME_SYNC_BACKGROUND", background, sizeof(background));
    allow_background = background[0] == '1'; memset(valid, 0, sizeof(valid));
    stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!stop_event) { closesocket(connection); WSACleanup(); goto fail; }
    HMODULE module;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCSTR)address, &module);
    initialized = 1; worker = CreateThread(NULL, 0, heartbeat, NULL, 0, NULL);
    if (!worker) { initialized = 0; CloseHandle(stop_event); closesocket(connection); WSACleanup(); goto fail; }
    ReleaseSRWLockExclusive(&guard); ReleaseSRWLockExclusive(&lifecycle); return 1;
fail:
    ReleaseSRWLockExclusive(&guard); ReleaseSRWLockExclusive(&lifecycle); return 0;
}
int gs_frame(unsigned role, unsigned rows, unsigned cols, const uint32_t* colors)
{
    if (!colors || role < 1 || role > 6 || !rows || !cols || rows > GS_MAX_COLORS || cols > GS_MAX_COLORS || rows * cols > GS_MAX_COLORS) return 0;
    AcquireSRWLockExclusive(&guard);
    if (!initialized) { ReleaseSRWLockExclusive(&guard); return 0; }
    GameSyncPacket* p = &frames[role - 1]; make_header(p);
    p->device = (uint16_t)((source_id << 8) | role); p->rows = rows; p->cols = cols; p->count = rows * cols;
    for (unsigned i = 0; i < p->count; ++i) p->colors[i] = colors[i] & 0xffffff;
    valid[role - 1] = 1; if (foreground()) send_packet(p);
    ReleaseSRWLockExclusive(&guard); return 1;
}
void gs_suspend(void)
{
    AcquireSRWLockExclusive(&guard);
    if (initialized) { memset(valid, 0, sizeof(valid)); release_session(); }
    ReleaseSRWLockExclusive(&guard);
}
void gs_clear(unsigned role)
{
    if (role < 1 || role > 6) return;
    AcquireSRWLockExclusive(&guard);
    if (initialized)
    {
        valid[role - 1] = 0;
        int any = 0;
        for (int i = 0; i < 6; ++i) any |= valid[i];
        if (!any) release_session();
        else
        {
            GameSyncPacket p; make_header(&p); p.kind = 3;
            p.device = (uint16_t)((source_id << 8) | role);
            send_packet(&p);
        }
    }
    ReleaseSRWLockExclusive(&guard);
}
void gs_shutdown(void)
{
    AcquireSRWLockExclusive(&lifecycle); AcquireSRWLockExclusive(&guard);
    if (!initialized) { ReleaseSRWLockExclusive(&guard); ReleaseSRWLockExclusive(&lifecycle); return; }
    initialized = 0; SetEvent(stop_event); ReleaseSRWLockExclusive(&guard);
    WaitForSingleObject(worker, INFINITE); AcquireSRWLockExclusive(&guard);
    release_session(); CloseHandle(worker); CloseHandle(stop_event); closesocket(connection); WSACleanup();
    memset(valid, 0, sizeof(valid)); ReleaseSRWLockExclusive(&guard); ReleaseSRWLockExclusive(&lifecycle);
}
