#include "server.h"

#include <cerrno>
#include <cstring>

// 全局变量初始化
int BaseSock = -1;
sockaddr_in Addr {};
// 请填入你的学号后四位作为端口号
u_short Port = 3902;
std::map<int, int> clients;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// 可选的工具函数示例（需要同学们完成或替换）
namespace {

std::string CurrentTimeString() {
    const std::time_t now = std::time(nullptr);
    std::string s = std::ctime(&now);
    // ctime() 末尾带 \n
    if (!s.empty() && s.back() == '\n') {
        s.pop_back();
    }
    return s;
}

std::string HostnameString() {
    char name[256] = {0};
    if (::gethostname(name, sizeof(name) - 1) != 0) {
        return "unknown";
    }
    return std::string(name);
}

int GetClientIdLocked(const int sock) {
    const auto it = clients.find(sock);
    if (it == clients.end()) {
        return -1;
    }
    return it->second;
}

}  // namespace

bool SendPacket(int sock, char type, const std::string& payload) {
    // TODO: 组装报文（类型 + 负载）并通过 send 发送
    std::string packet;
    // 以 '\n' 作为包结束符，便于接收端在 TCP 字节流中拆包。
    packet.reserve(2 + payload.size());
    packet.push_back(type);
    packet += payload;
    packet.push_back('\n');

    size_t sent = 0;
    while (sent < packet.size()) {
        const ssize_t n = ::send(sock, packet.data() + sent, packet.size() - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

void RemoveClient(int sock) {
    // TODO: 保护访问 clients 的互斥锁，删除断开的 sock，并关闭连接
    pthread_mutex_lock(&clients_mutex);
    clients.erase(sock);
    pthread_mutex_unlock(&clients_mutex);
    ::shutdown(sock, SHUT_RDWR);
    ::close(sock);
}

void BuildServer() {
    // TODO: 创建 socket，设置地址/端口，bind + listen，循环 accept 客户端
    //       为每个客户端创建线程 pthread_create(&tid, nullptr, &Recieve, arg);
    // 提示：记得为新连接存储 sock，并使用 pthread_detach(tid) 分离线程
    std::cout << "[SRV] Server Building\n";
    BaseSock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (BaseSock < 0) {
        std::cout << "[SRV] Server Build Failed.\n";
        return;
    }

    int opt = 1;
    (void)::setsockopt(BaseSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    Addr = {};
    Addr.sin_family = AF_INET;
    Addr.sin_addr.s_addr = htonl(INADDR_ANY);
    Addr.sin_port = htons(Port);

    if (::bind(BaseSock, reinterpret_cast<sockaddr*>(&Addr), sizeof(Addr)) < 0) {
        std::cout << "[SRV] Server Build Failed.\n";
        ::close(BaseSock);
        BaseSock = -1;
        return;
    }

    if (::listen(BaseSock, 16) < 0) {
        std::cout << "[SRV] Server Build Failed.\n";
        ::close(BaseSock);
        BaseSock = -1;
        return;
    }

    std::cout << "[SRV] Server Build Successfully.\n";
    std::cout << "[SRV] Waiting for packs.\n";

    while (true) {
        sockaddr_in client_addr {};
        socklen_t len = sizeof(client_addr);
        const int sock = ::accept(BaseSock, reinterpret_cast<sockaddr*>(&client_addr), &len);
        if (sock < 0) {
            std::cout << "[SYS] accept() failed: " << std::strerror(errno) << "\n";
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        clients[sock] = -1;  // 尚未 CONNECT 分配 ID
        pthread_mutex_unlock(&clients_mutex);

        auto* arg = new int(sock);
        pthread_t tid {};
        if (pthread_create(&tid, nullptr, &Recieve, arg) != 0) {
            std::cout << "[SYS] pthread_create() failed.\n";
            delete arg;
            RemoveClient(sock);
            continue;
        }
        pthread_detach(tid);
    }
}

void* Recieve(void* lpParameter) {
    // TODO: 将 void* 转为 int sock，循环 recv
    // 根据 Buffer[0] 的类型分发：CONNECT / TIME / NAME / LIST / MESSAGE / DISCONNECT
    // 需要准备 payload（Buffer+1...）并调用 SendPacket 返回结果
    // 断开时调用 RemoveClient(sock)
    const int sock = *reinterpret_cast<int*>(lpParameter);
    delete reinterpret_cast<int*>(lpParameter);

    static int next_id = 1;

    int self_id = -1;
    int time_req_count = 0;

    std::string inbuf;
    inbuf.reserve(MAXBUF * 2);

    char buffer[MAXBUF];
    while (true) {
        const ssize_t n = ::recv(sock, buffer, sizeof(buffer), 0);
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

            if (type == CONNECT) {
                int id = -1;
                pthread_mutex_lock(&clients_mutex);
                id = next_id++;
                clients[sock] = id;
                pthread_mutex_unlock(&clients_mutex);

                std::cout << "[SRV] Client " << id << " connect successfully!\n";
                (void)SendPacket(sock, CONNECT, std::to_string(id));
                std::cout << "[SRV] Client ID returned.\n";
                continue;
            }

            pthread_mutex_lock(&clients_mutex);
            self_id = GetClientIdLocked(sock);
            pthread_mutex_unlock(&clients_mutex);

            if (type == TIME) {
                ++time_req_count;
                (void)SendPacket(sock, TIME, CurrentTimeString());
                if (time_req_count == 100) {
                    std::cout << "[SRV] Client " << self_id << " TIME handled 100 times.\n";
                }
                continue;
            }

            if (type == NAME) {
                std::cout << "[SRV] Receive a NAME request from client " << self_id << "\n";
                (void)SendPacket(sock, NAME, HostnameString());
                std::cout << "[SRV] Name Sending Back Successfully.\n";
                continue;
            }

            if (type == LIST) {
                std::cout << "[SRV] Receive a LIST request from client " << self_id << "\n";
                std::string list_payload;
                pthread_mutex_lock(&clients_mutex);
                for (const auto& [csock, cid] : clients) {
                    if (cid > 0) {
                        list_payload += std::to_string(cid);
                        list_payload += '$';
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                (void)SendPacket(sock, LIST, list_payload);
                std::cout << "[SRV] List Sending Back Successfully.\n";
                continue;
            }

            if (type == MESSAGE) {
                std::cout << "[SRV] Receive a MESSAGE request from client " << self_id << "\n";
                const size_t pos = payload.find('$');
                if (pos == std::string::npos) {
                    (void)SendPacket(sock, MESSAGE, "Bad format. Use: id$content");
                    continue;
                }
                const std::string id_str = payload.substr(0, pos);
                const std::string content = payload.substr(pos + 1);

                int target_id = -1;
                try {
                    target_id = std::stoi(id_str);
                } catch (...) {
                    (void)SendPacket(sock, MESSAGE, "Bad target id.");
                    continue;
                }

                int sender_id = -1;
                pthread_mutex_lock(&clients_mutex);
                sender_id = GetClientIdLocked(sock);
                pthread_mutex_unlock(&clients_mutex);
                if (sender_id <= 0) {
                    (void)SendPacket(sock, MESSAGE, "Please CONNECT first.");
                    continue;
                }

                int target_sock = -1;
                pthread_mutex_lock(&clients_mutex);
                for (const auto& [csock, cid] : clients) {
                    if (cid == target_id) {
                        target_sock = csock;
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);

                if (target_sock < 0) {
                    (void)SendPacket(sock, MESSAGE, "Target client not found.");
                    continue;
                }

                const std::string forward_payload = std::to_string(sender_id) + "$" + content;
                (void)SendPacket(target_sock, SIGNAL, forward_payload);
                std::cout << "[SRV] Message Sending Success\n";
                continue;
            }

            if (type == DISCONNECT) {
                std::cout << "[SRV] Client " << self_id << " requested disconnect.\n";
                goto disconnect;
            }

            (void)SendPacket(sock, INVALID, "Unknown packet type.");
        }
    }

disconnect:


    if (self_id <= 0) {
        pthread_mutex_lock(&clients_mutex);
        self_id = GetClientIdLocked(sock);
        pthread_mutex_unlock(&clients_mutex);
    }
    if (self_id > 0) {
        std::cout << "[SRV] Client " << self_id << " Disconnect Successfully.\n";
    }
    RemoveClient(sock);
    return nullptr;
}

int main() {
    BuildServer();
}
