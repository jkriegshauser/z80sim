// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
//---------------------------------------------------------------------------

#ifndef processorH
#define processorH
#include <vcl.h>
#include <vector>
#include "z80cpu.h"

class Processor;
//---------------------------------------------------------------------------
void UndefinedOpcode( unsigned char );
//---------------------------------------------------------------------------
typedef void __fastcall (__closure *MemoryChangedEvent)( BYTE *startaddr, BYTE *lastaddr, int size );
typedef void __fastcall (__closure *IOPortWriteByteEvent)( int port, BYTE data );
typedef BYTE __fastcall (__closure *IOPortReadByteEvent)( int port );
typedef void __fastcall (__closure *BreakpointEvent)( Processor *processor, int addr );
typedef void __fastcall (__closure *ProcessorEvent)( Processor *processor );

typedef void (*MEMCHANGECALLBACK)( Processor *cpu, int startaddr, int endaddr );
typedef struct {
  int startaddr;
  int endaddr;
  MEMCHANGECALLBACK callback;
} MEMCHANGEEVENT;
//---------------------------------------------------------------------------
class Processor {
protected:
  BYTE *_mem;
  int _memsize;
  // Each CPU runs in the context of its own thread
  HANDLE _thread;
  DWORD _threadID;
  std::vector<MEMCHANGEEVENT> _memchangeevent;
  std::vector<int> _breakpoint;

  static DWORD WINAPI ProcessorThread( LPVOID p );
  virtual BYTE __fastcall ReadPort( int port );
  virtual void __fastcall WritePort( int port, BYTE data );

public:
  __fastcall Processor( BYTE *mem, int memsize );
  virtual __fastcall ~Processor();

  virtual void __fastcall reset() = 0;
  virtual void __fastcall start() = 0;
  virtual void __fastcall pause() = 0;
  virtual void __fastcall step() = 0;

  virtual void __fastcall terminate() = 0;

  virtual void __fastcall MemChanged( int startaddr, int endaddr = -1 );   // single byte or range

  virtual bool __fastcall pauseifbreakpoint( int pc );

  virtual bool __fastcall getRunning() = 0;
  virtual BYTE * __fastcall getmem() { return _mem; }
  virtual int __fastcall getmemsize() { return _memsize; }
  virtual long __fastcall getcycles() = 0;

  MemoryChangedEvent MemoryChanged;
  IOPortWriteByteEvent IOPortWriteByte;
  IOPortReadByteEvent IOPortReadByte;
  BreakpointEvent OnBreakpoint;
  ProcessorEvent OnSystemClock;
  ProcessorEvent OnPause;

  virtual unsigned long __fastcall RegisterMemoryChangeCallback( int startaddr, int endaddr, MEMWRITEPROC callback );
  virtual bool __fastcall RemoveMemoryChangeCallback( unsigned long );
  virtual int __fastcall RegisterBreakpoint( int addr );
  virtual void __fastcall RemoveBreakpoint( int index );
};
//---------------------------------------------------------------------------
#endif






