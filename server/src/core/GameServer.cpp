#include "core/GameServer.hpp"
#include "net/TcpServer.hpp"
#include "core/Matchmaker.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include "utils/HttpUtil.hpp"
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

bool GameServer::loadPlayerInfo(const std::shared_ptr<PlayerConnection>& player, const std::string& msg) {
    // Parse player info JSON manually
    auto idPos = msg.find("\"playerId\":");
    auto namePos = msg.find("\"username\":\"");
    if (idPos != std::string::npos && namePos != std::string::npos) {
        int playerId = std::stoi(msg.substr(idPos + 11, msg.find(',', idPos) - (idPos + 11)));
        int nameEnd = msg.find('"', namePos + 12);
        std::string username = msg.substr(namePos + 12, nameEnd - (namePos + 12));
        player->setPlayerInfo(playerId, username);

        std::cout << "[GameServer] Welcome ID: " << playerId
                  << ", username:" << username << "\n";
        return true;
    }
    return false;
}

bool GameServer::start() {
    running = false;
    if (!loadAvailableCardsFromService()) {
        std::cerr << "Failed to load cards, cannot start server\n";
        return false;
    }

    //Check that youve loaded all available cards
    // for (int i = 0; i < availableCards.size(); i++) {
    //     std::cout << availableCards[i]->getName() << "\n";
    // }

    try {
        // -------------------------------
        // 1️⃣ Start TcpServer
        // -------------------------------
        tcpServer = std::make_unique<TcpServer>(port);

        // Initialize Matchmaker (passive)
        matchmaker = std::make_unique<Matchmaker>();

        //Initialise MatchManager and wire callback
        matchManager = std::make_unique<MatchManager>(*this);
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
                    std::cout << "[GameServer] Player disconnected: " << p->getUsername() << "\n";
                    // Remove from queues / matches
                    if (tcpServer) tcpServer->enqueueDisconnect(p); //push disconnect action into queue, to drop the socket
                    if (matchmaker) matchmaker->removePlayer(p); //remove from queue if queueing
                    if (matchManager) matchManager->onPlayerDisconnected(p); //remove PendingMatch obj if any
                    
                }
            };

            // Message-received callback
            player->setMessageHandler(ConnectionState::Waiting,
                [this](const std::shared_ptr<PlayerConnection>& p, const std::string& msg) {
                    if (msg == "MATCH_ACCEPT\n") {
                        if (matchManager) matchManager->onAccept(p);
                    } else if (msg.find("{\"type\":\"player_info\"") != std::string::npos) {
                        if (loadPlayerInfo(p, msg) && matchmaker) {
                            matchmaker->enqueuePlayer(p);
                        }
                    } else if (msg == "QUEUE\n") {
                        if (matchmaker) matchmaker->enqueuePlayer(p);
                    }
                });

            // Start the read thread
            if (!player->start())
            {
                std::cerr << "[GameServer] Failed to start PlayerConnection for socket " << player->getSocket() << "\n";
            }
        };

    

        if (!tcpServer->start()) {
            std::cerr << "[GameServer] Failed to start TcpServer\n";
            tcpServer.reset(); // cleanup
            matchmaker.reset();
            matchManager.reset();
            return false;
        }

        std::cout << "[GameServer] TcpServer started on port " << port << "\n";
        std::cout << "[GameServer] Matchmaker initialized\n";

        running = true;
        return true;

    } catch (const std::exception& ex) {
        std::cerr << "[GameServer] Exception during GameServer startup: " << ex.what() << "\n";
        if (tcpServer) tcpServer->stop();
        tcpServer.reset();
        matchmaker.reset();
        matchManager.reset();
        running = false;
        return false;
    } catch (...) {
        std::cerr << "[GameServer] Unknown exception during GameServer startup\n";
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
    const std::string host = EnvUtil::getServiceHost("CARDS_SERVICE", "127.0.0.1", "api.myapp.com");
    const int port = EnvUtil::getServicePort("CARDS_SERVICE", 8082, 443);
    const std::string path = "/cards/cards";
    int statusCode = -1;
    std::string responseBody;
    
    if (!HttpUtil::sendHttp(host, port, "GET", path, "", statusCode, responseBody)) {
        std::cerr << "Failed to fetch cards from service\n";
        return false;
    }
    std::unordered_map<int, std::shared_ptr<ServerCard>> fetchedCards; // <-- now a map
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

            std::shared_ptr<ServerCard> card;

            if (typeLower == "creature") {
                card = std::static_pointer_cast<ServerCard>(
                    std::make_shared<CreatureCard>(name, effect, manaValue, cost, power, toughness, cid));            
            } else {
                card = std::static_pointer_cast<ServerCard>(
                    std::make_shared<SpellCard>(name, effect, manaValue, cost, cid)
                );
            }

            fetchedCards[cid] = card; // <-- store in map by card ID
        }

        pos = objEnd + 1;
    }

    if (fetchedCards.empty()) {
        std::cerr << "No cards fetched from service\n";
        return false;
    }

    availableCards = std::move(fetchedCards); // ✅ map assignment
    for (const auto& [id, card] : availableCards) {
    if (!card) {
        std::cerr << "FATAL: Null card in catalog! ID=" << id << "\n";
        return false;
    }
}
    std::cout << "Loaded " << availableCards.size() << " cards from service\n";
    return true;
}