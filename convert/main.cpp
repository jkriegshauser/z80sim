//---------------------------------------------------------------------------

#pragma hdrstop
#include <iostream>
#include <fstream>
#include <stdio.h>
//---------------------------------------------------------------------------

#pragma argsused
int main(int argc, char* argv[])
{
  char buffer[1024], pagetable[0x1f9];
  if( argc < 3 ) {
    std::cout << "Usage: convert <download file> <high memory binary>" << std::endl;
    return 0;
  }

  std::ifstream file( argv[1], std::ios_base::in );
  if( !file.is_open() ) {
    std::cout << "Failed to open download file" << std::endl;
    return 0;
  }
  FILE *f = fopen( argv[2], "r+b" );
  if( f == NULL ) {
    std::cout << "Failed to open high memory binary" << std::endl;
    return 0;
  }
  // Clear page table
  fseek( f, 0xa15, SEEK_SET );
  fread( pagetable, 1, 0x1f9, f );
  for( int i = 0; i < 48; ++i ) {
    char c = pagetable[i * 7];
    if( c != 0 && c != 6 ) {
      pagetable[i * 7] = 0x01;   // fitted page
      memset( pagetable + (i * 7) + 1, 0x00, 6 );
    }
  }
  while( file.getline( buffer, sizeof( buffer ) ) ) {
    if( buffer[0] == 'X' )
      continue;
    if( buffer[0] == '*' )
      break;
    unsigned char page; unsigned short start, end, checksum;
    page = (unsigned char)strtol( buffer, NULL, 16 );
    page += 3; //if( page >= 7 ) ++page;
    start = (unsigned short)strtol( buffer + 15, NULL, 16 );
    end = (unsigned short)strtol( buffer + 20, NULL, 16 );
    checksum = (unsigned short)strtol( buffer + 25, NULL, 16 );
    char *p = pagetable + (page * 7);
    p[0] = 0x02;
    *(unsigned short*)(p + 1) = start;
    *(unsigned short*)(p + 3) = end;
    *(unsigned short*)(p + 5) = checksum;
  }
  pagetable[0x1f8] = 0;
  for( int i = 0; i < 0x1f8; ++i ) {
    pagetable[0x1f8] ^= pagetable[i];
  }
  fseek( f, 0xa15, SEEK_SET );
  fwrite( pagetable, 1, sizeof(pagetable), f );
  // Clear load instruction
  fseek( f, 0x1b00, SEEK_SET );
  fputc( 0, f );
  fputc( 0, f );
  fclose( f ); f = NULL;

  while( file.getline( buffer, sizeof(buffer) ) ) {
    if( buffer[0] == '*' ) {
      fclose(f);
      f = NULL;
      continue;
    }
    int offset = strtol( buffer, NULL, 16 );
    if( f == NULL ) {
      char c[MAX_PATH];
      int page = (offset >> 16) & 0xFF;
      page += 3;
      if( page >= 7 ) ++page;
      sprintf( c, "page%d.bin", page );
      f = fopen( c, "w+b" );
      fseek( f, 0x7fff, SEEK_SET );
      fputc( 0, f );
    }
    offset &= 0xFFFF;
    fseek( f, offset, SEEK_SET );
    for( int i = 0; i < 16; ++i ) {
      buffer[i] = (unsigned char)strtol( buffer + 7 + (3 * i), NULL, 16 );
    }
    fwrite( buffer, 1, 16, f );
  }
  if( f ) {
    fclose( f );
  }

  return 0;
}
//---------------------------------------------------------------------------
