object Form2: TForm2
  Left = 0
  Top = 0
  BorderIcons = [biSystemMenu, biMinimize]
  BorderStyle = bsSingle
  Caption = #31526#21495#19979#36733#35774#32622
  ClientHeight = 96
  ClientWidth = 392
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Segoe UI'
  Font.Style = []
  OnCreate = FormCreate
  TextHeight = 15
  object RadioButtonMirrorServer: TRadioButton
    Left = 24
    Top = 16
    Width = 81
    Height = 17
    Caption = #38236#20687#26381#21153#22120
    TabOrder = 0
    OnClick = RadioButtonMirrorServerClick
  end
  object RadioButtonMicrosoftServer: TRadioButton
    Left = 128
    Top = 16
    Width = 89
    Height = 17
    Caption = #24494#36719#26381#21153#22120
    TabOrder = 1
    OnClick = RadioButtonMicrosoftServerClick
  end
  object RadioButtonCustomServer: TRadioButton
    Left = 24
    Top = 52
    Width = 89
    Height = 17
    Caption = #33258#23450#20041#26381#21153#22120
    TabOrder = 2
    OnClick = RadioButtonCustomServerClick
  end
  object EditCustomServer: TEdit
    Left = 128
    Top = 49
    Width = 241
    Height = 23
    TabOrder = 3
    OnChange = EditCustomServerChange
  end
end
