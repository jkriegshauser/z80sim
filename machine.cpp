// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "machine.h"
//---------------------------------------------------------------------------
__fastcall Machine::Machine() {
}
//---------------------------------------------------------------------------
__fastcall Machine::~Machine() {
}
//---------------------------------------------------------------------------
void __fastcall Machine::MemoryChanged( BYTE *startaddr, BYTE *lastaddr, int size ) {
  // default action: do nothing
}
//---------------------------------------------------------------------------
void __fastcall Machine::IOPortWriteByte( int port, BYTE data ) {
  _port[port] = data;
}
//---------------------------------------------------------------------------
BYTE __fastcall Machine::IOPortReadByte( int port ) {
  return _port[port];
}
//---------------------------------------------------------------------------
const Processor * __fastcall Machine::getProcessor() {
  return NULL; //_cpu;
}
//---------------------------------------------------------------------------
const BYTE * __fastcall Machine::getRAM() {
  return _mem;
}
//---------------------------------------------------------------------------
int __fastcall Machine::getRAMSize() {
  return _memsize;
}
//---------------------------------------------------------------------------
void __fastcall Machine::reset() {
  // _cpu->reset();
  z80_stop();
  z80_reset();
}
//---------------------------------------------------------------------------
void __fastcall Machine::start() {
  // _cpu->start();
  z80_start();
}
//---------------------------------------------------------------------------
void __fastcall Machine::pause() {
  // _cpu->pause();
  z80_stop();
}
//---------------------------------------------------------------------------
#pragma package(smart_init)
 