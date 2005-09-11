object Form1: TForm1
  Left = 759
  Top = 446
  BorderIcons = [biSystemMenu, biMinimize]
  BorderStyle = bsSingle
  Caption = 'Virtual Machine Output'
  ClientHeight = 324
  ClientWidth = 645
  Color = clMenu
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clBtnText
  Font.Height = -13
  Font.Name = 'Arial'
  Font.Style = []
  OldCreateOrder = False
  PixelsPerInch = 96
  TextHeight = 16
  object Label1: TLabel
    Left = 312
    Top = 24
    Width = 47
    Height = 16
    Caption = 'Display:'
  end
  object Image1: TPaintBox
    Left = 312
    Top = 48
    Width = 320
    Height = 240
    OnPaint = Image1Paint
  end
  object Button1: TButton
    Left = 8
    Top = 8
    Width = 75
    Height = 25
    Caption = 'Debug'
    TabOrder = 0
    OnClick = Button1Click
  end
  object bt1: TButton
    Tag = 65
    Left = 8
    Top = 88
    Width = 49
    Height = 25
    Caption = '1'
    TabOrder = 1
    OnClick = LastKeyPress
  end
  object bt2: TButton
    Tag = 33
    Left = 64
    Top = 88
    Width = 49
    Height = 25
    Caption = '2'
    TabOrder = 2
    OnClick = LastKeyPress
  end
  object bt3: TButton
    Tag = 17
    Left = 120
    Top = 88
    Width = 49
    Height = 25
    Caption = '3'
    TabOrder = 3
    OnClick = LastKeyPress
  end
  object bt4: TButton
    Tag = 72
    Left = 8
    Top = 120
    Width = 49
    Height = 25
    Caption = '4'
    TabOrder = 4
    OnClick = LastKeyPress
  end
  object bt5: TButton
    Tag = 40
    Left = 64
    Top = 120
    Width = 49
    Height = 25
    Caption = '5'
    TabOrder = 5
    OnClick = LastKeyPress
  end
  object bt6: TButton
    Tag = 24
    Left = 120
    Top = 120
    Width = 49
    Height = 25
    Caption = '6'
    TabOrder = 6
    OnClick = LastKeyPress
  end
  object bt7: TButton
    Tag = 68
    Left = 8
    Top = 152
    Width = 49
    Height = 25
    Caption = '7'
    TabOrder = 7
    OnClick = LastKeyPress
  end
  object bt8: TButton
    Tag = 36
    Left = 64
    Top = 152
    Width = 49
    Height = 25
    Caption = '8'
    TabOrder = 8
    OnClick = LastKeyPress
  end
  object bt9: TButton
    Tag = 20
    Left = 120
    Top = 152
    Width = 49
    Height = 25
    Caption = '9'
    TabOrder = 9
    OnClick = LastKeyPress
  end
  object bt0: TButton
    Tag = 34
    Left = 64
    Top = 184
    Width = 49
    Height = 25
    Caption = '0'
    TabOrder = 10
    OnClick = LastKeyPress
  end
  object btEnter: TButton
    Tag = 18
    Left = 120
    Top = 184
    Width = 49
    Height = 25
    Caption = 'Enter'
    TabOrder = 11
    OnClick = LastKeyPress
  end
  object btClear: TButton
    Tag = 66
    Left = 8
    Top = 184
    Width = 49
    Height = 25
    Caption = 'Clear'
    TabOrder = 12
    OnClick = LastKeyPress
  end
end
