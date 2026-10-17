#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int gs_init(unsigned source, const void* module_address);
int gs_frame(unsigned role, unsigned rows, unsigned cols, const uint32_t* colors);
void gs_suspend(void);
void gs_shutdown(void);
#ifdef __cplusplus
}
#endif
