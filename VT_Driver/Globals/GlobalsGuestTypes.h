#pragma once

#include "GlobalsCommonTypes.h"

// contains state that isn't stored in guest vmcs fields
struct alignas(16) guest_context // 来宾上下文结构（VM-Exit 时保存）
{
    union
    {
        uint64_t gpr[16]; // 16 个通用寄存器数组视图

        // aliases for general-purpose registers
        struct
        {
            union
            {
                uint64_t rax; // RAX（64 位）
                uint32_t eax; // EAX（32 位）
                uint16_t ax; // AX（16 位）
                uint8_t  al; // AL（8 位）
            };
            union
            {
                uint64_t rcx; // RCX（64 位）
                uint32_t ecx; // ECX（32 位）
                uint16_t cx; // CX（16 位）
                uint8_t  cl; // CL（8 位）
            };
            union
            {
                uint64_t rdx; // RDX（64 位）
                uint32_t edx; // EDX（32 位）
                uint16_t dx; // DX（16 位）
                uint8_t  dl; // DL（8 位）
            };
            union
            {
                uint64_t rbx; // RBX（64 位）
                uint32_t ebx; // EBX（32 位）
                uint16_t bx; // BX（16 位）
                uint8_t  bl; // BL（8 位）
            };

            // this is where RSP would be if it wasn't saved in the vmcs
            uint64_t rsp; // RSP（VMCS 已保存，但这里保留占位）

            union
            {
                uint64_t rbp; // RBP（64 位）
                uint32_t ebp; // EBP（32 位）
                uint16_t bp; // BP（16 位）
                uint8_t  bpl; // BPL（8 位）
            };
            union
            {
                uint64_t rsi; // RSI（64 位）
                uint32_t esi; // ESI（32 位）
                uint16_t si; // SI（16 位）
                uint8_t  sil; // SIL（8 位）
            };
            union
            {
                uint64_t rdi; // RDI（64 位）
                uint32_t edi; // EDI（32 位）
                uint16_t di; // DI（16 位）
                uint8_t  dil; // DIL（8 位）
            };
            union
            {
                uint64_t r8; // R8（64 位）
                uint32_t r8d; // R8D（32 位）
                uint16_t r8w; // R8W（16 位）
                uint8_t  r8b; // R8B（8 位）
            };
            union
            {
                uint64_t r9; // R9（64 位）
                uint32_t r9d; // R9D（32 位）
                uint16_t r9w; // R9W（16 位）
                uint8_t  r9b; // R9B（8 位）
            };
            union
            {
                uint64_t r10; // R10（64 位）
                uint32_t r10d; // R10D（32 位）
                uint16_t r10w; // R10W（16 位）
                uint8_t  r10b; // R10B（8 位）
            };
            union
            {
                uint64_t r11; // R11（64 位）
                uint32_t r11d; // R11D（32 位）
                uint16_t r11w; // R11W（16 位）
                uint8_t  r11b; // R11B（8 位）
            };
            union
            {
                uint64_t r12; // R12（64 位）
                uint32_t r12d; // R12D（32 位）
                uint16_t r12w; // R12W（16 位）
                uint8_t  r12b; // R12B（8 位）
            };
            union
            {
                uint64_t r13; // R13（64 位）
                uint32_t r13d; // R13D（32 位）
                uint16_t r13w; // R13W（16 位）
                uint8_t  r13b; // R13B（8 位）
            };
            union
            {
                uint64_t r14; // R14（64 位）
                uint32_t r14d; // R14D（32 位）
                uint16_t r14w; // R14W（16 位）
                uint8_t  r14b; // R14B（8 位）
            };
            union
            {
                uint64_t r15; // R15（64 位）
                uint32_t r15d; // R15D（32 位）
                uint16_t r15w; // R15W（16 位）
                uint8_t  r15b; // R15B（8 位）
            };
        };
    };

    // control registers
    uint64_t cr2; // CR2（页故障地址）
    uint64_t cr8; // CR8（TPR）

    // debug registers
    uint64_t dr0; // DR0（硬件断点 0）
    uint64_t dr1; // DR1（硬件断点 1）
    uint64_t dr2; // DR2（硬件断点 2）
    uint64_t dr3; // DR3（硬件断点 3）
    uint64_t dr6; // DR6（调试状态）

