// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
//---------------------------------------------------------------------------
#ifndef displayH
#define displayH
//---------------------------------------------------------------------------
#include <windows.h>
//---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
void dsp_init(HDC device);
void dsp_destroy(void);

void dsp_write(int address, BYTE data);
BYTE dsp_read(int address);
void dsp_reset(HDC device);
void dsp_draw(int force, HDC device);
#ifdef __cplusplus
}
#endif
//---------------------------------------------------------------------------
#endif
