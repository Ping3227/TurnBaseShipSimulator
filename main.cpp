#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include <iostream>
#include <iomanip>
#include <chrono>

// 常數定義
const int MAP_SIZE = 256;
const int MAX_ROUNDS = 100;
const bool VERBOSE_OUTPUT = true; // 控制是否輸出詳細資訊

// 玩家策略參數結構體
struct StrategyParams {
    double healthWeight;
    double missileWeight;
    double blockWeight;
    double targetWeight;
    double enemyDistanceWeight;
    double allyDistanceWeight;

    StrategyParams(bool isFirstPlayer) {
        if (isFirstPlayer) {
            healthWeight = 1.0;
            missileWeight = 0.8;
            blockWeight = 1.2;
            targetWeight = -1.0;
            enemyDistanceWeight = 0.5;
            allyDistanceWeight = -0.3;
        } else {
            healthWeight = 1.2;
            missileWeight = 0.9;
            blockWeight = 1.0;
            targetWeight = -0.8;
            enemyDistanceWeight = 0.6;
            allyDistanceWeight = -0.4;
        }
    }

    // 新增: 自定義參數建構函式
    StrategyParams(double hw, double mw, double bw, double tw, double ew, double aw)
        : healthWeight(hw), missileWeight(mw), blockWeight(bw),
          targetWeight(tw), enemyDistanceWeight(ew), allyDistanceWeight(aw) {}
};

// 座標結構
struct Position {
    int x, y;
    Position(int _x = 0, int _y = 0) : x(_x), y(_y) {}

    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }

    // 優化: 使用曼哈頓距離代替歐幾里得距離，減少sqrt運算
    double distanceTo(const Position& other) const {
        return std::abs(x - other.x) + std::abs(y - other.y);
    }
};

// 預計算傷害區域的快取
class DamageAreaCache {
public:
    static std::vector<Position>& getCrossDamageArea(const Position& target) {
        static std::vector<std::vector<Position>> cache(MAP_SIZE * MAP_SIZE);
        int index = target.y * MAP_SIZE + target.x;

        if (cache[index].empty()) {
            const int dx[] = {0, 1, 0, -1, 0};
            const int dy[] = {0, 0, 1, 0, -1};
            for (int i = 0; i < 5; ++i) {
                Position pos(target.x + dx[i], target.y + dy[i]);
                if (isValidPosition(pos)) {
                    cache[index].push_back(pos);
                }
            }
        }
        return cache[index];
    }

    static std::vector<Position>& getSquareDamageArea(const Position& target) {
        static std::vector<std::vector<Position>> cache(MAP_SIZE * MAP_SIZE);
        int index = target.y * MAP_SIZE + target.x;

        if (cache[index].empty()) {
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    Position pos(target.x + dx, target.y + dy);
                    if (isValidPosition(pos)) {
                        cache[index].push_back(pos);
                    }
                }
            }
        }
        return cache[index];
    }

private:
    static bool isValidPosition(const Position& pos) {
        return pos.x >= 0 && pos.x < MAP_SIZE && pos.y >= 0 && pos.y < MAP_SIZE;
    }
};

class Missile {
public:
    enum Type { CROSS, SQUARE };

private:
    Type type;  // 添加成員變數宣告

public:
    Missile(Type t) : type(t) {}

    std::vector<Position>& getDamageArea(const Position& target) const {
        return type == CROSS ?
            DamageAreaCache::getCrossDamageArea(target) :
            DamageAreaCache::getSquareDamageArea(target);
    }

    Type getType() const { return type; }
};
// 船艦類別
class Ship {
public:
    Ship(int maxHp, int moveRange, int crossMissiles, int squareMissiles)
        : maxHealth(maxHp), health(maxHp), moveRange(moveRange),
          remainingCrossMissiles(crossMissiles),
          remainingSquareMissiles(squareMissiles) {}

    bool isDead() const { return health <= 0; }
    int getHealth() const { return health; }
    int getMoveRange() const { return moveRange; }
    int getCrossMissiles() const { return remainingCrossMissiles; }
    int getSquareMissiles() const { return remainingSquareMissiles; }
    Position getPosition() const { return position; }

