#include "client.h"
#include "func.h"

bool connected = false;
int BaseSock = -1;
sockaddr_in Addr {};
pthread_t thread {};

// 显示帮助菜单
void Help(bool wrong) {
    if (wrong) {
        std::cout << "\n[SYS] Sorry, your input service is illegal. Please choose one from following services:" << std::endl;
    }

    std::cout << "\n[HELP]" << std::endl;
    std::cout << "connect\t Try to connect to the server." << std::endl;

    if (connected) {
        std::cout << "disconnect\t Disconnect from the server." << std::endl;
        std::cout << "time\t Get current time." << std::endl;
        std::cout << "name\t Get name of machine." << std::endl;
        std::cout << "list\t Get list of all clients." << std::endl;
        std::cout << "message\t Send message to other clients." << std::endl;
    }

    std::cout << "exit\t Exit from this communication." << std::endl;
    std::cout << "help\t Show all services for you." << std::endl;
}

void Initialize() {
    std::cout << "[SYS] Client Initialization Now" << std::endl << std::endl;
    std::cout << "[SYS] Nice to meet you, dear client! (^_~)" << std::endl;
    std::cout << "[SYS] Welcome to use my communication service." << std::endl;
    std::cout << "[SYS] Please choose one from the following services:" << std::endl;

    Help(false);

    std::string command;

    while (true) {
        PrintPrompt();
        if (!(std::cin >> command)) {
            break;
        }

        if (command == "help") {
            Help(false);
        } else if (command == "connect") {
            ServerConnect();
        } else if (command == "exit") {
            ExitComm();
            break;
        } else if (!connected) {
            Help(true);
        } else if (command == "disconnect") {
            ServerDisconnect();
        } else if (command == "time") {
            GetTime();
        } else if (command == "name") {
            GetName();
        } else if (command == "list") {
            ClientList();
        } else if (command == "message") {
            SendMessa();
        } else {
            Help(true);
        }
    }
}

int main() {
    Initialize();
}
