#include <defs.h>
#include <mmu.h>
#include <memlayout.h>
#include <clock.h>
#include <trap.h>
#include <x86.h>
#include <stdio.h>
#include <assert.h>
#include <console.h>
#include <kdebug.h>

#define TICK_NUM 100

static void print_ticks() {
    cprintf("%d ticks\n",TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* *
 * Interrupt descriptor table:
 *
 * Must be built at run time because shifted function addresses can't
 * be represented in relocation records.
 * */
static struct gatedesc idt[256] = {{0}};

static struct pseudodesc idt_pd = {
    sizeof(idt) - 1, (uintptr_t)idt
};

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S */
void
idt_init(void) {
     /* LAB1 YOUR CODE : STEP 2 */
     /* (1) Where are the entry addrs of each Interrupt Service Routine (ISR)?
      *     All ISR's entry addrs are stored in __vectors. where is uintptr_t __vectors[] ?
      *     __vectors[] is in kern/trap/vector.S which is produced by tools/vector.c
      *     (try "make" command in lab1, then you will find vector.S in kern/trap DIR)
      *     You can use  "extern uintptr_t __vectors[];" to define this extern variable which will be used later.
      * (2) Now you should setup the entries of ISR in Interrupt Description Table (IDT).
      *     Can you see idt[256] in this file? Yes, it's IDT! you can use SETGATE macro to setup each item of IDT
      * (3) After setup the contents of IDT, you will let CPU know where is the IDT by using 'lidt' instruction.
      *     You don't know the meaning of this instruction? just google it! and check the libs/x86.h to know more.
      *     Notice: the argument of lidt is idt_pd. try to find it!
      */
    // __vectors定义于vector.S中
    extern uintptr_t __vectors[];
    int i;
    for (i = 0; i < sizeof(idt) / sizeof(struct gatedesc); i ++)
        // 目标idt项为idt[i]
        // 该idt项为内核代码，所以使用GD_KTEXT段选择子
        // 中断处理程序的入口地址存放于__vectors[i]
        // 特权级为DPL_KERNEL
        SETGATE(idt[i], 0, GD_KTEXT, __vectors[i], DPL_KERNEL);
    // 设置从用户态转为内核态的中断的特权级为DPL_USER
    SETGATE(idt[T_SWITCH_TOK], 0, GD_KTEXT, __vectors[T_SWITCH_TOK], DPL_USER);
    // 加载该IDT
    lidt(&idt_pd);
}

static const char *
trapname(int trapno) {
    static const char * const excnames[] = {
        "Divide error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection",
        "Page Fault",
        "(unknown trap)",
        "x87 FPU Floating-Point Error",
        "Alignment Check",
        "Machine-Check",
        "SIMD Floating-Point Exception"
    };

    if (trapno < sizeof(excnames)/sizeof(const char * const)) {
        return excnames[trapno];
    }
    if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16) {
        return "Hardware Interrupt";
    }
    return "(unknown trap)";
}

/* trap_in_kernel - test if trap happened in kernel */
bool
trap_in_kernel(struct trapframe *tf) {
    return (tf->tf_cs == (uint16_t)KERNEL_CS);
}

static const char *IA32flags[] = {
    "CF", NULL, "PF", NULL, "AF", NULL, "ZF", "SF",
    "TF", "IF", "DF", "OF", NULL, NULL, "NT", NULL,
    "RF", "VM", "AC", "VIF", "VIP", "ID", NULL, NULL,
};

void
print_trapframe(struct trapframe *tf) {
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->tf_regs);
    cprintf("  ds   0x----%04x\n", tf->tf_ds);
    cprintf("  es   0x----%04x\n", tf->tf_es);
    cprintf("  fs   0x----%04x\n", tf->tf_fs);
    cprintf("  gs   0x----%04x\n", tf->tf_gs);
    cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
    cprintf("  err  0x%08x\n", tf->tf_err);
    cprintf("  eip  0x%08x\n", tf->tf_eip);
    cprintf("  cs   0x----%04x\n", tf->tf_cs);
    cprintf("  flag 0x%08x ", tf->tf_eflags);

    int i, j;
    for (i = 0, j = 1; i < sizeof(IA32flags) / sizeof(IA32flags[0]); i ++, j <<= 1) {
        if ((tf->tf_eflags & j) && IA32flags[i] != NULL) {
            cprintf("%s,", IA32flags[i]);
        }
    }
    cprintf("IOPL=%d\n", (tf->tf_eflags & FL_IOPL_MASK) >> 12);

    if (!trap_in_kernel(tf)) {
        cprintf("  esp  0x%08x\n", tf->tf_esp);
        cprintf("  ss   0x----%04x\n", tf->tf_ss);
    }
}

void
print_regs(struct pushregs *regs) {
    cprintf("  edi  0x%08x\n", regs->reg_edi);
    cprintf("  esi  0x%08x\n", regs->reg_esi);
    cprintf("  ebp  0x%08x\n", regs->reg_ebp);
    cprintf("  oesp 0x%08x\n", regs->reg_oesp);
    cprintf("  ebx  0x%08x\n", regs->reg_ebx);
    cprintf("  edx  0x%08x\n", regs->reg_edx);
    cprintf("  ecx  0x%08x\n", regs->reg_ecx);
    cprintf("  eax  0x%08x\n", regs->reg_eax);
}

