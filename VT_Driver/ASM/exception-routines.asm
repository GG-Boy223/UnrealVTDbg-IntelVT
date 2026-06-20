.code

public memcpy_safe

memcpy_safe proc
  mov r10, ehandler
  mov r11, rcx
  mov byte ptr [rcx], 0

  ; store RSI and RDI
  push rsi
  push rdi

  mov rsi, r8
  mov rdi, rdx
  mov rcx, r9

  rep movsb

ehandler:
  ; restore RDI and RSI
  pop rdi
  pop rsi

  ret
memcpy_safe endp

xsetbv_safe proc
  mov r10, ehandler
  mov r11, rcx
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
  ret
xsetbv_safe endp

wrmsr_safe proc
  mov r10, ehandler
  mov r11, rcx
  mov byte ptr [rcx], 0

  ; msr
  mov ecx, edx

  ; value
  mov eax, r8d
  mov rdx, r8
  shr rdx, 32

  wrmsr

ehandler:
  ret
wrmsr_safe endp

rdmsr_safe proc
  mov r10, ehandler
  mov r11, rcx
  mov byte ptr [rcx], 0

  ; msr
  mov ecx, edx

  rdmsr

  ; return value
  shl rdx, 32
  and rax, rdx

ehandler:
  ret
rdmsr_safe endp

end

