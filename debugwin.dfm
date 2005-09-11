object Form2: TForm2
  Left = 228
  Top = 663
  BorderIcons = [biSystemMenu, biMinimize]
  BorderStyle = bsSingle
  Caption = 'Debugging Information'
  ClientHeight = 457
  ClientWidth = 948
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poScreenCenter
  OnHide = FormHide
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object Label1: TLabel
    Left = 8
    Top = 8
    Width = 47
    Height = 13
    Caption = 'Registers:'
  end
  object Label2: TLabel
    Left = 360
    Top = 8
    Width = 70
    Height = 13
    Caption = 'Memory Block:'
  end
  object Label3: TLabel
    Left = 360
    Top = 27
    Width = 189
    Height = 13
    Caption = 'Monitor                      bytes from location'
  end
  object Label4: TLabel
    Left = 8
    Top = 248
    Width = 61
    Height = 13
    Caption = 'Disassembly:'
  end
  object Label5: TLabel
    Left = 8
    Top = 267
    Width = 213
    Height = 13
    Caption = 'Disassemble                      bytes from location'
  end
  object Label6: TLabel
    Left = 376
    Top = 256
    Width = 59
    Height = 13
    Caption = 'Breakpoints:'
  end
  object Label7: TLabel
    Left = 568
    Top = 251
    Width = 31
    Height = 13
    Caption = 'Stack:'
  end
  object Label8: TLabel
    Left = 624
    Top = 251
    Width = 134
    Height = 13
    Caption = 'Range: +/-                   bytes'
  end
  object btStepInto: TButton
    Left = 864
    Top = 360
    Width = 75
    Height = 25
    Caption = '&Step Into'
    TabOrder = 15
    OnClick = btStepIntoClick
  end
  object Memo1: TRichEdit
    Left = 8
    Top = 24
    Width = 305
    Height = 217
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -13
    Font.Name = 'Courier New'
    Font.Style = []
    ParentFont = False
    ReadOnly = True
    TabOrder = 0
  end
  object Memo2: TRichEdit
    Left = 360
    Top = 48
    Width = 577
    Height = 193
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'Courier New'
    Font.Style = []
    Lines.Strings = (
      
        '0x00000000: .. .. .. .. .. .. .. ..-.. .. .. .. .. .. .. .. ; ..' +
        '..............')
    ParentFont = False
    ReadOnly = True
    ScrollBars = ssVertical
    TabOrder = 6
  end
  object ebStart: TEdit
    Left = 552
    Top = 24
    Width = 57
    Height = 21
    TabOrder = 5
    Text = '0x0000'
    OnChange = ebStartChange
  end
  object ebLength: TEdit
    Left = 400
    Top = 24
    Width = 57
    Height = 21
    TabOrder = 4
    Text = '0x200'
    OnChange = ebStartChange
  end
  object ebDisLength: TEdit
    Left = 72
    Top = 264
    Width = 57
    Height = 21
    TabOrder = 1
    Text = '0x200'
    OnChange = ebDisStartChange
  end
  object ebDisStart: TEdit
    Left = 224
    Top = 264
    Width = 57
    Height = 21
    TabOrder = 2
    Text = '0x0000'
    OnChange = ebDisStartChange
  end
  object btRun: TButton
    Left = 864
    Top = 296
    Width = 75
    Height = 25
    Caption = '&Run'
    TabOrder = 13
    OnClick = btRunClick
  end
  object btStop: TButton
    Left = 864
    Top = 328
    Width = 75
    Height = 25
    Caption = 'S&top'
    TabOrder = 14
    OnClick = btStopClick
  end
  object btReset: TButton
    Left = 864
    Top = 424
    Width = 75
    Height = 25
    Caption = 'R&eset'
    TabOrder = 16
    OnClick = btResetClick
  end
  object ListBox1: TListBox
    Left = 376
    Top = 272
    Width = 97
    Height = 177
    ItemHeight = 13
    TabOrder = 7
    OnClick = ListBox1Click
    OnExit = ListBox1Click
  end
  object ebAddress: TEdit
    Left = 480
    Top = 352
    Width = 57
    Height = 21
    TabOrder = 8
    Text = '0x0000'
  end
  object btAdd: TButton
    Left = 480
    Top = 376
    Width = 57
    Height = 25
    Caption = '&Add'
    TabOrder = 9
    OnClick = btAddClick
  end
  object btDelete: TButton
    Left = 480
    Top = 408
    Width = 57
    Height = 25
    Caption = '&Delete'
    Enabled = False
    TabOrder = 10
    OnClick = btDeleteClick
  end
  object ListBox2: TListBox
    Left = 8
    Top = 288
    Width = 353
    Height = 161
    ExtendedSelect = False
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'Courier New'
    Font.Style = []
    ItemHeight = 14
    ParentFont = False
    TabOrder = 3
    TabWidth = 30
  end
  object ebStackLen: TEdit
    Left = 680
    Top = 248
    Width = 49
    Height = 21
    TabOrder = 11
    Text = '64'
    OnChange = ebStackLenChange
  end
  object ListBox3: TListBox
    Left = 568
    Top = 272
    Width = 273
    Height = 177
    ExtendedSelect = False
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'Courier New'
    Font.Style = []
    ItemHeight = 14
    ParentFont = False
    TabOrder = 12
    TabWidth = 30
  end
  object btStepOver: TButton
    Left = 864
    Top = 392
    Width = 75
    Height = 25
    Caption = 'Step &Over'
    TabOrder = 17
    OnClick = btStepOverClick
  end
  object btInterrupt: TButton
    Left = 864
    Top = 248
    Width = 75
    Height = 25
    Caption = '&Interrupt...'
    TabOrder = 18
    OnClick = btInterruptClick
  end
  object Timer1: TTimer
    OnTimer = Timer1Timer
    Left = 320
    Top = 40
  end
end
