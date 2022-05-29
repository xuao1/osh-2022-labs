#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>

#define Max_Client 32
#define ONE_MB 1048576  
#define LEN_BUF 10240


int used[Max_Client + 5];   // 标记这个index是否对应一个已加入的client
int client[Max_Client + 5]; // 每个client的fd

char msg_recv[LEN_BUF + 500]; // recv 到的信息

char one_msg[ONE_MB + 500];  // 准备 send 的一条信息
char one_msg2[1050000];
char msg_tobesend[2 * ONE_MB]; // 累计的信息
char msg_tobesend2[2 * ONE_MB];
char tmp[ONE_MB + 500];


void handle_chat(int index)
{
    memset(msg_tobesend, '\0', 2 * ONE_MB);
    ssize_t len;
    bool first_recv = true;

    while (true) {
        // 非阻塞 recv
        memset(msg_recv, '\0', LEN_BUF);
        len = recv(client[index], msg_recv, LEN_BUF, 0);
        if (len <= 0) {
            // 此客户端退出
            if (first_recv) {
                used[index] = 0;
                close(client[index]);
                return;
            }
            return; // 此次 recv 全部接受完毕，退出 recv
        }

        first_recv = false;

        int len_mts = strlen(msg_tobesend);
        for (int i = 0; i <= strlen(msg_recv); i++) {
            msg_tobesend[len_mts] = msg_recv[i];
            len_mts++;
        }

        for (int i = 0; i < strlen(msg_tobesend); i++) {
            if (msg_tobesend[i] == '\n') {
                // 划分消息，此回车之前的消息被发送
                int indexx;
                memset(one_msg, '\0', ONE_MB);
                strcpy(one_msg, "Message:");
                for (indexx = 0; indexx <= i; indexx++) {
                    one_msg[indexx + 8] = msg_tobesend[indexx];
                }
                one_msg[indexx + 8] = '\0';

                strcpy(tmp, one_msg);

                // 发送消息给聊天室的各个client
                for (int j = 0; j < Max_Client; j++) {
                    if (used[j] && j != index) {
                        memset(one_msg, '\0', ONE_MB);
                        strcpy(one_msg, tmp);
                        int send_len = send(client[j], one_msg, strlen(one_msg), 0);
                        while (send_len != strlen(one_msg)) {
                            memset(one_msg2, '\0', ONE_MB);
                            for (int k = 0; k + send_len <= strlen(one_msg); k++) {
                                one_msg2[k] = one_msg[k + send_len];
                            }
                            memset(one_msg, '\0', ONE_MB);
                            for (int k = 0; k <= strlen(one_msg2); k++) {
                                one_msg[k] = one_msg2[k];
                            }
                            send_len = send(client[j], one_msg, strlen(one_msg), 0);
                        }
                    }
                }

                int temp = i;
                memset(msg_tobesend2, '\0', 2 * ONE_MB);
                for (int k = 0; k + temp + 1 <= strlen(msg_tobesend); k++) {
                    msg_tobesend2[k] = msg_tobesend[k + i + 1];
                }
                memset(msg_tobesend, '\0', 2 * ONE_MB);
                for (int k = 0; k <= strlen(msg_tobesend2); k++) {
                    msg_tobesend[k] = msg_tobesend2[k];
                }
                i = -1;
            }
        }

    }
    return;
}

int main(int argc, char** argv) {
    int port = atoi(argv[1]);
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(addr);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr))) {
        perror("bind");
        return 1;
    }
    if (listen(fd, Max_Client)) {
        perror("listen");
        return 1;
    }

    int maxfdp = fd;    
    fd_set fds;     

    while (true) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        for (int i = 0; i < Max_Client; i++) {  
            // 每次执行 select 后，clients 数组中的一些位都会被清零，需要再恢复
            if (used[i]) {
                FD_SET(client[i], &fds);
            }
        }
        if (select(maxfdp + 1, &fds, NULL, NULL, NULL) > 0) { // 监视文件描述符的读变化
            if (FD_ISSET(fd, &fds)) {  // 有 client 要加入 
                int new_client = accept(fd, NULL, NULL);
                if (new_client == -1) {
                    perror("accept");
                    return 1;
                }

                fcntl(new_client, F_SETFL, fcntl(new_client, F_GETFL, 0) | O_NONBLOCK); // 将客户端的套接字设置成非阻塞

                if (new_client > maxfdp) {
                    maxfdp = new_client;
                }

                for (int i = 0; i < Max_Client; i++) {
                    if (!used[i]) {
                        used[i] = 1;
                        client[i] = new_client;
                        break;
                    }
                }
            }
            for (int i = 0; i < Max_Client; i++) {  // 有client接收到消息，可以read
                if (used[i] && FD_ISSET(client[i], &fds)) {
                    handle_chat(i);
                }
            }
        }
    }

    return 0;
}
