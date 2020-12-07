#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <algorithm>
#include <sys/errno.h>
#include <sys/select.h>
#include "header/Message/Message.h"

#define MAX_DATA_SIZE 1024

class User {
public:
    User() {};
    User(int _sock, int _id, char * _name, char * _password) {
        id = _id;
        sock = _sock;
        strcpy(name, _name);
        strcpy(password, _password);
    }
    int op;
    int id;
    int sock;
    char IP[15];
    char name[15];
    char password[20];
};

void printtime() {
    time_t timeep;
    time(&timeep);
    printf("%s", ctime(&timeep));
}

int main(int argc, char * argv[]) {
    if(argc != 6) {
        puts("./cient <IP> <PORT> <ID> <PASSWORD> <OP>");
        exit(0);
    }
    fd_set server_fd;
    char send_buff[MAX_DATA_SIZE], recv_buff[MAX_DATA_SIZE];
    User client;
    if(atoi(argv[5]) == 0) {
        printf("please input your user_name: ");
        scanf("%s", client.name);
    }
    Message send_message;
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    printf("%s loading...\n", argv[3]); 
    if(connect(server_sock, (sockaddr *) & server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        exit(1);
    }
    client.op = atoi(argv[5]);
    client.id = atoi(argv[3]);
    strcpy(client.password, argv[4]);
    if((write(server_sock, (char *) & client, sizeof(client))) <= 0) {
        perror("send message failed...");
        exit(1);
    }
    if(read(server_sock, recv_buff, MAX_DATA_SIZE) <= 0) {
        perror("read result failed...");
        exit(1);
    }
    if(atoi(argv[5]) == 1) {
        if(recv_buff[0] == 'n') {
            puts("please check user_name or user_password");
            exit(0);
        }
        puts("登录成功");
        puts("********************************************************");
        memset(recv_buff, 0, sizeof(recv_buff));
    } else {
        if(recv_buff[0] == 'n') {
            perror("register failed");
            exit(1);
        } else {
            printf("register succenss ans has login with user: %s\n", client.name);
        }
        memset(recv_buff, 0, sizeof(recv_buff));
    }
    int max_server_fd = 0;
    timeval timeout = {0};
    timeout.tv_usec = 500;
    while(true) {
        FD_ZERO(&server_fd);
        FD_SET(server_sock, &server_fd);
        max_server_fd = std::max(max_server_fd, server_sock);
        switch (select(max_server_fd + 1, &server_fd, NULL, NULL, &timeout)) {
            case -1: {
                perror("select error");
                exit(1);
            } 
            case 0: break;
            default : { 
                if(FD_ISSET(server_sock, &server_fd)) {
                     ssize_t recv_size = read(server_sock, recv_buff, MAX_DATA_SIZE);
                     if(recv_size <= 0) {
                        puts("server close");
                        close(server_sock);
                        exit(1);
                     }
                     printtime();
                     //printf("recive message!!!!!!!\n");
                     printf("%s\n", recv_buff);
                     memset(recv_buff, 0, sizeof(recv_buff));
                }
            }
        }
        FD_ZERO(&server_fd);
        FD_SET(STDIN_FILENO, &server_fd);
        switch (select(STDIN_FILENO + 1, &server_fd, NULL, NULL, &timeout)) {
            case -1: {
                perror("select error");
                exit(1);
            }
            case 0: break;
            default : {
                if(FD_ISSET(STDIN_FILENO, &server_fd)) {
                    ssize_t recv_len = read(STDIN_FILENO, recv_buff, MAX_DATA_SIZE);
                    if(recv_len <= 0) {
                        perror("read failed");
                        exit(1);
                    }
                    //check message is vaild
                    if(send_message.setMessage((std::string)recv_buff) == false) {
                        puts("Usage: message->user_id");
                        break;
                    }
                    ssize_t send_len = send(server_sock, (char *) &send_message , sizeof(Message), 0);
                    if(send_len <= 0) {
                        perror("write failed");
                        exit(1);
                    }
                    memset(recv_buff, 0, sizeof(recv_buff));
                }
                break;
            }
        }
    }
    return 0;
}