//---------------------------------------------------------------------------

#ifndef machineH
#define machineH
//---------------------------------------------------------------------------
#include "processor.h"
class Machine {
protected:
  BYTE *_mem;
  int _memsize;
  BYTE *_port;

public:
  __fastcall Machine();
  virtual __fastcall ~Machine();

  virtual void __fastcall MemoryChanged( BYTE *firstaddr, BYTE *lastaddr, int size );
  virtual void __fastcall IOPortWriteByte( int port, BYTE data );
  virtual BYTE __fastcall IOPortReadByte( int port );

  virtual const Processor * __fastcall getProcessor();
  virtual const BYTE * __fastcall getRAM();
  virtual int __fastcall getRAMSize();

  virtual void __fastcall reset();
  virtual void __fastcall start();
  virtual void __fastcall pause();
};
//---------------------------------------------------------------------------
#endif
