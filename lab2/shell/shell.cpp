// IO
#include <iostream>
// std::string
#include <string>
// std::vector
#include <vector>
// std::string 转 int
#include <sstream>
// PATH_MAX 等常量
#include <climits>
// POSIX API
#include <unistd.h>
// wait
#include <sys/wait.h>
#include <fcntl.h>

std::vector<std::string> split(std::string s, const std::string& delimiter);
int exec_cmd(std::string cmd, bool fork_or_not);// 返回值为0，是continue;不然的话，就是exit

int main() {
    // 不同步 iostream 和 cstdio 的 buffer
    std::ios::sync_with_stdio(false);

    // 用来存储读入的一行命令
    std::string cmd;
    while (true) {
        // 打印提示符
        std::cout << "# ";

        // 读入一行。std::getline 结果不包含换行符。
        std::getline(std::cin, cmd);

        // 按 | 分割开
        std::vector<std::string> cmds = split(cmd, "|");

        if (cmds.size() == 1) {
            // 没有管道操作
            int exec_return = exec_cmd(cmds[0], 1);
            if (exec_return == 0) continue;
            else return exec_return;
        }
        else {
            // 有管道操作
            int cmdv = cmds.size();
            int fd[cmdv + 3][2];
            for (int i = 0; i < cmdv - 1; i++) {
                pipe(fd[i]);
            }
            for (int i = 0; i < cmdv; i++) {
                pid_t pid = fork();
                if (pid < 0) {
                    std::cout << "fork failed\n";
                    continue;
                }
                else if (pid == 0) {
                    if (i > 0) { // 除了第一个命令，其他命令均要接收来自前一个命令的消息
                        dup2(fd[i - 1][0], 0);
                        close(fd[i - 1][1]);
                    }
                    if (i < cmdv - 1) {
                        dup2(fd[i][1], 1);
                        close(fd[i][0]);
                    }
                    int exec_return = exec_cmd(cmds[i], 0);// 这里很重要，已经在子进程了，不需要再创建子进程
                    if (exec_return == 0) continue;
                    else return exec_return;
                }
                else {
                    waitpid(pid, NULL, 0);
                    if (i < cmdv - 1) {
                        close(fd[i][1]);
                    }
                }
            }
            continue;
        }

    }
    return 0;
}

int process_redirect(std::vector<std::string> args, int& fd_redirect;)
{
    for (int i = 0; i < args.size(); i++) {
        if (args[i] == ">") {
            int fd_redirect = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd_redirect == -1) {
                std::cout << "'>' failed\n";
                return -1;
            }
            dup2(fd_redirect, 1);
            redirect = 1;
            args.erase(i + 1); args.erase(i);
        }
        else if (args[i] == ">>") {
            int fd_redirect = open(args[i + 1], O_RDWR | O_APPEND | O_CREAT, 0666);
            if (fd_redirect == -1) {
                std::cout << "'>>' failed\n";
                return -1;
            }
            dup2(fd_redirect, 1);
            redirect = 1;
            args.erase(i + 1); args.erase(i);
        }
        else if (args[i] == '<') {
            int fd_redirect = open(args[i + 1], O_RDONLY);
            if (fd_redirect == -1) {
                std::cout << "'<' failed\n";
                return -1;
            }
            dup2(fd_redirect, 0);
            redirect = 2;
            args.erase(i + 1); args.erase(i);
        }
    }
}

