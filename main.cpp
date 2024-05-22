// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "main.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm1 *Form1;
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner)
  : TForm(Owner) {
  Image1->Canvas->Brush->Color = clBlack;
  Image1->Canvas->FillRect( Image1->ClientRect );
  ice6k_init( Image1->Canvas );
}
//---------------------------------------------------------------------------
__fastcall TForm1::~TForm1() {
}
//---------------------------------------------------------------------------
void __fastcall TForm1::Button1Click(TObject *Sender)
{
  Form2->Show();
}
//---------------------------------------------------------------------------
void __fastcall TForm1::Image1Paint(TObject *Sender)
{
  dsp_draw(1, Image1->Canvas->Handle);
}
//---------------------------------------------------------------------------
void __fastcall TForm1::LastKeyPress(TObject *Sender)
{
  ice6k_kbd_keypress( (int)((TButton*)Sender)->Tag );
}
//---------------------------------------------------------------------------

