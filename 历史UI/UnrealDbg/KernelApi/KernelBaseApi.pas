unit KernelBaseApi;

interface

uses Winapi.Windows;

type
  TVirtualProtectEx = function(hProcess: THandle; lpAddress: Pointer; dwSize: DWORD; flNewProtect: DWORD; lpflOldProtect: Pointer): BOOL; stdcall;

var
  WindowsKernelBase: THandle;
  Sys_VirtualProtectEx: TVirtualProtectEx;

implementation

initialization
begin
  WindowsKernelBase := LoadLibrary('KernelBase.dll');
  Sys_VirtualProtectEx := GetProcAddress(WindowsKernelBase,'VirtualProtectEx');
end;

end.
