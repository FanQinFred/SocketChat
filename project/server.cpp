// server.cpp
// create by ReJ
// date: 2018/12/14
#include <cstdlib>
#include <cstdio>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <ctime>
#include <string>
#include <mysql/mysql.h>
#include <string.h>
#include <sys/select.h>
#include <set>
#include <unordered_map>
#include <vector>
#include "header/Message/Message.h"
#define max(a, b) (a > b ? a : b)
#define HOST "114.55.95.14"
#define USER_NAME "users"
#define PASSWORD "123456"
#define DATABASE "users"
#define BACKLOG 128
#define MAX_DATA_SIZE 1024
using std::string;
using std::set;
using std::vector;;

class User {
public:
    User(){};
    User(int _sock, int _id, char * _name, char * _password) {
        id = _id;
        sock = _sock;
        strcpy(name, _name);
        strcpy(password, _password);
    }
    bool operator < (const User & b) const {
        return id < b.id;
    }
    int op;
    int id;
    int sock;
    char IP[15];
    char name[15];
    char password[20];
};
class Server {
public:
    Server(int port) {
        find_user_info.clear();
        server_sock = socket(AF_INET, SOCK_STREAM, 0);
        client_sock = socket(AF_INET, SOCK_STREAM, 0);
        memset(&client_addr, 0, sizeof(client_addr));
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        //Work();
    }

    bool mysql_check_login(User & login_user) {
        MYSQL mysql_conn;
        MYSQL_RES * ptr_res;
        MYSQL_ROW result_row;
        char sql[256] = ("select * from clients where id = \'");
        strcat(sql, std::to_string(login_user.id).c_str());
        strcat(sql, const_cast<char *>("\' and user_password = \'"));
        strcat(sql, login_user.password);
        strcat(sql, const_cast<char *>("\'"));
        printf("sql: %s\n", sql);
        mysql_init(&mysql_conn);
        if(mysql_real_connect(&mysql_conn, HOST, USER_NAME, PASSWORD, DATABASE, 3306, NULL, CLIENT_FOUND_ROWS)) {
            int res = mysql_query(&mysql_conn, sql);
            if(res) {
                puts("select error");
                return false;
            } else {
                ptr_res = mysql_store_result(&mysql_conn);
                int row = mysql_num_rows(ptr_res);
                if(row == 0) {
                    printf("%d is not valid...\n", login_user.id);
                    return false;
                } else {
                    printf("accept %d...\n", login_user.id);
                    result_row = mysql_fetch_row(ptr_res);
                    strcpy(login_user.name, result_row[1]);
                    return true;
                }
            }
        } else {
            perror("mysql connect failed");
            return false;
        }

    }

    bool mysql_register(User &client) {
        MYSQL mysql_conn;
        MYSQL_RES * ptr_res;
        MYSQL_ROW result_row;
        mysql_init(&mysql_conn);
        char insr[256];
        sprintf(insr, "insert into clients values('%d', '%s', '%s');", client.id, client.name, client.password);
        char sql[256] = "select * from clients where id = \'";
        strcat(sql, std::to_string(client.id).c_str());
        strcat(sql, "\'");
        if(mysql_real_connect(&mysql_conn, HOST, USER_NAME, PASSWORD, DATABASE, 0, NULL, 0)) {
            int res = mysql_query(&mysql_conn, sql);
            if(res) {
                return false;
            } else {
                ptr_res = mysql_store_result(&mysql_conn);
                int row = mysql_num_rows(ptr_res);
                if(row > 0) return false;
                else {
                   res = mysql_query(&mysql_conn, insr);
                   if(res) return false;
                   return true;
                }
            }
        }
        return true;
    }

    void printtime() {
        time_t timeep;
        time(&timeep);
        printf("%s", ctime(&timeep));
    }

