// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
#ifndef pioH
#define pioH

#include "z80cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BYTE
typedef unsigned char BYTE;
#endif

struct _PIOSTATE;
typedef void (*PIOWRITEFUNC)(struct _PIOSTATE *pio, BYTE data);

typedef struct _PIOSTATE {
  BYTE icw;
  BYTE intmask;
  BYTE iomask;
  BYTE vector;
  int mode;
  PIOWRITEFUNC write;
  int getintmask;
  int getiomask;
  BYTE datareg;
  BYTE writereg;
  BYTE commandreg;
  int pending;
} PIOSTATE;

typedef enum {
  ppDATA,
  ppCOMMAND,
} PIOPORT;

void pio_init( PIOSTATE *pio );
void pio_reset( PIOSTATE *pio );
BYTE pio_read( PIOSTATE *pio, PIOPORT port );
void pio_write( PIOSTATE *pio, PIOPORT port, BYTE data );
void pio_data( PIOSTATE *pio, BYTE data );

#ifdef __cplusplus
}
#endif
#endif
