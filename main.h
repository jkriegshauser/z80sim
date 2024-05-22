// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
//---------------------------------------------------------------------------

#ifndef mainH
#define mainH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <stdio.h>
#include <winioctl.h>
#include "ICE6000.h"
#include "debugwin.h"
#include "display.h"
#include <ExtCtrls.hpp>
//---------------------------------------------------------------------------
class TForm1 : public TForm
{
__published:	// IDE-managed Components
  TButton *Button1;
  TLabel *Label1;
  TPaintBox *Image1;
  TButton *bt1;
  TButton *bt2;
  TButton *bt3;
  TButton *bt4;
  TButton *bt5;
  TButton *bt6;
  TButton *bt7;
  TButton *bt8;
  TButton *bt9;
  TButton *bt0;
  TButton *btEnter;
  TButton *btClear;
  void __fastcall Button1Click(TObject *Sender);
  void __fastcall Image1Paint(TObject *Sender);
  void __fastcall LastKeyPress(TObject *Sender);
private:	// User declarations
public:		// User declarations
  __fastcall TForm1(TComponent* Owner);
  __fastcall ~TForm1();

  volatile int lastkey;
};
//---------------------------------------------------------------------------
extern PACKAGE TForm1 *Form1;
//---------------------------------------------------------------------------
#endif
