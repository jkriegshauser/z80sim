// Copyright (C) 2002-2024 Joshua Kriegshauser
// All rights reserved.
//---------------------------------------------------------------------------
#include "display.h"
//---------------------------------------------------------------------------
BYTE dsp_mem[1 << 21];    // 2MB
BYTE dsp_LUTindex;
BYTE dsp_LUTrgb;
int dsp_LUT[256];
int dsp_LUTtemp;
BYTE dsp_reg[256];
int dsp_startaddr;
HDC dsp_hdc;
HBITMAP dsp_bitmap;
void *dsp_bits;
RGBQUAD dsp_temp;
int dsp_changed;
//---------------------------------------------------------------------------
void dsp_init( HDC device ) {
  #define bmi u_bitmap.u_bmi
  union {
    char u_buffer[ sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD) ];
    BITMAPINFO u_bmi;
  } u_bitmap;
  memset( &dsp_temp, 0x00, sizeof( dsp_temp ) );
  memset( &u_bitmap, 0x00, sizeof( u_bitmap ) );
  bmi.bmiHeader.biSize = sizeof( bmi.bmiHeader );
  bmi.bmiHeader.biWidth = 320;
  bmi.bmiHeader.biHeight = -240;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 8;
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biSizeImage = 0;
  bmi.bmiHeader.biXPelsPerMeter = GetDeviceCaps( device, HORZRES ) / GetDeviceCaps( device, HORZSIZE );
  bmi.bmiHeader.biYPelsPerMeter = GetDeviceCaps( device, VERTRES ) / GetDeviceCaps( device, VERTSIZE );
  bmi.bmiHeader.biClrUsed = 256;
  bmi.bmiHeader.biClrImportant = 0;
  dsp_hdc = CreateCompatibleDC( device );
  dsp_bitmap = CreateDIBSection( dsp_hdc, &bmi, DIB_RGB_COLORS, &dsp_bits, NULL, 0 );
  dsp_bitmap = SelectObject( dsp_hdc, dsp_bitmap );
  dsp_reset(device);
}
//---------------------------------------------------------------------------
void dsp_reset(HDC device) {
  char buf[sizeof(RGBQUAD) * 256];
  memset( &dsp_temp, 0x00, sizeof( dsp_temp ) );
  dsp_LUTindex = 0;
  dsp_LUTrgb = 0;
  dsp_startaddr = 0;
  // memset( buf, 0x00, sizeof( buf ) );
  SetDIBColorTable( dsp_hdc, 0, 256, (RGBQUAD*)buf );
  dsp_draw(1, device);
}
//---------------------------------------------------------------------------
void dsp_write( int addr, BYTE data ) {
  if( addr & 0x800000 ) {
    addr &= 0x1FFFFF;
    dsp_mem[addr] = data;
    if( addr >= dsp_startaddr && addr < 320*240 + dsp_startaddr ) {
      // GdiFlush();
      *((BYTE*)(dsp_bits) + addr - dsp_startaddr) = data;
      /*
      int x = (addr - dsp_startaddr)%320;
      int y = (addr - dsp_startaddr)/320;
      BitBlt( dsp_canvas->Handle, x, y, 1, 1, dsp_hdc, x, y, SRCCOPY );
      */
      dsp_changed = 1;
    }
  } else {
    switch( addr ) {
      case 0x10:  // red
        dsp_startaddr &= 0xFFFF00;
        dsp_startaddr |= data;
        break;
      case 0x11:  // green
        dsp_startaddr &= 0xFF00FF;
        dsp_startaddr |= (data << 8);
        break;
      case 0x12:  // blue
        dsp_startaddr &= 0xFFFF;
        dsp_startaddr |= (data << 16);
        // GdiFlush();
        memcpy( dsp_bits, dsp_mem + (dsp_startaddr<<=1), 320 * 240 );
        // dsp_startaddr <<= 1;
        dsp_changed = 1;
        // RedrawScreen();
        break;
      case 0x24:
        dsp_LUTindex = data;
        dsp_LUTrgb = 0;
        break;
      case 0x26:
        switch( dsp_LUTrgb ) {
          case 0: dsp_temp.rgbRed = data; break;
          case 1: dsp_temp.rgbGreen = data; break;
          case 2: dsp_temp.rgbBlue = data; break;
        }
        ++dsp_LUTrgb;
        if( dsp_LUTrgb > 2 ) {
          SetDIBColorTable( dsp_hdc, dsp_LUTindex, 1, &dsp_temp );
          dsp_LUTindex++;
          dsp_LUTrgb = 0;
        }
        break;
      default:
        dsp_reg[addr & 0xFF] = data;
    }
  }
}
//---------------------------------------------------------------------------
BYTE dsp_read( int addr ) {
  BYTE b;
  RGBQUAD rgb;
  if( addr & 0x800000 )
    return dsp_mem[addr & 0x1FFFFF];
  else {
    switch( addr ) {
      case 0x24:
        return dsp_LUTindex;
      case 0x26:
        GetDIBColorTable( dsp_hdc, dsp_LUTindex, 1, &rgb );
        switch( dsp_LUTrgb++ ) {
          case 0: b = rgb.rgbRed; break;
          case 1: b = rgb.rgbGreen; break;
          case 2: b = rgb.rgbBlue; break;
        }
        if( dsp_LUTrgb > 2 ) {
          dsp_LUTindex++;
          dsp_LUTrgb = 0;
        }
        return b;
      default:
        return dsp_reg[addr & 0xFF];
    }
  }
}
//---------------------------------------------------------------------------
void dsp_draw(int force, HDC device) {
  if( force || dsp_changed ) {
    // GdiFlush();
    // memcpy( dsp_bits, dsp_mem + dsp_startaddr, 320 * 240 );
    BitBlt( device, 0, 0, 320, 240, dsp_hdc, 0, 0, SRCCOPY );
    dsp_changed = 0;
  }
  // drawing = 0;
}
//---------------------------------------------------------------------------
void dsp_destroy() {
  DeleteObject( dsp_bitmap );
  DeleteDC( dsp_hdc );
}
//---------------------------------------------------------------------------
#pragma package(smart_init)
