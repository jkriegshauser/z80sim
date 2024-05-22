// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "interrupt.h"
#include "z80cpu.h"
#include "debugwin.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm3 *Form3;
//---------------------------------------------------------------------------
__fastcall TForm3::TForm3(TComponent* Owner)
  : TForm(Owner)
{
}
//---------------------------------------------------------------------------
bool __fastcall TForm3::ParseInt( AnsiString text, unsigned long *val ) {
  char *c = text.c_str();
  *val = strtol( text.c_str(), &c, 0 );
  return c != text.c_str();
}
//---------------------------------------------------------------------------
void __fastcall TForm3::ebInterruptChange(TObject *Sender)
{
  DWORD val;
  btOK->Enabled = ParseInt( ebInterrupt->Text, &val );
  if( val > 0xFF ) btOK->Enabled = False;
}
//---------------------------------------------------------------------------
void __fastcall TForm3::btNMIClick(TObject *Sender)
{
  z80_nmi();
}
//---------------------------------------------------------------------------
void __fastcall TForm3::btOKClick(TObject *Sender)
{
  unsigned long val;
  if( !ParseInt( ebInterrupt->Text, &val) ) return;
  z80_interrupt( val & 0xFF );
}
//---------------------------------------------------------------------------

