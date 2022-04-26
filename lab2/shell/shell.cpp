// IO
#include <iostream>
// std::string
#include <string>
// std::vector
#include <vector>
// std::string ת int
#include <sstream>
// PATH_MAX �ȳ���
#include <climits>
// POSIX API
#include <unistd.h>
// wait
#include <sys/wait.h>

std::vector<std::string> split(std::string s, const std::string& delimiter);
int exec_cmd(std::string cmd, bool fork_or_not);// ����ֵΪ0����continue;��Ȼ�Ļ�������exit

int main() {
    // ��ͬ�� iostream �� cstdio �� buffer
    std::ios::sync_with_stdio(false);

    // �����洢�����һ������
    std::string cmd;
    while (true) {
        // ��ӡ��ʾ��
        std::cout << "# ";

        // ����һ�С�std::getline ������������з���
        std::getline(std::cin, cmd);

        // �� | �ָ
        std::vector<std::string> cmds = split(cmd, " ");

        if (cmds.size() == 1) {
            // û�йܵ�����
            int exec_return = exec_cmd(cmds, 1);
            if (exec_return == 0) continue;
            else return exec_return;
        }
        else {
            // �йܵ�����
            int cmdv = cmds.size();
            int fd[cmdv+3][2];
            for (int i = 0; i < cmdv-1; i++) {
                pipe(fd[i]);
            }
            for (int i = 0; i < cmdv; i++) {
                pid_t pid = fork();
                if (pid < 0) {
                    std::cout << "fork failed\n";
                    continue;
                }
                else if (pid == 0) {
                    if (i > 0) { // ���˵�һ��������������Ҫ��������ǰһ���������Ϣ
                    dup2(fd[i - 1][0], 0);
                    close(fd[i - 1][1]);
                    }
                    if (i < cmdv - 1) {
                        dup2(fd[i][1], 1);
                        close(fd[i][0]);
                    }
                    int exec_return = exec_cmd(cmds, 0);// �������Ҫ���Ѿ����ӽ����ˣ�����Ҫ�ٴ����ӽ���
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

int exec_cmd(std::string cmd, bool fork_or_not)
{
    // ���ո�ָ�����Ϊ����
    std::vector<std::string> args = split(cmd, " ");

    // û�пɴ��������
    if (args.empty()) {
        return 0;
    }


    // ���Ĺ���Ŀ¼ΪĿ��Ŀ¼
    if (args[0] == "cd") {
        if (args.size() <= 1) {
            // �������Ϣ����ΪӢ�ģ���Ӣ���������ʵ�Ƿ� ASCII �������û���ر����õ�����£��ر��� Windows �£�������
            // �����Ȥ������������ GBK Unicode UTF-8 Codepage UTF-16 �Ƚ���ѧϰ
            std::cout << "Insufficient arguments\n";
            // ��Ҫ�� std::endl��std::endl = "\n" + fflush(stdout)
            return 0;
        }

        // ����ϵͳ API
        int ret = chdir(args[1].c_str());
        if (ret < 0) {
            std::cout << "cd failed\n";
        }
        return 0;
    }

    // ��ʾ��ǰ����Ŀ¼
    if (args[0] == "pwd") {
        std::string cwd;

        // Ԥ�ȷ���ÿռ�
        cwd.resize(PATH_MAX);

        // std::string to char *: &s[0]��C++17 ���Ͽ����� s.data()��
        // std::string ��֤���ڴ���������
        const char* ret = getcwd(&cwd[0], PATH_MAX);
        if (ret == nullptr) {
            std::cout << "cmd failed\n";
        }
        else {
            std::cout << ret << "\n";
        }
        return 0;
    }

    // ���û�������
    if (args[0] == "export") {
        for (auto i = ++args.begin(); i != args.end(); i++) {
            std::string key = *i;

            // std::string Ĭ��Ϊ��
            std::string value;

            // std::string::npos = std::string end
            // std::string ���� nullptr ��β�ģ���ȷʵ����һ����β�ַ� npos
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

    // �˳�
    if (args[0] == "exit") {
        if (args.size() <= 1) {
            return 1;
        }

        // std::string ת int
        std::stringstream code_stream(args[1]);
        int code = 0;
        code_stream >> code;

        // ת��ʧ��
        if (!code_stream.eof() || code_stream.fail()) {
            std::cout << "Invalid exit code\n";
            return 0;
        }

        return code;
    }

    // std::vector<std::string> ת char **
    char* arg_ptrs[args.size() + 1];
    for (auto i = 0; i < args.size(); i++) {
        arg_ptrs[i] = &args[i][0];
    }
    // exec p ϵ�е� argv ��Ҫ�� nullptr ��β
    arg_ptrs[args.size()] = nullptr;
    
    // �����Ƿ���Ҫ�����ӽ���
    if (fork_or_not == 1) {
        // �ⲿ����
        pid_t pid = fork();

        if (pid < 0) {
            std::cout << "fork failed\n";
            return 0;
        }

        if (pid == 0) {
            // ����ֻ���ӽ��̲Ż����
            // execvp ����ȫ�����ӽ��̽������Ĵ��룬������������� execvp ֮������Ĵ����û������
            // ��� execvp ֮��Ĵ��뱻�����ˣ��Ǿ��� execvp ��������
            execvp(args[0].c_str(), arg_ptrs);

            // ��������ֱ�ӱ���
            exit(255);
        }

        // ����ֻ�и����̣�ԭ���̣��Ż����
        int ret = wait(nullptr);
        if (ret < 0) {
            std::cout << "wait failed";
        }
    }

    else {
        execvp(args[0].c_str(), arg_ptrs);
        exit(255);
    }
    return 0;
}

// ����� cpp string split ʵ��
// https://stackoverflow.com/a/14266139/11691878
std::vector<std::string> split(std::string s, const std::string& delimiter) {
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        res.push_back(token);
        s = s.substr(pos + delimiter.length());
    }
    res.push_back(s);
    return res;
}
