#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define ONE_MB 1048576  

struct Pipe {
    int fd_send;
    int fd_recv;
};

void *handle_chat(void *data) {
    struct Pipe *pipe = (struct Pipe *)data;
    char msg_tobesend[2100000]; // �ۼƵ���Ϣ
    memset(msg_tobesend, '\0', ONE_MB);
    char buffer[1050000];   // ÿ�� recv ���ܵ�����Ϣ
    char one_msg[1050000] = "Message:";  // ׼�� send ��һ����Ϣ
    ssize_t len;
    // ������Ϣ������Ҫ��Σ�������Ϣ���Իس�Ϊ��������
    // ���һ�ν��յ�����Ϣ�������ᱻ�ضϣ���ô��һ�� recv �������ϢӦ��ƴ�ӵ��ϴ�û�������Ϣ��
    while ((len = recv(pipe->fd_send, buffer, ONE_MB, 0)) > 0) {
        int len_mts = strlen(msg_tobesend);
        for (int i = 0; i <= strlen(buffer); i++) {
            msg_tobesend[len_mts] = buffer[i];
            len_mts++;
        }
        for (int i = 0; i < strlen(msg_tobesend); i++) {
            if (msg_tobesend[i] == '\n') {
                // ������Ϣ���˻س�֮ǰ����Ϣ������
                int j;
                for (j = 0; j <= i; j++) {
                    one_msg[j + 8] = msg_tobesend[j];
                }
                one_msg[j+8] = '\0';
                // ������Ϣ�����һ�η���ȫ�����η���
                int send_len = send(pipe->fd_recv, one_msg, strlen(one_msg), 0);
                while (send_len != strlen(one_msg)) {
                    for (int k = 0; k + send_len <= strlen(one_msg); k++) {
                        one_msg[k] = one_msg[k + send_len];
                    }
                    send_len = send(pipe->fd_recv, one_msg, strlen(one_msg), 0);
                }
                int tmp = i;
                for (int k = 0; k + tmp + 1 <= strlen(msg_tobesend); k++) {
                    msg_tobesend[k] = msg_tobesend[k + i + 1];
                }
                i = -1;
            }
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
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
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind");
        return 1;
    }
    if (listen(fd, 2)) {
        perror("listen");
        return 1;
    }
    int fd1 = accept(fd, NULL, NULL);
    int fd2 = accept(fd, NULL, NULL);
    if (fd1 == -1 || fd2 == -1) {
        perror("accept");
        return 1;
    }
    pthread_t thread1, thread2;
    struct Pipe pipe1;
    struct Pipe pipe2;
    pipe1.fd_send = fd1;
    pipe1.fd_recv = fd2;
    pipe2.fd_send = fd2;
    pipe2.fd_recv = fd1;
    pthread_create(&thread1, NULL, handle_chat, (void *)&pipe1);
    pthread_create(&thread2, NULL, handle_chat, (void *)&pipe2);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    return 0;
}
