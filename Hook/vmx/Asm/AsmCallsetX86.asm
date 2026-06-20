.386
.model flat, stdcall
.stack 4096

.code

public __vm_call

__vm_call proc vmcall_reason: DWORD, vmcallinfo: DWORD
	push ecx
	push edx
    mov eax,066666666h
	mov ecx, vmcall_reason
	mov edx, vmcallinfo
	;vmcall
	db 0Fh
	db 01h
	db 0C1h
	pop edx
	pop ecx
    ret
__vm_call endp

end