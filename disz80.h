// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
#ifndef disz80H
#define disz80H
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Returns the null-terminated disassembled opcodes in output and the length
// of bytes used from loc
extern int disz80( unsigned char *mem, unsigned short PC, char *output );

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
 