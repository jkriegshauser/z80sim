// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "CTC.h"
#include "z80cpu.h"
//---------------------------------------------------------------------------
CTC::CTC() {
  reset();
}
//---------------------------------------------------------------------------
CTC::~CTC() {
}
//---------------------------------------------------------------------------
void CTC::reset() {
  for( int i = 0; i < 4; ++i ) {
    _bitmap[i] = 0;
    _interruptbase = 0;
    _readtimeconstant[i] = false;
    _running[i] = false;
    _timeconstant[i] = 0;
    _timeconstantremaining[i] = 0;
  }
}
//---------------------------------------------------------------------------
void CTC::tick() {
  for( int i = 0; i < 4; ++i ) {
    if( _running[i] && (_bitmap[i] & 0x40) == 0 ) {
      --_timeconstantremaining[i];
      if( _timeconstantremaining[i] == 0 && (_bitmap[i] & 0x80) ) {
        z80_interrupt( _interruptbase + (i << 1) );
        _timeconstantremaining[i] = _timeconstant[i] << (_bitmap[i]&0x20?8:4);
      }
    }
  }
}
//---------------------------------------------------------------------------
void CTC::trigger() {
  for( int i = 0; i < 4; ++i ) {
    if( _bitmap[i] & 0x40 ) {
      // counter mode
      if( _running[i] ) {
        --_timeconstantremaining[i];
        if( _timeconstantremaining[i] == 0 && (_bitmap[i] & 0x80) ) {
          z80_interrupt( _interruptbase + (i << 1) );
        }
      }
    } else {
      // timer mode
      if( !_running[i] && (_bitmap[i] & 0x10) > 0 ) {
        _running[i] = true;
      }
    }
  }
}
//---------------------------------------------------------------------------
bool CTC::write( int channel, BYTE data ) {
  channel &= (4 - 1);
  if( !_readtimeconstant[channel] && (data & 0x01) == 0 ) {
    // interrupt vector
    _interruptbase = data;
  } else {
    if( _readtimeconstant[channel] ) {
      _readtimeconstant[channel] = false;
      _timeconstant[channel] = data;
      if( !_running[channel] ) {
        _timeconstantremaining[channel] = _timeconstant[channel] << (_bitmap[channel]&0x20?8:4);
        if( (_bitmap[channel] & 0x08) == 0 )
          _running[channel] = true;
      }
    } else {
      _bitmap[channel] = data;
      if( _bitmap[channel] & 0x02 ) {
        _running[channel] = false;
      }
      if( _bitmap[channel] & 0x04 ) {
        _readtimeconstant[channel] = true;
      }
    }
  }
  return true;
}
//---------------------------------------------------------------------------
BYTE CTC::read( int channel ) {
  channel &= (4 - 1);
  return _timeconstantremaining[channel] >> (_bitmap[channel]&0x20?8:4);
}
//---------------------------------------------------------------------------

#pragma package(smart_init)
