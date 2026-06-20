object Form3: TForm3
  Left = 0
  Top = 0
  BorderIcons = [biSystemMenu, biMinimize]
  BorderStyle = bsSingle
  Caption = 'D'#21152#23494
  ClientHeight = 150
  ClientWidth = 563
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Segoe UI'
  Font.Style = []
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 15
  object Label1: TLabel
    Left = 16
    Top = 32
    Width = 94
    Height = 15
    Caption = #34394#24187#35843#35797#22120#26631#39064':'
  end
  object Label2: TLabel
    Left = 16
    Top = 80
    Width = 60
    Height = 15
    Caption = 'QQ'#32676#26085#24535':'
  end
  object Label3: TLabel
    Left = 168
    Top = 120
    Width = 198
    Height = 15
    Caption = #21487#23558'copyright.db'#25991#20214#25302#25341#36827#26469#35299#23494
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clRed
    Font.Height = -12
    Font.Name = 'Segoe UI'
    Font.Style = []
    ParentFont = False
  end
  object Edit1: TEdit
    Left = 109
    Top = 29
    Width = 436
    Height = 23
    TabOrder = 0
  end
  object Edit2: TEdit
    Left = 109
    Top = 77
    Width = 436
    Height = 23
    TabOrder = 1
  end
  object Button1: TButton
    Left = 454
    Top = 120
    Width = 91
    Height = 25
    Caption = #29983#25104#21152#23494#25991#20214
    TabOrder = 2
    OnClick = Button1Click
  end
end
