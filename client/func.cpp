#include "func.h"

#include <atomic>
#include <limits>

namespace {

constexpr int kTimeBurstCount = 100;

std::atomic<int> time_expected {0};
std::atomic<int> time_received {0};

bool SendPacket(const char type, const std::string& payload = "") {
    // TODO: 组装报文（type + payload）并发送
    if (!connected || BaseSock < 0) {
        std::cout << "[SYS] Not connected.\n";
        return false;
    }

    std::string packet;
    // 以 '\n' 作为包结束符，便于接收端在 TCP 字节流中拆包。
    packet.reserve(2 + payload.size());
    packet.push_back(type);
    packet += payload;
    packet.push_back('\n');

    size_t sent = 0;
    while (sent < packet.size()) {
        const ssize_t n = ::send(BaseSock, packet.data() + sent, packet.size() - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

}  // namespace

void PrintPrompt() {
    std::cout << "\n[CLI] Your command: " << std::flush;
}

void ServerConnect() {
    // TODO: 创建 socket，读 IP/Port，connect，设置 connected 状态，启动 Recieve 线程
    if (connected) {
        std::cout << "[SYS] Already connected.\n";
        return;
    }

    std::cout << "\n[CONNECT]\n";
    std::cout << "[SYS] Please enter the IP address and port you want to connect to.\n";

    std::string ip;
    int port = 0;
    std::cout << "[CLI] IP address: " << std::flush;
    if (!(std::cin >> ip)) {
        return;
    }
    std::cout << "[CLI] Port number: " << std::flush;
    if (!(std::cin >> port)) {
        return;
    }

    std::cout << "[SYS] Connecting ...\n";

    const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cout << "[SYS] Connection Failed.\n";
        return;
    }

    Addr = {};
    Addr.sin_family = AF_INET;
    Addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::inet_pton(AF_INET, ip.c_str(), &Addr.sin_addr) != 1) {
        std::cout << "[SYS] Connection Failed.\n";
        ::close(sock);
        return;
    }

    if (::connect(sock, reinterpret_cast<sockaddr*>(&Addr), sizeof(Addr)) < 0) {
        std::cout << "[SYS] Connection Failed.\n";
        ::close(sock);
        return;
    }

    std::cout << "[SYS] Connection Success!\n";

    BaseSock = sock;
    connected = true;

    if (pthread_create(&thread, nullptr, &Recieve, nullptr) != 0) {
        std::cout << "[SYS] Failed to create receive thread.\n";
        connected = false;
        ::close(BaseSock);
        BaseSock = -1;
        return;
    }
    pthread_detach(thread);

    // 通知服务器为本连接分配 ID
    (void)SendPacket(CONNECT);
}

void ServerDisconnect() {
    // TODO: 发送 DISCONNECT，关闭 socket，更新 connected 状态
    if (!connected) {
        std::cout << "[SYS] Not connected.\n";
        return;
    }

    std::cout << "\n[DISCONNECT]\n";
    (void)SendPacket(DISCONNECT);

    // shutdown 用来打断 Recieve 线程的阻塞 recv
    ::shutdown(BaseSock, SHUT_RDWR);
    ::close(BaseSock);
    BaseSock = -1;
    connected = false;
    std::cout << "[SYS] Disconnection Success\n";
}

void GetTime() {
    // TODO: 发送 TIME 请求
    std::cout << "\n[TIME]\n";

    time_expected.store(kTimeBurstCount);
    time_received.store(0);

    int sent_ok = 0;
    for (int i = 0; i < kTimeBurstCount; ++i) {
        if (SendPacket(TIME)) {
            ++sent_ok;
        }
    }
    std::cout << "[SYS] TIME requests sent: " << sent_ok << "/" << kTimeBurstCount << "\n";
}

void GetName() {
    // TODO: 发送 NAME 请求
    std::cout << "\n[NAME]\n";
    if (SendPacket(NAME)) {
        std::cout << "[SYS] Request Sending Success\n";
    }
}

void ClientList() {
    // TODO: 发送 LIST 请求
    std::cout << "\n[LIST]\n";
    if (SendPacket(LIST)) {
        std::cout << "[SYS] Request Sending Success\n";
    }
}

void SendMessa() {
    // TODO: 读取目标 id 和消息内容，发送 MESSAGE 报文（格式：id$内容）
    if (!connected) {
        std::cout << "[SYS] Not connected.\n";
        return;
    }

    std::cout << "\n[MESSAGE]\n";
    std::cout << "[SYS] Please Enter Other Clients' ID and the Message (only one line)\n";

    std::string id;
    std::cout << "[CLI] Client ID: " << std::flush;
    if (!(std::cin >> id)) {
        return;
    }

    // 读取剩余行（支持空格）
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::string content;
    std::cout << "[CLI] Message: " << std::flush;
    if (!std::getline(std::cin, content)) {
        return;
    }

    const std::string payload = id + "$" + content;
    if (SendPacket(MESSAGE, payload)) {
        std::cout << "[SYS] Message Sending Success.\n";
    }
}

void ExitComm() {
    // TODO: 如果已连接先关闭socket，再退出
    if (connected) {
        ServerDisconnect();
    }
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

    std::string inbuf;
    inbuf.reserve(MAXBUF * 2);

    char buffer[MAXBUF];
    while (connected && BaseSock >= 0) {
        const ssize_t n = ::recv(BaseSock, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            break;
        }

        inbuf.append(buffer, buffer + n);

        size_t line_end = 0;
        while ((line_end = inbuf.find('\n')) != std::string::npos) {
            const std::string packet = inbuf.substr(0, line_end);
            inbuf.erase(0, line_end + 1);

            if (packet.empty()) {
                continue;
            }

            const char type = packet[0];
            const std::string payload = packet.substr(1);

            switch (type) {
                case CONNECT:
                    std::cout << "\n[SYS] Your Client ID is " << payload << "\n";
                    break;
                case TIME: {
                    const int got = time_received.fetch_add(1) + 1;
                    const int expected = time_expected.load();
                    if (expected > 0) {
                        std::cout << "\n[SYS] (" << got << "/" << expected << ") Current Time is: " << payload << "\n";
                        if (got == expected) {
                            std::cout << "[SYS] Received all TIME responses: " << got << "/" << expected << "\n";
                        }
                    } else {
                        std::cout << "\n[SYS] Current Time is: " << payload << "\n";
                    }
                    break;
                }
                case NAME:
                    std::cout << "\n[SYS] Name of machine is: " << payload << "\n";
                    break;
                case LIST: {
                    std::cout << "\n[SYS] Current Clients List is: ";
                    if (payload.empty()) {
                        std::cout << "(empty)\n";
                        break;
                    }
                    // payload: id1$id2$...
                    size_t start = 0;
                    bool first = true;
                    while (start < payload.size()) {
                        const size_t pos = payload.find('$', start);
                        const std::string id = (pos == std::string::npos) ? payload.substr(start) : payload.substr(start, pos - start);
                        if (!id.empty()) {
                            if (!first) {
                                std::cout << ", ";
                            }
                            std::cout << id;
                            first = false;
                        }
                        if (pos == std::string::npos) {
                            break;
                        }
                        start = pos + 1;
                    }
                    std::cout << "\n";
                    break;
                }
                case SIGNAL: {
                    // payload: fromId$content
                    const size_t pos = payload.find('$');
                    if (pos == std::string::npos) {
                        std::cout << "\n[RECEIVE MESSAGE]\n";
                        std::cout << "[SYS] Receive a message:\n";
                        std::cout << payload << "\n";
                    } else {
                        const std::string from = payload.substr(0, pos);
                        const std::string content = payload.substr(pos + 1);
                        std::cout << "\n[RECEIVE MESSAGE]\n";
                        std::cout << "[SYS] Client " << from << " Just Sent A Message to you:\n";
                        std::cout << content << "\n";
                    }
                    break;
                }
                case MESSAGE:
                    std::cout << "\n[SYS] " << payload << "\n";
                    break;
                case INVALID:
                    std::cout << "\n[SYS] " << payload << "\n";
                    break;
                default:
                    std::cout << "\n[SYS] Unknown packet type: " << static_cast<int>(type) << ", payload: " << payload << "\n";
                    break;
            }

            PrintPrompt();
        }
    }

    if (connected) {
        // 连接被动断开时，重置状态
        connected = false;
        if (BaseSock >= 0) {
            ::close(BaseSock);
            BaseSock = -1;
        }
        std::cout << "\n[SYS] Connection closed by server.\n";
        PrintPrompt();
    }
    return nullptr;
}
