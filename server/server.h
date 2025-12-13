#ifndef _SERVER_H_
#define _SERVER_H_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <ctime>
#include <iostream>
#include <map>
#include <string>

// 常量与协议类型
constexpr size_t MAXBUF = 4096;
constexpr char INVALID = 0;
constexpr char CONNECT = 1;
constexpr char SIGNAL = 2;
constexpr char TIME = 3;
constexpr char NAME = 4;
constexpr char LIST = 5;
constexpr char MESSAGE = 6;
constexpr char DISCONNECT = 7;

void BuildServer();
void* Recieve(void* lpParameter);

extern int BaseSock;
extern sockaddr_in Addr;
extern u_short Port;
extern std::map<int, int> clients;
extern pthread_mutex_t clients_mutex;

#endif