    bool Work() {
        int bind_fd;
        if((bind_fd = bind(server_sock, (sockaddr *) & server_addr, sizeof(server_addr))) < 0) {
            perror("bind failed");
            exit(1);
        }
        puts("success to bind...");
        int lis;
        if((lis = listen(server_sock, BACKLOG)) < 0) {
            perror("listen failed");
            exit(1);
        }
        puts("success to listen server...");
        timeval timeout = {0};
        int server_max_fd = 0, client_max_fd = 0;

        socklen_t size = sizeof(client_addr);

        while(true) {
            FD_ZERO(&server_fd);
            FD_ZERO(&client_fd);
            FD_ZERO(&stdin_fd);
            find_user_info.clear();
            //printf("%d\n", server_sock);
            FD_SET(server_sock, &server_fd);
            server_max_fd = 0;
            server_max_fd = max(server_max_fd, server_sock);
            client_max_fd = 0;
            timeout.tv_usec = 500;
            // select accept
            switch (select(server_max_fd + 1, &server_fd, NULL, NULL, &timeout)) {
                case -1: {
                    perror("select failed");
                    exit(1);
                    break;
                }
                case 0: break;
                default : {
                    if(FD_ISSET(server_sock, &server_fd)) {
                        client_sock = accept(server_sock, (sockaddr * ) & client_addr, &size);
                        if(client_sock < 0) {
                            perror("accept failed");
                            exit(1);
                        }
                        if(read(client_sock, (char *) & client, sizeof(User)) < 0) {
                            perror("read failed");
                            close(client_sock);
                        } else {
                            printf("user_name: %d\nuser_password: %s\n", client.id, client.password);
                            if(client.op == 0) {
                                if(!mysql_register(client)) {
                                    strcpy(send_buff, "no");
                                    write(client_sock, send_buff, sizeof(send_buff));
                                    printf("%d register failed", client.id);
                                    break;
                                }
                            }
                            if(mysql_check_login(client)) {
                                client.sock = client_sock;
                                inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client.IP, sizeof(client.IP));
                                total_user.insert(client);
                                printf("%s has login...\n", client.name);
                                printf("now has %d users online...\n", (int)total_user.size());
                                strcpy(send_buff, "yes");
                                write(client_sock, send_buff, sizeof(send_buff));
                            } else {
                                strcpy(send_buff, "no");
                                write(client_sock, send_buff, sizeof(send_buff));
                                printf("Reject login!");
                            }
                            memset(send_buff, 0, MAX_DATA_SIZE);
                        }
                    }
                    break;          
                }
            } 
            /***************select accept end********************/ 

            for(auto user : total_user) {
                FD_SET(user.sock, &client_fd);
                find_user_info[user.id] = user;
                client_max_fd = max(client_max_fd, user.sock);
                //printf("%d\n", user.id);
            }

            /***************select read start*******************/
            switch (select(client_max_fd + 1, &client_fd, NULL, NULL, &timeout)) {
                case -1: {
                    perror("select failed");
                    exit(1);
                }    
                case 0: break;
                default :{
                    vector<User> offline_user;
                    offline_user.clear();
                    for(auto user : total_user) {
                        if(FD_ISSET(user.sock, &client_fd)) {
                            message.clear();
                            ssize_t recv_len = recv(user.sock, (char *) & message, sizeof(Message), 0);
                            if(recv_len <= 0) {
                                printf("%d has offline...\n", user.id);
                                if(find_user_info.find(user.id) != find_user_info.end())
                                    find_user_info.erase(user.id);
                                offline_user.push_back(user); 
                                close(user.sock);
                            } else {
                                printtime();
                                printf("%s -> %s: %s\n", user.name,find_user_info[message.getAddress()].name, message.getMessage());
                                sprintf(send_buff, "%s: %s", user.name, message.getMessage());
                                write(find_user_info[message.getAddress()].sock, send_buff, strlen(send_buff));
                                message.clear();
                                memset(send_buff, 0, sizeof(send_buff));
                            }
                            message.clear();
                        }
                    }
                    for(auto user : offline_user) {
                        total_user.erase(user);
                    }
                }
                break;
            }
            /************select read end***********************/
            
            FD_SET(STDIN_FILENO, &stdin_fd);

            /************select write start********************/
            
            switch (select(STDIN_FILENO + 1, &stdin_fd, NULL, NULL, &timeout)) {
                case -1: {
                    perror("select failed");
                    exit(1);
                }
                case 0: break;
                default : {
                    if(FD_ISSET(STDIN_FILENO, &stdin_fd)) {
                        read(STDIN_FILENO, send_buff, MAX_DATA_SIZE);
                        if(strlen(send_buff) == 0) break;
                        std::string send_message = "Server: " + (std::string)send_buff;
                        for(auto user : total_user) {
                            ssize_t len = write(user.sock, send_message.c_str(), send_message.length());
                            if(len < 0) {
                                perror("send faied");
                            } else {
                                printf("send Mesage to %s succenss!\n", user.name);
                            }
                        }
                    }          
                    break;
                }
            }
            /***************select write end*******************/
        }
    }
private:
    int client_sock;
    int server_sock;
    Message message;
    User client;
    std::unordered_map<int, User> find_user_info;
    std::set<User> total_user;
    sockaddr_in server_addr, client_addr;
    char IP[MAX_DATA_SIZE], recv_buff[MAX_DATA_SIZE], send_buff[MAX_DATA_SIZE];
    fd_set server_fd, client_fd, stdin_fd;
};

int main(int argc, char * argv[]) {
    if(argc != 2) {
        puts("Usage: ./server <PORT>");
        exit(1);
    }
    Server chat_server(atoi(argv[1]));
    chat_server.Work();
    return 0;
}
