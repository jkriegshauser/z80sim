// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
/**
 * Defines function prototypes for z80cpu.asm
 */

#ifndef z80cpuH
#define z80cpuH

// Set byte-size packing for the struct below
#pragma pack(push, 1)

typedef struct {
  unsigned short   AF;
  unsigned short   AFalt;
  unsigned short   BC;
  unsigned short   BCalt;
  unsigned short   DE;
  unsigned short   DEalt;
  unsigned short   HL;
  unsigned short   HLalt;
  unsigned short   IX;
  unsigned short   IY;
  unsigned short   SP;
  unsigned short   PC;
  unsigned short   IR;
  unsigned char    IFF1;
  unsigned char    IFF2;
} Z80REGS;

// Pointer-to-function prototype for the function that is called when an undefined opcode is encountered
typedef void (*UNDEFPROC)( unsigned char opcode );

// Pointer-to-function prototype for clock tick function
typedef void (*CLOCKPROC)( unsigned long ticks );

// Pointer-to-function prototype for the function that is called when an I/O read must occur
typedef unsigned char (*IOREADPROC)( unsigned short port );

// Pointer-to-function prototype for the function that is called when an I/O write must occur
typedef void (*IOWRITEPROC)( unsigned short port, unsigned char byte );

// Pointer-to-function prototype called when a breakpoint is reached
typedef void (*BREAKPOINTPROC)( unsigned short addr );

// Pointer-to-function prototype called when an address is written to
typedef void (*MEMWRITEPROC)( unsigned short addr );

#ifdef __cplusplus
extern "C" {
#endif

void z80_start(void);      // returns immediately
int z80_is_running(void);
void z80_stop(void);       // waits for cpu to actually stop
int z80_is_stopped(void);  // returns immediately
void z80_terminate(void);  // returns immediately
void z80_reset(void);
void z80_cpuloop(unsigned char *mem);    // does not return until terminate is called
void z80_step(void);       // controls CPU in cpu thread
int z80_is_stepped(void);  // returns immediately (indicates if CPU was single-stepped)
unsigned long z80_get_cycles(unsigned long *high);  // returns the current number of CPU cycles
unsigned char *z80_get_mem();

void z80_set_target_cps(unsigned long cps); // set the target emulation speed in MHz
unsigned long z80_get_last_cps(); // returns the number of clock cycles recorded over the last interval

// Sets the procedure that is called when an undefined opcode is encountered.  Provide NULL
// to not call any function.  Returns the old undef proc (or NULL if one was not assigned)
UNDEFPROC z80_set_undef_proc(UNDEFPROC newundefproc);

CLOCKPROC z80_set_clock_proc(CLOCKPROC newclockproc);

IOREADPROC z80_set_io_read_proc(IOREADPROC newioreadproc);

IOWRITEPROC z80_set_io_write_proc(IOWRITEPROC newiowriteproc);

BREAKPOINTPROC z80_set_breakpoint_proc(BREAKPOINTPROC newbreakpointproc);

int z80_interrupt(char num);    // Interrupt the CPU with the specified number.  Return is boolean (0 = couldn't interrupt)
void z80_nmi(void);             // Interrupt the CPU with the NMI

// The pointer that is returned can be used to set the regs to new values IF
// the cpu is stopped.  If the cpu is not stopped, the values are ignored and
// will be overwritten when the CPU is stopped.  These values are only accurate
// when the CPU is stopped.
Z80REGS *z80_getregs(void);

// return is boolean
int z80_register_breakpoint( unsigned short loc );
// return is boolean
int z80_unregister_breakpoint( unsigned short loc );

// returns a simple hash used to remove the memwrite callback
unsigned long z80_register_memwrite( unsigned short start, unsigned short last, MEMWRITEPROC proc );
// returns true if found and removed
int z80_unregister_memwrite( unsigned long hash );
#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif
