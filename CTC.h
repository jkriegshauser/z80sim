// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
//---------------------------------------------------------------------------

#ifndef CTCH
#define CTCH
//---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

void ctc_reset();
void ctc_addcycles(int cycles);
void ctc_trigger();

int ctc_write( int channel, unsigned char data );
unsigned char ctc_read( int channel );

#ifdef __cplusplus
}
#endif
//---------------------------------------------------------------------------
#endif
