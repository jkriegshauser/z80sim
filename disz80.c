#include "disz80.h"
#include "z80opcodes.h"
#include <stdio.h>

OPCODE *lookup( unsigned char *opcode, OPCODE *codepage );
void decodeAM( enum ADDRESSINGMODE am, unsigned char *mem, unsigned short *PC, OPCODE *op, char *buffer );
int getcodepagesize( OPCODE *codepage );

int disz80( unsigned char *mem, unsigned short PC, char *buffer ) {
  OPCODE *op, *codepage = z80codepage;
  int len = 0;

dolookup:
  op = lookup( mem + PC, codepage );
  if( op ) {
    if( op->am1 == amCODEPAGE ) {
      PC += op->size;
      len += op->size;
      codepage = (OPCODE*)op->mnemonic;
      goto dolookup;
    }
    strcpy( buffer, op->mnemonic );
    if( op->am1 != amNONE ) {
      if( buffer[strlen(buffer)-1] != ',' )
        strcat( buffer, " " );
      decodeAM( op->am1, mem, &PC, op, buffer );
      if( op->am2 != amNONE ) {
        strcat( buffer, "," );
        decodeAM( op->am2, mem, &PC, op, buffer );
      }
    }
    len += op->size;
  } else {
    strcpy( buffer, "db " );
    sprintf( buffer + strlen(buffer), "$%02x", (unsigned long)*(mem + PC) );
    ++len;
  }
  return len;
}

OPCODE *lookup( unsigned char *opcode, OPCODE *codepage ) {
  int i, size = getcodepagesize(codepage);
  for( i = 0; i < size; ++i ) {
    if( codepage[i].opcode == *opcode ) {
      return &codepage[i];
    }
  }
  return 0;
}

void decodeAM( enum ADDRESSINGMODE am, unsigned char *mem, unsigned short *PC, OPCODE *op, char *buffer ) {
  char byte;
  switch(am) {
    case amREGB: strcat(buffer, "b"); break;
    case amREGC: strcat(buffer, "c"); break;
    case amREGD: strcat(buffer, "d"); break;
    case amREGE: strcat(buffer, "e"); break;
    case amREGH: strcat(buffer, "h"); break;
    case amREGL: strcat(buffer, "l"); break;
    case amREGA: strcat(buffer, "a"); break;
    case amREGI: strcat(buffer, "i"); break;
    case amREGR: strcat(buffer, "r"); break;
    case amREGAF: strcat(buffer, "af"); break;
    case amREGAFALT: strcat(buffer, "af'"); break;
    case amREGBC: strcat(buffer, "bc"); break;
    case amREGDE: strcat(buffer, "de"); break;
    case amREGHL: strcat(buffer, "hl"); break;
    case amREGSP: strcat(buffer, "sp"); break;
    case amREGIX: strcat(buffer, "ix"); break;
    case amREGIY: strcat(buffer, "iy"); break;
    case amREGINDIRECT:
      if( *(mem + *PC - 2) == 0xcb ) {
        if( *(mem + *PC - 3) == 0xdd ) am = amINDEXX;
        else if( *(mem + *PC - 3) == 0xfd) am = amINDEXY;
        byte = *(mem + *PC - 1);
        goto do_index;
      } else {
        strcat(buffer, "(hl)");
      }
      break;
    case amINDBC: strcat(buffer, "(bc)"); break;
    case amINDDE: strcat(buffer, "(de)"); break;
    case amINDHL: strcat(buffer, "(hl)"); break;
    case amINDSP: strcat(buffer, "(sp)"); break;
    case amINDIX: strcat(buffer, "(ix)"); break;
    case amINDIY: strcat(buffer, "(iy)"); break;
    case amINDEXX:
    case amINDEXY:
      byte = *(mem + *PC + 1);
      *PC += 1;
do_index:
      if( byte < 0 )
        sprintf(buffer + strlen(buffer), "(%s-$%02x)", am == amINDEXX ? "ix" : "iy", (int)-byte );
      else
        sprintf(buffer + strlen(buffer), "(%s+$%02x)", am == amINDEXX ? "ix" : "iy", (int)byte );
      break;
    case amEXTENDED:
      sprintf(buffer + strlen(buffer), "($%04x)", *(unsigned short*)(mem + *PC + 1));
      break;
    case amIMMEDIATE:
      sprintf(buffer + strlen(buffer), "$%02x", (unsigned int)*(mem + *PC + 1));
      break;
    case amIMMEDEX:
      sprintf(buffer + strlen(buffer), "$%04x", (unsigned int)*(unsigned short*)(mem + *PC + 1));
      break;
    case amIOPORT:
      sprintf(buffer + strlen(buffer), "($%02x)", (unsigned int)*(mem + *PC + 1));
      break;
    case amIOPORTC: strcat(buffer, "(c)"); break;
    case amRELATIVE:
      byte = *(mem + *PC + 1);
      sprintf(buffer + strlen(buffer), "$%04x", (unsigned int)(*PC + 2 + (signed int)byte));
      break;
    case amNONE:
    case amCODEPAGE:           // If this is the addressing mode, the mnemonic member is actually a pointer to the next codepage
    default:
      break;
  }
  return;
}

int getcodepagesize( OPCODE *codepage ) {
  if( codepage == z80codepage ) return sizeof( z80codepage )/sizeof( OPCODE );
  if( codepage == CBcodepage ) return sizeof( CBcodepage )/sizeof( OPCODE );
  if( codepage == DDcodepage ) return sizeof( DDcodepage )/sizeof( OPCODE );
  if( codepage == EDcodepage ) return sizeof( EDcodepage )/sizeof( OPCODE );
  if( codepage == FDcodepage ) return sizeof( FDcodepage )/sizeof( OPCODE );
  return 0;
}
