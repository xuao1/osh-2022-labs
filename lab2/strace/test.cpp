#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <iostream>

int main()
{
    std::cout << "begin to fork\n";
    pid_t pid = fork();
    std::cout << "finish fork\n";
    if (pid == 0) {
        execl("/bin/ls", "ls", "-al", NULL);
	std::cout << "this is in child process\n";
    }
    else {
        std::cout << "this is in father process\n";
    }
    return 0;
}