    // SSE registers
    M128A xmm0; // XMM0
    M128A xmm1; // XMM1
    M128A xmm2; // XMM2
    M128A xmm3; // XMM3
    M128A xmm4; // XMM4
    M128A xmm5; // XMM5
    M128A xmm6; // XMM6
    M128A xmm7; // XMM7
    M128A xmm8; // XMM8
    M128A xmm9; // XMM9
    M128A xmm10; // XMM10
    M128A xmm11; // XMM11
    M128A xmm12; // XMM12
    M128A xmm13; // XMM13
    M128A xmm14; // XMM14
    M128A xmm15; // XMM15
};

// remember to update this value in vm-exit.asm
static_assert(FIELD_OFFSET(guest_context, cr2) == 0x80, "guest_context.cr2 ABI offset must match vm-exit-context.inc");
static_assert(FIELD_OFFSET(guest_context, dr6) == 0xB0, "guest_context.dr6 ABI offset must match vm-exit-context.inc");
static_assert(FIELD_OFFSET(guest_context, xmm0) == 0xC0, "guest_context.xmm0 ABI offset must match vm-exit-context.inc");
static_assert(FIELD_OFFSET(guest_context, xmm15) == 0x1B0, "guest_context.xmm15 ABI offset must match vm-exit-context.inc");
static_assert(sizeof(guest_context) == 0x1C0); // 保证结构大小与汇编保存/恢复一致
//size: 512
typedef struct _FXSAVE64 // FXSAVE64 布局（512 字节），保存 FPU/SSE 状态
{
    unsigned short FCW; // FPU 控制字
    unsigned short FSW; // FPU 状态字
    unsigned char FTW; // FPU 标记字
    unsigned char Reserved; // 保留对齐
    unsigned short FOP; // 最后一次 FPU 操作码
    UINT64 FPU_IP; // FPU 指令指针
    UINT64 FPU_DP; // FPU 数据指针
    unsigned int MXCSR; // SSE 控制/状态寄存器
    unsigned int MXCSR_MASK; // MXCSR 支持的位掩码
    unsigned __int64 FP_MM0; // MM0 低 64 位
    unsigned __int64 FP_MM0_H; // MM0 高 64 位
    unsigned __int64 FP_MM1; // MM1 低 64 位
    unsigned __int64 FP_MM1_H; // MM1 高 64 位
    unsigned __int64 FP_MM2; // MM2 低 64 位
    unsigned __int64 FP_MM2_H; // MM2 高 64 位
    unsigned __int64 FP_MM3; // MM3 低 64 位
    unsigned __int64 FP_MM3_H; // MM3 高 64 位
    unsigned __int64 FP_MM4; // MM4 低 64 位
    unsigned __int64 FP_MM4_H; // MM4 高 64 位
    unsigned __int64 FP_MM5; // MM5 低 64 位
    unsigned __int64 FP_MM5_H; // MM5 高 64 位
    unsigned __int64 FP_MM6; // MM6 低 64 位
    unsigned __int64 FP_MM6_H; // MM6 高 64 位
    unsigned __int64 FP_MM7; // MM7 低 64 位
    unsigned __int64 FP_MM7_H; // MM7 高 64 位
    unsigned __int64 XMM0; // XMM0 低 64 位
    unsigned __int64 XMM0_H; // XMM0 高 64 位
    unsigned __int64 XMM1; // XMM1 低 64 位
    unsigned __int64 XMM1_H; // XMM1 高 64 位
    unsigned __int64 XMM2; // XMM2 低 64 位
    unsigned __int64 XMM2_H; // XMM2 高 64 位
    unsigned __int64 XMM3; // XMM3 低 64 位
    unsigned __int64 XMM3_H; // XMM3 高 64 位
    unsigned __int64 XMM4; // XMM4 低 64 位
    unsigned __int64 XMM4_H; // XMM4 高 64 位
    unsigned __int64 XMM5; // XMM5 低 64 位
    unsigned __int64 XMM5_H; // XMM5 高 64 位
    unsigned __int64 XMM6; // XMM6 低 64 位
    unsigned __int64 XMM6_H; // XMM6 高 64 位
    unsigned __int64 XMM7; // XMM7 低 64 位
    unsigned __int64 XMM7_H; // XMM7 高 64 位
    unsigned __int64 XMM8; // XMM8 低 64 位
    unsigned __int64 XMM8_H; // XMM8 高 64 位
    unsigned __int64 XMM9; // XMM9 低 64 位
    unsigned __int64 XMM9_H; // XMM9 高 64 位
    unsigned __int64 XMM10; // XMM10 低 64 位
    unsigned __int64 XMM10_H; // XMM10 高 64 位
    unsigned __int64 XMM11; // XMM11 低 64 位
    unsigned __int64 XMM11_H; // XMM11 高 64 位
    unsigned __int64 XMM12; // XMM12 低 64 位
    unsigned __int64 XMM12_H; // XMM12 高 64 位
    unsigned __int64 XMM13; // XMM13 低 64 位
    unsigned __int64 XMM13_H; // XMM13 高 64 位
    unsigned __int64 XMM14; // XMM14 低 64 位
    unsigned __int64 XMM14_H; // XMM14 高 64 位
    unsigned __int64 XMM15; // XMM15 低 64 位
    unsigned __int64 XMM15_H; // XMM15 高 64 位
    unsigned __int64 res1; // 保留 1 低 64 位
    unsigned __int64 res1_H; // 保留 1 高 64 位
    unsigned __int64 res2; // 保留 2 低 64 位
    unsigned __int64 res2_H; // 保留 2 高 64 位
    unsigned __int64 res3; // 保留 3 低 64 位
    unsigned __int64 res3_H; // 保留 3 高 64 位
    unsigned __int64 res4; // 保留 4 低 64 位
    unsigned __int64 res4_H; // 保留 4 高 64 位
    unsigned __int64 res5; // 保留 5 低 64 位
    unsigned __int64 res5_H; // 保留 5 高 64 位
    unsigned __int64 res6; // 保留 6 低 64 位
    unsigned __int64 res6_H; // 保留 6 高 64 位
} FXSAVE64, * PFXSAVE64; // FXSAVE64 类型及指针别名