/* temporary trapframe or pointer to trapframe */
struct trapframe switchk2u, *switchu2k;

// 偏移	内容	含义
// +0	SS	用户态的栈段寄存器（段选择子）
// +4	ESP	用户态的栈指针（返回后要恢复）
// +8	EFLAGS	中断前的标志寄存器
// +12	CS	用户态代码段
// +16	EIP	中断发生时的下一条指令地址

/**
 * @brief 根据中断号分发处理不同的中断
 * @param tf 中断帧的指针，包含中断现场信息
 * @note 处理时钟中断、串口中断、键盘中断等
 */
static void
trap_dispatch(struct trapframe *tf) {
    char c;

    switch (tf->tf_trapno) {
    case IRQ_OFFSET + IRQ_TIMER:
        /* LAB1 时钟中断处理 */
        /* 处理流程:
         * 1. 更新全局tick计数器(在clock.c中定义)
         * 2. 每经过TICK_NUM个周期打印一次信息
         */
        ticks ++;  // 更新全局tick计数器
        if (ticks % TICK_NUM == 0)  // 每经过TICK_NUM个周期打印一次信息
            print_ticks();
        break;
    case IRQ_OFFSET + IRQ_COM1:
        // 处理串口中断，从串口读取字符并打印
        c = cons_getc();
        cprintf("serial [%03d] %c\n", c, c);
        break;
    case IRQ_OFFSET + IRQ_KBD:
        // 处理键盘中断，从键盘读取字符并打印
        c = cons_getc();
        cprintf("kbd [%03d] %c\n", c, c);
        break;
    //LAB1 挑战1: 用户态与内核态的切换
    // 🧠 整体的思想：“我把一个伪造的上下文塞在原栈上，然后偷偷告诉 CPU：嘿，等下你从这个位置恢复就行了哈！”
    case T_SWITCH_TOU:  // 切换到用户态
        if (tf->tf_cs != USER_CS) {
            switchk2u = *tf;
            switchk2u.tf_cs = USER_CS;
            switchk2u.tf_ds = switchk2u.tf_es = switchk2u.tf_ss = USER_DS;
            switchk2u.tf_esp = (uint32_t)tf + sizeof(struct trapframe) - 8;
		
            // set eflags, make sure ucore can use io under user mode.
            // if CPL > IOPL, then cpu will generate a general protection.
            switchk2u.tf_eflags |= FL_IOPL_MASK;

            //  修改当前栈顶的内容，让 iret 不是弹出当前 trapframe，而是我们构造的 新的那一份 switchu2k！
            *((uint32_t *)tf - 1) = (uint32_t)&switchk2u;
            //     tf 是当前 trapframe 的地址，它被保存在栈上（现在我们要让 CPU 返回的时候用另一个 trapframe）
            //     iret 从 esp 开始弹出 trapframe
            //     所以我们要把 esp（即 iret 的恢复基地址）改成新的 switchu2k
            // 也就是说：让 iret 从这个“新的 trapframe”中恢复上下文，而不是原来的 tf！
        }
        
    case T_SWITCH_TOK:  // 从用户态切换到内核态
        /* 修改 tf 里的内容（模拟一个“回到内核”的 trapframe） */
        // 把当前正在内核里运行的任务，从用户态上下文（CPL=3）切换回内核态（CPL=0），但不是直接切，而是通过构造一个新的 trapframe 并通过 iret 恢复它来完成切换。
        // panic("T_SWITCH_** ??\n");
        if (tf->tf_cs != KERNEL_CS) { // 当前运行时的代码段不是内核态（cs ≠ KERNEL_CS），所以我们要从用户态 → 内核态切回来。
            // 这些是在构造“新 trapframe”的准备工作：
            tf->tf_cs = KERNEL_CS; // 表示返回后代码会执行在哪个段（设置为内核段）
            tf->tf_ds = tf->tf_es = tf->tf_ss = KERNEL_DS; // 数据段寄存器和栈段寄存器设置为内核数据段
            tf->tf_eflags &= ~FL_IOPL_MASK;  // 降低特权级
            // 计算要切换到的栈帧的地址。
            // 数值减8是因为内核调用中断时CPU没有压入ss和esp。因为在用户→内核跨级中断中，CPU 自动压入了 SS 和 ESP，但 iret 不需要你再填这两个位置（它自己弹出来），所以我们：
            // 在新构造的 trapframe 里，只需要 memmove 到 ESP 上方，但是不覆盖已经压进去的 SS/ESP。
            switchu2k = (struct trapframe *)(tf->tf_esp - (sizeof(struct trapframe) - 8));
            // 把 原 trapframe 复制一份，放到我们刚刚准备好的用户栈上（省略掉 ss 和 esp）
            memmove(switchu2k, tf, sizeof(struct trapframe) - 8);
            *((uint32_t *)tf - 1) = (uint32_t)switchu2k;
        }
        break;
    case IRQ_OFFSET + IRQ_IDE1:
    case IRQ_OFFSET + IRQ_IDE2:
        /* IDE磁盘控制器中断，当前不做处理 */
        break;
    default:
        // 在内核态发生未知中断，这是一个错误
        if ((tf->tf_cs & 3) == 0) {
            print_trapframe(tf);  // 打印中断帧信息
            panic("unexpected trap in kernel.\n");
        }
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap() returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void
trap(struct trapframe *tf) {
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}

