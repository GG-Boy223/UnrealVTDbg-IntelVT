.code

include vm-exit-context.inc

extern vmexit_handler : proc
extern handle_vmresume_failure : proc
extern vmx_fatal_error : proc

VMXOFF_STATE_COMPLETED equ 08h
VMXOFF_STATE_RIP      equ 10h
VMXOFF_STATE_RSP      equ 18h
VMXOFF_STATE_RFLAGS   equ 20h
VMXOFF_STATE_CS       equ 28h
VMXOFF_STATE_SS       equ 30h
VMXOFF_STATE_CR0      equ 38h
VMXOFF_STATE_CR4      equ 40h

VMX_FATAL_VMXOFF_INSTRUCTION equ 2
VMX_FATAL_MISSING_VCPU       equ 6

; execution starts here after a vm-exit
vm_exit proc
  ; allocate space on the stack to store the guest context
  sub rsp, 1C0h

  ; general-purpose registers
  mov guest_context.$rax[rsp], rax
  mov guest_context.$rcx[rsp], rcx
  mov guest_context.$rdx[rsp], rdx
  mov guest_context.$rbx[rsp], rbx
  mov guest_context.$rbp[rsp], rbp
  mov guest_context.$rsi[rsp], rsi
  mov guest_context.$rdi[rsp], rdi
  mov guest_context.$r8[rsp],  r8
  mov guest_context.$r9[rsp],  r9
  mov guest_context.$r10[rsp], r10
  mov guest_context.$r11[rsp], r11
  mov guest_context.$r12[rsp], r12
  mov guest_context.$r13[rsp], r13
  mov guest_context.$r14[rsp], r14
  mov guest_context.$r15[rsp], r15

  ; control registers
  mov rax, cr2
  mov guest_context.$cr2[rsp], rax
  mov rax, cr8
  mov guest_context.$cr8[rsp], rax

  ; debug registers
  ; 先把物理 DR7 收敛为 DR7_FIXED_1（仅保留 bit10），清除 guest 的 Ln/Gn 断点使能与 GD。
  ; 目的：root 执行期间物理 DRx 仍保存 guest 的断点地址（收尾会写回），若 DR7 的使能位保持
  ; guest 值，root 代码一旦命中 guest 的断点地址就会在 root 触发 #DB，落到 root IDT 三重故障
  ; 路径。VM-entry 会用 GUEST_DR7 重新载入 guest 的 DR7，因此这里不需要保存/恢复。
  ; 读 DR7 本身安全：GUEST_DR7 已在 Vmcs/VmcsGuest.cpp 与 VmExit/VmExitDebugRegisters.cpp
  ; 中剔除 GD，物理 DR7 永不携带 general-detect。
  mov rax, dr7
  and eax, 400h
  mov dr7, rax

  mov rax, dr0
  mov guest_context.$dr0[rsp], rax
  mov rax, dr1
  mov guest_context.$dr1[rsp], rax
  mov rax, dr2
  mov guest_context.$dr2[rsp], rax
  mov rax, dr3
  mov guest_context.$dr3[rsp], rax
  mov rax, dr6
  mov guest_context.$dr6[rsp], rax

  ; SSE registers
  movaps guest_context.$xmm0[rsp], xmm0
  movaps guest_context.$xmm1[rsp], xmm1
  movaps guest_context.$xmm2[rsp], xmm2
  movaps guest_context.$xmm3[rsp], xmm3
  movaps guest_context.$xmm4[rsp], xmm4
  movaps guest_context.$xmm5[rsp], xmm5
  movaps guest_context.$xmm6[rsp], xmm6
  movaps guest_context.$xmm7[rsp], xmm7
  movaps guest_context.$xmm8[rsp], xmm8
  movaps guest_context.$xmm9[rsp], xmm9
  movaps guest_context.$xmm10[rsp], xmm10
  movaps guest_context.$xmm11[rsp], xmm11
  movaps guest_context.$xmm12[rsp], xmm12
  movaps guest_context.$xmm13[rsp], xmm13
  movaps guest_context.$xmm14[rsp], xmm14
  movaps guest_context.$xmm15[rsp], xmm15

  ; first argument is the guest context
  mov rcx, rsp ;param1
  mov rbp, rsp
  
  ; 调整rsp位于 16 字节边界上
  ; 因为浮点寄存器需要16字节对齐
  and rsp, 0FFFFFFFFFFFFFFF0h
  sub rsp, 512  ;Intel手册有介绍必须是512字节的内存空间
  fxsave64 [rsp]
  mov rdx, rsp ;param2, pointer to fxsave

  ; call vmexit_handler
  ; RSP is 16-byte aligned here. Reserve the mandatory 32-byte shadow space
  ; without changing call-site alignment; CALL then enters C++ with RSP mod 16 = 8.
  sub rsp, 20h
  call vmexit_handler
  add rsp, 20h
  fxrstor64 [rsp]
  mov rsp, rbp

  ; SSE registers
  movaps xmm0, guest_context.$xmm0[rsp]
  movaps xmm1, guest_context.$xmm1[rsp]
  movaps xmm2, guest_context.$xmm2[rsp]
  movaps xmm3, guest_context.$xmm3[rsp]
  movaps xmm4, guest_context.$xmm4[rsp]
  movaps xmm5, guest_context.$xmm5[rsp]
  movaps xmm6, guest_context.$xmm6[rsp]
  movaps xmm7, guest_context.$xmm7[rsp]
  movaps xmm8, guest_context.$xmm8[rsp]
  movaps xmm9, guest_context.$xmm9[rsp]
  movaps xmm10, guest_context.$xmm10[rsp]
  movaps xmm11, guest_context.$xmm11[rsp]
  movaps xmm12, guest_context.$xmm12[rsp]
  movaps xmm13, guest_context.$xmm13[rsp]
  movaps xmm14, guest_context.$xmm14[rsp]
  movaps xmm15, guest_context.$xmm15[rsp]

  ; 如果要停止虚拟化，vmexit_handler 将返回 true
  mov r15, rax

  ; debug registers
  mov rax, guest_context.$dr0[rsp]
  mov dr0, rax
  mov rax, guest_context.$dr1[rsp]
  mov dr1, rax
  mov rax, guest_context.$dr2[rsp]
  mov dr2, rax
  mov rax, guest_context.$dr3[rsp]
  mov dr3, rax
  mov rax, guest_context.$dr6[rsp]
  mov dr6, rax

  ; control registers
  mov rax, guest_context.$cr2[rsp]
  mov cr2, rax
  mov rax, guest_context.$cr8[rsp]
  mov cr8, rax

  ; general-purpose registers
  mov rax, guest_context.$rax[rsp]
  mov rcx, guest_context.$rcx[rsp]
  mov rdx, guest_context.$rdx[rsp]
  mov rbx, guest_context.$rbx[rsp]
  mov rbp, guest_context.$rbp[rsp]
  mov rsi, guest_context.$rsi[rsp]
  mov rdi, guest_context.$rdi[rsp]
  mov r8,  guest_context.$r8[rsp]
  mov r9,  guest_context.$r9[rsp]
  mov r10, guest_context.$r10[rsp]
  mov r11, guest_context.$r11[rsp]
  mov r12, guest_context.$r12[rsp]
  mov r13, guest_context.$r13[rsp]
  mov r14, guest_context.$r14[rsp]

  ; check the return value of handle_vm_exit() to see if we should terminate
  ; the virtual machine
  test r15b, r15b
  mov r15, guest_context.$r15[rsp]
  jnz stop_virtualization

  ; if handle_exit returned false, perform a vm-enter as usual
  vmresume
  ; 旧逻辑：VMRESUME失败后直接落入stop_virtualization，没有保存VMfail类型和关键VMCS字段。
  ; 新逻辑：仅在VMRESUME返回时写内存快照，不做文件IO，然后继续原来的VMXOFF/iret路径。
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  pushfq
  pop rcx
  ; The register-save sequence leaves RSP 16-byte aligned at this call site.
  sub rsp, 20h
  call handle_vmresume_failure
  add rsp, 20h
  mov r15, rax
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  mov guest_context.$rsp[rsp], r15
  mov r15, guest_context.$r15[rsp]
  jmp stop_virtualization

