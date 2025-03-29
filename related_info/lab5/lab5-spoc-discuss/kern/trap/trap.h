#ifndef __KERN_TRAP_TRAP_H__
#define __KERN_TRAP_TRAP_H__

#include <defs.h>

/* Trap Numbers */

/* Processor-defined: */
#define T_DIVIDE                0   // divide error
#define T_DEBUG                 1   // debug exception
#define T_NMI                   2   // non-maskable interrupt
#define T_BRKPT                 3   // breakpoint
#define T_OFLOW                 4   // overflow
#define T_BOUND                 5   // bounds check
#define T_ILLOP                 6   // illegal opcode
#define T_DEVICE                7   // device not available
#define T_DBLFLT                8   // double fault
// #define T_COPROC             9   // reserved (not used since 486)
#define T_TSS                   10  // invalid task switch segment
#define T_SEGNP                 11  // segment not present
#define T_STACK                 12  // stack exception
#define T_GPFLT                 13  // general protection fault
#define T_PGFLT                 14  // page fault
// #define T_RES                15  // reserved
#define T_FPERR                 16  // floating point error
#define T_ALIGN                 17  // aligment check
#define T_MCHK                  18  // machine check
#define T_SIMDERR               19  // SIMD floating point error

/* Hardware IRQ numbers. We receive these as (IRQ_OFFSET + IRQ_xx) */
#define IRQ_OFFSET              32  // IRQ 0 corresponds to int IRQ_OFFSET

#define IRQ_TIMER               0
#define IRQ_KBD                 1
#define IRQ_COM1                4
#define IRQ_IDE1                14
#define IRQ_IDE2                15
#define IRQ_ERROR               19
#define IRQ_SPURIOUS            31

/* *
 * These are arbitrarily chosen, but with care not to overlap
 * processor defined exceptions or interrupt vectors.
 * */
#define T_SWITCH_TOU                120    // user/kernel switch
#define T_SWITCH_TOK                121    // user/kernel switch

/* registers as pushed by pushal */
struct pushregs {
    uint32_t reg_edi;
    uint32_t reg_esi;
    uint32_t reg_ebp;
    uint32_t reg_oesp;          /* Useless */
    uint32_t reg_ebx;
    uint32_t reg_edx;
    uint32_t reg_ecx;
    uint32_t reg_eax;
};

/**
 * @brief 中断帧结构体
 * 
 * 保存中断发生时的寄存器状态和相关信息。
 * 当中断或异常发生时，CPU会自动压入一些信息，其余信息需手动保存。
 * 该结构从底向上反映了压栈的顺序。
 */
struct trapframe {
    struct pushregs tf_regs;    // 通用寄存器，由pusha指令压入
    uint16_t tf_gs;             // 段寄存器: GS
    uint16_t tf_padding0;       // 填充以对齐
    uint16_t tf_fs;             // 段寄存器: FS
    uint16_t tf_padding1;       // 填充以对齐
    uint16_t tf_es;             // 段寄存器: ES
    uint16_t tf_padding2;       // 填充以对齐
    uint16_t tf_ds;             // 段寄存器: DS
    uint16_t tf_padding3;       // 填充以对齐
    uint32_t tf_trapno;         // 中断号，需手动保存
    /* 以下由x86硬件自动压入 */
    uint32_t tf_err;            // 错误码(如果有的话)
    uintptr_t tf_eip;          // 指令指针
    uint16_t tf_cs;            // 代码段寄存器
    uint16_t tf_padding4;      // 填充以对齐
    uint32_t tf_eflags;        // 标志寄存器
    /* 以下仅在特权级发生变化时(如从用户态到内核态)才会压入 */
    uintptr_t tf_esp;          // 用户态栈指针
    uint16_t tf_ss;            // 用户态栈段寄存器
    uint16_t tf_padding5;      // 填充以对齐
} __attribute__((packed));     // 按实际占用大小进行内存对齐

void idt_init(void);
void print_trapframe(struct trapframe *tf);
void print_regs(struct pushregs *regs);
bool trap_in_kernel(struct trapframe *tf);

#endif /* !__KERN_TRAP_TRAP_H__ */

