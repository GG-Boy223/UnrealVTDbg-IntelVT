object frmCardCode: TfrmCardCode
  Left = 0
  Top = 0
  BorderIcons = [biSystemMenu, biMinimize]
  BorderStyle = bsSingle
  Caption = #21345#23494#27880#20876
  ClientHeight = 96
  ClientWidth = 397
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Segoe UI'
  Font.Style = []
  TextHeight = 15
  object Label1: TLabel
    Left = 11
    Top = 19
    Width = 63
    Height = 15
    Caption = #35831#36755#20837#21345#23494':'
  end
  object Label2: TLabel
    Left = 56
    Top = 68
    Width = 231
    Height = 15
    Caption = #27880#20876#21345#23494#25104#21151#21518#20250#33258#21160#29983#25104'License.key'#25991#20214'!'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clRed
    Font.Height = -12
    Font.Name = 'Segoe UI'
    Font.Style = []
    ParentFont = False
  end
  object Edit1: TEdit
    Left = 80
    Top = 16
    Width = 305
    Height = 23
    TabOrder = 0
  end
  object Button1: TButton
    Left = 310
    Top = 64
    Width = 75
    Height = 25
    Caption = #27880#20876
    TabOrder = 1
    OnClick = Button1Click
  end
end
