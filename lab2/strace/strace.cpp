#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

int main(int argc, char* argv[])
{
    pid_t child_pid = fork();
    struct user_regs_struct regs;
    int status = 0;

    if (child_pid < 0) {
        exit(1);
    }
    if (child_pid == 0) //子进程返回
    {
        ptrace(PTRACE_TRACEME, 0, 0, 0); //子进程在执行到 PTRACE_TRACEME 后会停住等待父进程指令
        execvp(argv[1], argv + 1);
        exit(1);
    }
    else
    {
        waitpid(child_pid, &status, 0);
        ptrace(PTRACE_SETOPTIONS, child_pid, NULL, PTRACE_O_TRACECLONE); //设置ptrace属性
        ptrace(PTRACE_SETOPTIONS, child_pid, NULL, PTRACE_O_TRACEFORK);
        ptrace(PTRACE_SETOPTIONS, child_pid, 0, PTRACE_O_EXITKILL);

        while (1)
        {
            ptrace(PTRACE_SYSCALL, child_pid, 0, 0); //让子进程恢复执行，并让子进程在下一次 syscall 的入口处停住；

            pid_t child_waited = waitpid(-1, &status, __WALL);//等待接收信号
            if (WIFEXITED(status))
            { //线程结束时，收到的信号
                fprintf(stderr, "\nthread %d exited with status %d\t\n", child_waited, WEXITSTATUS(status));
                break;
            }

            ptrace(PTRACE_GETREGS, child_pid, 0, &regs); //读取子进程中进行 syscall 时各个 CPU 寄存器的值
            long syscall = regs.orig_rax;

            fprintf(stderr, "%ld(%ld, %ld, %ld, %ld, %ld, %ld)", syscall,
                (long)regs.rdi, (long)regs.rsi, (long)regs.rdx,
                (long)regs.r10, (long)regs.r8, (long)regs.r9
            );


            //            if (WIFSTOPPED(status))
            //            {
            //                fprintf(stderr, "child %ld recvied signal %d\n", (long)child_waited, WSTOPSIG(status));
            //            }

            //            if (WIFSIGNALED(status))
            //            {
            //                fprintf(stderr, "child %ld recvied signal %d\n", (long)child_waited, WTERMSIG(status));
            //            }

            if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP)
            { //新线程被创建完成后，收到的信号，或者遇到断点时

                ptrace(PTRACE_CONT, child_waited, 1, NULL);
                continue;
            }

            if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP)
            {
                //当一个线程创建另一个线程返回时，收到的信号
                pid_t new_pid;
                if (((status >> 16) & 0xffff) == PTRACE_EVENT_CLONE|| ((status >> 16) & 0xffff) == PTRACE_EVENT_FORK )
		{
                    if (ptrace(PTRACE_GETEVENTMSG, child_waited, 0, &new_pid) != -1)
                    {
                        fprintf(stderr, "thread %d created\n", new_pid);
                    }

                }
            }

            if (WIFEXITED(status))
            { //线程结束时，收到的信号
                fprintf(stderr, "\nthread %d exited with status %d\t\n", child_waited, WEXITSTATUS(status));
                break;
            }


            ptrace(PTRACE_SYSCALL, child_pid, 0, 0); //让子进程恢复执行并停在 syscall 出口处

            waitpid(child_pid, &status, 0);
            if (WIFEXITED(status))
            { //线程结束时，收到的信号
                fprintf(stderr, "\nthread %d exited with status %d\t\n", child_waited, WEXITSTATUS(status));
                break;
            }

            ptrace(PTRACE_GETREGS, child_pid, 0, &regs);//通过读取寄存器 rax 来获取返回值
            fprintf(stderr, " = %ld\n", (long)regs.rax);
        }
    }
   // fprintf(stderr, "\n");
    return 0;
}
