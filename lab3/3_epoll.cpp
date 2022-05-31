#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#define Max_Client 32
#define ONE_MB 1048576  
#define LEN_BUF 1024


int used[Max_Client + 5];   // 标记这个index是否对应一个已加入的client
int client[Max_Client + 5]; // 每个client的fd

char msg_recv[LEN_BUF + 500]; // recv 到的信息

char one_msg[ONE_MB + 500];  // 准备 send 的一条信息
char one_msg2[1050000];
char msg_tobesend[2 * ONE_MB]; // 累计的信息
char msg_tobesend2[2 * ONE_MB];
char tmp[ONE_MB + 500];

void setnonblocking(int sock)
{
    int opts;
    opts = fcntl(sock, F_GETFL);
    if (opts < 0)
    {
        perror("fcntl(sock,GETFL)");
        exit(1);
    }
    opts = opts | O_NONBLOCK;
    if (fcntl(sock, F_SETFL, opts) < 0)
    {
        perror("fcntl(sock,SETFL,opts)");
        exit(1);
    }
}

void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0); //从兴趣列表删除该描述符
    close(fd);
}


void handle_chat(int epollfd, int index)
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
                removefd(epollfd, client[index]);
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


void addfd(int epollfd, int fd, bool enable_et)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN;
    if (enable_et)
        ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
    setnonblocking(fd);
    //printf("fd added to epoll!\n");
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

    //int maxfdp = fd;
    //fd_set fds;
    int epfd = epoll_create(Max_Client + 5);
    if (epfd < 0) {
        perror("epfd error");
        return 1;
    }

    static struct epoll_event events[Max_Client + 5];
    addfd(epfd, fd, true);

    while (true) {

        int epoll_events_count = epoll_wait(epfd, events, Max_Client, -1);
        if (epoll_events_count < 0) {
            perror("epoll failure");
            break;
        }

        for (int i = 0; i < epoll_events_count; i++) {
            int sockfd = events[i].data.fd;

            if (sockfd == fd) {
                // 新用户连接
                int new_client = accept(fd, NULL, NULL);
                if (new_client == -1) {
                    perror("accept");
                    return 1;
                }
                addfd(epfd, new_client, true);

                for (int j = 0; j < Max_Client; j++) {
                    if (!used[j]) {
                        used[j] = 1;
                        client[j] = new_client;
                        break;
                    }
                }
            }

            else {
                // 有用户发消息
                for (int j = 0; j < Max_Client; j++) {
                    if (sockfd == client[j]) {
                        handle_chat(epfd, j);
                    }
                }
            }
        }
    }

    return 0;
}
