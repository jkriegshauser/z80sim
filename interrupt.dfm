object Form3: TForm3
  Left = 580
  Top = 943
  Width = 189
  Height = 131
  Caption = 'Enter Interrupt'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poMainFormCenter
  PixelsPerInch = 96
  TextHeight = 13
  object Label1: TLabel
    Left = 8
    Top = 24
    Width = 42
    Height = 13
    Caption = 'Interrupt:'
  end
  object ebInterrupt: TEdit
    Left = 8
    Top = 40
    Width = 73
    Height = 21
    Hint = 'Please Enter a number between 0x00 and 0xFF'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 0
    Text = '0x30'
    OnChange = ebInterruptChange
  end
  object btOK: TButton
    Left = 96
    Top = 8
    Width = 75
    Height = 25
    Caption = '&OK'
    ModalResult = 1
    TabOrder = 1
    OnClick = btOKClick
  end
  object btCancel: TButton
    Left = 96
    Top = 40
    Width = 75
    Height = 25
    Caption = '&Cancel'
    ModalResult = 2
    TabOrder = 2
  end
  object btNMI: TButton
    Left = 96
    Top = 72
    Width = 75
    Height = 25
    Caption = '&NMI'
    ModalResult = 4
    TabOrder = 3
    OnClick = btNMIClick
  end
end
