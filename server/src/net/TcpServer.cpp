//Socket Handling
#include "net/TcpServer.hpp"
#include <stdexcept>
#include <iostream>

//initialise listeners BEFORE construction of object
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