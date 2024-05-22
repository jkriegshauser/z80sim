// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
//---------------------------------------------------------------------------

#ifndef interruptH
#define interruptH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
//---------------------------------------------------------------------------
class TForm3 : public TForm
{
__published:	// IDE-managed Components
  TEdit *ebInterrupt;
  TButton *btOK;
  TButton *btCancel;
  TLabel *Label1;
  TButton *btNMI;
  void __fastcall ebInterruptChange(TObject *Sender);
        void __fastcall btNMIClick(TObject *Sender);
        void __fastcall btOKClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
  __fastcall TForm3(TComponent* Owner);
  static bool __fastcall ParseInt( AnsiString text, unsigned long *val );
};
//---------------------------------------------------------------------------
extern PACKAGE TForm3 *Form3;
//---------------------------------------------------------------------------
#endif
