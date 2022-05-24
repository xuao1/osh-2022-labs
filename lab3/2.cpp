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
    // ����ź��Ǵ������������������⣬������Ͷ���Ϊ�գ���ô���ͽ��̾���Ҫ�ȴ�
    // recv �������Ͷ���д�����ݺ�signal ���ͽ���
    queue<char*> send_queue;
    // ����Ϣ���е�д���ɾ�����������ǻ���ģ�������Ҫ����
    int client_fd = 0;
}Clients[Max_Client+5];


void* handle_recv(void* data)
{
    int fd = *(int*)data;
    int fd1;
    while (true) {
        fd1 = accept(fd, NULL, NULL); // ��֤�û��˳��������û�����ʹ����һ client
        if (fd == -1) {
            perror("accept");
            return NULL;
        }
        int k; // ��¼��ǰ client �� Clients ������±�
        for (k = 0; k < Max_Client; k++) { // �¼����client������client����
            if (Clients[k].client_fd == 0) {
                Clients[k].client_fd = fd1;
                break;
            }
        }
        if (k == Max_Client) { // �������Ѿ���32��client
            perror("join in chatting");
            return NULL;
        }
        ssize_t len;
        char msg_tobesend[2*ONE_MB]; // �ۼƵ���Ϣ
        memset(msg_tobesend, '\0', 2*ONE_MB);
        char msg_recv[ONE_MB + 500]; // recv ������Ϣ
        char one_msg[ONE_MB + 500] = "Message:";  // ׼�� send ��һ����Ϣ
        while (len = recv(fd1, msg_recv, Max_Client, 0) > 0) { // ������������
            int len_mts = strlen(msg_tobesend);
            for (int i = 0; i <= strlen(msg_recv); i++) {
                msg_tobesend[len_mts] = msg_recv[i];
                len_mts++;
            }
            for (int i = 0; i < strlen(msg_tobesend); i++) {
                if (msg_tobesend[i] == '\n') {
                    // ������Ϣ���˻س�֮ǰ����Ϣ������
                    int index;
                    for (index = 0; index <= i; index++) {
                        one_msg[index + 8] = msg_tobesend[index];
                    }
                    one_msg[index + 8] = '\0';

                    // ������Ϣ�������ҵĸ���client
                    for (int j = 0; j < Max_Client; j++) {
                        if (Clients[j].client_fd != 0 && Clients[j].client_fd != fd1) {
                            pthread_mutex_lock(&Clients[j].send_mutex); // ����
                            Clients[j].send_queue.push(one_msg);     // ����
                            pthread_cond_signal(&Clients[j].cv);        // signal������������������
                            pthread_mutex_unlock(&Clients[j].send_mutex);   // �ͷ�
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
        if (len <= 0) { // recv ����������ӹر�
            Clients[k].client_fd = 0;
        }
    }
}

void* handle_send(void* data)
{
    struct Client* c = (struct Client*)data;
    while (true) {
        pthread_mutex_lock(&c->send_mutex); // ����
        while (c->send_queue.empty()) {
            pthread_cond_wait(&c->cv, &c->send_mutex);
        } // ����������������
        // �������½��У�˵������Ϣ������
        char one_msg[1050000];  // ׼�� send ��һ����Ϣ
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