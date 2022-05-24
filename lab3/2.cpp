#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <queue>

#define ONE_MB 1048576  
#define Max_Client 32

using namespace std;

struct Client {
    pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  cv = PTHREAD_COND_INITIALIZER;
    // 这个信号是处理生产者消费者问题，如果发送队列为空，那么发送进程就需要等待
    // recv 进程向发送队列写入内容后，signal 发送进程
    queue<char*> send_queue;
    // 对消息队列的写入和删除操作必须是互斥的，所以需要加锁
    int client_fd = 0;
}Clients[Max_Client+5];


void* handle_recv(void* data)
{
    int fd = *(int*)data;
    int fd1;
    while (true) {
        fd1 = accept(fd, NULL, NULL); // 保证用户退出后，仍有用户可以使用这一 client
        if (fd == -1) {
            perror("accept");
            return NULL;
        }
        int k; // 记录当前 client 在 Clients 数组的下标
        for (k = 0; k < Max_Client; k++) { // 新加入的client，加入client数组
            if (Clients[k].client_fd == 0) {
                Clients[k].client_fd = fd1;
                break;
            }
        }
        if (k == Max_Client) { // 聊天室已经有32个client
            perror("join in chatting");
            return NULL;
        }
        ssize_t len;
        char msg_tobesend[2*ONE_MB]; // 累计的信息
        memset(msg_tobesend, '\0', 2*ONE_MB);
        char msg_recv[ONE_MB + 500]; // recv 到的信息
        char one_msg[ONE_MB + 500] = "Message:";  // 准备 send 的一条信息
        while (len = recv(fd1, msg_recv, Max_Client, 0) > 0) { // 正常接收数据
            int len_mts = strlen(msg_tobesend);
            for (int i = 0; i <= strlen(msg_recv); i++) {
                msg_tobesend[len_mts] = msg_recv[i];
                len_mts++;
            }
            for (int i = 0; i < strlen(msg_tobesend); i++) {
                if (msg_tobesend[i] == '\n') {
                    // 划分消息，此回车之前的消息被发送
                    int index;
                    for (index = 0; index <= i; index++) {
                        one_msg[index + 8] = msg_tobesend[index];
                    }
                    one_msg[index + 8] = '\0';

                    // 发送消息给聊天室的各个client
                    for (int j = 0; j < Max_Client; j++) {
                        if (Clients[j].client_fd != 0 && Clients[j].client_fd != fd1) {
                            pthread_mutex_lock(&Clients[j].send_mutex); // 加锁
                            Clients[j].send_queue.push(one_msg);     // 发送
                            pthread_cond_signal(&Clients[j].cv);        // signal，生产者消费者问题
                            pthread_mutex_unlock(&Clients[j].send_mutex);   // 释放
                        }
                    }

                    int tmp = i;
                    for (int j = 0; j + tmp + 1 <= strlen(msg_tobesend); j++) {
                        msg_tobesend[j] = msg_tobesend[j + i + 1];
                    }
                    i = -1;
                }
            }
        }
        if (len <= 0) { // recv 出错或者连接关闭
            Clients[k].client_fd = 0;
        }
    }
}

void* handle_send(void* data)
{
    struct Client* c = (struct Client*)data;
    while (true) {
        pthread_mutex_lock(&c->send_mutex); // 加锁
        while (c->send_queue.empty()) {
            pthread_cond_wait(&c->cv, &c->send_mutex);
        } // 生产者消费者问题
        // 继续向下进行，说明有消息待发送
        char one_msg[1050000];  // 准备 send 的一条信息
        memset(one_msg, '\0', ONE_MB);
        strcpy(one_msg, c->send_queue.front());
        c->send_queue.pop();
        pthread_mutex_unlock(&c->send_mutex);
        int send_len = send(c->client_fd, one_msg, strlen(one_msg), 0);
        while (send_len != strlen(one_msg)) {
            for (int k = 0; k + send_len <= strlen(one_msg); k++) {
                one_msg[k] = one_msg[k + send_len];
            }
            send_len = send(c->client_fd, one_msg, strlen(one_msg), 0);
        }
    }
}


int main(int argc, char** argv) 
{
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
    if (listen(fd, 2)) {
        perror("listen");
        return 1;
    }
    pthread_t thread_recv[Max_Client];
    pthread_t thread_send[Max_Client];
    for (int i = 0; i < Max_Client; i++) {
        pthread_create(&thread_recv[i], NULL, handle_recv, (void *)&fd);
        pthread_create(&thread_send[i], NULL, handle_send, (void *)&Clients[i]);
    }
    for (int i = 0; i < Max_Client; i++) {
        pthread_join(thread_recv[i], NULL);
        pthread_join(thread_send[i], NULL);
    }
    
    return 0;
}