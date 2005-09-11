//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "z80.h"
#include "codepage.h"

//---------------------------------------------------------------------------
__fastcall Z80::Z80( BYTE *mem, int memsize ) : Processor(mem, memsize) {
  // ::setmemloc( mem );
  this->reset();
  Z80REGS *regs = ::z80_getregs();
  _regs.u_af[0].af = regs->AF;
  _regs.u_af[1].af = regs->AFalt;
  _regs.u_bc[0].bc = regs->BC;
  _regs.u_bc[1].bc = regs->BCalt;
  _regs.u_de[0].de = regs->DE;
  _regs.u_de[1].de = regs->DEalt;
  _regs.u_hl[0].hl = regs->HL;
  _regs.u_hl[1].hl = regs->HLalt;
  _regs.ix = regs->IX;
  _regs.iy = regs->IY;
  _regs.sp = regs->SP;
  _regs.pc = regs->PC;
  _regs.i  = regs->IR >> 8;
  _regs.r  = regs->IR & 0xF;
  _regs.iff1 = regs->IFF1;
  _regs.iff2 = regs->IFF2;
}
//---------------------------------------------------------------------------
__fastcall Z80::~Z80() {
}
//---------------------------------------------------------------------------
void __fastcall Z80::reset() {
  ::z80_reset();
}
//---------------------------------------------------------------------------
void __fastcall Z80::step() {
  ::z80_step();
}
//---------------------------------------------------------------------------
void __fastcall Z80::start() {
  ::z80_start();
}
//---------------------------------------------------------------------------
void __fastcall Z80::pause() {
  ::z80_stop();
}
//---------------------------------------------------------------------------
void __fastcall Z80::terminate() {
  ::z80_terminate();
}
//---------------------------------------------------------------------------
bool __fastcall Z80::getRunning() {
  return ::z80_is_running();
}
//---------------------------------------------------------------------------
long __fastcall Z80::getcycles() {
  return ::z80_get_cycles( NULL );
}
//---------------------------------------------------------------------------
bool __fastcall Z80::interrupt( BYTE data ) {
  return ::z80_interrupt(data);
}
//---------------------------------------------------------------------------
void __fastcall Z80::NMI() {
  ::z80_nmi();
}
//---------------------------------------------------------------------------
void __fastcall Z80::handleNMI() {
  /*
  _cycles += 5;
  _regs.sp -= 2;
  *(WORD*)(_mem + _regs.sp) = _regs.pc;
  _regs.pc = 0x0066;
  MemChanged( _regs.sp, _regs.sp + 1 );
  _regs.iff1 = false;
  _nmipending = false;
  */
}
//---------------------------------------------------------------------------
void __fastcall Z80::handleInterrupt() {
  /*
  if( !_regs.iff1 ) return;
  _regs.iff1 = _regs.iff2 = false;
  switch( _interruptmode ) {
    case 0:
      break;
    case 1:
      _cycles += 5;
      _regs.sp -= 2;
      *(WORD*)(_mem + _regs.sp) = _regs.pc;
      _regs.pc = 0x0038;
      MemChanged( _regs.sp, _regs.sp + 1 );
      break;
    case 2:
      _cycles += 19;
      _regs.sp -= 2;
      *(WORD*)(_mem + _regs.sp) = _regs.pc;
      _regs.pc = *(WORD*)(_mem + (_regs.i << 8) + (_interruptpending & 0xFE));
      MemChanged( _regs.sp, _regs.sp + 1 );
      break;
  }
  _interruptpending = -1;
  */
}
//---------------------------------------------------------------------------
void __fastcall Z80::UndefinedOpcode() {
  pause();
}
//---------------------------------------------------------------------------
Z80REGS * __fastcall Z80::getregs() {
  return ::z80_getregs();
}
//---------------------------------------------------------------------------
BYTE * __fastcall Z80::fetch( int pc, int &len, OPCODE **op ) {
  BYTE *start = _mem + pc, *p = start;
  len = 1;
  int i, j;

  j = 0;
  while( true ) {
    for( i = 0; i < sizeof(_codepage[j])/sizeof(OPCODE); ++i ) {
      if( *p == _codepage[j][i].opcode ) {
        if( _codepage[j][i].codepage > 0 ) {
          j = _codepage[j][i].codepage;
          ++p;
          ++len;
          break;
        }
        len += GetAddressingModeLength( _codepage[j][i].primary );
        len += GetAddressingModeLength( _codepage[j][i].secondary );
        *op = (OPCODE*)&_codepage[j][i];
        return start;
      }
    }
    // Undefined opcode
    if( i == sizeof( _codepage[j] )/sizeof(OPCODE) ) {
      //*op = (OPCODE*)&_codepage[0][0];    // NOP
      //return start;
      len = 0;
      *op = NULL;
      return start;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall Z80::decode( BYTE *instr, OPCODE *op, int pc, char *buffer ) {
  char *p;
  if( instr && op && op->name ) {
    strcpy( buffer, op->name );
    p = strchr( buffer, ' ' );
    if( p ) {
      *p = '\t';
    } else
      strcat( buffer, "\t" );
    decodeAM( op->primary, instr + op->opcodelen, pc, buffer + strlen(buffer) );
    decodeAM( op->secondary, instr + op->opcodelen + GetAddressingModeLength( op->primary ), pc, buffer + strlen(buffer), true );
  } else {
    buffer[0] = 0;
    switch( instr[0] ) {
      case 0xFD:
      case 0xDD:
        switch( instr[1] ) {
          case 0xCB:
            switch( instr[3] ) {
            }
        }
    }
    if( buffer[0] == 0 )
      sprintf( buffer, "db\t$%02X", *instr );
  }
}
//---------------------------------------------------------------------------
void __fastcall Z80::decodeAM( ADDRESSINGMODE ad, BYTE *data, int pc, char *buffer, bool comma ) {
  buffer[0] = 0;
  if( comma ) buffer++;
  buffer[0] = 0;
  switch( ad ) {
    case amIMMEDIATE: sprintf( buffer, "$%02x", *data ); break;
    case amIMMEDIATEEX: sprintf( buffer, "$%04x", *(WORD*)data ); break;
    case amRELATIVE: sprintf( buffer, "$%04x", pc + (signed char)data[0] ); break;
    case amEXTENDED: sprintf( buffer, "($%04x)", *(WORD*)data ); break;
    case amINDEXX: sprintf( buffer, "(ix%s%d)", (*data & 0x80) ? "" : "+", (signed char)data[0] ); break;
    case amINDEXY: sprintf( buffer, "(iy%s%d)", (*data & 0x80) ? "" : "+", (signed char)data[0] ); break;
    case amREGA: strcpy( buffer, "a" ); break;
    case amREGF: strcpy( buffer, "f" ); break;
    case amREGB: strcpy( buffer, "b" ); break;
    case amREGC: strcpy( buffer, "c" ); break;
    case amREGD: strcpy( buffer, "d" ); break;
    case amREGE: strcpy( buffer, "e" ); break;
    case amREGH: strcpy( buffer, "h" ); break;
    case amREGL: strcpy( buffer, "l" ); break;
    case amREGA_: strcpy( buffer, "a'" ); break;
    case amREGF_: strcpy( buffer, "f'" ); break;
    case amREGB_: strcpy( buffer, "b'" ); break;
    case amREGC_: strcpy( buffer, "c'" ); break;
    case amREGD_: strcpy( buffer, "d'" ); break;
    case amREGE_: strcpy( buffer, "e'" ); break;
    case amREGH_: strcpy( buffer, "h'" ); break;
    case amREGL_: strcpy( buffer, "l'" ); break;
    case amREGI: strcpy( buffer, "i" ); break;
    case amREGR: strcpy( buffer, "r" ); break;
    case amREGAF: strcpy( buffer, "af" ); break;
    case amREGBC: strcpy( buffer, "bc" ); break;
    case amREGDE: strcpy( buffer, "de" ); break;
    case amREGHL: strcpy( buffer, "hl" ); break;
    case amREGAF_: strcpy( buffer, "af'" ); break;
    case amREGBC_: strcpy( buffer, "bc'" ); break;
    case amREGDE_: strcpy( buffer, "de'" ); break;
    case amREGHL_: strcpy( buffer, "hl'" ); break;
    case amREGSP: strcpy( buffer, "sp" ); break;
    case amREGIX: strcpy( buffer, "ix" ); break;
    case amREGIY: strcpy( buffer, "iy" ); break;
    case amREGINDBC: strcpy( buffer, "(bc)" ); break;
    case amREGINDDE: strcpy( buffer, "(de)" ); break;
    case amREGINDHL: strcpy( buffer, "(hl)" ); break;
    case amREGINDSP: strcpy( buffer, "(sp)" ); break;
    case amREGINDIX: strcpy( buffer, "(ix)" ); break;
    case amREGINDIY: strcpy( buffer, "(iy)" ); break;
    case amPAGEZERO: sprintf( buffer, "$%02x", *(data - 1) & 0x38 ); break;
    case amBIT:
    case amIMPLIED:
    default:
      break;
  }
  if( buffer[0] != 0 && comma )
    *(buffer-1) = ',';
}
//---------------------------------------------------------------------------
int __fastcall Z80::GetAddressingModeLength( ADDRESSINGMODE ad ) {
  int len;
  switch( ad ) {
    case amBIT:
    case amPAGEZERO:
    case amRELATIVE:
    case amINDEXX:
    case amINDEXY:
    case amSKIPONE:
    case amIMMEDIATE: len = 1; break;
    case amEXTENDED:
    case amIMMEDIATEEX: len = 2; break;
    default: len = 0;
  }
  return len;
}
//---------------------------------------------------------------------------
BYTE Z80::readreg8( REGDEF8 rd ) {
  BYTE b;
  switch( rd ) {
    case rbA: b = _regs.u_af[0].s_af.a; break;
    case rbB: b = _regs.u_bc[0].s_bc.b; break;
    case rbC: b = _regs.u_bc[0].s_bc.c; break;
    case rbD: b = _regs.u_de[0].s_de.d; break;
    case rbE: b = _regs.u_de[0].s_de.e; break;
    case rbH: b = _regs.u_hl[0].s_hl.h; break;
    case rbL: b = _regs.u_hl[0].s_hl.l; break;
  }
  return b;
}
//---------------------------------------------------------------------------
void Z80::setreg8( REGDEF8 rd, BYTE b ) {
  switch( rd ) {
    case rbA: _regs.u_af[0].s_af.a = b; break;
    case rbB: _regs.u_bc[0].s_bc.b = b; break;
    case rbC: _regs.u_bc[0].s_bc.c = b; break;
    case rbD: _regs.u_de[0].s_de.d = b; break;
    case rbE: _regs.u_de[0].s_de.e = b; break;
    case rbH: _regs.u_hl[0].s_hl.h = b; break;
    case rbL: _regs.u_hl[0].s_hl.l = b; break;
  }
}
//---------------------------------------------------------------------------
WORD Z80::readreg16( REGDEF16 rd ) {
  WORD w;
  switch( rd ) {
    case rwBC: w = _regs.u_bc[0].bc; break;
    case rwDE: w = _regs.u_de[0].de; break;
    case rwHL: w = _regs.u_hl[0].hl; break;
    case rwSP: w = _regs.sp; break;
  }
  return w;
}
//---------------------------------------------------------------------------
void Z80::setreg16( REGDEF16 rd, WORD w ) {
  switch( rd ) {
    case rwBC: _regs.u_bc[0].bc = w; break;
    case rwDE: _regs.u_de[0].de = w; break;
    case rwHL: _regs.u_hl[0].hl = w; break;
    case rwSP: _regs.sp = w; break;
  }
}
//---------------------------------------------------------------------------
BYTE Z80::add( BYTE a1, BYTE a2, BYTE &flags, bool carry ) {
  BYTE result;
  int bigresult;
  flags = 0;
  if( (((a1 & 0xF) + (a2 & 0xF)) & 0xF0) != 0 ) flags |= H_SET;
  result = a1 + a2 + (carry?1:0);
  if( (unsigned)a1 + (unsigned)a2 > 255 ) flags |= C_SET;
  bigresult = (int)a1 + (int)a2 + (int)(carry?1:0);
  if( bigresult < -128 || bigresult > 127 ) flags |= PV_SET;
  if( result == 0 ) flags |= Z_SET;
  if( result & 0x80 ) flags |= S_SET;
  return result;
}
//---------------------------------------------------------------------------
BYTE Z80::sub( BYTE s1, BYTE s2, BYTE &flags, bool carry ) {
  BYTE result;
  int bigresult;
  flags = 0;  // clear flags
  if( carry ) s2++;
  if( (s2 & 0xF) > (s1 & 0xF) ) flags |= H_SET;   // half-carry
  if( s2 > s1 ) flags |= C_SET;
  result = s1 - s2;
  bigresult = (int)s1 - (int)s2;
  if( bigresult < -128 || bigresult > 127 ) flags |= PV_SET;
  if( result == 0 ) flags |= Z_SET;
  if( result & 0x80 ) flags |= S_SET;
  flags |= N_SET;
  return result;
}
//---------------------------------------------------------------------------
BYTE Z80::parity( BYTE b ) {
  BYTE b1;
  bool parity = true;
  for( b1 = 0x80; b1 != 0; b1>>=1 )
    if( b & b1 ) parity = !parity;
  return parity?PV_SET:0;
}
//---------------------------------------------------------------------------
void Z80::LDREGS( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->setreg8( (instr[0] & 0x38)>>3, cpu->readreg8(instr[0] & 0x7) );
  cpu->_cycles += 4;
}
//---------------------------------------------------------------------------
void Z80::LDRN( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->setreg8( (instr[0] & 0x38)>>3, instr[1] );
  cpu->_cycles += 7;
}
//---------------------------------------------------------------------------
void Z80::LDRHL( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->setreg8( (instr[0] & 0x38)>>3, cpu->_mem[cpu->_regs.u_hl[0].hl] );
  cpu->_cycles += 7;
}
//---------------------------------------------------------------------------
void Z80::LDRI( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->setreg8( (instr[1] & 0x38)>>3, cpu->_mem[(instr[0]==0xDD?cpu->_regs.ix:cpu->_regs.iy)+(signed char)instr[2]] );
  cpu->_cycles += 19;
}
//---------------------------------------------------------------------------
void Z80::LDHLR( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_mem[cpu->_regs.u_hl[0].hl] = cpu->readreg8( instr[0] & 0x7 );
  cpu->_cycles += 7;
  cpu->MemChanged( cpu->_regs.u_hl[0].hl );
}
//---------------------------------------------------------------------------
void Z80::LDIxR( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  int addr = (instr[0]==0xDD?cpu->_regs.ix:cpu->_regs.iy)+(signed char)instr[2];
  cpu->_mem[addr] = cpu->readreg8( instr[1] & 0x7 );
  cpu->_cycles += 19;
  cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::LDHLN( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  int addr = cpu->_regs.u_hl[0].hl;
  cpu->_mem[addr] = instr[1];
  cpu->_cycles += 10;
  cpu->MemChanged( addr, addr + 1 );
}
//---------------------------------------------------------------------------
void Z80::LDIN( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  int addr = (instr[0]==0xDD?cpu->_regs.ix:cpu->_regs.iy)+(signed char)instr[2];
  cpu->_mem[addr] = instr[3];
  cpu->_cycles += 19;
  cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::LDA( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b;
  switch( instr[0] ) {
    case 0x0A: b = cpu->_mem[cpu->_regs.u_bc[0].bc]; cpu->_cycles += 7; break;
    case 0x1A: b = cpu->_mem[cpu->_regs.u_de[0].de]; cpu->_cycles += 7; break;
    case 0x3A: b = cpu->_mem[*(WORD*)(instr + 1)]; cpu->_cycles += 13; break;
    case 0xED:
      switch( instr[1] ) {
        case 0x57: b = cpu->_regs.i; break;
        case 0x5F: b = cpu->_regs.r; break;
      }
      cpu->_regs.u_af[0].s_af.f &= C_SET;    // clear all but carry
      cpu->_regs.u_af[0].s_af.f |= (b&0x80?S_SET:0)|(b==0?Z_SET:0)|(cpu->_regs.iff2?PV_SET:0); // set applicable bits
      cpu->_cycles += 9;
      break;
  }
  cpu->_regs.u_af[0].s_af.a = b;
}
//---------------------------------------------------------------------------
void Z80::LDxA( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b = cpu->_regs.u_af[0].s_af.a;
  int addr = -1;
  switch( instr[0] ) {
    case 0x02: cpu->_mem[addr = cpu->_regs.u_bc[0].bc] = b; cpu->_cycles += 7; break;
    case 0x12: cpu->_mem[addr = cpu->_regs.u_de[0].de] = b; cpu->_cycles += 7; break;
    case 0x32: cpu->_mem[addr = (*(WORD*)(instr + 1))] = b; cpu->_cycles += 13; break;
    case 0xED:
      switch( instr[1] ) {
        case 0x47: cpu->_regs.i = b; break;
        case 0x4F: cpu->_regs.r = b; break;
      }
      cpu->_cycles += 9;
      break;
  }
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::LDDDNN( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->setreg16( (instr[0]& 0x70)>>4, *(WORD*)(instr + 1) );
  cpu->_cycles += 10;
}
//---------------------------------------------------------------------------
void Z80::LDINN( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  if( instr[0] == 0xDD ) {
    cpu->_regs.ix = *(WORD*)(instr + 2);
  } else {
    cpu->_regs.iy = *(WORD*)(instr + 2);
  }
  cpu->_cycles += 14;
}
//---------------------------------------------------------------------------
void Z80::LDDDNNI( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  if( instr[0] == 0x2A ) {
    cpu->_regs.u_hl[0].hl = *(WORD*)&cpu->_mem[*(WORD*)(instr + 1)];
    cpu->_cycles += 16;
  } else {
    cpu->setreg16( (instr[1]&0x30)>>4, *(WORD*)&cpu->_mem[*(WORD*)(instr + 2)] );
    cpu->_cycles += 20;
  }
}
//---------------------------------------------------------------------------
void Z80::LDINNI( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  if( instr[0] == 0xDD ) {
    cpu->_regs.ix = *(WORD*)&cpu->_mem[*(WORD*)(instr + 2)];
  } else {
    cpu->_regs.iy = *(WORD*)&cpu->_mem[*(WORD*)(instr + 2)];
  }
  cpu->_cycles += 20;
}
//---------------------------------------------------------------------------
void Z80::LDNNIDD( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  int addr;
  if( instr[0] == 0x22 ) {
    *(WORD*)(&cpu->_mem[addr = *(WORD*)(instr + 1)]) = cpu->_regs.u_hl[0].hl;
    cpu->_cycles += 16;
  } else {
    *(WORD*)(&cpu->_mem[addr = *(WORD*)(instr + 2)]) = cpu->readreg16( (instr[1]&0x30)>>4 );
    cpu->_cycles += 20;
  }
  cpu->MemChanged( addr, addr + 1 );
}
//---------------------------------------------------------------------------
void Z80::LDNNII( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  WORD w;
  if( instr[0] == 0xDD ) {
    w = cpu->_regs.ix;
  } else {
    w = cpu->_regs.iy;
  }
  int addr;
  *(WORD*)(cpu->_mem + (addr = *(WORD*)(instr + 2))) = w;
  cpu->_cycles += 20;
  cpu->MemChanged( addr, addr + 1 );
}
//---------------------------------------------------------------------------
void Z80::LDSP( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  switch( instr[0] ) {
    case 0xF9: cpu->_regs.sp = cpu->_regs.u_hl[0].hl; cpu->_cycles += 6; break;
    case 0xDD: cpu->_regs.sp = cpu->_regs.ix; cpu->_cycles += 10; break;
    case 0xFD: cpu->_regs.sp = cpu->_regs.iy; cpu->_cycles += 10; break;
  }
}
//---------------------------------------------------------------------------
void Z80::PUSHQQ( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_regs.sp -= 2;
  switch( instr[0] ) {
    case 0xDD: *(WORD*)(cpu->_mem + cpu->_regs.sp) = cpu->_regs.ix; cpu->_cycles += 15; break;
    case 0xFD: *(WORD*)(cpu->_mem + cpu->_regs.sp) = cpu->_regs.iy; cpu->_cycles += 15; break;
    case 0xF5: *(WORD*)(cpu->_mem + cpu->_regs.sp) = cpu->_regs.u_af[0].af; cpu->_cycles += 11; break;
    default:
      *(WORD*)(cpu->_mem + cpu->_regs.sp) = cpu->readreg16( (instr[0]&0x30)>>4 );
      cpu->_cycles += 11;
  }
  cpu->MemChanged( cpu->_regs.sp, cpu->_regs.sp + 1 );
}
//---------------------------------------------------------------------------
void Z80::POPQQ( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  switch( instr[0] ) {
    case 0xDD: cpu->_regs.ix = *(WORD*)(cpu->_mem + cpu->_regs.sp); cpu->_cycles += 14; break;
    case 0xFD: cpu->_regs.iy = *(WORD*)(cpu->_mem + cpu->_regs.sp); cpu->_cycles += 14; break;
    case 0xF1: cpu->_regs.u_af[0].af = *(WORD*)(cpu->_mem + cpu->_regs.sp); cpu->_cycles += 10; break;
    default:
      cpu->setreg16( (instr[0]&0x30)>>4, *(WORD*)(cpu->_mem + cpu->_regs.sp) );
      cpu->_cycles += 10;
  }
  cpu->_regs.sp += 2;
}
//---------------------------------------------------------------------------
void Z80::EX( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  WORD temp;
  int addr = -1;
  switch( instr[0] ) {
    case 0x08: temp = cpu->_regs.u_af[1].af; cpu->_regs.u_af[1].af = cpu->_regs.u_af[0].af; cpu->_regs.u_af[0].af = temp; cpu->_cycles += 4; break;
    case 0xD9:
      temp = cpu->_regs.u_bc[1].bc; cpu->_regs.u_bc[1].bc = cpu->_regs.u_bc[0].bc; cpu->_regs.u_bc[0].bc = temp;
      temp = cpu->_regs.u_de[1].de; cpu->_regs.u_de[1].de = cpu->_regs.u_de[0].de; cpu->_regs.u_de[0].de = temp;
      temp = cpu->_regs.u_hl[1].hl; cpu->_regs.u_hl[1].hl = cpu->_regs.u_hl[0].hl; cpu->_regs.u_hl[0].hl = temp;
      cpu->_cycles += 4;
      break;
    case 0xDD: addr = cpu->_regs.sp; temp = *(WORD*)(cpu->_mem + cpu->_regs.sp); *(WORD*)(cpu->_mem + cpu->_regs.sp) = cpu->_regs.ix; cpu->_regs.ix = temp; cpu->_cycles += 23; break;
    case 0xE3: addr = cpu->_regs.sp; temp = *(WORD*)(cpu->_mem + cpu->_regs.sp); *(WORD*)(cpu->_mem + cpu->_regs.sp) = cpu->_regs.u_hl[0].hl; cpu->_regs.u_hl[0].hl = temp; cpu->_cycles += 19; break;
    case 0xEB: temp = cpu->_regs.u_hl[0].hl; cpu->_regs.u_hl[0].hl = cpu->_regs.u_de[0].de; cpu->_regs.u_de[0].de = temp; cpu->_cycles += 4; break;
    case 0xFD: addr = cpu->_regs.sp; temp = *(WORD*)(cpu->_mem + cpu->_regs.sp); *(WORD*)(cpu->_mem + cpu->_regs.sp) = cpu->_regs.iy; cpu->_regs.iy = temp; cpu->_cycles += 23; break;
  }
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::LDI( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_mem[cpu->_regs.u_de[0].de] = cpu->_mem[cpu->_regs.u_hl[0].hl];
  cpu->MemChanged( cpu->_regs.u_de[0].de );
  ++cpu->_regs.u_de[0].de;
  ++cpu->_regs.u_hl[0].hl;
  --cpu->_regs.u_bc[0].bc;
  cpu->_cycles += 16;
  cpu->_regs.u_af[0].s_af.f &= S_SET|Z_SET|C_SET;
  cpu->_regs.u_af[0].s_af.f |= (cpu->_regs.u_bc[0].bc!=0?PV_SET:0);
}
//---------------------------------------------------------------------------
void Z80::LDIR( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  //do {
    LDI( cpu, NULL, opcode );
    if( cpu->_regs.u_bc[0].bc != 0 ) {
      cpu->_cycles += 5;
      cpu->_regs.pc -= 2;   // repeat the instruction
    }
  //} while( cpu->_regs.u_bc[0].bc != 0 );
  cpu->_regs.u_af[0].s_af.f &= S_SET|Z_SET|C_SET;
}
//---------------------------------------------------------------------------
void Z80::LDD( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_mem[cpu->_regs.u_de[0].de] = cpu->_mem[cpu->_regs.u_hl[0].hl];
  cpu->MemChanged( cpu->_regs.u_de[0].de );
  --cpu->_regs.u_de[0].de;
  --cpu->_regs.u_hl[0].hl;
  --cpu->_regs.u_bc[0].bc;
  cpu->_cycles += 16;
  cpu->_regs.u_af[0].s_af.f &= S_SET|Z_SET|C_SET;
  cpu->_regs.u_af[0].s_af.f |= (cpu->_regs.u_bc[0].bc!=0?PV_SET:0);
}
//---------------------------------------------------------------------------
void Z80::LDDR( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  //do {
    LDD( cpu, NULL, opcode );
    if( cpu->_regs.u_bc[0].bc != 0 ) {
      cpu->_cycles += 5;
      cpu->_regs.pc -= 2;   // repeat the instruction
    }
  //} while( cpu->_regs.u_bc[0].bc != 0 );
  cpu->_regs.u_af[0].s_af.f &= S_SET|Z_SET|C_SET;
}
//---------------------------------------------------------------------------
void Z80::CPI( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE origflags = cpu->_regs.u_af[0].s_af.f;
  BYTE temp = sub( cpu->_regs.u_af[0].s_af.a, cpu->_mem[cpu->_regs.u_hl[0].hl], cpu->_regs.u_af[0].s_af.f, false );
  ++cpu->_regs.u_hl[0].hl;
  --cpu->_regs.u_bc[0].bc;
  cpu->_cycles += 16;
  cpu->_regs.u_af[0].s_af.f &= !(PV_SET|C_SET);  // clear PV and C bits
  cpu->_regs.u_af[0].s_af.f |= (cpu->_regs.u_bc[0].bc!=0?PV_SET:0)|(origflags&C_SET);
}
//---------------------------------------------------------------------------
void Z80::CPIR( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  //do {
    CPI( cpu, NULL, opcode );
    if( !(cpu->_regs.u_af[0].s_af.f & Z_SET) && cpu->_regs.u_bc[0].bc != 0 ) {
      cpu->_cycles += 5;
      cpu->_regs.pc -= 2;   // repeat the instruction
    }
  //} while( !(cpu->_regs.u_af[0].s_af.f & Z_SET) && cpu->_regs.u_bc[0].bc != 0 );
}
//---------------------------------------------------------------------------
void Z80::CPD( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE origflags = cpu->_regs.u_af[0].s_af.f;
  BYTE temp = sub( cpu->_regs.u_af[0].s_af.a, cpu->_mem[cpu->_regs.u_hl[0].hl], cpu->_regs.u_af[0].s_af.f, false );
  ++cpu->_regs.u_de[0].de;
  ++cpu->_regs.u_hl[0].hl;
  --cpu->_regs.u_bc[0].bc;
  cpu->_cycles += 16;
  cpu->_regs.u_af[0].s_af.f &= !(PV_SET|C_SET);  // clear PV and C bits
  cpu->_regs.u_af[0].s_af.f |= (cpu->_regs.u_bc[0].bc!=0?PV_SET:0)|(origflags&C_SET);
}
//---------------------------------------------------------------------------
void Z80::CPDR( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  //do {
    CPD( cpu, NULL, opcode );
    if( !(cpu->_regs.u_af[0].s_af.f & Z_SET) && cpu->_regs.u_bc[0].bc != 0 ) {
      cpu->_cycles += 5;
      cpu->_regs.pc -= 2;   // repeat the instruction
    }
  //} while( !(cpu->_regs.u_af[0].s_af.f & Z_SET) && cpu->_regs.u_bc[0].bc != 0 );
}
//---------------------------------------------------------------------------
void Z80::ADD8( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b;
  switch( instr[0] ) {
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x85:
    case 0x87: b = cpu->readreg8( instr[0] & 0x7 ); cpu->_cycles += 4; break;
    case 0x86: b = cpu->_mem[cpu->_regs.u_hl[0].hl]; cpu->_cycles += 7; break;
    case 0xC6: b = instr[1]; cpu->_cycles += 7; break;
    case 0xDD: b = cpu->_mem[cpu->_regs.ix+(signed char)instr[2]]; cpu->_cycles += 16; break;
    case 0xFD: b = cpu->_mem[cpu->_regs.iy+(signed char)instr[2]]; cpu->_cycles += 16; break;
  }
  cpu->_regs.u_af[0].s_af.a = add( cpu->_regs.u_af[0].s_af.a, b, cpu->_regs.u_af[0].s_af.f, false );
}
//---------------------------------------------------------------------------
void Z80::ADC8( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b;
  switch( instr[0] ) {
    case 0x88:
    case 0x89:
    case 0x8A:
    case 0x8B:
    case 0x8C:
    case 0x8D:
    case 0x8F: b = cpu->readreg8( instr[0] & 0x7 ); cpu->_cycles += 4; break;
    case 0x8E: b = cpu->_mem[cpu->_regs.u_hl[0].hl]; cpu->_cycles += 7; break;
    case 0xCE: b = instr[1]; cpu->_cycles += 7; break;
    case 0xDD: b = cpu->_mem[cpu->_regs.ix+(signed char)instr[2]]; cpu->_cycles += 19; break;
    case 0xFD: b = cpu->_mem[cpu->_regs.iy+(signed char)instr[2]]; cpu->_cycles += 19; break;
  }
  cpu->_regs.u_af[0].s_af.a = add( cpu->_regs.u_af[0].s_af.a, b, cpu->_regs.u_af[0].s_af.f, cpu->_regs.u_af[0].s_af.f & C_SET );
}
//---------------------------------------------------------------------------
void Z80::SUB8( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b;
  switch( instr[0] ) {
    case 0x90:
    case 0x91:
    case 0x92:
    case 0x93:
    case 0x94:
    case 0x95:
    case 0x97: b = cpu->readreg8( instr[0] & 0x7 ); cpu->_cycles += 4; break;
    case 0x96: b = cpu->_mem[cpu->_regs.u_hl[0].hl]; cpu->_cycles += 7; break;
    case 0xD6: b = instr[1]; cpu->_cycles += 7; break;
    case 0xDD: b = cpu->_mem[cpu->_regs.ix+(signed char)instr[2]]; cpu->_cycles += 19; break;
    case 0xFD: b = cpu->_mem[cpu->_regs.iy+(signed char)instr[2]]; cpu->_cycles += 19; break;
  }
  cpu->_regs.u_af[0].s_af.a = sub( cpu->_regs.u_af[0].s_af.a, b, cpu->_regs.u_af[0].s_af.f, false );
}
//---------------------------------------------------------------------------
void Z80::SBC8( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b;
  switch( instr[0] ) {
    case 0x98:
    case 0x99:
    case 0x9A:
    case 0x9B:
    case 0x9C:
    case 0x9D:
    case 0x9F: b = cpu->readreg8( instr[0] & 0x7 ); ++cpu->_cycles; break;
    case 0x9E: b = cpu->_mem[cpu->_regs.u_hl[0].hl]; cpu->_cycles += 2; break;
    case 0xDE: b = instr[1]; cpu->_cycles += 2; break;
    case 0xDD: b = cpu->_mem[cpu->_regs.ix+(signed char)instr[2]]; cpu->_cycles += 5; break;
    case 0xFD: b = cpu->_mem[cpu->_regs.iy+(signed char)instr[2]]; cpu->_cycles += 5; break;
  }
  cpu->_regs.u_af[0].s_af.a = sub( cpu->_regs.u_af[0].s_af.a, b, cpu->_regs.u_af[0].s_af.f, cpu->_regs.u_af[0].s_af.f & C_SET );
}
//---------------------------------------------------------------------------
void Z80::AND8( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b;
  switch( instr[0] ) {
    case 0xA0:
    case 0xA1:
    case 0xA2:
    case 0xA3:
    case 0xA4:
    case 0xA5:
    case 0xA7: b = cpu->readreg8( instr[0] & 0x7 ); cpu->_cycles += 4; break;
    case 0xA6: b = cpu->_mem[cpu->_regs.u_hl[0].hl]; cpu->_cycles += 7; break;
    case 0xDD: b = cpu->_mem[cpu->_regs.ix+(signed char)instr[2]]; cpu->_cycles += 19; break;
    case 0xE6: b = instr[1]; cpu->_cycles += 7; break;
    case 0xFD: b = cpu->_mem[cpu->_regs.iy+(signed char)instr[2]]; cpu->_cycles += 19; break;
  }
  b = (cpu->_regs.u_af[0].s_af.a &= b);
  cpu->_regs.u_af[0].s_af.f = 0;
  cpu->_regs.u_af[0].s_af.f |= (b&0x80?S_SET:0)|(b==0?Z_SET:0)|parity(b);
}
//---------------------------------------------------------------------------
void Z80::OR8( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b;
  switch( instr[0] ) {
    case 0xB0:
    case 0xB1:
    case 0xB2:
    case 0xB3:
    case 0xB4:
    case 0xB5:
    case 0xB7: b = cpu->readreg8( instr[0] & 0x7 ); cpu->_cycles += 4; break;
    case 0xB6: b = cpu->_mem[cpu->_regs.u_hl[0].hl]; cpu->_cycles += 7; break;
    case 0xDD: b = cpu->_mem[cpu->_regs.ix+(signed char)instr[2]]; cpu->_cycles += 19; break;
    case 0xF6: b = instr[1]; cpu->_cycles += 7; break;
    case 0xFD: b = cpu->_mem[cpu->_regs.iy+(signed char)instr[2]]; cpu->_cycles += 19; break;
  }
  b = (cpu->_regs.u_af[0].s_af.a |= b);
  cpu->_regs.u_af[0].s_af.f = 0;
  cpu->_regs.u_af[0].s_af.f |= (b&0x80?S_SET:0)|(b==0?Z_SET:0)|parity(b);
}
//---------------------------------------------------------------------------
void Z80::XOR8( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b;
  switch( instr[0] ) {
    case 0xA8:
    case 0xA9:
    case 0xAA:
    case 0xAB:
    case 0xAC:
    case 0xAD:
    case 0xAF: b = cpu->readreg8( instr[0] & 0x7 ); cpu->_cycles += 4; break;
    case 0xAE: b = cpu->_mem[cpu->_regs.u_hl[0].hl]; cpu->_cycles += 7; break;
    case 0xDD: b = cpu->_mem[cpu->_regs.ix+(signed char)instr[2]]; cpu->_cycles += 19; break;
    case 0xEE: b = instr[1]; cpu->_cycles += 7; break;
    case 0xFD: b = cpu->_mem[cpu->_regs.iy+(signed char)instr[2]]; cpu->_cycles += 19; break;
  }
  b = (cpu->_regs.u_af[0].s_af.a ^= b);
  cpu->_regs.u_af[0].s_af.f = 0;
  cpu->_regs.u_af[0].s_af.f |= (b&0x80?S_SET:0)|(b==0?Z_SET:0)|parity(b);
}
//---------------------------------------------------------------------------
void Z80::CP( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b;
  switch( instr[0] ) {
    case 0xB8:
    case 0xB9:
    case 0xBA:
    case 0xBB:
    case 0xBC:
    case 0xBD:
    case 0xBF: b = cpu->readreg8( instr[0] & 0x7 ); cpu->_cycles += 4; break;
    case 0xBE: b = cpu->_mem[cpu->_regs.u_hl[0].hl]; cpu->_cycles += 7; break;
    case 0xDD: b = cpu->_mem[cpu->_regs.ix+(signed char)instr[2]]; cpu->_cycles += 19; break;
    case 0xFE: b = instr[1]; cpu->_cycles += 7; break;
    case 0xFD: b = cpu->_mem[cpu->_regs.iy+(signed char)instr[2]]; cpu->_cycles += 19; break;
  }
  sub( cpu->_regs.u_af[0].s_af.a, b, cpu->_regs.u_af[0].s_af.f, false );
}
//---------------------------------------------------------------------------
void Z80::INC8( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b;
  cpu->_cycles += 4;
  int addr = -1;
  switch( instr[0] ) {
    case 0x04: b = ++cpu->_regs.u_bc[0].s_bc.b; break;
    case 0x0C: b = ++cpu->_regs.u_bc[0].s_bc.c; break;
    case 0x14: b = ++cpu->_regs.u_de[0].s_de.d; break;
    case 0x1C: b = ++cpu->_regs.u_de[0].s_de.e; break;
    case 0x24: b = ++cpu->_regs.u_hl[0].s_hl.h; break;
    case 0x2C: b = ++cpu->_regs.u_hl[0].s_hl.l; break;
    case 0x3C: b = ++cpu->_regs.u_af[0].s_af.a; break;
    case 0x34: b = ++cpu->_mem[addr = cpu->_regs.u_hl[0].hl]; cpu->_cycles += 7; break;
    case 0xDD: b = ++cpu->_mem[addr = (cpu->_regs.ix+(signed char)instr[2])]; cpu->_cycles += 19; break;
    case 0xFD: b = ++cpu->_mem[addr = (cpu->_regs.iy+(signed char)instr[2])]; cpu->_cycles += 19; break;
  }
  cpu->_regs.u_af[0].s_af.f &= C_SET;
  cpu->_regs.u_af[0].s_af.f |= (b&0x80?S_SET:0)|(b==0?Z_SET:0)|(b&0xF==0?H_SET:0)|(b==0x80?PV_SET:0);
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::DEC8( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b;
  cpu->_cycles += 4;
  int addr;
  switch( instr[0] ) {
    case 0x05: b = --cpu->_regs.u_bc[0].s_bc.b; break;
    case 0x0D: b = --cpu->_regs.u_bc[0].s_bc.c; break;
    case 0x15: b = --cpu->_regs.u_de[0].s_de.d; break;
    case 0x1D: b = --cpu->_regs.u_de[0].s_de.e; break;
    case 0x25: b = --cpu->_regs.u_hl[0].s_hl.h; break;
    case 0x2D: b = --cpu->_regs.u_hl[0].s_hl.l; break;
    case 0x3D: b = --cpu->_regs.u_af[0].s_af.a; break;
    case 0x35: b = --cpu->_mem[addr = cpu->_regs.u_hl[0].hl]; cpu->_cycles += 7; break;
    case 0xDD: b = --cpu->_mem[addr = (cpu->_regs.ix+(signed char)instr[2])]; cpu->_cycles += 19; break;
    case 0xFD: b = --cpu->_mem[addr = (cpu->_regs.iy+(signed char)instr[2])]; cpu->_cycles += 19; break;
  }
  cpu->_regs.u_af[0].s_af.f &= C_SET;
  cpu->_regs.u_af[0].s_af.f |= (b&0x80?S_SET:0)|(b==0?Z_SET:0)|(b&0xF==0?H_SET:0)|(b==0x7F?PV_SET:0)|N_SET;
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::DAA( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE low = (cpu->_regs.u_af[0].s_af.a & 0xF),
       high = (cpu->_regs.u_af[0].s_af.a >> 4),
       flags = cpu->_regs.u_af[0].s_af.f,
       b;
  if( flags & N_SET ) {
    if( !(flags&C_SET) && high <= 8 && (flags&H_SET) && low >= 6 ) {
      b = add( cpu->_regs.u_af[0].s_af.a, 0xFA, flags, false );
    } else if( (flags&C_SET) && high >= 7 && !(flags&H_SET) && low <= 9 ) {
      b = add( cpu->_regs.u_af[0].s_af.a, 0xA0, flags, false );
    } else if( (flags&C_SET) && (high == 6||high == 7) && (flags&H_SET) && low >= 6 ) {
      b = add( cpu->_regs.u_af[0].s_af.a, 0x9A, flags, false );
    }
    flags |= N_SET;
  } else {
    if( !(flags&C_SET) && ((high <= 8 && !(flags&H_SET) && low >= 10)||(high <= 9 && (flags&H_SET) && low <=3)) ) {
      b = add( cpu->_regs.u_af[0].s_af.a, 0x6, flags, false );
    } else if( low <= 9 && !(flags&H_SET) && ((high <= 2 && (flags&C_SET))||((high >= 10) && !(flags&C_SET))) ) {
      b = add( cpu->_regs.u_af[0].s_af.a, 0x60, flags, false );
    } else if( (low >= 10 && !(flags&H_SET) && ((high >= 9 && !(flags&C_SET))||(high <= 2 && (flags&C_SET)))) ||
               (low <= 3 && (flags&H_SET) && ((high >= 10 && !(flags&C_SET))||(high <= 3 && (flags&C_SET)))) ) {
      b = add( cpu->_regs.u_af[0].s_af.a, 0x66, flags, false );
    }
    flags &= ~N_SET;
  }
  cpu->_cycles += 4;
}
//---------------------------------------------------------------------------
void Z80::CPL( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_regs.u_af[0].s_af.a = ~cpu->_regs.u_af[0].s_af.a;
  cpu->_regs.u_af[0].s_af.f |= (H_SET|N_SET);
  cpu->_cycles += 4;
}
//---------------------------------------------------------------------------
void Z80::NEG( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  if( cpu->_regs.u_af[0].s_af.a == 0x80 ) {
    cpu->_regs.u_af[0].s_af.f = (S_SET|PV_SET|N_SET|C_SET);
  } else if( cpu->_regs.u_af[0].s_af.a == 0 ) {
    cpu->_regs.u_af[0].s_af.f = (Z_SET|N_SET);
  } else {
    cpu->_regs.u_af[0].s_af.a = sub( 0, cpu->_regs.u_af[0].s_af.a, cpu->_regs.u_af[0].s_af.f, false );
    cpu->_regs.u_af[0].s_af.f &= ~PV_SET;
    cpu->_regs.u_af[0].s_af.f |= C_SET;
  }
  cpu->_cycles += 8;
}
//---------------------------------------------------------------------------
void Z80::CCF( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE *flags;
  cpu->_cycles += 4;
  flags = &cpu->_regs.u_af[0].s_af.f;
  *flags &= ~(H_SET|N_SET);
  *flags |= (*flags&C_SET?H_SET:0);
  *flags ^= C_SET;
}
//---------------------------------------------------------------------------
void Z80::SCF( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE *flags;
  cpu->_cycles += 4;
  flags = &cpu->_regs.u_af[0].s_af.f;
  *flags &= ~(H_SET|N_SET);
  *flags |= C_SET;
}
//---------------------------------------------------------------------------
void Z80::HALT( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_halted = true;
  if( !cpu->_interruptpending ) {
    cpu->_regs.pc--;
  }
  cpu->_cycles += 4;
}
//---------------------------------------------------------------------------
void Z80::DI( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 4;
  cpu->_regs.iff1 = cpu->_regs.iff2 = false;
}
//---------------------------------------------------------------------------
void Z80::EI( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 4;
  cpu->_interruptenabledelay = true;
  // cpu->_regs.iff1 = cpu->_regs.iff2 = true;
}
//---------------------------------------------------------------------------
void Z80::IM( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 8;
  switch( instr[1] ) {
    case 0x46: cpu->_interruptmode = 0; break;
    case 0x56: cpu->_interruptmode = 1; break;
    case 0x5E: cpu->_interruptmode = 2; break;
  }
}
//---------------------------------------------------------------------------
void Z80::ADD16( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  WORD *w1, w2, result;
  bool half, carry = false, subtr = false;
  cpu->_cycles += 11;
  switch( instr[0] ) {
    case 0x09: w1 = &cpu->_regs.u_hl[0].hl; w2 = cpu->_regs.u_bc[0].bc; break;
    case 0x19: w1 = &cpu->_regs.u_hl[0].hl; w2 = cpu->_regs.u_de[0].de; break;
    case 0x29: w1 = &cpu->_regs.u_hl[0].hl; w2 = *w1; break;
    case 0x39: w1 = &cpu->_regs.u_hl[0].hl; w2 = cpu->_regs.sp; break;
    case 0xDD:
      cpu->_cycles += 4;
      switch( instr[1] ) {
        case 0x09: w1 = &cpu->_regs.ix; w2 = cpu->_regs.u_bc[0].bc; break;
        case 0x19: w1 = &cpu->_regs.ix; w2 = cpu->_regs.u_de[0].de; break;
        case 0x29: w1 = &cpu->_regs.ix; w2 = *w1; break;
        case 0x39: w1 = &cpu->_regs.ix; w2 = cpu->_regs.sp; break;
      }
      break;
    case 0xFD:
      cpu->_cycles += 4;
      switch( instr[1] ) {
        case 0x09: w1 = &cpu->_regs.iy; w2 = cpu->_regs.u_bc[0].bc; break;
        case 0x19: w1 = &cpu->_regs.iy; w2 = cpu->_regs.u_de[0].de; break;
        case 0x29: w1 = &cpu->_regs.iy; w2 = *w1; break;
        case 0x39: w1 = &cpu->_regs.iy; w2 = cpu->_regs.sp; break;
      }
    case 0xED:
      cpu->_cycles += 4;
      carry = cpu->_regs.u_af[0].s_af.f&C_SET;
      switch( instr[1] ) {
        case 0x42: subtr = true;
        case 0x4A: w1 = &cpu->_regs.u_hl[0].hl; w2 = cpu->_regs.u_bc[0].bc; break;
        case 0x52: subtr = true;
        case 0x5A: w1 = &cpu->_regs.u_hl[0].hl; w2 = cpu->_regs.u_de[0].de; break;
        case 0x62: subtr = true;
        case 0x6A: w1 = &cpu->_regs.u_hl[0].hl; w2 = *w1; break;
        case 0x72: subtr = true;
        case 0x7A: w1 = &cpu->_regs.u_hl[0].hl; w2 = cpu->_regs.sp; break;
      }
  }
  if( subtr ) {
    result = (WORD)sub( *w1 & 0xFF, w2 & 0xFF, cpu->_regs.u_af[0].s_af.f, carry );
    result |= (sub( *w1 >> 8, w2 >> 8, cpu->_regs.u_af[0].s_af.f, half = (cpu->_regs.u_af[0].s_af.f&C_SET))<<8);
  } else {
    result = (WORD)add( *w1 & 0xFF, w2 & 0xFF, cpu->_regs.u_af[0].s_af.f, carry );
    result |= (add( *w1 >> 8, w2 >> 8, cpu->_regs.u_af[0].s_af.f, half = (cpu->_regs.u_af[0].s_af.f&C_SET))<<8);
  }
  *w1 = result;
  BYTE save = cpu->_regs.u_af[0].s_af.f;
  cpu->_regs.u_af[0].s_af.f &= ~Z_SET;
  if( result == 0 ) cpu->_regs.u_af[0].s_af.f |= Z_SET;
  if( cpu->_regs.u_af[0].s_af.f != save ) {
    //cpu->pause();
  }
  if( half ) cpu->_regs.u_af[0].s_af.f |= H_SET;
}
//---------------------------------------------------------------------------
void Z80::INC16( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 6;
  switch( instr[0] ) {
    case 0x03: ++cpu->_regs.u_bc[0].bc; break;
    case 0x13: ++cpu->_regs.u_de[0].de; break;
    case 0x23: ++cpu->_regs.u_hl[0].hl; break;
    case 0x33: ++cpu->_regs.sp; break;
    case 0xDD: ++cpu->_regs.ix; cpu->_cycles += 4; break;
    case 0xFD: ++cpu->_regs.iy; cpu->_cycles += 4; break;
  }
}
//---------------------------------------------------------------------------
void Z80::DEC16( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 6;
  switch( instr[0] ) {
    case 0x0B: --cpu->_regs.u_bc[0].bc; break;
    case 0x1B: --cpu->_regs.u_de[0].de; break;
    case 0x2B: --cpu->_regs.u_hl[0].hl; break;
    case 0x3B: --cpu->_regs.sp; break;
    case 0xDD: --cpu->_regs.ix; cpu->_cycles += 4; break;
    case 0xFD: --cpu->_regs.iy; cpu->_cycles += 4; break;
  }
}
//---------------------------------------------------------------------------
void Z80::RxCA( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 4;
  bool carry;
  switch( instr[0] ) {
    case 0x07: carry = cpu->_regs.u_af[0].s_af.a & 0x80; cpu->_regs.u_af[0].s_af.a <<= 1; if( carry ) cpu->_regs.u_af[0].s_af.a |= 0x01; break;
    case 0x0F: carry = cpu->_regs.u_af[0].s_af.a & 0x01; cpu->_regs.u_af[0].s_af.a >>= 1; if( carry ) cpu->_regs.u_af[0].s_af.a |= 0x80; break;
  }
  cpu->_regs.u_af[0].s_af.f &= (S_SET|Z_SET|PV_SET);
  if( carry ) cpu->_regs.u_af[0].s_af.f |= C_SET;
}
//---------------------------------------------------------------------------
void Z80::RxA( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 4;
  bool carry;
  switch( instr[0] ) {
    case 0x17: carry = cpu->_regs.u_af[0].s_af.a & 0x80; cpu->_regs.u_af[0].s_af.a <<= 1; if( cpu->_regs.u_af[0].s_af.f & C_SET ) cpu->_regs.u_af[0].s_af.a |= 0x01; break;
    case 0x1F: carry = cpu->_regs.u_af[0].s_af.a & 0x01; cpu->_regs.u_af[0].s_af.a >>= 1; if( cpu->_regs.u_af[0].s_af.f & C_SET ) cpu->_regs.u_af[0].s_af.a |= 0x80; break;
  }
  cpu->_regs.u_af[0].s_af.f &= (S_SET|Z_SET|PV_SET);
  if( carry ) cpu->_regs.u_af[0].s_af.f |= C_SET;
}
//---------------------------------------------------------------------------
void Z80::DDCB( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE pseudo[5];
  REGISTERS save;
  memcpy( &save, &cpu->_regs, sizeof( REGISTERS ) );
  pseudo[1] = rbHL;
  cpu->_regs.u_hl[0].hl = cpu->_regs.ix + (signed char)instr[2];
  cpu->_cycles += 4;
  switch( instr[3] ) {
    case 0x06: RLC( cpu, pseudo, opcode ); break;
    case 0x0E: RRC( cpu, pseudo, opcode ); break;
    case 0x16: RL( cpu, pseudo, opcode ); break;
    case 0x1E: RR( cpu, pseudo, opcode ); break;
    case 0x26: SLA( cpu, pseudo, opcode ); break;
    case 0x2E: SRA( cpu, pseudo, opcode ); break;
    case 0x3E: SRL( cpu, pseudo, opcode ); break;
    case 0x46:
    case 0x4E:
    case 0x56:
    case 0x5E:
    case 0x66:
    case 0x6E:
    case 0x76:
    case 0x7E: pseudo[1] |= (instr[3]&0x38); BIT( cpu, pseudo, opcode ); break;
    case 0xC6:
    case 0xCE:
    case 0xD6:
    case 0xDE:
    case 0xE6:
    case 0xEE:
    case 0xF6:
    case 0xFE: pseudo[1] |= (instr[3]&0x38); SET( cpu, pseudo, opcode ); break;
  }
  cpu->_regs.u_hl[0].hl = save.u_hl[0].hl;
}
//---------------------------------------------------------------------------
void Z80::FDCB( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE pseudo[5];
  REGISTERS save;
  memcpy( &save, &cpu->_regs, sizeof( REGISTERS ) );
  pseudo[1] = rbHL;
  cpu->_regs.u_hl[0].hl = cpu->_regs.iy + (signed char)instr[2];
  cpu->_cycles += 8;
  switch( instr[3] ) {
    case 0x06: RLC( cpu, pseudo, opcode ); break;
    case 0x0E: RRC( cpu, pseudo, opcode ); break;
    case 0x16: RL( cpu, pseudo, opcode ); break;
    case 0x1E: RR( cpu, pseudo, opcode ); break;
    case 0x26: SLA( cpu, pseudo, opcode ); break;
    case 0x2E: SRA( cpu, pseudo, opcode ); break;
    case 0x3E: SRL( cpu, pseudo, opcode ); break;
    case 0x46:
    case 0x4E:
    case 0x56:
    case 0x5E:
    case 0x66:
    case 0x6E:
    case 0x76:
    case 0x7E: pseudo[1] |= (instr[3]&0x38); BIT( cpu, pseudo, opcode ); break;
    case 0x86:
    case 0x8E:
    case 0x96:
    case 0x9E:
    case 0xA6:
    case 0xAE:
    case 0xB6:
    case 0xBE: pseudo[1] |= (instr[3]&0x38); RES( cpu, pseudo, opcode ); break;
    case 0xC6:
    case 0xCE:
    case 0xD6:
    case 0xDE:
    case 0xE6:
    case 0xEE:
    case 0xF6:
    case 0xFE: pseudo[1] |= (instr[3]&0x38); SET( cpu, pseudo, opcode ); break;
  }
  cpu->_regs.u_hl[0].hl = save.u_hl[0].hl;
}
//---------------------------------------------------------------------------
void Z80::RLC( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE *b;
  bool bit7;
  int addr = -1;
  cpu->_cycles += 8;
  switch( instr[1] & 0x07 ) {
    case rbA:  b = &cpu->_regs.u_af[0].s_af.a; break;
    case rbB:  b = &cpu->_regs.u_bc[0].s_bc.b; break;
    case rbC:  b = &cpu->_regs.u_bc[0].s_bc.c; break;
    case rbD:  b = &cpu->_regs.u_de[0].s_de.d; break;
    case rbE:  b = &cpu->_regs.u_de[0].s_de.e; break;
    case rbH:  b = &cpu->_regs.u_hl[0].s_hl.h; break;
    case rbL:  b = &cpu->_regs.u_hl[0].s_hl.l; break;
    case rbHL: b = cpu->_mem + (addr = cpu->_regs.u_hl[0].hl); cpu->_cycles += 7; break;
  }
  bit7 = *b & 0x80;
  *b <<= 1;
  if( bit7 ) *b |= 0x01;
  cpu->_regs.u_af[0].s_af.f = (*b&0x80?S_SET:0)|(*b==0?Z_SET:0)|parity(*b)|(bit7?C_SET:0);
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::RL( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE *b;
  bool bit7;
  int addr = -1;
  cpu->_cycles += 8;
  switch( instr[1] & 0x7 ) {
    case rbA:  b = &cpu->_regs.u_af[0].s_af.a; break;
    case rbB:  b = &cpu->_regs.u_bc[0].s_bc.b; break;
    case rbC:  b = &cpu->_regs.u_bc[0].s_bc.c; break;
    case rbD:  b = &cpu->_regs.u_de[0].s_de.d; break;
    case rbE:  b = &cpu->_regs.u_de[0].s_de.e; break;
    case rbH:  b = &cpu->_regs.u_hl[0].s_hl.h; break;
    case rbL:  b = &cpu->_regs.u_hl[0].s_hl.l; break;
    case rbHL: b = cpu->_mem + (addr = cpu->_regs.u_hl[0].hl); cpu->_cycles += 7; break;
  }
  bit7 = *b & 0x80;
  *b <<= 1;
  if( cpu->_regs.u_af[0].s_af.f & C_SET ) *b |= 0x01;
  cpu->_regs.u_af[0].s_af.f = (*b&0x80?S_SET:0)|(*b==0?Z_SET:0)|parity(*b)|(bit7?C_SET:0);
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::RRC( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE *b;
  bool bit0;
  int addr = -1;
  cpu->_cycles += 8;
  switch( instr[1] & 0x07 ) {
    case rbA:  b = &cpu->_regs.u_af[0].s_af.a; break;
    case rbB:  b = &cpu->_regs.u_bc[0].s_bc.b; break;
    case rbC:  b = &cpu->_regs.u_bc[0].s_bc.c; break;
    case rbD:  b = &cpu->_regs.u_de[0].s_de.d; break;
    case rbE:  b = &cpu->_regs.u_de[0].s_de.e; break;
    case rbH:  b = &cpu->_regs.u_hl[0].s_hl.h; break;
    case rbL:  b = &cpu->_regs.u_hl[0].s_hl.l; break;
    case rbHL: b = cpu->_mem + (addr = cpu->_regs.u_hl[0].hl); cpu->_cycles += 7; break;
  }
  bit0 = *b & 0x01;
  *b >>= 1;
  if( bit0 ) *b |= 0x80;
  cpu->_regs.u_af[0].s_af.f = (*b&0x80?S_SET:0)|(*b==0?Z_SET:0)|parity(*b)|(bit0?C_SET:0);
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::RR( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE *b;
  bool bit0;
  int addr = -1;
  cpu->_cycles += 8;
  switch( instr[1] & 0x7 ) {
    case rbA:  b = &cpu->_regs.u_af[0].s_af.a; break;
    case rbB:  b = &cpu->_regs.u_bc[0].s_bc.b; break;
    case rbC:  b = &cpu->_regs.u_bc[0].s_bc.c; break;
    case rbD:  b = &cpu->_regs.u_de[0].s_de.d; break;
    case rbE:  b = &cpu->_regs.u_de[0].s_de.e; break;
    case rbH:  b = &cpu->_regs.u_hl[0].s_hl.h; break;
    case rbL:  b = &cpu->_regs.u_hl[0].s_hl.l; break;
    case rbHL: b = cpu->_mem + (addr = cpu->_regs.u_hl[0].hl); cpu->_cycles += 7; break;
  }
  bit0 = *b & 0x01;
  *b >>= 1;
  if( cpu->_regs.u_af[0].s_af.f & C_SET ) *b |= 0x80;
  cpu->_regs.u_af[0].s_af.f = (*b&0x80?S_SET:0)|(*b==0?Z_SET:0)|parity(*b)|(bit0?C_SET:0);
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::SLA( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE *b;
  bool bit7;
  int addr = -1;
  cpu->_cycles += 8;
  switch( instr[1] & 0x07 ) {
    case rbA:  b = &cpu->_regs.u_af[0].s_af.a; break;
    case rbB:  b = &cpu->_regs.u_bc[0].s_bc.b; break;
    case rbC:  b = &cpu->_regs.u_bc[0].s_bc.c; break;
    case rbD:  b = &cpu->_regs.u_de[0].s_de.d; break;
    case rbE:  b = &cpu->_regs.u_de[0].s_de.e; break;
    case rbH:  b = &cpu->_regs.u_hl[0].s_hl.h; break;
    case rbL:  b = &cpu->_regs.u_hl[0].s_hl.l; break;
    case rbHL: b = cpu->_mem + (addr = cpu->_regs.u_hl[0].hl); cpu->_cycles += 7; break;
  }
  bit7 = *b & 0x80;
  *b <<= 1;
  cpu->_regs.u_af[0].s_af.f = (*b&0x80?S_SET:0)|(*b==0?Z_SET:0)|parity(*b)|(bit7?C_SET:0);
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::SRA( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE *b;
  bool bit0;
  int addr = -1;
  cpu->_cycles += 8;
  switch( instr[1] & 0x07 ) {
    case rbA:  b = &cpu->_regs.u_af[0].s_af.a; break;
    case rbB:  b = &cpu->_regs.u_bc[0].s_bc.b; break;
    case rbC:  b = &cpu->_regs.u_bc[0].s_bc.c; break;
    case rbD:  b = &cpu->_regs.u_de[0].s_de.d; break;
    case rbE:  b = &cpu->_regs.u_de[0].s_de.e; break;
    case rbH:  b = &cpu->_regs.u_hl[0].s_hl.h; break;
    case rbL:  b = &cpu->_regs.u_hl[0].s_hl.l; break;
    case rbHL: b = cpu->_mem + (addr = cpu->_regs.u_hl[0].hl); cpu->_cycles += 7; break;
  }
  bit0 = *b & 0x01;
  *b >>= 1;
  if( *b & 0x40 ) *b |= 0x80;
  cpu->_regs.u_af[0].s_af.f = (*b&0x80?S_SET:0)|(*b==0?Z_SET:0)|parity(*b)|(bit0?C_SET:0);
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::SRL( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE *b;
  bool bit0;
  int addr = -1;
  cpu->_cycles += 8;
  switch( instr[1] & 0x07 ) {
    case rbA:  b = &cpu->_regs.u_af[0].s_af.a; break;
    case rbB:  b = &cpu->_regs.u_bc[0].s_bc.b; break;
    case rbC:  b = &cpu->_regs.u_bc[0].s_bc.c; break;
    case rbD:  b = &cpu->_regs.u_de[0].s_de.d; break;
    case rbE:  b = &cpu->_regs.u_de[0].s_de.e; break;
    case rbH:  b = &cpu->_regs.u_hl[0].s_hl.h; break;
    case rbL:  b = &cpu->_regs.u_hl[0].s_hl.l; break;
    case rbHL: b = cpu->_mem + (addr = cpu->_regs.u_hl[0].hl); cpu->_cycles += 7; break;
  }
  bit0 = *b & 0x01;
  *b >>= 1;
  cpu->_regs.u_af[0].s_af.f = (*b&0x80?S_SET:0)|(*b==0?Z_SET:0)|parity(*b)|(bit0?C_SET:0);
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::RxD( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b = cpu->_mem[cpu->_regs.u_hl[0].hl];
  cpu->_cycles += 18;
  if( instr[1] == 0x6F ) {
    cpu->_mem[cpu->_regs.u_hl[0].hl] <<= 4;
    cpu->_mem[cpu->_regs.u_hl[0].hl] |= ( cpu->_regs.u_af[0].s_af.a & 0xF );
    cpu->_regs.u_af[0].s_af.a &= 0xF0;
    cpu->_regs.u_af[0].s_af.a |= ( b >> 4 );
  } else {
    cpu->_mem[cpu->_regs.u_hl[0].hl] >>= 4;
    cpu->_mem[cpu->_regs.u_hl[0].hl] |= ( cpu->_regs.u_af[0].s_af.a << 4 );
    cpu->_regs.u_af[0].s_af.a &= 0xF0;
    cpu->_regs.u_af[0].s_af.a |= ( b & 0xF );
  }
  cpu->_regs.u_af[0].s_af.f &= C_SET;
  b = cpu->_regs.u_af[0].s_af.a;
  cpu->_regs.u_af[0].s_af.a |= (b&0x80?S_SET:0)|(b==0?Z_SET:0)|parity(b);
  cpu->MemChanged( cpu->_regs.u_hl[0].hl );
}
//---------------------------------------------------------------------------
void Z80::BIT( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE mask = 0x01 << ( ( instr[1] >> 3 ) & 0x7 );
  BYTE *b;
  int addr = -1;
  cpu->_cycles += 8;
  switch( instr[1] & 0x07 ) {
    case rbA:  b = &cpu->_regs.u_af[0].s_af.a; break;
    case rbB:  b = &cpu->_regs.u_bc[0].s_bc.b; break;
    case rbC:  b = &cpu->_regs.u_bc[0].s_bc.c; break;
    case rbD:  b = &cpu->_regs.u_de[0].s_de.d; break;
    case rbE:  b = &cpu->_regs.u_de[0].s_de.e; break;
    case rbH:  b = &cpu->_regs.u_hl[0].s_hl.h; break;
    case rbL:  b = &cpu->_regs.u_hl[0].s_hl.l; break;
    case rbHL: b = cpu->_mem + (addr = cpu->_regs.u_hl[0].hl); cpu->_cycles += 4; break;
  }
  cpu->_regs.u_af[0].s_af.f &= C_SET;
  cpu->_regs.u_af[0].s_af.f |= (*b&mask?0:Z_SET)|H_SET;
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::SET( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE mask = 0x01 << ( ( instr[1] >> 3 ) & 0x7 );
  BYTE *b;
  int addr = -1;
  cpu->_cycles += 8;
  switch( instr[1] & 0x07 ) {
    case rbA:  b = &cpu->_regs.u_af[0].s_af.a; break;
    case rbB:  b = &cpu->_regs.u_bc[0].s_bc.b; break;
    case rbC:  b = &cpu->_regs.u_bc[0].s_bc.c; break;
    case rbD:  b = &cpu->_regs.u_de[0].s_de.d; break;
    case rbE:  b = &cpu->_regs.u_de[0].s_de.e; break;
    case rbH:  b = &cpu->_regs.u_hl[0].s_hl.h; break;
    case rbL:  b = &cpu->_regs.u_hl[0].s_hl.l; break;
    case rbHL: b = cpu->_mem + (addr = cpu->_regs.u_hl[0].hl); cpu->_cycles += 7; break;
  }
  *b |= mask;
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::RES( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE mask = 0x01 << ( ( instr[1] >> 3 ) & 0x7 );
  BYTE *b;
  int addr = -1;
  cpu->_cycles += 8;
  switch( instr[1] & 0x07 ) {
    case rbA:  b = &cpu->_regs.u_af[0].s_af.a; break;
    case rbB:  b = &cpu->_regs.u_bc[0].s_bc.b; break;
    case rbC:  b = &cpu->_regs.u_bc[0].s_bc.c; break;
    case rbD:  b = &cpu->_regs.u_de[0].s_de.d; break;
    case rbE:  b = &cpu->_regs.u_de[0].s_de.e; break;
    case rbH:  b = &cpu->_regs.u_hl[0].s_hl.h; break;
    case rbL:  b = &cpu->_regs.u_hl[0].s_hl.l; break;
    case rbHL: b = cpu->_mem + (addr = cpu->_regs.u_hl[0].hl); cpu->_cycles += 7; break;
  }
  *b &= ~mask;
  if( addr >= 0 )
    cpu->MemChanged( addr );
}
//---------------------------------------------------------------------------
void Z80::JP( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 10;
  bool cond = false;
  WORD val = *(WORD*)(instr + 1);
  switch( instr[0] ) {
    case 0xE9: val = cpu->_regs.u_hl[0].hl; cpu->_cycles -= 6;
    case 0xC3: cond = true; break;
    case 0xC2: cond = !(cpu->_regs.u_af[0].s_af.f & Z_SET); break;
    case 0xCA: cond = (cpu->_regs.u_af[0].s_af.f & Z_SET); break;
    case 0xD2: cond = !(cpu->_regs.u_af[0].s_af.f & C_SET); break;
    case 0xDA: cond = (cpu->_regs.u_af[0].s_af.f & C_SET); break;
    case 0xE2: cond = !(cpu->_regs.u_af[0].s_af.f & PV_SET); break;
    case 0xEA: cond = (cpu->_regs.u_af[0].s_af.f & PV_SET); break;
    case 0xF2: cond = !(cpu->_regs.u_af[0].s_af.f & S_SET); break;
    case 0xFA: cond = (cpu->_regs.u_af[0].s_af.f & S_SET); break;
    case 0xDD: val = cpu->_regs.ix; cond = true; cpu->_cycles--; break;
    case 0xFD: val = cpu->_regs.iy; cond = true; cpu->_cycles--; break;
  }
  if( cond )
    cpu->_regs.pc = val;
}
//---------------------------------------------------------------------------
void Z80::JR( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 7;
  bool cond = false;
  switch( instr[0] ) {
    case 0x18: cond = true; break;
    case 0x20: cond = !(cpu->_regs.u_af[0].s_af.f & Z_SET); break;
    case 0x28: cond = (cpu->_regs.u_af[0].s_af.f & Z_SET); break;
    case 0x30: cond = !(cpu->_regs.u_af[0].s_af.f & C_SET); break;
    case 0x38: cond = (cpu->_regs.u_af[0].s_af.f & C_SET); break;
    case 0x10: cond = --cpu->_regs.u_bc[0].s_bc.b != 0; ++cpu->_cycles; break;
  }
  if( cond ) {
    cpu->_cycles += 5;
    cpu->_regs.pc += (signed char)instr[1];
  }
}
//---------------------------------------------------------------------------
void Z80::CALL( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 10;
  bool cond = false;
  WORD val = *(WORD*)(instr + 1);
  switch( instr[0] ) {
    case 0xC7:
    case 0xCF:
    case 0xD7:
    case 0xDF:
    case 0xE7:
    case 0xEF:
    case 0xF7:
    case 0xFF: cond = true; cpu->_cycles -= 6; val = (WORD)(instr[0]&0x38); break;
    case 0xCD: cond = true; break;
    case 0xC4: cond = !(cpu->_regs.u_af[0].s_af.f & Z_SET); break;
    case 0xCC: cond = (cpu->_regs.u_af[0].s_af.f & Z_SET); break;
    case 0xD4: cond = !(cpu->_regs.u_af[0].s_af.f & C_SET); break;
    case 0xDC: cond = (cpu->_regs.u_af[0].s_af.f & C_SET); break;
    case 0xE4: cond = !(cpu->_regs.u_af[0].s_af.f & PV_SET); break;
    case 0xEC: cond = (cpu->_regs.u_af[0].s_af.f & PV_SET); break;
    case 0xF4: cond = !(cpu->_regs.u_af[0].s_af.f & S_SET); break;
    case 0xFC: cond = (cpu->_regs.u_af[0].s_af.f & S_SET); break;
  }
  if( cond ) {
    cpu->_cycles += 7;
    cpu->_regs.sp -= 2;
    *(WORD*)(cpu->_mem + cpu->_regs.sp) = cpu->_regs.pc;
    cpu->_regs.pc = val;
    cpu->MemChanged( cpu->_regs.sp, cpu->_regs.sp + 1 );
  }
}
//---------------------------------------------------------------------------
void Z80::RET( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 5;
  bool cond = false;
  switch( instr[0] ) {
    case 0xED: cpu->_cycles += 3; if( instr[1] == 0x45 ) cpu->_regs.iff1 = cpu->_regs.iff2;
    case 0xC9: cond = true; --cpu->_cycles; break;
    case 0xC0: cond = !(cpu->_regs.u_af[0].s_af.f & Z_SET); break;
    case 0xC8: cond = (cpu->_regs.u_af[0].s_af.f & Z_SET); break;
    case 0xD0: cond = !(cpu->_regs.u_af[0].s_af.f & C_SET); break;
    case 0xD8: cond = (cpu->_regs.u_af[0].s_af.f & C_SET); break;
    case 0xE0: cond = !(cpu->_regs.u_af[0].s_af.f & PV_SET); break;
    case 0xE8: cond = (cpu->_regs.u_af[0].s_af.f & PV_SET); break;
    case 0xF0: cond = !(cpu->_regs.u_af[0].s_af.f & S_SET); break;
    case 0xF8: cond = (cpu->_regs.u_af[0].s_af.f & S_SET); break;
  }
  if( cond ) {
    cpu->_cycles += 6;
    cpu->_regs.pc = *(WORD*)(cpu->_mem + cpu->_regs.sp);
    cpu->_regs.sp += 2;
  }
}
//---------------------------------------------------------------------------
void Z80::INPORT( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b;
  cpu->_cycles += 11;
  switch( instr[0] ) {
    case 0xDB: cpu->_regs.u_af[0].s_af.a = cpu->ReadPort( instr[1] ); break;
    case 0xED:
      ++cpu->_cycles;
      cpu->setreg8( (instr[1] >> 3) & 0x7, cpu->ReadPort( cpu->_regs.u_bc[0].s_bc.c ) );
      b = cpu->readreg8( (instr[1] >> 3) & 0x7 );
      cpu->_regs.u_af[0].s_af.f &= C_SET;
      cpu->_regs.u_af[0].s_af.f |= (b&0x80?S_SET:0)|(b==0?Z_SET:0)|parity(b);
      break;
  }
}
//---------------------------------------------------------------------------
void Z80::INI( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 16;
  cpu->_mem[cpu->_regs.u_hl[0].hl] = cpu->ReadPort( cpu->_regs.u_bc[0].s_bc.c );
  --cpu->_regs.u_bc[0].s_bc.b;
  ++cpu->_regs.u_hl[0].hl;
  cpu->_regs.u_af[0].s_af.f &= ~Z_SET;
  cpu->_regs.u_af[0].s_af.f |= N_SET|(cpu->_regs.u_bc[0].s_bc.b==0?Z_SET:0);
  cpu->MemChanged( cpu->_regs.u_hl[0].hl - 1 );
}
//---------------------------------------------------------------------------
void Z80::INIR( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  INI( cpu, instr, opcode );
  if( !(cpu->_regs.u_af[0].s_af.f & Z_SET) ) {
    cpu->_cycles += 5;
    cpu->_regs.pc -= 2;
  }
}
//---------------------------------------------------------------------------
void Z80::IND( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 16;
  cpu->_mem[cpu->_regs.u_hl[0].hl] = cpu->ReadPort( cpu->_regs.u_bc[0].s_bc.c );
  --cpu->_regs.u_bc[0].s_bc.b;
  --cpu->_regs.u_hl[0].hl;
  cpu->_regs.u_af[0].s_af.f &= ~Z_SET;
  cpu->_regs.u_af[0].s_af.f |= N_SET|(cpu->_regs.u_bc[0].s_bc.b==0?Z_SET:0);
  cpu->MemChanged( cpu->_regs.u_hl[0].hl + 1 );
}
//---------------------------------------------------------------------------
void Z80::INDR( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  IND( cpu, instr, opcode );
  if( !(cpu->_regs.u_af[0].s_af.f & Z_SET) ) {
    cpu->_cycles += 5;
    cpu->_regs.pc -= 2;
  }
}
//---------------------------------------------------------------------------
void Z80::OUTPORT( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  BYTE b;
  cpu->_cycles += 11;
  switch( instr[0] ) {
    case 0xD3: cpu->WritePort( instr[1], cpu->_regs.u_af[0].s_af.a ); break;
    case 0xED:
      ++cpu->_cycles;
      cpu->WritePort( cpu->_regs.u_bc[0].s_bc.c, cpu->readreg8( (instr[1]>>3)&7) );
      break;
  }
}
//---------------------------------------------------------------------------
void Z80::OUTI( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 16;
  cpu->WritePort( cpu->_regs.u_bc[0].s_bc.c, cpu->_mem[cpu->_regs.u_hl[0].hl] );
  --cpu->_regs.u_bc[0].s_bc.b;
  ++cpu->_regs.u_hl[0].hl;
  cpu->_regs.u_af[0].s_af.f &= ~Z_SET;
  cpu->_regs.u_af[0].s_af.f |= N_SET|(cpu->_regs.u_bc[0].s_bc.b==0?Z_SET:0);
}
//---------------------------------------------------------------------------
void Z80::OTIR( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  OUTI( cpu, instr, opcode );
  if( !(cpu->_regs.u_af[0].s_af.f & Z_SET) ) {
    cpu->_cycles += 5;
    cpu->_regs.pc -= 2;
  }
}
//---------------------------------------------------------------------------
void Z80::OUTD( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  cpu->_cycles += 16;
  cpu->WritePort( cpu->_regs.u_bc[0].s_bc.c, cpu->_mem[cpu->_regs.u_hl[0].hl] );
  --cpu->_regs.u_bc[0].s_bc.b;
  --cpu->_regs.u_hl[0].hl;
  cpu->_regs.u_af[0].s_af.f &= ~Z_SET;
  cpu->_regs.u_af[0].s_af.f |= N_SET|(cpu->_regs.u_bc[0].s_bc.b==0?Z_SET:0);
  cpu->MemChanged( cpu->_regs.u_hl[0].hl + 1 );
}
//---------------------------------------------------------------------------
void Z80::OTDR( Z80 *cpu, BYTE *instr, OPCODE *opcode ) {
  OUTD( cpu, instr, opcode );
  if( !(cpu->_regs.u_af[0].s_af.f & Z_SET) ) {
    cpu->_cycles += 5;
    cpu->_regs.pc -= 2;
  }
}
//---------------------------------------------------------------------------
#pragma package(smart_init)

