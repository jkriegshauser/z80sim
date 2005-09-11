//---------------------------------------------------------------------------

#ifndef z80H
#define z80H

#include <stdio.h>
#include "processor.h"
#include "z80cpu.h"
//---------------------------------------------------------------------------
enum ADDRESSINGMODE {
  amNA,
  amIMMEDIATE,
  amIMMEDIATEEX,
  amPAGEZERO,
  amRELATIVE,
  amEXTENDED,
  amINDEXX,
  amINDEXY,
  amREGA,
  amREGF,
  amREGB,
  amREGC,
  amREGD,
  amREGE,
  amREGH,
  amREGL,
  amREGA_,
  amREGF_,
  amREGB_,
  amREGC_,
  amREGD_,
  amREGE_,
  amREGH_,
  amREGL_,
  amREGI,
  amREGR,
  amREGAF,
  amREGBC,
  amREGDE,
  amREGHL,
  amREGAF_,
  amREGBC_,
  amREGDE_,
  amREGHL_,
  amREGSP,
  amREGIX,
  amREGIY,
  amIMPLIED,
  amREGINDBC,
  amREGINDDE,
  amREGINDHL,
  amREGINDSP,
  amREGINDIX,
  amREGINDIY,
  amBIT,
  amSKIPONE,
};
//---------------------------------------------------------------------------
class Z80;
struct _opcode;
typedef void (*OPERATION)( Z80 *cpu, BYTE *instruction, struct _opcode *opcode );
typedef struct _opcode {
  char *name;
  BYTE opcode;
  ADDRESSINGMODE primary;
  ADDRESSINGMODE secondary;
  OPERATION op;
  int cycles;     // if op is NULL, this will add the number of clock cycles
  int opcodelen;  // size of opcode-only portion of the instruction
  int codepage;   // for shift instructions
} OPCODE;
//---------------------------------------------------------------------------
// Flag defines
#define S_SET     0x80
#define Z_SET     0x40
#define Y_SET     0x20 /* undocumented */
#define H_SET     0x10
#define X_SET     0x08 /* undocumented */
#define PV_SET    0x04
#define N_SET     0x02
#define C_SET     0x01
//---------------------------------------------------------------------------
typedef struct {
  union {
    struct {
      BYTE f;
      BYTE a;
    } s_af;
    WORD af;
  } u_af[2];
  union {
    struct {
      BYTE c;
      BYTE b;
    } s_bc;
    WORD bc;
  } u_bc[2];
  union {
    struct {
      BYTE e;
      BYTE d;
    } s_de;
    WORD de;
  } u_de[2];
  union {
    struct {
      BYTE l;
      BYTE h;
    } s_hl;
    WORD hl;
  } u_hl[2];
  BYTE i;
  BYTE r;
  WORD ix;
  WORD iy;
  WORD sp;
  WORD pc;
  bool iff1;
  bool iff2;
} REGISTERS;
//---------------------------------------------------------------------------
enum REGDEF8 {
  rbB = 0,
  rbC = 1,
  rbD = 2,
  rbE = 3,
  rbH = 4,
  rbL = 5,
  rbHL = 6,
  rbA = 7,
};
//---------------------------------------------------------------------------
enum REGDEF16 {
  rwBC = 0,
  rwDE = 1,
  rwHL = 2,
  rwSP = 3,
};
//---------------------------------------------------------------------------
class Z80 : public Processor {
private:
  static const OPCODE _codepage[][256];
  REGISTERS _regs;
  int _interruptmode;
  bool _interruptenabledelay;
  int _interruptpending;
  bool _nmipending;
  int _cycles;
  bool _halted;

  BYTE readreg8( REGDEF8 rd );
  void setreg8( REGDEF8 rd, BYTE val );
  WORD readreg16( REGDEF16 rd );
  void setreg16( REGDEF16 rd, WORD val );

  WORD readval( ADDRESSINGMODE ad );
  void setval( ADDRESSINGMODE ad, BYTE val );

  void __fastcall decodeAM( ADDRESSINGMODE ad, BYTE *data, int pc, char *buffer, bool comma = false );

  int __fastcall GetAddressingModeLength( ADDRESSINGMODE ad );

  void __fastcall UndefinedOpcode();

  void __fastcall handleNMI();
  void __fastcall handleInterrupt();

  static BYTE add( BYTE a1, BYTE a2, BYTE &flags, bool carry );
  static BYTE sub( BYTE s1, BYTE s2, BYTE &flags, bool carry );
  static BYTE parity( BYTE b );

  static void LDREGS( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDRN( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDRHL( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDRI( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDHLR( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDIxR( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDHLN( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDIN( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDA( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDxA( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDDDNN( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDINN( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDDDNNI( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDINNI( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDNNIDD( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDNNII( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDSP( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void PUSHQQ( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void POPQQ( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void EX( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDI( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDIR( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDD( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void LDDR( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void CPI( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void CPIR( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void CPD( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void CPDR( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void ADD8( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void ADC8( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void SUB8( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void SBC8( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void AND8( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void OR8( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void XOR8( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void CP( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void INC8( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void DEC8( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void DAA( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void CPL( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void NEG( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void CCF( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void SCF( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void HALT( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void DI( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void EI( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void IM( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void ADD16( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void INC16( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void DEC16( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void RxCA( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void RxA( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void DDCB( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void FDCB( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void RLC( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void RL( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void RRC( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void RR( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void SLA( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void SRA( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void SRL( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void RxD( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void BIT( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void SET( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void RES( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void JP( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void JR( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void CALL( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void RET( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void INPORT( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void INI( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void INIR( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void IND( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void INDR( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void OUTPORT( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void OUTI( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void OTIR( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void OUTD( Z80 *cpu, BYTE *instruction, OPCODE *opcode );
  static void OTDR( Z80 *cpu, BYTE *instruction, OPCODE *opcode );

public:
  __fastcall Z80( BYTE *mem, int memsize );
  __fastcall ~Z80();

  void __fastcall reset();
  void __fastcall step();
  void __fastcall start();
  void __fastcall pause();
  void __fastcall terminate();
  Z80REGS * __fastcall getregs();
  BYTE * __fastcall fetch( int pc, int &len, OPCODE **op );
  void __fastcall decode( BYTE *instr, OPCODE *op, int pc, char *buffer );

  bool __fastcall getRunning();
  long __fastcall getcycles();

  bool __fastcall interrupt( BYTE data );
  void __fastcall NMI();
};
//---------------------------------------------------------------------------
#endif
