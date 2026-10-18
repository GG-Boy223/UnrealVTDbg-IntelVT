.code

HOST_EXCEPTION_RECOVERY_MAGIC equ 4856585245434F56h

public memcpy_safe

memcpy_safe proc
  mov r10, ehandler
  mov r11, rcx
  mov rax, HOST_EXCEPTION_RECOVERY_MAGIC
  mov qword ptr [r11 + 18h], rax
  mov byte ptr [rcx], 0

  ; store RSI and RDI
  push rsi
  push rdi

  mov rsi, r8
  mov rdi, rdx
  mov rcx, r9

  rep movsb

ehandler:
  mov qword ptr [r11 + 18h], 0
  ; restore RDI and RSI
  pop rdi
  pop rsi

  ret
memcpy_safe endp

xsetbv_safe proc
  mov r10, ehandler
  mov r11, rcx
  mov rax, HOST_EXCEPTION_RECOVERY_MAGIC
  mov qword ptr [r11 + 18h], rax
  mov byte ptr [rcx], 0

  ; idx
  mov ecx, edx

  ; value (low part)
  mov eax, r8d

  ; value (high part)
  mov rdx, r8
  shr rdx, 32

  xsetbv

ehandler:
  mov qword ptr [r11 + 18h], 0
  ret
xsetbv_safe endp

wrmsr_safe proc
  mov r10, ehandler
  mov r11, rcx
  mov rax, HOST_EXCEPTION_RECOVERY_MAGIC
  mov qword ptr [r11 + 18h], rax
  mov byte ptr [rcx], 0

  ; msr
  mov ecx, edx

  ; value
  mov eax, r8d
  mov rdx, r8
  shr rdx, 32

  wrmsr

ehandler:
  mov qword ptr [r11 + 18h], 0
  ret
wrmsr_safe endp

rdmsr_safe proc
  mov r10, ehandler
  mov r11, rcx
  mov rax, HOST_EXCEPTION_RECOVERY_MAGIC
  mov qword ptr [r11 + 18h], rax
  mov byte ptr [rcx], 0

  ; msr
  mov ecx, edx

  rdmsr

  ; return value
  shl rdx, 32
  ; RDMSR returns EDX:EAX. The old AND lost most legal MSR values and could make
  ; VM-exit handlers consume corrupted MSR data on newer Intel CPUs.
  ; and rax, rdx
  or rax, rdx

ehandler:
  mov qword ptr [r11 + 18h], 0
  ret
rdmsr_safe endp

end
