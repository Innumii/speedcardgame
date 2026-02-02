//Orchestration
#include "core/Server.hpp"

Server::Server(int port):tcpServer(port){

}

void Server::run() {
    tcpServer.start();
    while (true) {
        
    }
}