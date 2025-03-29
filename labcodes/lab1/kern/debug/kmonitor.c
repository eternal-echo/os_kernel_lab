#include <stdio.h>
#include <string.h>
#include <trap.h>
#include <kmonitor.h>
#include <kdebug.h>

/**
 * @brief 简单的命令行内核监视器，用于控制内核和交互式地探索系统
 * @note 这个结构定义了kernel monitor的基本命令框架，被整个内核调试系统使用
 */

/* 命令结构体：定义了内核监视器支持的命令格式 */
struct command {
    const char *name;    // 命令名称
    const char *desc;    // 命令描述
    // 命令处理函数指针，返回-1时强制退出监视器
    // @param argc 参数数量
    // @param argv 参数数组
    // @param tf   中断帧指针
    // @return int 返回值，-1表示退出监视器
    int(*func)(int argc, char **argv, struct trapframe *tf);
};

static struct command commands[] = {
    {"help", "Display this list of commands.", mon_help},
    {"kerninfo", "Display information about the kernel.", mon_kerninfo},
    {"backtrace", "Print backtrace of stack frame.", mon_backtrace},
};

#define NCOMMANDS (sizeof(commands)/sizeof(struct command))

/***** Kernel monitor command interpreter *****/

#define MAXARGS         16
#define WHITESPACE      " \t\n\r"

/**
 * @brief 解析命令缓冲区为以空白字符分隔的参数数组
 * @param buf   输入的命令字符串
 * @param argv  解析后的参数数组
 * @return int  返回解析得到的参数个数
 * @note 该函数用于将输入字符串分割成参数数组，供命令执行使用
 */
static int
parse(char *buf, char **argv) {
    int argc = 0;
    while (1) {
        // 跳过连续的空白字符，并将其替换为字符串结束符'\0'
        while (*buf != '\0' && strchr(WHITESPACE, *buf) != NULL) {
            *buf ++ = '\0';
        }
        // 如果已到达字符串末尾，结束解析
        if (*buf == '\0') {
            break;
        }

        // 保存参数指针并继续扫描
        if (argc == MAXARGS - 1) {
            cprintf("Too many arguments (max %d).\n", MAXARGS);
        }
        // 保存当前参数的起始位置
        argv[argc ++] = buf;
        // 跳过当前参数的所有非空白字符
        while (*buf != '\0' && strchr(WHITESPACE, *buf) == NULL) {
            buf ++;
        }
    }
    return argc;
}

/**
 * @brief 执行命令行输入的命令
 * @param buf  输入的命令字符串
 * @param tf   中断帧指针
 * @return int 执行结果，-1表示退出监视器
 * @note 该函数是kernel monitor的核心功能函数，负责命令的解析和分发
 */
static int
runcmd(char *buf, struct trapframe *tf) {
    char *argv[MAXARGS];
    // 解析命令字符串，获取参数个数和参数数组
    int argc = parse(buf, argv);
    if (argc == 0) {
        return 0;
    }
    
    // 遍历命令表，查找匹配的命令并执行
    int i;
    for (i = 0; i < NCOMMANDS; i ++) {
        if (strcmp(commands[i].name, argv[0]) == 0) {
            // 调用对应的命令处理函数，传入去除命令名的参数
            return commands[i].func(argc - 1, argv + 1, tf);
        }
    }
    // 未找到匹配的命令，打印错误信息
    cprintf("Unknown command '%s'\n", argv[0]);
    return 0;
}

/***** Implementations of basic kernel monitor commands *****/

void
kmonitor(struct trapframe *tf) {
    cprintf("Welcome to the kernel debug monitor!!\n");
    cprintf("Type 'help' for a list of commands.\n");

    if (tf != NULL) {
        print_trapframe(tf);
    }

    char *buf;
    while (1) {
        if ((buf = readline("K> ")) != NULL) {
            if (runcmd(buf, tf) < 0) {
                break;
            }
        }
    }
}

/* mon_help - print the information about mon_* functions */
int
mon_help(int argc, char **argv, struct trapframe *tf) {
    int i;
    for (i = 0; i < NCOMMANDS; i ++) {
        cprintf("%s - %s\n", commands[i].name, commands[i].desc);
    }
    return 0;
}

/* *
 * mon_kerninfo - call print_kerninfo in kern/debug/kdebug.c to
 * print the memory occupancy in kernel.
 * */
int
mon_kerninfo(int argc, char **argv, struct trapframe *tf) {
    print_kerninfo();
    return 0;
}

/* *
 * mon_backtrace - call print_stackframe in kern/debug/kdebug.c to
 * print a backtrace of the stack.
 * */
int
mon_backtrace(int argc, char **argv, struct trapframe *tf) {
    print_stackframe();
    return 0;
}

