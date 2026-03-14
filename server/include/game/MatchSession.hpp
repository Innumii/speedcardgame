#ifndef MATCHSESSION_HPP
#define MATCHSESSION_HPP

#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <optional>
#include <unordered_map>
#include <mutex>
#include <queue>

#include "objects/ServerDeck.h"
#include "objects/ServerCard.h"

class PlayerConnection;

// ----------------------------
// Lightweight server board
// ----------------------------
struct ServerBoard {
    int laneCount = 5;
    std::vector<std::optional<int>> lanes[2]; // card IDs
    std::vector<std::optional<std::pair<int, int>>> augments[2]; // buffs/debuffs (eg. 1,2 , 3,-1 , etc.)
    std::vector<std::optional<int>> continuousEffects[2]; // continuous effects (eg. 1:Trample, 2:Double Strike, etc.)

    std::vector<int> discard[2];

    //Lane indexes go 0-4
    ServerBoard(int lanesCount = 5) : laneCount(lanesCount) {
        lanes[0].resize(laneCount);
        lanes[1].resize(laneCount);
        augments[0].resize(laneCount);
        augments[1].resize(laneCount);
        continuousEffects[0].resize(laneCount);
        continuousEffects[1].resize(laneCount);
    }
};

// ----------------------------
// Player match state
// ----------------------------
struct PlayerState {
    int id = -1;
    int health = 100;
    int fatigueDamage = 1;
    int mana = 0;
    ServerDeck deck;
    std::vector<int> hand; // card IDs only
};

// ----------------------------
// Player action
// ----------------------------
/* EXAMPLE:
{player=0, type="SUMMON", args=[2]}
{player=1, type="SPELL", args=[1]}
{player=0, type="DISCARD", args=[3]}
*/
struct PlayerAction {
    int playerIndex;
    std::string type;
    std::vector<int> args;
};

class MatchSession : public std::enable_shared_from_this<MatchSession>{
public:
    MatchSession(std::shared_ptr<PlayerConnection> playerA,
                 std::shared_ptr<PlayerConnection> playerB, const std::unordered_map<int, std::shared_ptr<ServerCard>>& cardCatalog);
    ~MatchSession();

    MatchSession(const MatchSession&) = delete;
    MatchSession& operator=(const MatchSession&) = delete;

    bool start();
    void stop();
    void handlePlayerMessage(int playerIndex, const std::vector<char>& raw);
    void processActions();

    //setup phase
    void setupDecks();
    void sendOpeningHands();
    bool drawAndSend(int playerIndex);
    bool loadDeckForPlayer(int playerId, ServerDeck& outDeck);

    //--------------player actions--------------
    //PLAY <playerId> <cardId> <lane>
    void handleSummon(int playerIndex, int cardId, int lane);
    //PLAY <playerId> <cardId> <lane> <targetLane> <clientTargetIndex>
    //if clientTargetIndex == playerId means targets itself, otherwise target opponent
    void handleSpell(int playerIndex, int cardId, int lane, std::optional<int> targetId = std::nullopt, std::optional<int> targetIndex = std::nullopt);
    //DISCARD <playerId> <cardId>
    void handleDiscard(int playerIndex, int cardId);
    //--------------player actions--------------

    const ServerCard* getCard(int id) const;
    int getLaneCount() const;
    const std::string getUsername(int playerIndex) const;

    //--------------board actions--------------
    //AUGMENT <playerId> <lane> <powerDelta> <toughnessDelta> 
    void augmentCreature(int targetPlayerIndex, int lane, std::pair<int,int> augment); // buffs/debuffs creature
    //DESTROY <playerId> <lane>
    void destroyCreature(int targetPlayerIndex, int lane); //removes creature from board, resets augments/effects vectors
    //HP <playerId> <delta>
    void augmentHP(int playerIndex, int amount); //raises or lowers player health 
    //SET <playerId> <lane>
    void setCreature(int targetPlayerIndex, int lane, std::pair<int,int> augment); // buffs/debuffs creature
    //--------------board actions--------------

    //Battle Phase
    void resolveAttackPhase();

    // COMBAT <playerAId> <playerBId> <lane> <playerA's card power> <playerB's card power>
    // DIRECT <attacker ID> <lane> <damage>
    void resolveLaneCombat(int lane);
    int getCreaturePower(int playerIndex, int lane);
    int getCreatureToughness(int playerIndex, int lane);


private:
    const int handLimit = 7;
    const int drawInterval = 5;
    const int attackInterval = 5; 

    void gameLoop();
    void handleDisconnect();

    // Players
    std::shared_ptr<PlayerConnection> playerA;
    std::shared_ptr<PlayerConnection> playerB;
    std::unordered_map<int, std::shared_ptr<ServerCard>> cardCatalog;
    std::thread gameThread;
    std::atomic<bool> running{false};

    //Queue for storing player actions, so each action can be performed individually
    std::queue<PlayerAction> actionQueue;
    std::mutex actionMutex;

    // Authoritative state
    PlayerState players[2];   // 0 = A, 1 = B
    ServerBoard board;

    bool parseDeckJson(const std::string& jsonStr, ServerDeck& outDeck);
    std::vector<int>::iterator findCardInHand(PlayerState& player, int cardId);
};

#endif