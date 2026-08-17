; Must stay ABI-identical to guest-context.h.
guest_context struct
  ; general-purpose registers
  $rax qword ?
  $rcx qword ?
  $rdx qword ?
  $rbx qword ?
  $rsp qword ? ; guest RSP, also reused for the validated VMXOFF state pointer
  $rbp qword ?
  $rsi qword ?
  $rdi qword ?
  $r8  qword ?
  $r9  qword ?
  $r10 qword ?
  $r11 qword ?
  $r12 qword ?
  $r13 qword ?
  $r14 qword ?
  $r15 qword ?

  ; control registers
  $cr2 qword ?
  $cr8 qword ?

  ; debug registers
  $dr0 qword ?
  $dr1 qword ?
  $dr2 qword ?
  $dr3 qword ?
  $dr6 qword ?

  ; SSE registers
  $xmm0 oword ?
  $xmm1 oword ?
  $xmm2 oword ?
  $xmm3 oword ?
  $xmm4 oword ?
  $xmm5 oword ?
  $xmm6 oword ?
  $xmm7 oword ?
  $xmm8 oword ?
  $xmm9 oword ?
  $xmm10 oword ?
  $xmm11 oword ?
  $xmm12 oword ?
  $xmm13 oword ?
  $xmm14 oword ?
  $xmm15 oword ?
guest_context ends
