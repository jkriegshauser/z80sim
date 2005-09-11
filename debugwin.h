//---------------------------------------------------------------------------

#ifndef debugwinH
#define debugwinH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <stdio.h>
// #include "z80.h"
#include "machine.h"
#include "main.h"
#include "interrupt.h"
#include <ComCtrls.hpp>
#include "z80cpu.h"
#include <ExtCtrls.hpp>
//---------------------------------------------------------------------------
const char frame[] = "0x0000: .. .. .. .. .. .. .. ..-.. .. .. .. .. .. .. .. ; ................";
const char hexchars[] = "0123456789ABCDEF";
//---------------------------------------------------------------------------
class TForm2 : public TForm
{
__published:	// IDE-managed Components
  TLabel *Label1;
  TButton *btStepInto;
  TRichEdit *Memo1;
  TRichEdit *Memo2;
  TLabel *Label2;
  TEdit *ebStart;
  TEdit *ebLength;
  TLabel *Label3;
  TLabel *Label4;
  TLabel *Label5;
  TEdit *ebDisLength;
  TEdit *ebDisStart;
  TButton *btRun;
  TButton *btStop;
  TButton *btReset;
  TListBox *ListBox1;
  TEdit *ebAddress;
  TLabel *Label6;
  TButton *btAdd;
  TButton *btDelete;
  TListBox *ListBox2;
  TLabel *Label7;
  TEdit *ebStackLen;
  TLabel *Label8;
  TListBox *ListBox3;
  TButton *btStepOver;
  TButton *btInterrupt;
        TTimer *Timer1;
  void __fastcall btStepIntoClick(TObject *Sender);
  void __fastcall ebStartChange(TObject *Sender);
  void __fastcall FormShow(TObject *Sender);
  void __fastcall ebDisStartChange(TObject *Sender);
  void __fastcall btRunClick(TObject *Sender);
  void __fastcall btStopClick(TObject *Sender);
  void __fastcall btResetClick(TObject *Sender);
  void __fastcall btAddClick(TObject *Sender);
  void __fastcall ListBox1Click(TObject *Sender);
  void __fastcall btDeleteClick(TObject *Sender);
  void __fastcall ebStackLenChange(TObject *Sender);
  void __fastcall btStepOverClick(TObject *Sender);
  void __fastcall btInterruptClick(TObject *Sender);
  void __fastcall FormHide(TObject *Sender);
        void __fastcall Timer1Timer(TObject *Sender);
private:	// User declarations
  int _writeHandler, _diswriteHandler;
  int _stackWriteHandler;
  int _stepoverpc;
  long _start, _len, _disstart, _dislen;

  Z80REGS _last;
  // BreakpointEvent _breakpoint;
  BREAKPOINTPROC  _breakpoint;
  int _lastpage;
  BYTE *_symbol;
  int _symbolsize;

public:		// User declarations
  __fastcall TForm2(TComponent* Owner);
  __fastcall ~TForm2();

  void __fastcall UpdateRegs();
  void __fastcall UpdateMemBlock();
  void __fastcall UpdateDisBlock();

  static void Breakpoint( unsigned short addr );
  void __fastcall OnPause();

  static bool __fastcall ParseInt( AnsiString text, unsigned long *val );

  static void MemWrite( unsigned short addr );
  static void DisMemWrite( unsigned short addr );
  static void stackChanged( unsigned short addr );
};
//---------------------------------------------------------------------------
extern PACKAGE TForm2 *Form2;
//---------------------------------------------------------------------------
#endif
