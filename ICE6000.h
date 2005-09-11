//---------------------------------------------------------------------------

#ifndef ICE6000H
#define ICE6000H

#include <vcl.h>
#include "main.h"
#include "display.h"
#include "ctc.h"
#include "pio.h"
//---------------------------------------------------------------------------
#define CPUSPEED 14700000
#define REDRAWDELAY  (CPUSPEED/30)
// #define PAGEFILEIO
//---------------------------------------------------------------------------
DWORD WINAPI ice6k_cpu( LPVOID mem );

int ice6k_init( TCanvas *display );
void ice6k_destroy(void);

int ice6k_load_page( int page );
#ifndef PAGEFILEIO
int ice6k_save_page( int page );
#endif
int ice6k_load_high_page();
int ice6k_save_high_page();
void ice6k_writeio( unsigned short port, unsigned char data );
BYTE ice6k_readio( unsigned short port );
void ice6k_reset();

void ice6k_systemclock( unsigned long cycles );
void ice6k_getkey( PIOSTATE *pio, BYTE data );
void ice6k_kbd_keypress( int keycoord );

int ice6k_getcurrentpage();
//---------------------------------------------------------------------------
#endif
