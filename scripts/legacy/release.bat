@echo off
set "TargetDir=-VTDebugger"

rmdir /S /Q "%TargetDir%"

mkdir "%TargetDir%\x64"
mkdir "%TargetDir%\x64\res"

xcopy /s /q x64\Release\*.exe "%TargetDir%\x64\"
xcopy /s /q x64\Release\*.dll "%TargetDir%\x64\"
copy /y x64\Release\DbgkSysWin10.sys "%TargetDir%\x64\"
copy /y x64\Release\DbgkSysWin11.sys "%TargetDir%\x64\"
copy /y x64\Release\VT_Driver.sys "%TargetDir%\x64\"

xcopy /s /q _deps "%TargetDir%\x64\"