static_assert(sizeof(FXSAVE64) == 512); // 确保结构大小符合 FXSAVE 规范

typedef struct _PageEventBasic // 页事件基础信息（断点触发时上报）
{
    unsigned __int64 VirtualAddress; // 触发的虚拟地址
    unsigned __int64 PhysicalAddress; // 触发的物理地址
    unsigned __int64 CR3; //in case of kernel or other process
    unsigned __int64 FSBASE; // FS 基址
    unsigned __int64 GSBASE; // GS 基址
    unsigned __int64 GSBASE_KERNEL; // 内核 GS 基址（备用）
    unsigned __int64 FLAGS; // RFLAGS 快照
    unsigned __int64 RAX; // RAX
    unsigned __int64 RBX; // RBX
    unsigned __int64 RCX; // RCX
    unsigned __int64 RDX; // RDX
    unsigned __int64 RSI; // RSI
    unsigned __int64 RDI; // RDI
    unsigned __int64 R8; // R8
    unsigned __int64 R9; // R9
    unsigned __int64 R10; // R10
    unsigned __int64 R11; // R11
    unsigned __int64 R12; // R12
    unsigned __int64 R13; // R13
    unsigned __int64 R14; // R14
    unsigned __int64 R15; // R15
    unsigned __int64 RBP; // RBP
    unsigned __int64 RSP; // RSP
    unsigned __int64 RIP; // RIP
    unsigned __int64 DR0; // DR0
    unsigned __int64 DR1; // DR1
    unsigned __int64 DR2; // DR2
    unsigned __int64 DR3; // DR3
    unsigned __int64 DR6; // DR6
    unsigned __int64 DR7; // DR7
    unsigned short CS; // CS 选择子
    unsigned short DS; // DS 选择子
    unsigned short ES; // ES 选择子
    unsigned short SS; // SS 选择子
    unsigned short FS; // FS 选择子
    unsigned short GS; // GS 选择子
    unsigned int Count; //number of times this block has been seen, or heartbeat when used for internal dbvm bp
} PageEventBasic, * PPageEventBasic; // 页事件基础结构与指针别名

typedef struct _PageEventExtended // 页事件扩展信息（含 FPU/SSE）
{
    PageEventBasic basic; // 基础事件数据
    FXSAVE64 fpudata; // FPU/SSE 状态
} PageEventExtended, * PPageEventExtended; // 扩展事件结构与指针别名

typedef struct _BrokenThreadEntry // 断点触发线程状态记录
{
    int inuse;  //是否已被使用
    int continueMethod; //0=no, 1=single step, 2=run  (resets to 0 after taking a step.  if 2 then inuse turns false
    int watchid; //导致中断的 watchid。如果是单步，则为 -1

    unsigned __int64 UserModeLoop; //完成某步后要去哪里
    unsigned __int64 KernelModeLoop; // 完成某步后的内核跳转

    PageEventExtended state; //包含 CR3、FSBASE 和 GSBASE
} BrokenThreadEntry, * PBrokenThreadEntry; // 断点线程状态与指针别名