    void setPosition(const Position& pos) { position = pos; }
    void takeDamage(int damage) { health = std::max(0, health - damage); }

    bool useMissile(Missile::Type type) {
        if (type == Missile::CROSS && remainingCrossMissiles > 0) {
            --remainingCrossMissiles;
            return true;
        } else if (type == Missile::SQUARE && remainingSquareMissiles > 0) {
            --remainingSquareMissiles;
            return true;
        }
        return false;
    }

    double getValue(const StrategyParams& params) const {
        return (health * params.healthWeight +
                (remainingCrossMissiles + remainingSquareMissiles) *
                params.missileWeight);
    }

    // 優化: 預計算並快取可能的移動位置
    const std::vector<Position>& getPossibleMoves() {
        if (cachedMoves.empty()) {
            for (int dx = -moveRange; dx <= moveRange; ++dx) {
                for (int dy = -moveRange; dy <= moveRange; ++dy) {
                    if (std::abs(dx) + std::abs(dy) <= moveRange) {
                        Position newPos(position.x + dx, position.y + dy);
                        if (isValidPosition(newPos)) {
                            cachedMoves.push_back(newPos);
                        }
                    }
                }
            }
        }
        return cachedMoves;
    }

private:
    int maxHealth;
    int health;
    int moveRange;
    int remainingCrossMissiles;
    int remainingSquareMissiles;
    Position position;
    std::vector<Position> cachedMoves;

    static bool isValidPosition(const Position& pos) {
        return pos.x >= 0 && pos.x < MAP_SIZE && pos.y >= 0 && pos.y < MAP_SIZE;
    }
};

// 玩家類別
class Player {
public:
    Player(bool isFirst, const StrategyParams& customParams = StrategyParams(true))
        : isFirstPlayer(isFirst),
          params(customParams) {
        initializeShips(isFirst);
    }

    void placeShips() {
        std::random_device rd;
        std::mt19937 gen(rd());

        if (VERBOSE_OUTPUT) {
            std::cout << (isFirstPlayer ? "Player 1" : "Player 2")
                      << " placing ships:\n";
        }

        for (size_t i = 0; i < ships.size(); ++i) {
            bool placed = false;
            while (!placed) {
                int x = isFirstPlayer ?
                    (gen() % (MAP_SIZE / 3)) :
                    (MAP_SIZE - 1 - (gen() % (MAP_SIZE / 3)));
                int y = gen() % MAP_SIZE;

                Position pos(x, y);
                if (canPlaceShip(pos)) {
                    ships[i].setPosition(pos);
                    if (VERBOSE_OUTPUT) {
                        std::cout << "Ship " << i + 1 << " placed at ("
                                  << x << "," << y << ")\n";
                    }
                    placed = true;
                }
            }
        }
        if (VERBOSE_OUTPUT) std::cout << "\n";
    }

    struct MoveDecision {
        Position position;
        double score;
        std::string explanation;
    };

    MoveDecision chooseMovePosition(Ship& ship, const Player& enemy) {
        const std::vector<Position>& moves = ship.getPossibleMoves();
        MoveDecision best{ship.getPosition(),
                         -std::numeric_limits<double>::infinity(),
                         "No valid moves"};

        if (VERBOSE_OUTPUT) {
            std::cout << (isFirstPlayer ? "Player 1" : "Player 2")
                      << " evaluating moves for ship at ("
                      << ship.getPosition().x << ","
                      << ship.getPosition().y << "):\n";
        }

        for (const Position& move : moves) {
            if (!canMoveTo(move)) continue;

            std::string explanation;
            double score = evaluateMove(move, ship, enemy, explanation);

            if (score > best.score) {
                best = {move, score, explanation};
            }
        }

        if (VERBOSE_OUTPUT) {
            std::cout << "Chosen move: (" << best.position.x << ","
                      << best.position.y << ") with score "
                      << best.score << "\n\n";
        }

        return best;
    }

    struct AttackDecision {
        Position position;
        Missile::Type missileType;
        double score;
        std::string explanation;
    };

