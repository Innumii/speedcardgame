#include "core/GameServer.hpp"
#include "net/TcpServer.hpp"
#include "core/Matchmaker.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

GameServer::GameServer(int port)
    : port(port), running(false)
{
}

bool GameServer::start() {
    running = false;

    try {
        // -------------------------------
        // 1️⃣ Start TcpServer
        // -------------------------------
        tcpServer = std::make_unique<TcpServer>(port);

        // Initialize Matchmaker (passive)
        matchmaker = std::make_unique<Matchmaker>();

        //Initialise MatchManager and wire callback
        matchManager = std::make_unique<MatchManager>();
        matchManager->setMatchmaker(matchmaker.get());

        matchmaker->onMatchReady = [this](auto a, auto b) {
            matchManager->onPairFound(a, b);
        };

        // Register callback for new clients
        tcpServer->onClientConnected = [this](std::shared_ptr<PlayerConnection> player)
        {
            // Use weak_ptr to avoid cycles in lambdas
            std::weak_ptr<PlayerConnection> weakPlayer = player;

            // Disconnection callback
            player->onDisconnected = [this, weakPlayer]()
            {
                if (auto p = weakPlayer.lock())
                {
                    std::cout << "Player disconnected: " << p->getUsername() << "\n";

                    // Remove from queues / matches
                    if (matchmaker) matchmaker->removePlayer(p);
                    if (matchManager) matchManager->onPlayerDisconnected(p);
                    
                    std::thread([p, this]() {
                        p->stop();                  // use this to trigger the thread joining
                        tcpServer->removeClient(p);
                        std::cout << "[DEBUG] PlayerConnection removed for: " << p->getUsername() << "\n";
                    }).detach();
                }
            };

            // Message-received callback
            player->onMessageReceived = [this, weakPlayer](const std::vector<char>& rawMsg)
            {
                if (auto p = weakPlayer.lock())
                {
                    std::string msg(rawMsg.begin(), rawMsg.end());

                    if (msg == "MATCH_ACCEPT\n")
                    {
                        if (matchManager) matchManager->onAccept(p);
                    }
                    else if (msg.find("{\"type\":\"player_info\"") != std::string::npos)
                    {
                        // Parse player info JSON manually
                        auto idPos = msg.find("\"playerId\":");
                        auto namePos = msg.find("\"username\":\"");
                        if (idPos != std::string::npos && namePos != std::string::npos)
                        {
                            int playerId = std::stoi(msg.substr(idPos + 11, msg.find(',', idPos) - (idPos + 11)));
                            int nameEnd = msg.find('"', namePos + 12);
                            std::string username = msg.substr(namePos + 12, nameEnd - (namePos + 12));
                            p->setPlayerInfo(playerId, username);

                            std::cout << "Player info received: ID=" << playerId
                                    << ", username=" << username << "\n";

                            // Enqueue for matchmaking
                            if (matchmaker) matchmaker->enqueuePlayer(p);
                        }
                    }
                    else
                    {
                        // Forward to active match sessions if needed
                        // e.g., matchManager->routeToMatchSession(p, msg);
                    }
                }
            };

            // Start the read thread
            if (!player->start())
            {
                std::cerr << "Failed to start PlayerConnection for socket " << player->getSocket() << "\n";
            }
        };

    

        if (!tcpServer->start()) {
            std::cerr << "Failed to start TcpServer\n";
            tcpServer.reset(); // cleanup
            matchmaker.reset();
            matchManager.reset();
            return false;
        }

        std::cout << "TcpServer started on port " << port << "\n";
        std::cout << "Matchmaker initialized\n";

        running = true;
        return true;

    } catch (const std::exception& ex) {
        std::cerr << "Exception during GameServer startup: " << ex.what() << "\n";
        if (tcpServer) tcpServer->stop();
        tcpServer.reset();
        matchmaker.reset();
        matchManager.reset();
        running = false;
        return false;
    } catch (...) {
        std::cerr << "Unknown exception during GameServer startup\n";
        if (tcpServer) tcpServer->stop();
        tcpServer.reset();
        matchmaker.reset();
        matchManager.reset();
        running = false;
        return false;
    }
}

void GameServer::stop() {
    if (!running) return;

    running = false;
    std::cout << "Stopping GameServer...\n";

    // Stop TcpServer first
    if (tcpServer) {
        tcpServer->stop();
        tcpServer.reset();
    }

    // Matchmaker is passive; just destroy
    matchmaker.reset();

    // Wake main thread if blocked
    shutdownCv.notify_all();
}

void GameServer::waitForShutdown() {
    std::unique_lock<std::mutex> lock(shutdownMutex);
    shutdownCv.wait(lock, [this]() { return !running.load(); });
}
