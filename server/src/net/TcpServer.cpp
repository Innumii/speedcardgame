//Socket Handling
#include "net/TcpServer.hpp"
#include <stdexcept>
#include <iostream>

//socket libraries
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> //sockaddr_in
#include <unistd.h> //close()

//memset()
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <unistd.h>

//initialise listeners BEFORE construction of object
//socket should be in invalid state first (-1)
TcpServer::TcpServer(int port):listenPort(port),listenSocket(-1) {

}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {

}

void TcpServer::stop() {

}

void TcpServer::acceptClients() {

}

void TcpServer::handleClient(int clientSocket) {

}