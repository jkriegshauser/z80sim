// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
//---------------------------------------------------------------------------

#include <stdio.h>
#pragma hdrstop

#include "ICE6000.h"
#include "z80cpu.h"

//---------------------------------------------------------------------------
#define NUM_BOOT 4
#define NUM_APP  64
#define NUM_PAGES (NUM_BOOT+NUM_APP)
//---------------------------------------------------------------------------
int ice_currpage;
HANDLE ice_thread;
DWORD ice_threadID;
int ice_redraw;
TCanvas *ice_disp;
PIOSTATE ice_pioa,ice_piob;
int ice_keydata;
int ice_keymask;
BYTE ice_mem[65536];    // 64K
#ifndef PAGEFILEIO
BYTE ice_pages[NUM_PAGES][32768];      // 48 pages of 32k each
#endif
int ice_memsize;
BYTE ice_port[256];
//---------------------------------------------------------------------------
int ice6k_init( TCanvas *display ) {
  ice_currpage = -1;
  ice_disp = display;
  ice_keydata = 0;

  dsp_init(display->Handle);
  z80_set_target_cps( 14700000 );
  // z80_set_undef_proc( &UndefinedOpcode );
  z80_set_io_write_proc( &ice6k_writeio );
  z80_set_io_read_proc( &ice6k_readio );
  z80_set_clock_proc( &ice6k_systemclock );

  ctc_reset();
  pio_init( &ice_pioa );
  ice_pioa.write = &ice6k_getkey;
  pio_init( &ice_piob );

  ice_redraw = REDRAWDELAY;
#ifdef PAGEFILEIO
  if( !ice6k_load_page( 0 ) )
    return 0;
#else
  for( int i = 0; i < NUM_PAGES; ++i )
    if( !ice6k_load_page( i ) )
      return 0;
  memcpy( ice_mem, ice_pages[ice_currpage = 0], 32768 );
#endif

  // Load high mem page
  if( !ice6k_load_high_page() )
    return 0;

  ice_thread = CreateThread( NULL, 0, &ice6k_cpu, ice_mem, CREATE_SUSPENDED, &ice_threadID );
  //SetThreadPriority( _thread, THREAD_PRIORITY_HIGHEST );

  ResumeThread( ice_thread );
  return 1;
}
//---------------------------------------------------------------------------
void ice6k_destroy() {
  z80_stop();
  z80_terminate();
  WaitForSingleObject(ice_thread, 30000);
#ifdef PAGEFILEIO
  ice6k_load_page( -1 );
#else
  if( ice_currpage >= 4 )
    memcpy( ice_pages[ice_currpage], ice_mem, 32768 );
  for( int i = 4; i < NUM_PAGES; ++i )
    ice6k_save_page( i );
#endif
  ice6k_save_high_page();
  dsp_destroy();
}
//---------------------------------------------------------------------------
#ifndef PAGEFILEIO
int ice6k_save_page( int page ) {
  char c[MAX_PATH];
  HANDLE h;
  DWORD cb;
  bool ret = false;

  sprintf( c, "images\\page%d.bin", page );
  h = CreateFile( c, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
  if( h != INVALID_HANDLE_VALUE ) {
    WriteFile( h, ice_pages[page], 32768, &cb, NULL );
    CloseHandle(h);
    ret = true;
  }
  return ret;
}
#endif
//---------------------------------------------------------------------------
int ice6k_load_page( int page ) {
  char c[MAX_PATH];
  HANDLE h;
  DWORD cb;
  bool ret = true;
#ifdef PAGEFILEIO
  if( page == ice_currpage ) return true;

  if( ice_currpage >= 4 ) {
    sprintf( c, "images\\page%d.bin", ice_currpage );
    h = CreateFile( c, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if( h != INVALID_HANDLE_VALUE ) {
      WriteFile( h, ice_mem, 32768, &cb, NULL );
      CloseHandle( h );
    }
  }
#endif

  if( page >= 0 ) {
    sprintf( c, "images\\page%d.bin", page );
    h = CreateFile( c, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
#ifdef PAGEFILEIO
    if( h == INVALID_HANDLE_VALUE || !ReadFile( h, ice_mem, 32768, &cb, NULL ) || cb != 32768 ) {
      memset( ice_mem, 0x00, 32768 );
#else
    if( h == INVALID_HANDLE_VALUE || !ReadFile( h, ice_pages[page],32768, &cb, NULL ) || cb != 32768 ) {
      memset( ice_pages[page], 0x00, 32768 );
#endif
    } else {
      ret = true;
    }
#ifdef PAGEFILEIO
    ice_currpage = page;
#endif
    // _cpu->MemChanged( 0, 32767 );
    if( h != INVALID_HANDLE_VALUE )
      CloseHandle( h );
  }
  return ret;
}
//---------------------------------------------------------------------------
int ice6k_load_high_page() {
  char c[MAX_PATH];
  HANDLE h;
  DWORD cb;
  bool ret;

  strcpy( c, "images\\high.bin" );
  h = CreateFile( c, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
  if( h == INVALID_HANDLE_VALUE )
    return false;
  if( ret = ReadFile( h, ice_mem + 32768, 32768, &cb, NULL ) ) {
    // _cpu->MemChanged( 32768, 65535 );
  }
  CloseHandle( h );
  return ret;
}
//---------------------------------------------------------------------------
int ice6k_save_high_page() {
  char c[MAX_PATH];
  HANDLE h;
  DWORD cb;
  bool ret;

  strcpy( c, "images\\high.bin" );
  h = CreateFile( c, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
  if( h == INVALID_HANDLE_VALUE )
    return false;
  ret = WriteFile( h, ice_mem + 32768, 32768, &cb, NULL );
  CloseHandle( h );
  return ret;
}
//---------------------------------------------------------------------------
void ice6k_writeio( unsigned short port, unsigned char data ) {
  int i;
  switch( port & 0xFF ) {
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
      ctc_write( port & 0xFF, data );
      break;
    case 0x1c:
      pio_write( &ice_pioa, ppDATA, data );
      break;
    case 0x1d:
      pio_write( &ice_pioa, ppCOMMAND, data );
      break;
    case 0x1e:
      pio_write( &ice_piob, ppDATA, data );
      break;
    case 0x1f:
      pio_write( &ice_piob, ppCOMMAND, data );
      break;
    case 0x28:
      if( (data & 0xF) != (ice_port[0x28] & 0xF) ) {
        ice_port[0x28] = data;
        if( ~ice_keymask & ice_keydata ) {
          pio_data( &ice_pioa, ice_keymask & ~ice_keydata );
        ice_keydata = 0;
        } else
          pio_data( &ice_pioa, ice_keymask );
      }
      break;
    /*
    case 0x48:  // User Interface Register
      if( (data & 0x02) ) {
        MessageBeep( (UINT)-1 );
        // z80_stop();
      }
      break;
    */
    case 0x63:
      i = (*(DWORD*)(ice_port + 0x60)) & 0xFFFFFF;
      dsp_write( i, data );
      ++i;
      *(DWORD*)(ice_port + 0x60) = i;
      break;
    case 0xCC:
#ifdef PAGEFILEIO
      ice6k_load_page( data );
#else
      if( ice_currpage != data ) {
        if( ice_currpage >= 4 )
          memcpy( ice_pages[ice_currpage], ice_mem, 32768 );
        memcpy( ice_mem, ice_pages[ice_currpage = data], 32768 );
      }
#endif
      break;
  }
  ice_port[port & 0xff] = data;
}
//---------------------------------------------------------------------------
BYTE ice6k_readio( unsigned short port ) {
  int i;
  BYTE b;
  switch( port & 0xFF ) {
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
      return ctc_read( port & 0xFF );
    case 0x1c:
      return pio_read( &ice_pioa, ppDATA );
    case 0x1d:
      return pio_read( &ice_pioa, ppCOMMAND );
    case 0x1e:
      if( ice_port[0x1e] == 0 )
        return 0x08;
      break;
    case 0x1f:
      return pio_read( &ice_piob, ppCOMMAND );
    case 0x20:  // system services?
      return 0x02;  // board type
    case 0x63:
      i = (*(DWORD*)(ice_port + 0x60)) & 0xFFFFFF;
      b = dsp_read( i );
      ++i;
      *(DWORD*)(ice_port + 0x60) = i;
      return b;
    case 0x65:
      i = (*(DWORD*)(ice_port + 0x60)) & 0xFFFFFF;
      b = dsp_read( i );
      return b;
    case 0xC9:  // FSM id?
      return 0xA6; // bit 2 - cpu speed indicator
  }
  return ice_port[port & 0xff];
}
//---------------------------------------------------------------------------
void ice6k_getkey( PIOSTATE *pio, BYTE data ) {
  BYTE olddata = data;
  data &= 0xf;
  data |= (ice_port[0x28] << 4);
  ice_keymask = data;
}
//---------------------------------------------------------------------------
void ice6k_kbd_keypress( int keycoord ) {
  ice_keydata |= keycoord;
}
//---------------------------------------------------------------------------
void ice6k_systemclock( unsigned long cycles ) {
  ctc_addcycles(cycles);
  ice_redraw -= cycles;
  if( ice_redraw <= 0 ) {
    ice_redraw += REDRAWDELAY;
    dsp_draw(0, ice_disp->Handle);
   }
}
//---------------------------------------------------------------------------
void ice6k_reset() {
  ice6k_save_high_page();
#ifdef PAGEFILEIO
  ice6k_load_page(0);
#else
  if( ice_currpage >= 4 )
    memcpy( ice_pages[ice_currpage], ice_mem, 32768 );
  memcpy( ice_mem, ice_pages[ice_currpage = 0], 32768 );
#endif
  ice6k_load_high_page();
  memset( ice_port, 0x00, sizeof( ice_port ) );
  ctc_reset();
  dsp_reset(ice_disp->Handle);
  z80_stop();
  z80_reset();
}
//---------------------------------------------------------------------------
int ice6k_getcurrentpage() {
  return ice_currpage;
}
//---------------------------------------------------------------------------
DWORD WINAPI ice6k_cpu( LPVOID p ) {
  z80_cpuloop( (unsigned char*)p );
  return 0;
}
//---------------------------------------------------------------------------

#pragma package(smart_init)
