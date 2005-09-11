//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "debugwin.h"
#include "disz80.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm2 *Form2;
//---------------------------------------------------------------------------
__fastcall TForm2::TForm2(TComponent* Owner)
  : TForm(Owner)
{
  _writeHandler = _diswriteHandler = _stackWriteHandler = _stepoverpc = -1;
  memset( &_last, 0x00, sizeof( _last ) );
  //Z80 *cpu = (Z80*)Form1->getMachine()->getProcessor();
  //cpu->OnPause = OnPause;
  _lastpage = -1;
  _symbol = NULL;
}
//---------------------------------------------------------------------------
__fastcall TForm2::~TForm2() {
  if( _symbol ) {
    delete _symbol;
    _symbol = NULL;
  }
}
//---------------------------------------------------------------------------
void __fastcall TForm2::UpdateRegs() {
  if( z80_is_running() ) return;
  HANDLE h;
  Z80REGS curr;
  memcpy( &curr, z80_getregs(), sizeof( Z80REGS ) );
  Memo1->Clear();
  char buff[100];
  sprintf( buff, "AF:  0x%04X   AF': 0x%04X", curr.AF, curr.AFalt );
  Memo1->Lines->Add( buff );
  sprintf( buff, "BC:  0x%04X   BC': 0x%04X", curr.BC, curr.BCalt );
  Memo1->Lines->Add( buff );
  sprintf( buff, "DE:  0x%04X   DE': 0x%04X", curr.DE, curr.DEalt );
  Memo1->Lines->Add( buff );
  sprintf( buff, "HL:  0x%04X   HL': 0x%04X", curr.HL, curr.HLalt );
  Memo1->Lines->Add( buff );
  sprintf( buff, "IX:  0x%04X   IY:  0x%04X", curr.IX, curr.IY );
  Memo1->Lines->Add( buff );
  sprintf( buff, "SP:  0x%04X   PC:  0x%04X", curr.SP, curr.PC );
  Memo1->Lines->Add( buff );
  sprintf( buff, "I:   0x%02X     R:   0x%02X  ", curr.IR >> 8, curr.IR & 0xFF );
  Memo1->Lines->Add( buff );
  sprintf( buff, "iff1: %d, iff2: %d", curr.IFF1, curr.IFF2 );
  Memo1->Lines->Add( buff );
  Memo1->Lines->Add( "" );
  sprintf( buff, "Cycles: %u (0x%08X)", z80_get_cycles(NULL), z80_get_cycles(NULL) );
  Memo1->Lines->Add( buff );
  int page = ice6k_getcurrentpage();
  sprintf( buff, "Current page: %d (0x%02X)", page, page );
  Memo1->Lines->Add( buff );
  if( _lastpage != page ) {
    DWORD size;
    _lastpage = page;
    sprintf( buff, "images\\page%d.sym", page );
    if( _symbol ) {
      delete _symbol;
      _symbol = NULL;
      _symbolsize = 0;
    }
    h = CreateFile( buff, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if( h != INVALID_HANDLE_VALUE ) {
      _symbol = new BYTE[ (_symbolsize = GetFileSize( h, NULL )) + 1 ];
      _symbol[_symbolsize] = 0;
      if( !ReadFile( h, _symbol, _symbolsize, &size, NULL ) ) {
        delete _symbol;
        _symbol = NULL;
        _symbolsize = 0;
      }
      CloseHandle( h );
    }
  }
  Memo1->Lines->Add("");
  // Highlight necessary fields
  bool stackchanged = false;
  if( _last.AF != curr.AF ) { Memo1->SelStart = 5; Memo1->SelLength = 6; Memo1->SelAttributes->Color = clRed; }
  if( _last.BC != curr.BC ) { Memo1->SelStart = 27+5; Memo1->SelLength = 6; Memo1->SelAttributes->Color = clRed; }
  if( _last.DE != curr.DE ) { Memo1->SelStart = (2*27)+5; Memo1->SelLength = 6; Memo1->SelAttributes->Color = clRed; }
  if( _last.HL != curr.HL ) { Memo1->SelStart = (3*27)+5; Memo1->SelLength = 6; Memo1->SelAttributes->Color = clRed; }
  if( _last.IX != curr.IX )                 { Memo1->SelStart = (4*27)+5; Memo1->SelLength = 6; Memo1->SelAttributes->Color = clRed; }
  if( _last.IY != curr.IY )                 { Memo1->SelStart = (4*27)+19; Memo1->SelLength = 6; Memo1->SelAttributes->Color = clRed; }
  if( _last.AFalt != curr.AFalt ) { Memo1->SelStart = 19; Memo1->SelLength = 6; Memo1->SelAttributes->Color = clRed; }
  if( _last.BCalt != curr.BCalt ) { Memo1->SelStart = 27+19; Memo1->SelLength = 6; Memo1->SelAttributes->Color = clRed; }
  if( _last.DEalt != curr.DEalt ) { Memo1->SelStart = (2*27)+19; Memo1->SelLength = 6; Memo1->SelAttributes->Color = clRed; }
  if( _last.HLalt != curr.HLalt ) { Memo1->SelStart = (3*27)+19; Memo1->SelLength = 6; Memo1->SelAttributes->Color = clRed; }
  if( _last.SP != curr.SP )                 { Memo1->SelStart = (5*27)+5; Memo1->SelLength = 6; Memo1->SelAttributes->Color = clRed; stackchanged = true; }
  if( _last.PC != curr.PC )                 { Memo1->SelStart = (5*27)+19; Memo1->SelLength = 6; Memo1->SelAttributes->Color = clRed; }
  if( ( _last.IR & 0xFF00 ) != ( curr.IR & 0xFF00 ) )                  { Memo1->SelStart = (6*27)+5; Memo1->SelLength = 4; Memo1->SelAttributes->Color = clRed; }
  if( ( _last.IR & 0xFF ) != ( curr.IR & 0xFF ) )                  { Memo1->SelStart = (6*27)+19; Memo1->SelLength = 4; Memo1->SelAttributes->Color = clRed; }
  if( _last.IFF1 != curr.IFF1 )             { Memo1->SelStart = (7*27)+6; Memo1->SelLength = 1; Memo1->SelAttributes->Color = clRed; }
  if( _last.IFF2 != curr.IFF2 )             { Memo1->SelStart = (7*27)+15; Memo1->SelLength = 1; Memo1->SelAttributes->Color = clRed; }
  Memo1->SelStart = 0;
  Memo1->SelLength = 0;
  memcpy( &_last, &curr, sizeof( Z80REGS ) );
  // Update the stack display
  if( stackchanged )
    ebStackLenChange(NULL);
  // Highlight the line in the disassembly
  int addr = z80_getregs()->PC;
  bool found = false;
  while( !found ) {
    if( addr >= _disstart && addr < ( _disstart + _dislen ) ) {
      for( int i = 0; i < ListBox2->Items->Count; ++i ) {
        if( addr == (DWORD)ListBox2->Items->Objects[i] ) {
          ListBox2->ItemIndex = i;
          ListBox2->Perform( LB_SETTOPINDEX, i - 5, 0 );
          found = true;
          break;
        }
      }
    }
    if( !found ) {
      ebDisStart->Text = AnsiString( "0x" ) + AnsiString::IntToHex(addr, 1);
      ebDisStartChange(this);
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TForm2::UpdateMemBlock() {
  if( z80_is_running() ) { return; }
  Memo2->Clear();
  int start = _start, len = _len;
  char buff[sizeof(frame)];
  char *mem = z80_get_mem();
  unsigned char c;
  int memlen = 65536;
  if( start > memlen || start + len > memlen)
    return;
  DWORD oldmask = SendMessage( Memo2->Handle, EM_SETEVENTMASK, 0, 0 );
  while( len > 0 ) {
    memcpy( buff, frame, sizeof(frame) );
    AnsiString s = AnsiString::IntToHex( start, 4 );
    memcpy( buff + 2, s.c_str(), s.Length() );
    for( int i = 8, j = 58; i < 8 + (16 * 3); i += 3, ++j, --len ) {
      c = mem[start++];
      if( c > 0 ) {
        buff[i] = hexchars[(c & 0xF0) >> 4];
        buff[i + 1] = hexchars[c & 0xF];
        if( c >= 0x20 && c <= 0x7E )
          buff[j] = c;
      } else {
        buff[i] = buff[i+1] = '0';
      }
    }
    Memo2->Lines->Add( buff );
  }
  SendMessage( Memo2->Handle, EM_SETEVENTMASK, 0, oldmask );
}
//---------------------------------------------------------------------------
void __fastcall TForm2::btStepIntoClick(TObject *Sender)
{
  z80_step();
  UpdateRegs();
  dsp_draw(0, Form1->Image1->Canvas->Handle);
  // UpdateMemBlock(cpu);
}
//---------------------------------------------------------------------------
bool __fastcall TForm2::ParseInt( AnsiString text, unsigned long *val ) {
  char *c = text.c_str();
  *val = strtol( text.c_str(), &c, 0 );
  return c != text.c_str();
}
//---------------------------------------------------------------------------
void __fastcall TForm2::ebStartChange(TObject *Sender)
{
  if( !ParseInt( ebStart->Text, (unsigned long *)&_start ) )
    return;
  if( !ParseInt( ebLength->Text, (unsigned long *)&_len ) )
    return;
  _start &= ~0x0F;    // start on 16-byte boundary
  _len--;  // length is zero-based
  if( _start + _len > 0xFFFF ) _len = 0xFFFF - _start;
  _len |= 0xF;       // length will stop on 16-byte boundary
  z80_unregister_memwrite( _writeHandler );
  // cpu->RemoveMemoryChangeCallback( _writeHandler );
  // _writeHandler = cpu->RegisterMemoryChangeCallback( _start, _start + _len, &MemWrite );
  _writeHandler = z80_register_memwrite( _start, _start + _len, &MemWrite );
  UpdateMemBlock();
}
//---------------------------------------------------------------------------
void __fastcall TForm2::FormShow(TObject *Sender)
{
  // Z80 *cpu = (Z80*)Form1->getMachine()->getProcessor();
  _breakpoint = ::z80_set_breakpoint_proc( &Breakpoint ); // cpu->OnBreakpoint;
  // cpu->OnBreakpoint = Breakpoint;
  memcpy( &_last, z80_getregs(), sizeof( Z80REGS ) );
  ebStartChange(Sender);
  ebDisStartChange(Sender);
  ebStackLenChange(Sender);
  UpdateRegs();
}
//---------------------------------------------------------------------------
void TForm2::MemWrite( unsigned short addr ) {
  Form2->UpdateMemBlock();
}
//---------------------------------------------------------------------------
void TForm2::DisMemWrite( unsigned short addr ) {
  Form2->UpdateDisBlock();
}
//---------------------------------------------------------------------------
void __fastcall TForm2::UpdateDisBlock() {
  if( z80_is_running() ) { return; }
  char buffer[80],buffer2[20];
  int pc = _disstart;
  int len;
  BYTE *mem = z80_get_mem();
  // Memo3->Clear();
  ListBox2->Clear();
  // INSTRUCTIONFORMAT format;
  // HWND hwnd = Memo3->Handle;
  // DWORD oldmask = SendMessage( hwnd, EM_SETEVENTMASK, 0, 0 );
  DWORD index;
  DWORD beginpc;
  // RECT rect;
  // SendMessage( hwnd, EM_GETRECT, 0, (LPARAM)&rect );
  // int height = Memo3->Font->Height;
  int height = ListBox2->Font->Height;
  if( height < 0 ) height = -height;
  int addr = z80_getregs()->PC;
  int selectindex = -1;
  ListBox2->Items->BeginUpdate();
  while( pc < _disstart + _dislen ) {
    sprintf( buffer, "0x%04X\t", pc );
    try {
      beginpc = pc;
      // mem = cpu->fetch( pc, len, &op );
      len = disz80(mem, pc, buffer2);
      if( len > 0 ) {
        for( int i = 0; i < len; ++i )
          sprintf( buffer + strlen(buffer), "%02x", mem[pc + i] );
        if( len < 4 ) strcat(buffer, "\t");
        pc += len;
      } else {
        sprintf( buffer + strlen(buffer), "%02x\t", mem[pc] );
        pc++;
      }
      strcat( buffer, "\t" );
      strcat(buffer, buffer2);
      // cpu->decode( mem, op, pc, buffer + strlen(buffer) );
      // Find any matching symbols
      if( _symbol ) {
        char *p = buffer, *p2, *p3, addr[9];
        int len = strlen(buffer), len2;
        // Look for the current address
        *(DWORD*)addr = 0x30303030;
        memcpy( addr + 4, buffer + 2, 4 );
        addr[8] = 0;
        p2 = strstr( (char*)_symbol, addr );
        if( p2 ) {
          char symbol[100];
          p3 = (p2 += 9);
          len2 = 0; while( *p3 && *p3 != '\n' && *p3 != '\r' ) { p3++; len2++; }
          memcpy( symbol, p2, len2 );
          symbol[len2] = ':';
          symbol[len2+1] = 0;
          ListBox2->Items->Add( symbol );
        }
        // Find the symbols in the current line
        while( (p = strchr( p, '$' )) ) {
          p += 2;
          if( p - buffer + 4 <= len ) {
            *(DWORD*)addr = 0x30303030;
            memcpy( addr + 4, p, 4 );
            addr[8] = 0;
            p2 = strstr( (char*)_symbol, addr );
            if( p2 ) {
              p3 = (p2 += 9);
              len2 = 0; while( *p3 && *p3 != '\n' && *p3 != '\r' ) { p3++; len2++; }
              if( len2 != 5 )
                memmove( p + 4 + (len2-5), p + 4, len - (p + 4 - buffer) + 1);
              memcpy( p - 1, p2, len2 );
            }
          }
        }
      }
      index = ListBox2->Items->Add( buffer );
      if( index >= 0 ) ListBox2->Items->Objects[index] = (TObject *)(beginpc);
      if( addr == beginpc ) {
        selectindex = index;
      }
    } catch( Exception &e ) {
      ListBox2->Clear();
      break;
    }
  }
  if( selectindex >= 0 ) {
    ListBox2->ItemIndex = selectindex;
    ListBox2->Perform( LB_SETTOPINDEX, selectindex - 5, 0 );
  }
  ListBox2->Items->EndUpdate();
  // SendMessage( hwnd, EM_SETEVENTMASK, 0, oldmask );
}
//---------------------------------------------------------------------------
void __fastcall TForm2::ebDisStartChange(TObject *Sender)
{
  // Z80 *cpu = (Z80*)Form1->getMachine()->getProcessor();
  if( !ParseInt( ebDisStart->Text, (unsigned long *)&_disstart ) )
    return;
  if( !ParseInt( ebDisLength->Text, (unsigned long *)&_dislen ) )
    return;
  _len--;  // length is zero-based
  if( _len > 0xFFF ) _len = 0xFFF;
  // cpu->RemoveMemoryChangeCallback( _diswriteHandler );
  z80_unregister_memwrite( _diswriteHandler );
  // _diswriteHandler = cpu->RegisterMemoryChangeCallback( _disstart, _disstart + _dislen, &DisMemWrite );
  _diswriteHandler = z80_register_memwrite( _disstart, _disstart + _dislen, &DisMemWrite );
  UpdateDisBlock();
}
//---------------------------------------------------------------------------
void __fastcall TForm2::btRunClick(TObject *Sender)
{
  // Z80 *cpu = (Z80*)Form1->getMachine()->getProcessor();
  // cpu->setRunning( true );
  btRun->Enabled = false;
  btStop->Enabled = true;
  btStepInto->Enabled = false;
  btStepOver->Enabled = false;
  // cpu->start();
  z80_start();
  // Sleep(1000);
  // btStop->Click();
  /*
  btRun->Enabled = true;
  btStop->Enabled = false;
  btStepInto->Enabled = true;
  btStepOver->Enabled = true;
  if( Sender )
    ListBox3->Clear();
  FormShow( Sender );
  */
}
//---------------------------------------------------------------------------

void __fastcall TForm2::btStopClick(TObject *Sender)
{
  // Z80 *cpu = (Z80*)Form1->getMachine()->getProcessor();
  // cpu->pause();
  z80_stop();
  btRun->Enabled = true;
  btStop->Enabled = false;
  btStepInto->Enabled = true;
  btStepOver->Enabled = true;
  ebStartChange(this);
  //ebDisStartChange(this);
  ListBox3->Clear();
  ebStackLenChange(this);
  UpdateRegs();
}
//---------------------------------------------------------------------------
void __fastcall TForm2::btResetClick(TObject *Sender)
{
  ice6k_reset();
  UpdateRegs();
  /*
  for( int i = 0; i < ListBox1->Items->Count; ++i ) {
    if( ParseInt(ListBox1->Items->Strings[i], &handler.address) ) {
      index = cpu->registerBreakpoint( &handler );
      ListBox1->Items->Objects[i] = (TObject *)index;
    }
  }
  */
}
//---------------------------------------------------------------------------
void __fastcall TForm2::btAddClick(TObject *Sender)
{
  // Z80 *cpu = (Z80*)Form1->getMachine()->getProcessor();
  char hex[11];
  int addr;
  if( !ParseInt( ebAddress->Text, (unsigned long*)&addr ) )
    return;
  sprintf( hex, "0x%04X", addr );
  for( int i =0; i < ListBox1->Items->Count; ++i )
    if( ListBox1->Items->Strings[i] == hex )
      return;
  int index = ListBox1->Items->Add( hex );
  if( index >= 0 ) {
    // if( cpu->RegisterBreakpoint( addr ) )
    if( z80_register_breakpoint( addr ) )
      ListBox1->Items->Objects[index] = (TObject *)addr;
    else ListBox1->Items->Delete( index );
  }
}
//---------------------------------------------------------------------------
void __fastcall TForm2::ListBox1Click(TObject *Sender)
{
  if( ListBox1->ItemIndex >= 0 )
    btDelete->Enabled = true;
  else
    btDelete->Enabled = false;
}
//---------------------------------------------------------------------------
void __fastcall TForm2::btDeleteClick(TObject *Sender)
{
  if( ListBox1->ItemIndex >= 0 ) {
    z80_unregister_breakpoint( (int)ListBox1->Items->Objects[ListBox1->ItemIndex] );
    ListBox1->Items->Delete(ListBox1->ItemIndex);
  }
}
//---------------------------------------------------------------------------
void __fastcall TForm2::ebStackLenChange(TObject *Sender)
{
  // Rebuild the stack display
  // Z80 *cpu = (Z80*)Form1->getMachine()->getProcessor();
  if( z80_is_running() ) return; // don't update if running
  int physical = z80_getregs()->SP;
  DWORD i, range;
  int index, selectindex = -1;
  char buffer[100];
  static int lastphysical = 0;

  if( !ParseInt( ebStackLen->Text, &range ) )
    return;

  physical &= 0xFFFF;
  // range &= ~1; // align on 2 byte boundary

  for( i = 0; i < ListBox3->Items->Count; ++i ) {
    if( ListBox3->Items->Objects[i] == (TObject *)physical )
      break;
  }

  if( i == ListBox3->Items->Count ) {
    // Couldn't find the stack pointer in the list, so rebuild the window
    ListBox3->Clear();
    for( i = ( physical + range > 0xFFFF ? 0xFFFE : physical + range ); i >= physical - range; i -= 2 ) {
      sprintf( buffer, "0x%04X  %04X", i, *(WORD*)(z80_get_mem() + i ) );
      index = ListBox3->Items->Add( buffer );
      if( index >= 0 )
        ListBox3->Items->Objects[index] = (TObject *)i;
      if( i == physical ) selectindex = index;
    }
  } else {
    // found the stack pointer, make do with what we've got. i is the index of the
    // stack pointer
    selectindex = i;
    if( (int)ListBox3->Items->Objects[0] < physical + range ) {
      // need to insert some items above
      for( i = ( physical + range > 0xFFFF ? 0xFFFE : physical + range ); i > (int)ListBox3->Items->Objects[0]; i -= 2 ) {
        sprintf( buffer, "0x%04X  %04X", i, *(WORD*)(z80_get_mem() + i) );
        ListBox3->Items->Insert( 0, buffer );
        ListBox3->Items->Objects[0] = (TObject *)i;
        ++selectindex;
      }
    }
    if( ( i = (int)ListBox3->Items->Objects[ListBox3->Items->Count - 1] ) > physical - range ) {
      // need to insert some items below
      for( ; i > physical - range; i -= 2 ) {
        sprintf( buffer, "0x%04X  %04X", i, *(WORD*)(z80_get_mem() + i) );
        index = ListBox3->Items->Add( buffer );
        if( index >= 0 ) {
          ListBox3->Items->Objects[index] = (TObject *)i;
        }
      }
    }
  }
  lastphysical = physical;
  // cpu->RemoveMemoryChangeCallback( _stackWriteHandler );
  z80_unregister_memwrite( _stackWriteHandler );
  // _stackWriteHandler = cpu->RegisterMemoryChangeCallback( (int)ListBox3->Items->Objects[ListBox3->Items->Count - 1], (int)ListBox3->Items->Objects[0] + 1, &stackChanged );
  _stackWriteHandler = z80_register_memwrite( (int)ListBox3->Items->Objects[ListBox3->Items->Count - 1], (int)ListBox3->Items->Objects[0] + 1, &stackChanged );
  if( selectindex >= 0 ) {
    ListBox3->ItemIndex = selectindex;
    ListBox3->Perform( LB_SETTOPINDEX, selectindex - 6, 0 );
  }
}
//---------------------------------------------------------------------------
void __fastcall TForm2::btStepOverClick(TObject *Sender)
{
  int index;
  // Z80 *cpu = (Z80*)Form1->getMachine()->getProcessor();
  AnsiString s = ListBox2->Items->Strings[ListBox2->ItemIndex];
  if( ( s.Pos( "call" ) || s.Pos( "djnz" ) || s.Pos( "ldir" ) || s.Pos( "lddr" ) || s.Pos( "cpir" ) || s.Pos( "cpdr" ) || s.Pos( "inir" ) || s.Pos( "indr" ) || s.Pos( "otir" ) || s.Pos( "otdr" ) ) && ListBox2->ItemIndex < ListBox2->Items->Count - 1 ) {
    // if( cpu->RegisterBreakpoint( _stepoverpc = (int)ListBox2->Items->Objects[ListBox2->ItemIndex + 1] ) )
    if( z80_register_breakpoint( _stepoverpc = (int)ListBox2->Items->Objects[ListBox2->ItemIndex + 1] ) )
      btRunClick( NULL );
  } else {
    btStepIntoClick( NULL );
  }
  /*
  s = s.SubString( 12, s.Length() );
  if( s.Pos( "call" ) > 0 && ListBox2->ItemIndex < ListBox2->Items->Count - 1 ) {
    handler.address = (DWORD)ListBox2->Items->Objects[ListBox2->ItemIndex + 1];
    handler.handler = NULL;
    index = cpu->registerBreakpoint( &handler );
    btRunClick( NULL );
    cpu->removeBreakpoint( index );
  } else {
    btStepIntoClick( Sender );
  }
  */

}
//---------------------------------------------------------------------------
void TForm2::stackChanged( unsigned short addr ) {
  if( z80_is_running() ) { return; } // don't update if running
  // Find the applicable index
  int i;
  int base;
  char buffer[100], *p;
  AnsiString s;
  try {
    s = Form2->ListBox2->Items->Strings[Form2->ListBox2->ItemIndex];
  } catch( Exception &e ) { }
  strcpy( buffer, s.c_str() );
  p = strstr( buffer, "\t\t" );  // two tabs together
  if( p == NULL ) {
    p = strchr( buffer, '\t' );  // first tab
    if( p )
      p = strchr( p + 1, '\t' );  // second tab
  } else
    p++;
  s = AnsiString( p ? p + 1 : buffer );
  // s = s.SubString( 12, s.Length() );
  for( i = 1; i <= s.Length(); ++i ) if( s[i] == '\t' ) s[i] = ' ';
  for( i = 0; i < Form2->ListBox3->Items->Count; ++i ) {
    base = (int)Form2->ListBox3->Items->Objects[i];
    if( addr >= base && addr < base + 2 ) {
      sprintf( buffer, "0x%04X  %04X     (%s)", base, *(WORD*)(z80_get_mem() + base), s.c_str() );
      Form2->ListBox3->Items->Strings[i] = buffer;
      break;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TForm2::btInterruptClick(TObject *Sender)
{
  // Z80 *cpu = ((Z80*)Form1->getMachine()->getProcessor());
  DWORD val;
  int i;
  if( (i = Form3->ShowModal()) == mrOk ) {
    if( ParseInt( Form3->ebInterrupt->Text, &val ) && val <= 0xFF ) {
      // cpu->pause();
      z80_stop();
      // if( !cpu->interrupt(val) )
      if( !z80_interrupt(val) )
        // cpu->start();
        z80_start();
      else {
        btStop->Click();
      }
    }
  } else if( i == mrRetry ) {
    // cpu->pause();
    z80_stop();
    // cpu->NMI();
    z80_nmi();
    btStop->Click();
  }
}
//---------------------------------------------------------------------------
void TForm2::Breakpoint( unsigned short addr ) {
  // Z80 *cpu = (Z80*)processor;
  Form2->btStopClick( Form2 );
  if( addr == Form2->_stepoverpc ) {
    ::z80_unregister_breakpoint( Form2->_stepoverpc );
    // cpu->RemoveBreakpoint( _stepoverpc );
    Form2->_stepoverpc = -1;
  }
  if( Form2->_breakpoint )
    (*Form2->_breakpoint)( addr );
  dsp_draw(0, Form1->Image1->Canvas->Handle);
}
//---------------------------------------------------------------------------
void __fastcall TForm2::FormHide(TObject *Sender)
{
  // Z80 *cpu = (Z80*)Form1->getMachine()->getProcessor();
  // cpu->OnBreakpoint = _breakpoint;
  ::z80_set_breakpoint_proc( _breakpoint );
}
//---------------------------------------------------------------------------
void __fastcall TForm2::OnPause( ) {
  btStop->Click();
}
//---------------------------------------------------------------------------
void __fastcall TForm2::Timer1Timer(TObject *Sender)
{
  Memo1->Lines->Strings[12] = "Effective CPU speed: " + AnsiString(z80_get_last_cps());
}
//---------------------------------------------------------------------------

