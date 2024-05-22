// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop
#include "processor.h"
#include <stdio.h>
//---------------------------------------------------------------------------
__fastcall Processor::Processor( BYTE *mem, int memsize ) {
  _mem = mem;
  _memsize = memsize;

  MemoryChanged = NULL;
  IOPortWriteByte = NULL;
  IOPortReadByte = NULL;
  OnBreakpoint = NULL;
  OnSystemClock = OnPause = NULL;

  _thread = CreateThread( NULL, 0, &ProcessorThread, this, CREATE_SUSPENDED, &_threadID );
  //SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
  //SetThreadPriority( _thread, THREAD_PRIORITY_HIGHEST );
  ResumeThread( _thread );
}
//---------------------------------------------------------------------------
unsigned long __fastcall Processor::RegisterMemoryChangeCallback( int startaddr, int endaddr, MEMWRITEPROC callback ) {
  return ::z80_register_memwrite( startaddr, endaddr, callback );
  /*
  MEMCHANGEEVENT mce;
  mce.startaddr = startaddr;
  mce.endaddr = endaddr;
  mce.callback = callback;

  std::vector<MEMCHANGEEVENT>::iterator p = _memchangeevent.insert( _memchangeevent.end(), mce );
  return (int)p;
  */
}
//---------------------------------------------------------------------------
bool __fastcall Processor::RemoveMemoryChangeCallback( unsigned long index ) {
  return ::z80_unregister_memwrite( index );
  // if( index >= 0 )
  //   _memchangeevent.erase( (std::vector<MEMCHANGEEVENT>::iterator)index );
}
//---------------------------------------------------------------------------
BYTE __fastcall Processor::ReadPort( int port ) {
  if( IOPortReadByte )
    return IOPortReadByte(port);
  return 0x00;
}
//---------------------------------------------------------------------------
void __fastcall Processor::WritePort( int port, BYTE data ) {
  if( IOPortWriteByte )
    IOPortWriteByte( port, data );
}
//---------------------------------------------------------------------------
void __fastcall Processor::MemChanged( int startaddr, int endaddr ) {
  if( startaddr >= 0x800 && startaddr <= 0x3aaf ) {
    //pause();
  }
  std::vector<MEMCHANGEEVENT>::iterator p;
  for( p = _memchangeevent.begin(); p != _memchangeevent.end(); ++p ) {
    if( endaddr < 0 ) {
      if( startaddr >= p->startaddr && startaddr <= p->endaddr && p->callback )
        (*p->callback)( this, startaddr, startaddr );
    } else {
      // 4 cases -
      // 1. given block completely within callback block
      if( ( startaddr >= p->startaddr && endaddr <= p->endaddr ) ||
      // 2. given block starts within callback block
          ( startaddr >= p->startaddr && startaddr <= p->endaddr ) ||
      // 3. given block ends within callback block
          ( endaddr >= p->startaddr && endaddr <= p->endaddr ) ||
      // 4. the callback block is completely within given block
          ( p->startaddr >= startaddr && p->endaddr <= endaddr ) ) {
        if( p->callback )
          (*p->callback)( this, startaddr, endaddr );
      }
    }
  }
}
//---------------------------------------------------------------------------
int __fastcall Processor::RegisterBreakpoint( int addr ) {
  /*
  std::vector<int>::iterator p = _breakpoint.insert( _breakpoint.end(), addr );
  return (int)p;
  */
  return ::z80_register_breakpoint( (unsigned short)addr );
}
//---------------------------------------------------------------------------
void __fastcall Processor::RemoveBreakpoint( int index ) {
  /*
  if( index >= 0 )
    _breakpoint.erase( (std::vector<int>::iterator)index );
  */
  ::z80_unregister_breakpoint( (unsigned short)index );
}
//---------------------------------------------------------------------------
bool __fastcall Processor::pauseifbreakpoint( int pc ) {
  /*
  std::vector<int>::iterator p;
  if( !_running ) return false;
  for( p = _breakpoint.begin(); p != _breakpoint.end(); ++p )
    if( *p == pc ) {
      pause();
      if( OnBreakpoint )
        OnBreakpoint( this, pc );
      return true;
    }
  */
  return false;
}
//---------------------------------------------------------------------------
__fastcall Processor::~Processor() {
  ::z80_terminate();
  if( WaitForSingleObject( _thread, 30000 ) != WAIT_OBJECT_0 ) {
  }
  CloseHandle( _thread );
}
//---------------------------------------------------------------------------
DWORD WINAPI Processor::ProcessorThread( LPVOID p ) {
  Processor *cpu = (Processor*)p;
  ::z80_set_undef_proc( &UndefinedOpcode );
  ::z80_cpuloop( cpu->_mem );
  return 0;
}
//---------------------------------------------------------------------------
void UndefinedOpcode( unsigned char opcode ) {
  char buffer[80];
  ::sprintf( buffer, "Undefined opcode encountered: %02X", opcode );
  Application->MessageBox( buffer, "Undefined Opcode", MB_OK|MB_ICONEXCLAMATION );
}
//---------------------------------------------------------------------------
#pragma package(smart_init)


