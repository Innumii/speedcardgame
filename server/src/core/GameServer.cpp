#include "core/GameServer.hpp"
#include "net/TcpServer.hpp"
#include "core/Matchmaker.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include "utils/EnvUtil.hpp"
#include "utils/JsonUtil.hpp"
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib/httplib.h"

GameServer::GameServer(int port)
    : port(port), running(false)
{
}

bool GameServer::start() {
    running = false;
    if (!loadAvailableCardsFromService()) {
        std::cerr << "Failed to load cards, cannot start server\n";
        return false;
    }

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
        // matchManager->setServer(this);

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
                    if (tcpServer) tcpServer->enqueueDisconnect(p);
                    
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
    std::cout << "[GameServer] Stopping...\n";

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

bool GameServer::loadAvailableCardsFromService() {
    const std::string host = EnvUtil::getEnvOrDefault("CARDS_SERVICE_HOST", "host.docker.internal");
    const int port = EnvUtil::getEnvIntOrDefault("CARDS_SERVICE_PORT", 8082);
    const std::string path = "/cardbase/cards";

    int statusCode = -1;
    std::string responseBody;

    if (!sendHttp(host, port, "GET", path, "", statusCode, responseBody)) {
        std::cerr << "Failed to fetch cards from service\n";
        return false;
    }

    std::vector<std::unique_ptr<Card>> fetchedCards;
    std::size_t pos = 0;

    while (true) {
        const std::size_t objStart = responseBody.find('{', pos);
        if (objStart == std::string::npos) break;
        const std::size_t objEnd = responseBody.find('}', objStart);
        if (objEnd == std::string::npos) break;
        const std::string obj = responseBody.substr(objStart, objEnd - objStart + 1);

        int cid = -1;
        int cost = 0;
        int value = 0;
        int power = 0;
        int toughness = 0;
        std::string name;
        std::string type;
        std::string effect;

        JsonUtil::readJsonIntField(obj, "cid", cid);
        JsonUtil::readJsonStringField(obj, "name", name);
        JsonUtil::readJsonStringField(obj, "type", type);
        JsonUtil::readJsonIntField(obj, "cost", cost);
        JsonUtil::readJsonIntField(obj, "value", value);
        JsonUtil::readJsonIntField(obj, "power", power);
        JsonUtil::readJsonIntField(obj, "toughness", toughness);
        JsonUtil::readJsonStringField(obj, "effect", effect);

        if (!name.empty()) {
            std::string typeLower = type;
            std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);
            const int manaValue = value > 0 ? value : cost;
            if (typeLower == "creature") {
                fetchedCards.push_back(std::make_unique<CreatureCard>(name, effect, manaValue, cost, power, toughness, cid));
            } else {
                fetchedCards.push_back(std::make_unique<SpellCard>(name, effect, manaValue, cost, cid));
            }
        }

        pos = objEnd + 1;
    }

    if (fetchedCards.empty()) {
        std::cerr << "No cards fetched from service\n";
        return false;
    }

    availableCards = std::move(fetchedCards);
    std::cout << "Loaded " << availableCards.size() << " cards from service\n";
    return true;
}

bool GameServer::sendHttp(const std::string& host, int port, const std::string& method,
              const std::string& path, const std::string& body,
              int& statusCode, std::string& responseBody) {

        bool useHttps = (port == 443);
        httplib::Result res;

        if (useHttps) {
            httplib::SSLClient client(host.c_str(), port);
            client.enable_server_certificate_verification(false); // for self-signed
            client.set_follow_location(true);

            if (method == "GET")      res = client.Get(path.c_str());
            else if (method == "POST") res = client.Post(path.c_str(), body, "application/json");
            else if (method == "PUT")  res = client.Put(path.c_str(), body, "application/json");
            else if (method == "PATCH") res = client.Patch(path.c_str(), body, "application/json");
            else if (method == "DELETE") res = client.Delete(path.c_str());
            else return false;

        } else {
            httplib::Client client(host.c_str(), port);
            client.set_follow_location(true);

            if (method == "GET")      res = client.Get(path.c_str());
            else if (method == "POST") res = client.Post(path.c_str(), body, "application/json");
            else if (method == "PUT")  res = client.Put(path.c_str(), body, "application/json");
            else if (method == "PATCH") res = client.Patch(path.c_str(), body, "application/json");
            else if (method == "DELETE") res = client.Delete(path.c_str());
            else return false;
        }

        if (!res) {
            statusCode = -1;
            responseBody.clear();
            return false; // network error
        }

        statusCode = res->status;
        responseBody = res->body;
        return true;
    }