#include <sys/ptrace.h>
#include <sys/user.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include<sys/types.h>
#include<sys/wait.h>

int main(int argc, char* argv[])
{
    pid_t pid = fork();
    struct user_regs_struct regs;
    int status;

    if (pid < 0) {
        exit(1);
    }
    else if (pid == 0) {
        ptrace(PTRACE_TRACEME, 0, 0, 0); //子进程在执行到 PTRACE_TRACEME 后会停住等待父进程指令
        execvp(argv[1], argv + 1);
        exit(1);
    }
    else {
        waitpid(pid, &status, 0);
        ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_EXITKILL);

        while (1) {
            ptrace(PTRACE_SYSCALL, pid, 0, 0); //让子进程恢复执行，并让子进程在下一次 syscall 的入口处停住；

            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) { // 如果子进程退出了, 那么终止跟踪
                break;
            }

            ptrace(PTRACE_GETREGS, pid, 0, &regs); //读取子进程中进行 syscall 时各个 CPU 寄存器的值
            long syscall = regs.orig_rax;

            fprintf(stderr, "%ld(%ld, %ld, %ld, %ld, %ld, %ld)", syscall,
                (long)regs.rdi, (long)regs.rsi, (long)regs.rdx,
                (long)regs.r10, (long)regs.r8, (long)regs.r9
            );

            ptrace(PTRACE_SYSCALL, pid, 0, 0); //让子进程恢复执行并停在 syscall 出口处

            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) { // 如果子进程退出了, 那么终止跟踪
                break;
            }

            ptrace(PTRACE_GETREGS, pid, 0, &regs);//通过读取寄存器 rax 来获取返回值
            fprintf(stderr, " = %ld\n", (long)regs.rax);
        }
    }
    fprintf(stderr, "\n");
    return 0;
}