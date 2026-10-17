#pragma once
// Minimal host-test shim. Production DLLs are built with the actual MinGW headers.
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#define WINAPI
#define __declspec(value)
typedef uint32_t DWORD;
typedef int BOOL;
typedef void* HINSTANCE;
typedef void* LPVOID;
typedef void* HANDLE;
typedef uint64_t ULONGLONG;
static __thread DWORD shim_last_error;
static inline void SetLastError(DWORD error){shim_last_error=error;}
static inline DWORD GetLastError(void){return shim_last_error;}
extern ULONGLONG test_tick;
static inline ULONGLONG GetTickCount64(void){return test_tick;}
#define WAIT_TIMEOUT 258
#define INFINITE 0xffffffff
static inline HANDLE CreateEventA(void* security,BOOL manual,BOOL initial,const char* name){(void)security;(void)manual;(void)initial;(void)name;return (void*)1;}
static inline HANDLE CreateThread(void* security,size_t stack,DWORD (*callback)(void*),void* data,DWORD flags,DWORD* id){(void)security;(void)stack;(void)callback;(void)data;(void)flags;(void)id;return (void*)2;}
static inline DWORD WaitForSingleObject(HANDLE handle,DWORD duration){(void)handle;(void)duration;return 0;}
static inline BOOL CloseHandle(HANDLE handle){(void)handle;return 1;}
static inline BOOL SetEvent(HANDLE handle){(void)handle;return 1;}
#define TRUE 1
#define FALSE 0
typedef pthread_rwlock_t SRWLOCK;
#define SRWLOCK_INIT PTHREAD_RWLOCK_INITIALIZER
static inline void AcquireSRWLockExclusive(SRWLOCK* lock){pthread_rwlock_wrlock(lock);}
static inline void ReleaseSRWLockExclusive(SRWLOCK* lock){pthread_rwlock_unlock(lock);}
static inline void AcquireSRWLockShared(SRWLOCK* lock){pthread_rwlock_rdlock(lock);}
static inline void ReleaseSRWLockShared(SRWLOCK* lock){pthread_rwlock_unlock(lock);}
static inline void* GetProcessHeap(void){return NULL;}
static inline void* HeapAlloc(void* heap,DWORD flags,size_t size){(void)heap;(void)flags;return malloc(size);}
static inline BOOL HeapFree(void* heap,DWORD flags,void* p){(void)heap;(void)flags;free(p);return TRUE;}
#define WT_EXECUTEDEFAULT 0
static inline BOOL QueueUserWorkItem(DWORD (*callback)(void*),void* data,DWORD flags){(void)flags;callback(data);return TRUE;}
