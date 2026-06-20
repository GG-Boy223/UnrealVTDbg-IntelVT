.data
extern DbgkCreateThread_jcc_instruction : dq
extern PspExitThread_jcc_instruction : dq

.code

public Asm_DbgkCreateThread_CMP_Debugport_01
public Asm_PspExitThread_CMP_Debugport_01

SAVE_Context macro
	push rax
	push rbx
	push rcx
	push rdx
	push rsi
	push rdi
	push rbp
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	pushfq
endm

RESTORE_Context macro
	popfq
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rbp
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rbx
	pop rax
endm

Asm_DbgkCreateThread_CMP_Debugport_01 proc
	SAVE_Context
	sub rsp, 40h
	mov r8, rsp
	mov rax, rsp
	add rax, 0C0h
	mov rcx, 0
	mov rbx, 0d7b32a726e23dbc2h  ;DbgkCreateThread 经16位md5哈希摘要
loop_start:
	cmp qword ptr [rax + rcx], rbx
	jz loop_end
	add rcx,8
	jmp loop_start
loop_end:
	lea rax,[rax + rcx + 8]  ;&s.DebugPort
	cmp qword ptr [rax], 0 ;if ( Process->DebugPort )
	pushfq
	pop rbx
	mov [r8 + 40h], rbx  ;覆盖原栈的rflags
	mov rsp, r8
	add rsp, 40h
	RESTORE_Context
	push DbgkCreateThread_jcc_instruction
	ret
Asm_DbgkCreateThread_CMP_Debugport_01 endp

Asm_PspExitThread_CMP_Debugport_01 proc
	SAVE_Context
	sub rsp, 40h
	mov r8, rsp
	mov rax, rsp
	add rax, 0C0h
	mov rcx, 0
	mov rbx, 0fbbd6252520ea3d5h  ;PspExitThread 经16位md5哈希摘要
loop_start:
	cmp qword ptr [rax + rcx], rbx
	jz loop_end
	add rcx,8
	jmp loop_start
loop_end:
	lea rax,[rax + rcx + 8]  ;&s.DebugPort
	cmp qword ptr [rax], 0 ;if ( Process->DebugPort )
	pushfq
	pop rbx
	mov [r8 + 40h], rbx  ;覆盖原栈的rflags
	mov rsp, r8
	add rsp, 40h
	RESTORE_Context
	push PspExitThread_jcc_instruction
	ret
Asm_PspExitThread_CMP_Debugport_01 endp

end