    AttackDecision chooseAttackPosition(Ship& ship, const Player& enemy) {
        AttackDecision best{{-1, -1}, Missile::CROSS,
                           -std::numeric_limits<double>::infinity(),
                           "No valid attacks"};

        if (VERBOSE_OUTPUT) {
            std::cout << (isFirstPlayer ? "Player 1" : "Player 2")
                      << " evaluating attacks for ship at ("
                      << ship.getPosition().x << ","
                      << ship.getPosition().y << "):\n";
        }

        // 僅在有飛彈時評估攻擊
        if (ship.getCrossMissiles() > 0 || ship.getSquareMissiles() > 0) {
            // 優化: 只檢查敵人可能移動到的位置
            std::vector<Position> potentialTargets;
            for (const Ship& enemyShip : enemy.getShips()) {
                if (!enemyShip.isDead()) {
                    const std::vector<Position>& enemyMoves =
                        const_cast<Ship&>(enemyShip).getPossibleMoves();
                    potentialTargets.insert(potentialTargets.end(),
                                          enemyMoves.begin(), enemyMoves.end());
                }
            }

            // 對每個潛在目標評估攻擊
            for (const Position& target : potentialTargets) {
                for (int type = 0; type <= 1; ++type) {
                    Missile::Type missileType =
                        type == 0 ? Missile::CROSS : Missile::SQUARE;

                    if ((missileType == Missile::CROSS &&
                         ship.getCrossMissiles() == 0) ||
                        (missileType == Missile::SQUARE &&
                         ship.getSquareMissiles() == 0)) {
                        continue;
                    }

                    std::string explanation;
                    double score = evaluateAttack(target, enemy,
                                                missileType, explanation);

                    if (score > best.score) {
                        best = {target, missileType, score, explanation};
                    }
                }
            }
        }

        if (VERBOSE_OUTPUT && best.score > 0) {
            std::cout << "Chosen attack: (" << best.position.x << ","
                      << best.position.y << ") with "
                      << (best.missileType == Missile::CROSS ?
                          "CROSS" : "SQUARE")
                      << " missile, score " << best.score << "\n\n";
        }

        return best;
    }

    const std::vector<Ship>& getShips() const { return ships; }
    bool isDefeated() const {
        return std::all_of(ships.begin(), ships.end(),
                          [](const Ship& s) { return s.isDead(); });
    }

    const StrategyParams& getParams() const { return params; }

private:
    bool isFirstPlayer;
    std::vector<Ship> ships;
    StrategyParams params;

    void initializeShips(bool isFirst) {
        if(isFirst) {
            for(int i=0; i<2; i++) {
                ships.push_back(Ship(1, 2, 0, 3));
            }
            for (int i=0; i<2; i++) {
                ships.push_back(Ship(2, 3, 4, 2));
            }
            for (int i=0; i<4; i++) {
                ships.push_back(Ship(3, 4, 5, 4));
            }
        } else {
            for(int i=0; i<3; i++) {
                ships.push_back(Ship(1, 2, 0, 3));
            }
            for (int i=0; i<3; i++) {
                ships.push_back(Ship(2, 3, 4, 2));
            }
            for (int i=0; i<3; i++) {
                ships.push_back(Ship(3, 4, 5, 4));
            }
        }
    }

    bool canPlaceShip(const Position& pos) const {
        for (const Ship& ship : ships) {
            if (!ship.isDead() &&
                ship.getPosition().distanceTo(pos) < 2) {
                return false;
            }
        }
        return true;
    }

    bool canMoveTo(const Position& pos) const {
        return pos.x >= 0 && pos.x < MAP_SIZE &&
               pos.y >= 0 && pos.y < MAP_SIZE &&
               canPlaceShip(pos);
    }

