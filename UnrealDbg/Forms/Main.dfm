object Form1: TForm1
  Left = 0
  Top = 0
  BorderIcons = [biSystemMenu, biMinimize]
  BorderStyle = bsSingle
  Caption = #34394#24187#35843#35797#22120'   by: F3  '
  ClientHeight = 510
  ClientWidth = 882
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Segoe UI'
  Font.Style = []
  OnCloseQuery = FormCloseQuery
  OnCreate = FormCreate
  OnDestroy = FormDestroy
  PixelsPerInch = 96
  TextHeight = 15
  object RichEdit1: TRichEdit
    Left = 8
    Top = 282
    Width = 866
    Height = 220
    BorderWidth = 2
    Color = clBlack
    Font.Charset = GB2312_CHARSET
    Font.Color = clLime
    Font.Height = -12
    Font.Name = #23435#20307
    Font.Style = []
    Lines.Strings = (
      'RichEdit1')
    ParentFont = False
    ReadOnly = True
    ScrollBars = ssVertical
    TabOrder = 0
  end
  object GroupBox1: TGroupBox
    Left = 8
    Top = 8
    Width = 401
    Height = 121
    Caption = #31995#32479#20449#24687
    TabOrder = 1
    object Label_SystemName: TLabel
      Left = 16
      Top = 24
      Width = 55
      Height = 15
      Caption = #31995#32479#21517#31216':'
    end
    object Label_SystemVer: TLabel
      Left = 16
      Top = 56
      Width = 55
      Height = 15
      Caption = #31995#32479#29256#26412':'
    end
    object Label_CPU: TLabel
      Left = 17
      Top = 88
      Width = 52
      Height = 15
      Caption = 'CPU'#22411#21495':'
    end
  end
  object GroupBox2: TGroupBox
    Left = 8
    Top = 135
    Width = 401
    Height = 137
    Caption = #21151#33021#21306#22495
    TabOrder = 2
    object CheckBox1: TCheckBox
      Left = 17
      Top = 24
      Width = 97
      Height = 17
      Caption = #20445#25252#35843#35797#22120
      Checked = True
      State = cbChecked
      TabOrder = 0
    end
    object CheckBox2: TCheckBox
      Left = 17
      Top = 70
      Width = 97
      Height = 17
      Caption = #26080#30165#36719#20214#26029#28857
      Checked = True
      State = cbChecked
      TabOrder = 1
    end
    object CheckBox3: TCheckBox
      Left = 17
      Top = 47
      Width = 97
      Height = 17
      Caption = #26080#30165#30828#20214#26029#28857
      Checked = True
      State = cbChecked
      TabOrder = 2
    end
    object CheckBox4: TCheckBox
      Left = 17
      Top = 93
      Width = 97
      Height = 17
      Caption = #24322#24120#36807#28388
      Checked = True
      State = cbChecked
      TabOrder = 3
    end
    object CheckBox5: TCheckBox
      Left = 17
      Top = 116
      Width = 97
      Height = 17
      Caption = #38543#26426#36827#31243#21517
      TabOrder = 4
    end
    object CheckBox6: TCheckBox
      Left = 158
      Top = 24
      Width = 115
      Height = 17
      Caption = #24378#21046#21093#31163#35843#35797#22120
      Checked = True
      State = cbChecked
      TabOrder = 5
    end
    object EnterVTDebuggingMode: TButton
      Left = 155
      Top = 61
      Width = 166
      Height = 63
      Caption = #36827#20837'VT'#35843#35797#27169#24335
      TabOrder = 6
      OnClick = EnterVTDebuggingModeClick
    end
  end
  object PageControl1: TPageControl
    Left = 432
    Top = 8
    Width = 442
    Height = 268
    ActivePage = TabSheet2
    TabOrder = 3
    object TabSheet1: TTabSheet
      Caption = #35843#35797#22120#21015#34920
      object DbgListView: TListView
        Left = 3
        Top = 3
        Width = 428
        Height = 232
        Columns = <
          item
            Caption = #24207#21495
          end
          item
            Caption = #35843#35797#22120
            Width = 200
          end
          item
            Caption = #25991#20214#36335#24452
            Width = 500
          end>
        GridLines = True
        ReadOnly = True
        RowSelect = True
        TabOrder = 0
        ViewStyle = vsReport
        OnMouseDown = DbgListViewMouseDown
      end
    end
    object TabSheet2: TTabSheet
      Caption = #23450#21046#21270#23545#25239
      ImageIndex = 1
      DesignSize = (
        434
        238)
      object pgcConfrontation: TPageControl
        Left = 3
        Top = 3
        Width = 428
        Height = 232
        ActivePage = TabSheet3
        Anchors = [akLeft, akTop, akRight, akBottom]
        TabOrder = 0
        object TabSheet3: TTabSheet
          Caption = 'TL('#29579#26435#19982#33258#30001')'
          object Image1: TImage
            Left = 369
            Top = 167
            Width = 32
            Height = 32
            AutoSize = True
            Picture.Data = {
              055449636F6E0000010001002020100000000000E80200001600000028000000
              2000000040000000010004000000000080020000000000000000000000000000
              0000000000000000000080000080000000808000800000008000800080800000
              80808000C0C0C0000000FF0000FF000000FFFF00FF000000FF00FF00FFFF0000
              FFFFFF0000000000000000000000000000000000000000000000000000000000
              0000000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFF000FFFFFFFFFFFFFFFFFFFFFF
              FFFFFFF000FFFFFFFFFFFF00000FFFFFFFFFFFF000FFFFFFFFFF00FF0FF00FFF
              FFFFFFF000FFFFFFFFF0FF00000FF0FFFFFFFFF000FFFFFFFF0FFFFF0FFFFF0F
              FFFFFFF000FFFFFFF0FFFF00000FFFF0FFFFFFF000FFFFFFF0FFFFFF0FFFFFF0
              FFFFFFF000FFFFFF0F0F0FF000FF0F0F0FFFFFF000FFFFFF0F0F0F0FFF0F0F0F
              0FFFFFF000FFFFFF0000000F0F0000000FFFFFF000FFFFFF0F0F0F0FFF0F0F0F
              0FFFFFF000FFFFFF0F0F0FF000FF0F0F0FFFFFF000FFFFFFF0FFFFFF0FFFFFF0
              FFFFFFF000FFFFFFF0FFFF00000FFFF0FFFFFFF000FFFFFFFF0FFFFF0FFFFF0F
              FFFFFFF000FFFFFFFFF0FF00000FF0FFFFFFFFF000FFFFFFFFFF00FF0FF00FFF
              FFFFFFF000FFFFFFFFFFFF00000FFFFFFFFFFFF000FFFFFFFFFFFFFFFFFFFFFF
              FFFFFFF000FFFFFFFFFFFFFFFFFFFFFFFFFFFFF000FFFFFFFFFFFFFFFFFFFFFF
              FFFFFFF000000000000000000000000000000000008880CCCCCCCCCCCCCCCCCC
              CCC08880008080CCCCCCCCCCCCCCCCCCCCC08080008880CCCCCCCCCCCCCCCCCC
              CCC0888000000000000000000000000000000000000000000000000000000000
              0000000000000000000000000000000000000000000000000000000000000000
              00000000FFFFFFFF800000008000000080000000800000008000000080000000
              8000000080000000800000008000000080000000800000008000000080000000
              8000000080000000800000008000000080000000800000008000000080000000
              800000008000000080000000800000008000000080000000FFFFFFFFFFFFFFFF
              FFFFFFFF}
            OnMouseDown = Image1MouseDown
            OnMouseMove = Image1MouseMove
            OnMouseUp = Image1MouseUp
          end
          object Label1: TLabel
            Left = 260
            Top = 178
            Width = 107
            Height = 15
            Caption = #25302#21160#36873#25321#30446#26631#31383#21475':'
          end
          object Label2: TLabel
            Left = 17
            Top = 135
            Width = 256
            Height = 45
            Caption = #27880#24847': '#23450#21046#21270#23545#25239#38656#35201#23454#26102#20851#27880#28216#25103#26356#26032#65292' '#33509#13#10#28216#25103#26356#26032#21518#23545#25239#21487#33021#24050#32463#22833#25928#12290#13#10#35831#32852#31995#31649#29702#20154#21592#65281
            Font.Charset = DEFAULT_CHARSET
            Font.Color = clRed
            Font.Height = -12
            Font.Name = 'Segoe UI'
            Font.Style = []
            ParentFont = False
          end
          object TL_EnabledConfrontat: TCheckBox
            Left = 16
            Top = 3
            Width = 137
            Height = 17
            Caption = #21551#21160#23450#21046#21270#23545#25239
            TabOrder = 0
            OnClick = TL_EnabledConfrontatClick
          end
          object GroupBox3: TGroupBox
            Left = 19
            Top = 33
            Width = 174
            Height = 96
            Caption = #23450#21046#21270#23545#25239
            TabOrder = 1
            object TL_HandlerGetTickCountCheck: TCheckBox
              Left = 16
              Top = 32
              Width = 169
              Height = 17
              Caption = #22788#29702'GetTickCount'#26816#27979
              TabOrder = 0
              OnClick = TL_HandlerGetTickCountCheckClick
            end
            object TL_BlockResumeThread: TCheckBox
              Left = 16
              Top = 56
              Width = 137
              Height = 17
              Caption = #38459#27490#28216#25103#24674#22797#32447#31243
              Enabled = False
              TabOrder = 1
              OnClick = TL_BlockResumeThreadClick
            end
          end
        end
      end
    end
  end
  object OpenDialog1: TOpenDialog
    Left = 796
    Top = 82
  end
  object DebuggerPopupMenu: TPopupMenu
    Left = 684
    Top = 90
    object StartDebuggerMenu: TMenuItem
      Caption = #21551#21160
      OnClick = StartDebuggerMenuClick
    end
    object AddDebuggerMenu: TMenuItem
      Caption = #28155#21152
      OnClick = AddDebuggerMenuClick
    end
    object DelDebuggerMenu: TMenuItem
      Caption = #31227#38500
      OnClick = DelDebuggerMenuClick
    end
  end
end
