#include "server.h"

// 全局变量初始化
int BaseSock = -1;
sockaddr_in Addr {};
// 请填入你的学号后四位作为端口号
u_short Port = 22;
std::map<int, int> clients;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// 可选的工具函数示例（需要同学们完成或替换）
bool SendPacket(int sock, char type, const std::string& payload) {
    // TODO: 组装报文（类型 + 负载）并通过 send 发送
    return false;
}

void RemoveClient(int sock) {
    // TODO: 保护访问 clients 的互斥锁，删除断开的 sock，并关闭连接
}

void BuildServer() {
    // TODO: 创建 socket，设置地址/端口，bind + listen，循环 accept 客户端
    //       为每个客户端创建线程 pthread_create(&tid, nullptr, &Recieve, arg);
    // 提示：记得为新连接存储 sock，并使用 pthread_detach(tid) 分离线程
}

void* Recieve(void* lpParameter) {
    // TODO: 将 void* 转为 int sock，循环 recv
    // 根据 Buffer[0] 的类型分发：CONNECT / TIME / NAME / LIST / MESSAGE / DISCONNECT
    // 需要准备 payload（Buffer+1...）并调用 SendPacket 返回结果
    // 断开时调用 RemoveClient(sock)
    return nullptr;
}

int main() {
    BuildServer();
}