    double evaluateMove(const Position& move, const Ship& ship,
                       const Player& enemy, std::string& explanation) {
        double score = 0;
        explanation = "";

        // 計算與敵人的距離得分
        double enemyDistanceScore = 0;
        for (const Ship& enemyShip : enemy.getShips()) {
            if (!enemyShip.isDead()) {
                double dist = move.distanceTo(enemyShip.getPosition());
                enemyDistanceScore += params.enemyDistanceWeight / (dist + 1);
            }
        }
        score += enemyDistanceScore;

        // 計算與隊友的距離得分
        double allyDistanceScore = 0;
        for (const Ship& allyShip : ships) {
            if (!allyShip.isDead() && &allyShip != &ship) {
                double dist = move.distanceTo(allyShip.getPosition());
                allyDistanceScore += params.allyDistanceWeight / (dist + 1);
            }
        }
        score += allyDistanceScore;

        // 計算格檔和被瞄準得分
        int blockCount = 0, targetCount = 0;
        for (const Ship& enemyShip : enemy.getShips()) {
            if (!enemyShip.isDead()) {
                for (const Ship& allyShip : ships) {
                    if (!allyShip.isDead() && &allyShip != &ship) {
                        Position allyPos = allyShip.getPosition();
                        if (isInLine(enemyShip.getPosition(), allyPos, move)) {
                            ++blockCount;
                        }
                    }
                }

                if (isInLine(enemyShip.getPosition(), move, move)) {
                    ++targetCount;
                }
            }
        }

        score += blockCount * params.blockWeight * ship.getValue(params);
        score += targetCount * params.targetWeight * ship.getValue(params);

        return score;
    }

    double evaluateAttack(const Position& target, const Player& enemy,
                         Missile::Type missileType, std::string& explanation) {
        double score = 0;
        explanation = "";

        Missile missile(missileType);
        std::vector<Position>& damageArea = missile.getDamageArea(target);

        // 計算可能傷害到的敵人價值
        double potentialDamage = 0;
        for (const Ship& enemyShip : enemy.getShips()) {
            if (!enemyShip.isDead()) {
                const std::vector<Position>& moves =
                    const_cast<Ship&>(enemyShip).getPossibleMoves();
                double moveCount = moves.size();

                for (const Position& pos : damageArea) {
                    for (const Position& move : moves) {
                        if (pos == move) {
                            potentialDamage += enemyShip.getValue(enemy.getParams()) /
                                             moveCount;
                        }
                    }
                }
            }
        }

        score += potentialDamage;

        // 檢查是否會誤傷隊友
        for (const Ship& allyShip : ships) {
            if (!allyShip.isDead()) {
                for (const Position& pos : damageArea) {
                    if (pos == allyShip.getPosition()) {
                        return -std::numeric_limits<double>::infinity();
                    }
                }
            }
        }

        return score;
    }

    bool isInLine(const Position& from, const Position& to,
                  const Position& point) const {
        // 優化: 使用曼哈頓距離
        double d1 = point.distanceTo(from);
        double d2 = point.distanceTo(to);
        double d3 = from.distanceTo(to);
        return std::abs(d1 + d2 - d3) < 0.1;
    }
};

class Game {
public:
    Game() : player1(true), player2(false), round(0) {
        map.resize(MAP_SIZE, std::vector<char>(MAP_SIZE, '.'));
    }

    // 新增: 使用自定義參數建構遊戲
    Game(const StrategyParams& p1Params, const StrategyParams& p2Params)
        : player1(true, p1Params), player2(false, p2Params), round(0) {
        map.resize(MAP_SIZE, std::vector<char>(MAP_SIZE, '.'));
    }

    struct GameResult {
        int rounds;
        int p1Ships;
        int p1Health;
        int p2Ships;
        int p2Health;
        int winner; // 1 for player1, 2 for player2, 0 for draw
        double duration; // in seconds
    };