stop_virtualization:
  ; we'll be dirtying these registers in order to setup the
  ; stack so we need to store and restore them before we can use them.
  ; also note that we're not allocating any stack space for the trap
  ; frame since we can just reuse the space allocated for the guest
  ; context.
  ; 我们将弄脏这些寄存器以便设置栈，因此我们需要在使用它们之前存储和恢复它们。
  ; 另请注意，我们没有为陷阱帧分配任何栈空间，因为我们可以重用为guest上下文分配的空间。
  ; 以下开始填充_MACHINE_FRAME结构
  push rax
  push rdx
  push rbp
  mov rdx, guest_context.$rsp[rsp + 18h]
  test rdx, rdx
  jz stop_virtualization_missing_state
  lea rbp, [rsp + 38h]

  ; push SS
  mov rax, [rdx + VMXOFF_STATE_SS]
  mov [rbp - 00h], rax

  ; push RSP
  mov rax, [rdx + VMXOFF_STATE_RSP]
  mov [rbp - 08h], rax

  ; push RFLAGS
  mov rax, [rdx + VMXOFF_STATE_RFLAGS]
  mov [rbp - 10h], rax

  ; push CS
  mov rax, [rdx + VMXOFF_STATE_CS]
  mov [rbp - 18h], rax

  ; push RIP
  mov rax, [rdx + VMXOFF_STATE_RIP]
  mov [rbp - 20h], rax

  ; the C++ exit-handler needs to ensure that the control register shadows
  ; contain the current guest control register values (even the guest-owned
  ; bits!) before returning.

  ; Keep the validated recovery frame across VMXOFF. RBP is restored below.
  mov rbp, rdx
  vmxoff
  jbe stop_virtualization_vmxoff_failed
  mov qword ptr [rbp + VMXOFF_STATE_COMPLETED], 1

  ; restore cr0 and cr4
  mov rax, [rbp + VMXOFF_STATE_CR0]
  mov cr0, rax
  mov rax, [rbp + VMXOFF_STATE_CR4]
  mov cr4, rax

  ; restore the dirty registers
  pop rbp
  pop rdx
  pop rax

  ; we use iretq in order to do the following all in one instruction:
  ; 我们使用 iretq 来在一条指令中完成以下所有操作：
  ;
  ;   pop RIP
  ;   pop CS
  ;   pop RFLAGS
  ;   pop RSP
  ;   pop SS
  ;
  iretq

stop_virtualization_missing_state:
  mov rcx, VMX_FATAL_MISSING_VCPU
  xor rdx, rdx
  xor r8, r8
  sub rsp, 20h
  call vmx_fatal_error
  int 3

stop_virtualization_vmxoff_failed:
  pushfq
  pop rdx
  mov r8, [rbp + VMXOFF_STATE_RIP]
  mov rcx, VMX_FATAL_VMXOFF_INSTRUCTION
  sub rsp, 20h
  call vmx_fatal_error
  int 3

vm_exit endp

end