int exec_cmd(std::string cmd, bool fork_or_not)
{
    // 按空格分割命令为单词
    std::vector<std::string> args = split(cmd, " ");
    //   for(int i=0;i<args.size();i++) std::cout<<"***"<<args[i]<<"***\n";
       // 没有可处理的命令
    if (args.empty()) {
        return 0;
    }

    int redirect = 0;
    int fd_redirect = 0;

    // 更改工作目录为目标目录
    if (args[0] == "cd") {
        redirect = process_redirect(args, fd_redirect);
        if (args.size() <= 1) {
            // 输出的信息尽量为英文，非英文输出（其实是非 ASCII 输出）在没有特别配置的情况下（特别是 Windows 下）会乱码
            // 如感兴趣可以自行搜索 GBK Unicode UTF-8 Codepage UTF-16 等进行学习
            std::cout << "Insufficient arguments\n";
            // 不要用 std::endl，std::endl = "\n" + fflush(stdout)
            return 0;
        }

        // 调用系统 API
        int ret = chdir(args[1].c_str());
        if (ret < 0) {
            std::cout << "cd failed\n";
        }
        if (redirect == 1) {
            dup2(1, fd_redirect);
        }
        else if (redirect == 2) {
            dup2(0, fd_redirect);
        }
        return 0;
    }

    // 显示当前工作目录
    if (args[0] == "pwd") {
        redirect = process_redirect(args, fd_redirect);
        std::string cwd;

        // 预先分配好空间
        cwd.resize(PATH_MAX);

        // std::string to char *: &s[0]（C++17 以上可以用 s.data()）
        // std::string 保证其内存是连续的
        const char* ret = getcwd(&cwd[0], PATH_MAX);
        if (ret == nullptr) {
            std::cout << "cmd failed\n";
        }
        else {
            std::cout << ret << "\n";
        }
        if (redirect == 1) {
            dup2(1, fd_redirect);
        }
        else if (redirect == 2) {
            dup2(0, fd_redirect);
        }
        return 0;
    }

    // 设置环境变量
    if (args[0] == "export") {
        for (auto i = ++args.begin(); i != args.end(); i++) {
            std::string key = *i;

            // std::string 默认为空
            std::string value;

            // std::string::npos = std::string end
            // std::string 不是 nullptr 结尾的，但确实会有一个结尾字符 npos
            size_t pos;
            if ((pos = i->find('=')) != std::string::npos) {
                key = i->substr(0, pos);
                value = i->substr(pos + 1);
            }

            int ret = setenv(key.c_str(), value.c_str(), 1);
            if (ret < 0) {
                std::cout << "export failed\n";
            }
        }
        return 0;
    }

    // 退出
    if (args[0] == "exit") {
        if (args.size() <= 1) {
            return 1;
        }

        // std::string 转 int
        std::stringstream code_stream(args[1]);
        int code = 0;
        code_stream >> code;

        // 转换失败
        if (!code_stream.eof() || code_stream.fail()) {
            std::cout << "Invalid exit code\n";
            return 0;
        }

        return code;
    }

    // std::vector<std::string> 转 char **
    char* arg_ptrs[args.size() + 1];
    for (auto i = 0; i < args.size(); i++) {
        arg_ptrs[i] = &args[i][0];
    }
    // exec p 系列的 argv 需要以 nullptr 结尾
    arg_ptrs[args.size()] = nullptr;

    // 根据是否需要创建子进程
    if (fork_or_not == 1) {
        // 外部命令
        pid_t pid = fork();

        if (pid < 0) {
            std::cout << "fork failed\n";
            return 0;
        }

        if (pid == 0) {
            // 这里只有子进程才会进入
            // execvp 会完全更换子进程接下来的代码，所以正常情况下 execvp 之后这里的代码就没意义了
            // 如果 execvp 之后的代码被运行了，那就是 execvp 出问题了
            redirect = process_redirect(args, fd_redirect);
            execvp(args[0].c_str(), arg_ptrs);

            // 所以这里直接报错
            exit(255);
        }

        // 这里只有父进程（原进程）才会进入
        int ret = wait(nullptr);
        if (ret < 0) {
            std::cout << "wait failed";
        }
    }

    else {
        redirect = process_redirect(args, fd_redirect);
        execvp(args[0].c_str(), arg_ptrs);
        exit(255);
    }

    return 0;
}

// 经典的 cpp string split 实现
// https://stackoverflow.com/a/14266139/11691878
std::vector<std::string> split(std::string s, const std::string& delimiter) {
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        bool check_nul = 1;
        for (int i = 0; i < token.length(); i++) {
            if (token[i] != ' ') check_nul = 0;
        }
        //       std::cout<<token<<"***"<<check_nul<<"*\n";
        if (check_nul == 0) res.push_back(token);
        s = s.substr(pos + delimiter.length());
    }
    bool check_nul = 1;
    for (int i = 0; i < s.length(); i++) {
        if (s[i] != ' ') check_nul = 0;
    }
    if (check_nul == 0) res.push_back(s);
    return res;
}