    GameResult run() {
        auto startTime = std::chrono::high_resolution_clock::now();

        if (VERBOSE_OUTPUT) {
            std::cout << "Game Start!\n\n";
            std::cout << "Phase: Player 1 placing ships\n";
        }

        player1.placeShips();
        updateMap();
        if (VERBOSE_OUTPUT) printStatus();

        if (VERBOSE_OUTPUT) {
            std::cout << "Phase: Player 2 placing ships\n";
        }
        player2.placeShips();
        updateMap();
        if (VERBOSE_OUTPUT) printStatus();

        while (!isGameOver()) {
            ++round;
            if (VERBOSE_OUTPUT) {
                std::cout << "\nRound " << round << " Start!\n\n";
                std::cout << "Phase: Player 1 choosing attack positions\n";
            }

            // 玩家1攻擊階段
            std::vector<std::tuple<Position, Missile::Type, Ship*>> p1Attacks;
            for (Ship& ship : const_cast<std::vector<Ship>&>(player1.getShips())) {
                if (!ship.isDead()) {
                    auto decision = player1.chooseAttackPosition(ship, player2);
                    if (decision.score > 0) {
                        p1Attacks.emplace_back(decision.position,
                                             decision.missileType,
                                             &ship);
                    }
                }
            }

            // 玩家2移動階段
            if (VERBOSE_OUTPUT) {
                std::cout << "Phase: Player 2 moving ships\n";
            }
            for (Ship& ship : const_cast<std::vector<Ship>&>(player2.getShips())) {
                if (!ship.isDead()) {
                    auto decision = player2.chooseMovePosition(ship, player1);
                    ship.setPosition(decision.position);
                }
            }
            updateMap();
            if (VERBOSE_OUTPUT) printStatus();

            // 玩家1攻擊觸發
            if (VERBOSE_OUTPUT) {
                std::cout << "Phase: Player 1 attacks triggering\n";
            }
            for (const auto& [pos, type, ship] : p1Attacks) {
                if (ship->useMissile(type)) {  // 檢查並消耗飛彈
                    handleAttack(pos, type, player1, player2);
                }
            }
            updateMap();
            if (VERBOSE_OUTPUT) printStatus();

            if (isGameOver()) break;

            // 玩家2攻擊階段
            if (VERBOSE_OUTPUT) {
                std::cout << "Phase: Player 2 choosing attack positions\n";
            }
            std::vector<std::tuple<Position, Missile::Type, Ship*>> p2Attacks;
            for (Ship& ship : const_cast<std::vector<Ship>&>(player2.getShips())) {
                if (!ship.isDead()) {
                    auto decision = player2.chooseAttackPosition(ship, player1);
                    if (decision.score > 0) {
                        p2Attacks.emplace_back(decision.position,
                                             decision.missileType,
                                             &ship);
                    }
                }
            }

            // 玩家1移動階段
            if (VERBOSE_OUTPUT) {
                std::cout << "Phase: Player 1 moving ships\n";
            }
            for (Ship& ship : const_cast<std::vector<Ship>&>(player1.getShips())) {
                if (!ship.isDead()) {
                    auto decision = player1.chooseMovePosition(ship, player2);
                    ship.setPosition(decision.position);
                }
            }
            updateMap();
            if (VERBOSE_OUTPUT) printStatus();

            // 玩家2攻擊觸發
            if (VERBOSE_OUTPUT) {
                std::cout << "Phase: Player 2 attacks triggering\n";
            }
            for (const auto& [pos, type, ship] : p2Attacks) {
                if (ship->useMissile(type)) {  // 檢查並消耗飛彈
                    handleAttack(pos, type, player2, player1);
                }
            }
            updateMap();
            if (VERBOSE_OUTPUT) printStatus();
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(endTime - startTime).count();

        return getGameResult(duration);
    }

private:
    Player player1, player2;
    std::vector<std::vector<char>> map;
    int round;

    void handleAttack(const Position& target, Missile::Type missileType,
                     Player& attacker, Player& defender) {
        if (VERBOSE_OUTPUT) {
            std::cout << "Attack at (" << target.x << "," << target.y << ") with "
                      << (missileType == Missile::CROSS ? "CROSS" : "SQUARE")
                      << " missile\n";
        }

        Missile missile(missileType);
        std::vector<Position>& damageArea = missile.getDamageArea(target);

        for (const Position& pos : damageArea) {
            for (Ship& ship : const_cast<std::vector<Ship>&>(defender.getShips())) {
                if (!ship.isDead() && ship.getPosition() == pos) {
                    ship.takeDamage(1);
                    if (VERBOSE_OUTPUT) {
                        std::cout << "Hit ship at (" << pos.x << "," << pos.y
                                  << "), damage dealt: 1\n";
                    }
                }
            }
        }
    }

    void updateMap() {
        if (!VERBOSE_OUTPUT) return;

        for (auto& row : map) {
            std::fill(row.begin(), row.end(), '.');
        }

        for (const Ship& ship : player1.getShips()) {
            if (!ship.isDead()) {
                Position pos = ship.getPosition();
                map[pos.y][pos.x] = '1';
            }
        }

        for (const Ship& ship : player2.getShips()) {
            if (!ship.isDead()) {
                Position pos = ship.getPosition();
                map[pos.y][pos.x] = '2';
            }
        }
    }

    void printStatus() const {
        if (!VERBOSE_OUTPUT) return;

        std::cout << "\nCurrent game state:\n";
        std::cout << "Round: " << round << "\n\n";

        printPlayerStatus(player1, "Player 1");
        printPlayerStatus(player2, "Player 2");
        std::cout << "\n";
    }

    void printPlayerStatus(const Player& player, const std::string& name) const {
        std::cout << name << " ships status:\n";
        int shipNum = 1;
        for (const Ship& ship : player.getShips()) {
            std::cout << "Ship " << shipNum++ << ": ";
            if (ship.isDead()) {
                std::cout << "Destroyed\n";
            } else {
                Position pos = ship.getPosition();
                std::cout << "HP=" << ship.getHealth()
                         << ", Cross Missiles=" << ship.getCrossMissiles()
                         << ", Square Missiles=" << ship.getSquareMissiles()
                         << ", Position=(" << pos.x << "," << pos.y << ")\n";
            }
        }
    }

    bool isGameOver() const {
        if (round >= MAX_ROUNDS) return true;
        if (player1.isDefeated() || player2.isDefeated()) return true;

        bool player1HasMissiles = false;
        bool player2HasMissiles = false;

        for (const Ship& ship : player1.getShips()) {
            if (!ship.isDead() &&
                (ship.getCrossMissiles() > 0 ||
                 ship.getSquareMissiles() > 0)) {
                player1HasMissiles = true;
                break;
            }
        }

        for (const Ship& ship : player2.getShips()) {
            if (!ship.isDead() &&
                (ship.getCrossMissiles() > 0 ||
                 ship.getSquareMissiles() > 0)) {
                player2HasMissiles = true;
                break;
            }
        }

        return !player1HasMissiles && !player2HasMissiles;
    }

    GameResult getGameResult(double duration) const {
        int p1Ships = 0, p1Health = 0;
        int p2Ships = 0, p2Health = 0;

        for (const Ship& ship : player1.getShips()) {
            if (!ship.isDead()) {
                ++p1Ships;
                p1Health += ship.getHealth();
            }
        }

        for (const Ship& ship : player2.getShips()) {
            if (!ship.isDead()) {
                ++p2Ships;
                p2Health += ship.getHealth();
            }
        }

        int winner;
        if (p1Ships > p2Ships || (p1Ships == p2Ships && p1Health > p2Health)) {
            winner = 1;
        } else if (p2Ships > p1Ships || (p1Ships == p2Ships && p2Health > p1Health)) {
            winner = 2;
        } else {
            winner = 0;
        }

        if (VERBOSE_OUTPUT) {
            std::cout << "\nGame Over!\n";
            std::cout << "Total Rounds: " << round << "\n\n";
            std::cout << "Player 1: " << p1Ships << " ships remaining, "
                      << "total HP: " << p1Health << "\n";
            std::cout << "Player 2: " << p2Ships << " ships remaining, "
                      << "total HP: " << p2Health << "\n\n";

            if (winner == 1) {
                std::cout << "Player 1 Wins!\n";
            } else if (winner == 2) {
                std::cout << "Player 2 Wins!\n";
            } else {
                std::cout << "It's a Draw!\n";
            }
        }

        return GameResult{round, p1Ships, p1Health, p2Ships, p2Health,
                         winner, duration};
    }
};

// 新增: 參數實驗函數
// 實驗結果結構體
struct ExperimentResult {
    int p1Wins = 0;
    int p2Wins = 0;
    int draws = 0;
    int totalRounds = 0;
    // 玩家1的統計
    int p1TotalShipsRemaining = 0;
    int p1TotalHealthRemaining = 0;
    // 玩家2的統計
    int p2TotalShipsRemaining = 0;
    int p2TotalHealthRemaining = 0;
    double totalDuration = 0.0;
};

void runParameterExperiment() {
    const int EXPERIMENT_ROUNDS = 100;
    std::vector<std::pair<std::string, StrategyParams>> paramSets = {
        {"Aggressive", StrategyParams(1.0, 1.2, 0.8, -0.5, 0.7, -0.2)},
        {"Defensive", StrategyParams(1.2, 0.8, 1.2, -1.2, 0.4, -0.5)},
        {"Balanced", StrategyParams(1.0, 1.0, 1.0, -1.0, 0.5, -0.3)},
        {"Missile-focused", StrategyParams(0.8, 1.5, 0.7, -0.8, 0.6, -0.2)},
        {"Health-focused", StrategyParams(1.5, 0.7, 1.0, -1.0, 0.5, -0.4)}
    };

    // 創建結果矩陣
    std::vector<std::vector<ExperimentResult>> results(
        paramSets.size(), std::vector<ExperimentResult>(paramSets.size()));

    // 執行實驗
    for (size_t i = 0; i < paramSets.size(); ++i) {
        for (size_t j = 0; j < paramSets.size(); ++j) {
            std::cout << "Testing P1:" << paramSets[i].first << " vs P2:"
                     << paramSets[j].first << "...\n";

            for (int k = 0; k < EXPERIMENT_ROUNDS; ++k) {
                Game game(paramSets[i].second, paramSets[j].second);
                auto result = game.run();

                // 更新統計資料
                if (result.winner == 1) {
                    results[i][j].p1Wins++;
                    results[i][j].p1TotalShipsRemaining += result.p1Ships;
                    results[i][j].p1TotalHealthRemaining += result.p1Health;
                } else if (result.winner == 2) {
                    results[i][j].p2Wins++;
                    results[i][j].p2TotalShipsRemaining += result.p2Ships;
                    results[i][j].p2TotalHealthRemaining += result.p2Health;
                } else {
                    results[i][j].draws++;
                }

                results[i][j].totalRounds += result.rounds;
                results[i][j].totalDuration += result.duration;
            }
        }
    }

    // 計算每個策略作為P1和P2時的總體表現
    struct StrategyPerformance {
        double p1WinRate = 0.0;
        double p2WinRate = 0.0;
        double avgRounds = 0.0;
        int totalGamesAsP1 = 0;
        int totalGamesAsP2 = 0;
        int totalWinsAsP1 = 0;
        int totalWinsAsP2 = 0;
    };

    std::vector<StrategyPerformance> performances(paramSets.size());

    // 計算每個策略的表現
    for (size_t i = 0; i < paramSets.size(); ++i) {
        for (size_t j = 0; j < paramSets.size(); ++j) {
            if (i == j) continue;

            // 作為P1時的統計
            performances[i].totalGamesAsP1 += EXPERIMENT_ROUNDS;
            performances[i].totalWinsAsP1 += results[i][j].p1Wins;

            // 作為P2時的統計
            performances[j].totalGamesAsP2 += EXPERIMENT_ROUNDS;
            performances[j].totalWinsAsP2 += results[i][j].p2Wins;
        }
    }

    // 輸出結果
    std::cout << "\nStrategy Performance Analysis:\n";
    std::cout << "============================\n\n";

    // 輸出表頭
    std::cout << std::left << std::setw(20) << "Strategy"
              << std::right << std::setw(15) << "P1 Win Rate"
              << std::setw(15) << "P2 Win Rate"
              << std::setw(20) << "Total P1 Wins"
              << std::setw(20) << "Total P2 Wins" << "\n";
    std::cout << std::string(90, '-') << "\n";

    // 找出最佳P1和P2策略
    size_t bestP1Strategy = 0;
    size_t bestP2Strategy = 0;
    double bestP1WinRate = 0.0;
    double bestP2WinRate = 0.0;

    // 輸出每個策略的表現
    for (size_t i = 0; i < paramSets.size(); ++i) {
        double p1WinRate = static_cast<double>(performances[i].totalWinsAsP1) /
                          performances[i].totalGamesAsP1 * 100;
        double p2WinRate = static_cast<double>(performances[i].totalWinsAsP2) /
                          performances[i].totalGamesAsP2 * 100;

        // 更新最佳策略
        if (p1WinRate > bestP1WinRate) {
            bestP1WinRate = p1WinRate;
            bestP1Strategy = i;
        }
        if (p2WinRate > bestP2WinRate) {
            bestP2WinRate = p2WinRate;
            bestP2Strategy = i;
        }

        std::cout << std::left << std::setw(20) << paramSets[i].first
                  << std::right << std::fixed << std::setprecision(1)
                  << std::setw(14) << p1WinRate << "%"
                  << std::setw(14) << p2WinRate << "%"
                  << std::setw(20) << performances[i].totalWinsAsP1
                  << std::setw(20) << performances[i].totalWinsAsP2 << "\n";
    }

    // 輸出最佳策略建議
    std::cout << "\nStrategy Recommendations:\n";
    std::cout << "=======================\n";
    std::cout << "Best strategy as Player 1: " << paramSets[bestP1Strategy].first
              << " (Win Rate: " << std::fixed << std::setprecision(1)
              << bestP1WinRate << "%)\n";
    std::cout << "Best strategy as Player 2: " << paramSets[bestP2Strategy].first
              << " (Win Rate: " << bestP2WinRate << "%)\n\n";

    // 輸出詳細對戰統計
    std::cout << "Detailed Matchup Statistics:\n";
    std::cout << "==========================\n\n";

    for (size_t i = 0; i < paramSets.size(); ++i) {
        for (size_t j = 0; j < paramSets.size(); ++j) {
            if (i == j) continue;

            std::cout << "P1:" << paramSets[i].first << " vs P2:"
                     << paramSets[j].first << ":\n";
            std::cout << "  P1 Wins: " << results[i][j].p1Wins << " ("
                     << std::fixed << std::setprecision(1)
                     << (static_cast<double>(results[i][j].p1Wins) /
                         EXPERIMENT_ROUNDS * 100) << "%)\n";
            std::cout << "  P2 Wins: " << results[i][j].p2Wins << " ("
                     << (static_cast<double>(results[i][j].p2Wins) /
                         EXPERIMENT_ROUNDS * 100) << "%)\n";
            std::cout << "  Draws: " << results[i][j].draws << " ("
                     << (static_cast<double>(results[i][j].draws) /
                         EXPERIMENT_ROUNDS * 100) << "%)\n";

            if (results[i][j].p1Wins > 0) {
                std::cout << "  P1 Avg ships remaining: "
                         << static_cast<double>(results[i][j].p1TotalShipsRemaining) /
                            results[i][j].p1Wins << "\n";
                std::cout << "  P1 Avg health remaining: "
                         << static_cast<double>(results[i][j].p1TotalHealthRemaining) /
                            results[i][j].p1Wins << "\n";
            }

            if (results[i][j].p2Wins > 0) {
                std::cout << "  P2 Avg ships remaining: "
                         << static_cast<double>(results[i][j].p2TotalShipsRemaining) /
                            results[i][j].p2Wins << "\n";
                std::cout << "  P2 Avg health remaining: "
                         << static_cast<double>(results[i][j].p2TotalHealthRemaining) /
                            results[i][j].p2Wins << "\n";
            }

            std::cout << "  Average rounds: "
                     << static_cast<double>(results[i][j].totalRounds) /
                        EXPERIMENT_ROUNDS << "\n";
            std::cout << "  Average duration: "
                     << results[i][j].totalDuration / EXPERIMENT_ROUNDS
                     << " seconds\n\n";
        }
    }
}

int main() {
    std::cout << "Naval Battle Game Simulation\n";
    std::cout << "============================\n\n";

    // 運行參數實驗
    runParameterExperiment();

    return 0;
}