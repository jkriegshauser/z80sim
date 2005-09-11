//---------------------------------------------------------------------------
#include "CTC.h"
#include "z80cpu.h"
//---------------------------------------------------------------------------
int _interruptbase;
unsigned char ctc_bitmap[4];
int _readtimeconstant[4];
int _running[4];
unsigned char _timeconstant[4];
unsigned short _timeconstantremaining[4];
//---------------------------------------------------------------------------
void ctc_reset() {
  int i;
  for( i = 0; i < 4; ++i ) {
    ctc_bitmap[i] = 0;
    _interruptbase = 0;
    _readtimeconstant[i] = 0;
    _running[i] = 0;
    _timeconstant[i] = 0;
    _timeconstantremaining[i] = 0;
  }
}
//---------------------------------------------------------------------------
void ctc_addcycles(int cycles) {
  int i;
  for( i = 0; i < 4; ++i ) {
    if( _running[i] && (ctc_bitmap[i] & 0x40) == 0 ) {
      if( _timeconstantremaining[i] <= cycles && (ctc_bitmap[i] & 0x80) ) {
        z80_interrupt( _interruptbase + (i << 1) );
        _timeconstantremaining[i] = _timeconstant[i] << (ctc_bitmap[i]&0x20?8:4);
      } else {
        _timeconstantremaining[i] -= cycles;
      }
    }
  }
}
//---------------------------------------------------------------------------
void ctc_trigger() {
  int i;
  for( i = 0; i < 4; ++i ) {
    if( ctc_bitmap[i] & 0x40 ) {
      // counter mode
      if( _running[i] ) {
        --_timeconstantremaining[i];
        if( _timeconstantremaining[i] == 0 && (ctc_bitmap[i] & 0x80) ) {
          z80_interrupt( _interruptbase + (i << 1) );
        }
      }
    } else {
      // timer mode
      if( !_running[i] && (ctc_bitmap[i] & 0x10) > 0 ) {
        _running[i] = 1;
      }
    }
  }
}
//---------------------------------------------------------------------------
int ctc_write( int channel, unsigned char data ) {
  channel &= (4 - 1);
  if( !_readtimeconstant[channel] && (data & 0x01) == 0 ) {
    // interrupt vector
    _interruptbase = data;
  } else {
    if( _readtimeconstant[channel] ) {
      _readtimeconstant[channel] = 0;
      _timeconstant[channel] = data;
      if( !_running[channel] ) {
        _timeconstantremaining[channel] = _timeconstant[channel] << (ctc_bitmap[channel]&0x20?8:4);
        if( (ctc_bitmap[channel] & 0x08) == 0 )
          _running[channel] = 1;
      }
    } else {
      ctc_bitmap[channel] = data;
      if( ctc_bitmap[channel] & 0x02 ) {
        _running[channel] = 0;
      }
      if( ctc_bitmap[channel] & 0x04 ) {
        _readtimeconstant[channel] = 1;
      }
    }
  }
  return 1;
}
//---------------------------------------------------------------------------
unsigned char ctc_read( int channel ) {
  channel &= (4 - 1);
  return _timeconstantremaining[channel] >> (ctc_bitmap[channel]&0x20?8:4);
}
//---------------------------------------------------------------------------

#pragma package(smart_init)
