#ifndef _FUNC_H_
#define _FUNC_H_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <iostream>
#include <string>

// 协议常量
constexpr size_t MAXBUF = 256;
constexpr char INVALID = 0;
constexpr char CONNECT = 1;
constexpr char SIGNAL = 2;
constexpr char TIME = 3;
constexpr char NAME = 4;
constexpr char LIST = 5;
constexpr char MESSAGE = 6;
constexpr char DISCONNECT = 7;

void ServerConnect();
void ServerDisconnect();
void GetTime();
void GetName();
void ClientList();
void SendMessa();
void ExitComm();
void* Recieve(void* lpParameter);
void PrintPrompt();

extern bool connected;
extern int BaseSock;
extern sockaddr_in Addr;
extern pthread_t thread;

#endif
