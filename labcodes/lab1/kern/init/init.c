#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <kdebug.h>
#include <picirq.h>
#include <trap.h>
#include <clock.h>
#include <intr.h>
#include <pmm.h>
#include <kmonitor.h>
int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);
static void lab1_switch_test(void);

int
kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    cons_init();                // init the console

    const char *message = "(THU.CST) os is loading ...";
    cprintf("%s\n\n", message);

    print_kerninfo();

    grade_backtrace();

    pmm_init();                 // init physical memory management

    pic_init();                 // init interrupt controller
    idt_init();                 // init interrupt descriptor table

    clock_init();               // init clock interrupt
    intr_enable();              // enable irq interrupt

    //LAB1: CAHLLENGE 1 If you try to do it, uncomment lab1_switch_test()
    // user/kernel mode switch test
    lab1_switch_test();

    /* do nothing */
    while (1);
}

void __attribute__((noinline))
grade_backtrace2(int arg0, int arg1, int arg2, int arg3) {
    mon_backtrace(0, NULL, NULL);
}

void __attribute__((noinline))
grade_backtrace1(int arg0, int arg1) {
    grade_backtrace2(arg0, (int)&arg0, arg1, (int)&arg1);
}

void __attribute__((noinline))
grade_backtrace0(int arg0, int arg1, int arg2) {
    grade_backtrace1(arg0, arg2);
}

void
grade_backtrace(void) {
    grade_backtrace0(0, (int)kern_init, 0xffff0000);
}

static void
lab1_print_cur_status(void) {
    static int round = 0;
    uint16_t reg1, reg2, reg3, reg4;
    asm volatile (
            "mov %%cs, %0;"
            "mov %%ds, %1;"
            "mov %%es, %2;"
            "mov %%ss, %3;"
            : "=m"(reg1), "=m"(reg2), "=m"(reg3), "=m"(reg4));
    cprintf("%d: @ring %d\n", round, reg1 & 3);
    cprintf("%d:  cs = %x\n", round, reg1);
    cprintf("%d:  ds = %x\n", round, reg2);
    cprintf("%d:  es = %x\n", round, reg3);
    cprintf("%d:  ss = %x\n", round, reg4);
    round ++;
}

/**
 * @brief 切换到用户态的函数
 * @details 通过中断的方式从内核态切换到用户态
 *          具体步骤如下:
 *          1. 在栈上预留8字节空间，用于保存用户态的ss和esp
 *          2. 触发T_SWITCH_TOU中断，在中断处理过程中完成特权级切换
 *          3. 恢复栈指针，使其指向正确的位置
 * 
 * @note 该函数使用内联汇编实现
 *       T_SWITCH_TOU是预定义的中断号，用于触发特权级切换
 *       栈的调整是为了模拟中断发生时的硬件压栈操作
 */
static void
lab1_switch_to_user(void) {
    //LAB1 CHALLENGE 1 : TODO
	asm volatile (
	    "sub $0x8, %%esp \n"
	    "int %0 \n"
	    "movl %%ebp, %%esp"
	    : 
	    : "i"(T_SWITCH_TOU)
	);
}

/**
 * @brief 切换到内核态的函数
 * @details 通过中断的方式从用户态切换到内核态
 *          具体步骤如下:
 *          1. 直接触发T_SWITCH_TOK中断，在中断处理过程中完成特权级切换
 *          2. 由于是从低特权级到高特权级，不需要预留栈空间
 *          3. 恢复栈指针，使其指向正确的位置
 * 
 * @note 该函数使用内联汇编实现
 *       T_SWITCH_TOK是预定义的中断号，用于触发特权级切换
 *       与switch_to_user不同，这里不需要预留栈空间
 */
static void
lab1_switch_to_kernel(void) {
    //LAB1 CHALLENGE 1 :  TODO
    asm volatile (
        "int %0 \n"            // 触发T_SWITCH_TOK中断，切换到内核态
        "movl %%ebp, %%esp \n" // 恢复栈指针
        : 
        : "i"(T_SWITCH_TOK)    // 中断号作为立即数输入
    );
}

static void
lab1_switch_test(void) {
    lab1_print_cur_status();
    cprintf("+++ switch to  user  mode +++\n");
    lab1_switch_to_user();
    lab1_print_cur_status();
    cprintf("+++ switch to kernel mode +++\n");
    lab1_switch_to_kernel();
    lab1_print_cur_status();
}

