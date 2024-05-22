// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
#include "pio.h"

void pio_init( PIOSTATE *pio ) {
  pio->icw = 0;
  pio->write = 0;
  pio_reset(pio);
}

void pio_reset( PIOSTATE *pio ) {
  pio->mode = 0;
  pio->intmask = 0xFF;
  pio->icw &= ~0x80;
  pio->pending = 0;
  pio->getintmask = 0;
  pio->getiomask = 0;
  pio->commandreg = 0;
  pio->datareg = 0;
}

BYTE pio_read( PIOSTATE *pio, PIOPORT port ) {
  if( port == ppCOMMAND ) {
    return pio->commandreg;
  } else {
    switch( pio->mode ) {
      case 1: // input mode
      case 2: // bi-di mode
      case 3: // bit control mode
        pio->pending = 0;
        return pio->datareg;
    }
  }
  return 0;
}

void pio_write( PIOSTATE *pio, PIOPORT port, BYTE data ) {
  if( port == ppCOMMAND ) {
    pio->commandreg = data;
    if( pio->getintmask ) {
      pio->intmask = data;
      pio->getintmask = 0;
    } else if( pio->getiomask ) {
      pio->iomask = data;
      pio->getiomask = 0;
    } else if( data & 0x1 == 0 ) {
      pio->vector = data;
    } else if( data & 0xf == 0xf ) {
      pio->mode = data >> 6;
      if( data & 0xc0 == 0xc0 ) pio->getiomask = 1;
    } else if( data & 0xf == 0x7 ) {
      pio->icw = data;
      if( data & 0x10 ) pio->getintmask = 1;
    }
  } else {
    switch( pio->mode ) {
      case 0: // output mode
      case 2: // bi-di mode
        if( pio->write ) (pio->write)( pio, data );
        break;
      case 3: // bit control mode
        if( pio->write ) (pio->write)( pio, data );
        pio->writereg = (data & ~pio->iomask);
        break;
    }
  }
}

void pio_data( PIOSTATE *pio, BYTE data ) {
  if( pio->mode == 3 || (!pio->pending && pio->mode != 0) ) {
    if( pio->mode == 3 ) {
      data &= pio->iomask;
      data |= pio->writereg;
    }
    pio->datareg = data;
    if( pio->mode == 1 && (pio->icw & 0x80) ) z80_interrupt( pio->vector );
    if( pio->mode == 3 && (pio->icw & 0x80) ) {
      if( pio->icw & 0x20 ) data = ~data;
      if((pio->icw & 0x40) && (data & pio->iomask) == pio->iomask) z80_interrupt( pio->vector );
      else if( data & pio->iomask ) z80_interrupt( pio->vector );
    }
  }
}
