#include "func.h"

namespace {

bool SendPacket(char type, const std::string& payload = "") {
    // TODO: 组装报文（type + payload）并发送
    return false;
}

}  // namespace

void PrintPrompt() {
    std::cout << "\n[CLI] Your command: " << std::flush;
}

void ServerConnect() {
    // TODO: 创建 socket，读 IP/Port，connect，设置 connected 状态，启动 Recieve 线程
}

void ServerDisconnect() {
    // TODO: 发送 DISCONNECT，关闭 socket，更新 connected 状态
}

void GetTime() {
    // TODO: 发送 TIME 请求
}

void GetName() {
    // TODO: 发送 NAME 请求
}

void ClientList() {
    // TODO: 发送 LIST 请求
}

void SendMessa() {
    // TODO: 读取目标 id 和消息内容，发送 MESSAGE 报文（格式：id$内容）
}

void ExitComm() {
    // TODO: 如果已连接先关闭socket，再退出
}

void* Recieve(void* lpParameter) {
    // TODO: 循环 recv，解析 type/payload，并打印到终端
    // CONNECT: 输出自身 ID
    // TIME: 输出当前时间
    // NAME: 输出服务器主机名
    // LIST: 解析 id 列表，格式类似：id1$id2$...
    // MESSAGE: 提示目标不存在
    // SIGNAL: 打印来自其他客户端的消息（id$内容）
    (void)lpParameter;
    return nullptr;
}
