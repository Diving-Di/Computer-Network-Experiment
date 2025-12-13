#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <string>

void Initialize();
void Help(bool wrong);

extern bool connected;
extern int BaseSock;
extern sockaddr_in Addr;
extern pthread_t thread;

#endif
