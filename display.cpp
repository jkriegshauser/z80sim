//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "display.h"
//---------------------------------------------------------------------------
Display::Display( TCanvas *canvas ) {
  #define bmi u_bitmap.u_bmi
  union {
    char u_buffer[ sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD) ];
    BITMAPINFO u_bmi;
  } u_bitmap;
  _canvas = canvas;
  memset( &_temp, 0x00, sizeof( _temp ) );
  memset( &u_bitmap, 0x00, sizeof( u_bitmap ) );
  bmi.bmiHeader.biSize = sizeof( bmi.bmiHeader );
  bmi.bmiHeader.biWidth = 320;
  bmi.bmiHeader.biHeight = -240;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 8;
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biSizeImage = 0;
  bmi.bmiHeader.biXPelsPerMeter = GetDeviceCaps( _canvas->Handle, HORZRES ) / GetDeviceCaps( _canvas->Handle, HORZSIZE );
  bmi.bmiHeader.biYPelsPerMeter = GetDeviceCaps( _canvas->Handle, VERTRES ) / GetDeviceCaps( _canvas->Handle, VERTSIZE );
  bmi.bmiHeader.biClrUsed = 256;
  bmi.bmiHeader.biClrImportant = 0;
  _hdc = CreateCompatibleDC( canvas->Handle );
  // _bitmap = CreateCompatibleBitmap( _hdc, 320, 240 );
  _bitmap = CreateDIBSection( _hdc, &bmi, DIB_RGB_COLORS, &_bits, NULL, 0 );
  _bitmap = SelectObject( _hdc, _bitmap );
  reset();
}
//---------------------------------------------------------------------------
void Display::reset() {
  /*
  memset( _mem, 0x00, sizeof( _mem ) );
  memset( _LUT, 0x00, sizeof( _LUT ) );
  //memset( _LUTtemp, 0x00, sizeof( _LUTtemp ) );
  _LUTtemp = 0;
  */
  memset( &_temp, 0x00, sizeof( _temp ) );
  _LUTindex = 0;
  _LUTrgb = 0;
  _startaddr = 0;
  char buf[sizeof(RGBQUAD) * 256];
  memset( buf, 0x00, sizeof( buf ) );
  SetDIBColorTable( _hdc, 0, 256, (RGBQUAD*)buf );
  RedrawScreen(true);
}
//---------------------------------------------------------------------------
void Display::write( int addr, BYTE data ) {
  if( addr & 0x800000 ) {
    addr &= 0x1FFFFF;
    _mem[addr] = data;
    if( addr >= _startaddr && addr < 320*240 ) {
      *((BYTE*)(_bits) + addr - _startaddr) = data;
      /*
      int x = (addr - _startaddr)%320;
      int y = (addr - _startaddr)/320;
      BitBlt( _canvas->Handle, x, y, 1, 1, _hdc, x, y, SRCCOPY );
      */
      _changed = true;
      /*
      // _canvas->Pixels[x][y] = (TColor)_LUT[data];
      HDC hdc = _canvas->Handle;
      SetPixel( hdc, x, y, _LUT[data] );
      */
    }
  } else {
    switch( addr ) {
      case 0x10:  // red
        _startaddr &= 0xFFFF00;
        _startaddr |= data;
        break;
      case 0x11:  // green
        _startaddr &= 0xFF00FF;
        _startaddr |= (data << 8);
        break;
      case 0x12:  // blue
        _startaddr &= 0xFFFF;
        _startaddr |= (data << 16);
        memcpy( _bits, _mem + _startaddr, 320 * 240 );
        _changed = true;
        // RedrawScreen();
        break;
      case 0x24:
        _LUTindex = data;
        _LUTrgb = 0;
        break;
      case 0x26:
        /*
        _LUTtemp[_LUTrgb++] = data;
        */
        // _LUTtemp &= ~(0xFF << (_LUTrgb * 8));
        // _LUTtemp |= data << (_LUTrgb++ * 8);
        switch( _LUTrgb ) {
          case 0: _temp.rgbRed = data; break;
          case 1: _temp.rgbGreen = data; break;
          case 2: _temp.rgbBlue = data; break;
        }
        ++_LUTrgb;
        if( _LUTrgb > 2 ) {
          //memcpy( _LUT[_LUTindex], _LUTtemp, sizeof( _LUTtemp ) );
          // _LUT[_LUTindex] = _LUTtemp;
          SetDIBColorTable( _hdc, _LUTindex, 1, &_temp );
          _LUTindex++;
          _LUTrgb = 0;
        }
        break;
      default:
        _reg[addr & 0xFF] = data;
    }
  }
}
//---------------------------------------------------------------------------
BYTE Display::read( int addr ) {
  BYTE b;
  RGBQUAD rgb;
  if( addr & 0x800000 )
    return _mem[addr & 0x1FFFFF];
  else {
    switch( addr ) {
      case 0x24:
        return _LUTindex;
      case 0x26:
        //b = _LUT[_LUTindex][_LUTrgb++];
        GetDIBColorTable( _hdc, _LUTindex, 1, &rgb );
        switch( _LUTrgb++ ) {
          case 0: b = rgb.rgbRed; break;
          case 1: b = rgb.rgbGreen; break;
          case 2: b = rgb.rgbBlue; break;
        }
        // b = (_LUT[_LUTindex] >> (_LUTrgb++ * 8)) & 0xFF;
        if( _LUTrgb > 2 ) {
          _LUTindex++;
          _LUTrgb = 0;
        }
        return b;
      default:
        return _reg[addr & 0xFF];
    }
  }
  //return 0;
}
//---------------------------------------------------------------------------
void Display::RedrawScreen(bool force) {
  if( force || _changed ) {
    BitBlt( _canvas->Handle, 0, 0, 320, 240, _hdc, 0, 0, SRCCOPY );
    _changed = false;
  }
}
//---------------------------------------------------------------------------
Display::~Display() {
  DeleteObject( _bitmap );
  DeleteDC( _hdc );
}
//---------------------------------------------------------------------------
#pragma package(smart_init)
