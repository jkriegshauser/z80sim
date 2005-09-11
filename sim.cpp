//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop
USERES("sim.res");
USEFORM("main.cpp", Form1);
USEFORM("interrupt.cpp", Form3);
USEUNIT("ice6000.cpp");
USEFORM("debugwin.cpp", Form2);
USEUNIT("disz80.c");
USEUNIT("display.c");
USEASM("z80cpu.asm");
USEUNIT("CTC.c");
USEUNIT("pio.c");
//---------------------------------------------------------------------------
#include <stdio.h>
// extern EXCEPTION_RECORD *ExceptionRecord;
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
  try
  {
     Application->Initialize();
     Application->CreateForm(__classid(TForm1), &Form1);
     Application->CreateForm(__classid(TForm3), &Form3);
     Application->CreateForm(__classid(TForm2), &Form2);
     Application->Run();
  }
  catch (Exception &exception)
  {
    Application->ShowException(&exception);
  }
  return 0;
}
//---------------------------------------------------------------------------
