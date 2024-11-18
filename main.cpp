﻿#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include <iostream>
#include <iomanip>
#include <chrono>

// 常數定義
const int MAP_SIZE = 256;
const int MAX_ROUNDS = 100;
const bool VERBOSE_OUTPUT = false;
const double EPSILON = 1e-6;  // 用於浮點數比較

// 玩家策略參數結構體
struct StrategyParams {
    double healthWeight;
    double missileWeight;
    double blockWeight;
    double targetWeight;
    double enemyDistanceWeight;
    double allyDistanceWeight;
    double attackThreshold;


    StrategyParams(bool isFirstPlayer) {
        if (isFirstPlayer) {
            healthWeight = 1.0;
            missileWeight = 0.8;
            blockWeight = 1.2;
            targetWeight = -1.0;
            enemyDistanceWeight = 0.5;
            allyDistanceWeight = -0.3;
            attackThreshold=0;
        } else {
            healthWeight = 1.2;
            missileWeight = 0.9;
            blockWeight = 1.0;
            targetWeight = -0.8;
            enemyDistanceWeight = 0.6;
            allyDistanceWeight = -0.4;
            attackThreshold=0;
        }
    }

    StrategyParams(double hw, double mw, double bw, double tw, double ew, double aw,double at)
        : healthWeight(hw), missileWeight(mw), blockWeight(bw),
          targetWeight(tw), enemyDistanceWeight(ew), allyDistanceWeight(aw) ,attackThreshold(at){}
};


// 座標結構
struct Position {
    int x, y;
    Position(int _x = 0, int _y = 0) : x(_x), y(_y) {}

    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }

    double distanceTo(const Position& other) const {
        return std::sqrt(std::pow(x - other.x, 2) + std::pow(y - other.y, 2));
    }

    // 新增: 向量運算相關運算子
    Position operator+(const Position& other) const {
        return Position(x + other.x, y + other.y);
    }

    Position operator-(const Position& other) const {
        return Position(x - other.x, y - other.y);
    }

    Position operator*(double scalar) const {
        return Position(static_cast<int>(x * scalar), static_cast<int>(y * scalar));
    }

    double dot(const Position& other) const {
        return x * other.x + y * other.y;
    }

    double length() const {
        return std::sqrt(x * x + y * y);
    }
};

// 射線結構
struct Ray {
    Position origin;
    Position direction;

    Ray(const Position& from, const Position& to)
        : origin(from), direction(to - from) {}

    // 修改: 計算點到射線的距離和相對位置
    std::pair<double, double> distanceAndProjection(const Position& point) const {
        Position v = point - origin;
        double t = v.dot(direction) / direction.dot(direction);

        // 投影點到目標點的距離
        Position projection = origin + direction * t;
        double distance = point.distanceTo(projection);

        return {distance, t};
    }
};


// 飛彈類別
class Missile {
public:
    enum Type { CROSS, SQUARE };

private:
    Type type;

public:
    Missile(Type t) : type(t) {}

    std::vector<Position> getDamageArea(const Position& target) const {
        std::vector<Position> area;
        if (type == CROSS) {
            // 十字範圍
            const int dx[] = {0, 1, 0, -1, 0};
            const int dy[] = {0, 0, 1, 0, -1};
            for (int i = 0; i < 5; ++i) {
                Position pos(target.x + dx[i], target.y + dy[i]);
                if (isValidPosition(pos)) {
                    area.push_back(pos);
                }
            }
        } else {
            // 九宮格範圍
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    Position pos(target.x + dx, target.y + dy);
                    if (isValidPosition(pos)) {
                        area.push_back(pos);
                    }
                }
            }
        }
        return area;
    }

    Type getType() const { return type; }

private:
    bool isValidPosition(const Position& pos) const {
        return pos.x >= 0 && pos.x < MAP_SIZE && pos.y >= 0 && pos.y < MAP_SIZE;
    }
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

    void setPosition(const Position& pos) {
        position = pos;
        // 位置改變時清除快取
        cachedMoves.clear();
    }

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

    std::vector<Position> getPossibleMoves() {
        // 每次都重新計算可能的移動位置
        std::vector<Position> moves;
        for (int dx = -moveRange; dx <= moveRange; ++dx) {
            for (int dy = -moveRange; dy <= moveRange; ++dy) {
                if (std::abs(dx) + std::abs(dy) <= moveRange) {
                    Position newPos(position.x + dx, position.y + dy);
                    if (isValidPosition(newPos)) {
                        moves.push_back(newPos);
                    }
                }
            }
        }
        return moves;
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
        : isFirstPlayer(isFirst), params(customParams) {
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
        std::vector<Position> moves = ship.getPossibleMoves();
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

            if (score > best.score && score>params.attackThreshold) {
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
        std::vector<Position> potentialTargets;  // 新增: 記錄射線上的目標
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

    if (ship.getCrossMissiles() > 0 || ship.getSquareMissiles() > 0) {
        // 收集所有敵人的可能位置
        std::vector<std::pair<Position, double>> allEnemyPositions;
        for (const Ship& enemyShip : enemy.getShips()) {
            if (enemyShip.isDead()) continue;

            std::vector<Position> possibleMoves =
                const_cast<Ship&>(enemyShip).getPossibleMoves();
            double valuePerPosition = enemyShip.getValue(enemy.getParams()) /
                                    possibleMoves.size();

            for (const Position& pos : possibleMoves) {
                allEnemyPositions.emplace_back(pos, valuePerPosition);
            }
        }

        // 對每個可能的敵人位置發射射線
        for (const auto& [target, baseValue] : allEnemyPositions) {
            Ray ray(ship.getPosition(), target);
            bool pathBlocked = false;
            std::vector<Position> targetsOnRay;
            double totalScore = 0;

            // 檢查射線上的所有敵方可能位置
            for (const auto& [enemyPos, enemyValue] : allEnemyPositions) {
                auto [distance, t] = ray.distanceAndProjection(enemyPos);
                if (t > EPSILON && distance < EPSILON) {
                    targetsOnRay.push_back(enemyPos);
                    totalScore += enemyValue;
                }
            }

            // 檢查是否有友軍阻擋
            for (const Ship& allyShip : ships) {
                if (!allyShip.isDead() && &allyShip != &ship) {
                    auto [distance, t] = ray.distanceAndProjection(
                        allyShip.getPosition());
                    if (t > EPSILON && t < 1.0 && distance < EPSILON) {
                        pathBlocked = true;
                        break;
                    }
                }
            }

            if (!pathBlocked && !targetsOnRay.empty()) {
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
                    score += totalScore;

                    if (score > best.score) {
                        best = {target, missileType, score, explanation,
                              targetsOnRay};
                    }
                }
            }
        }
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
    double attackThreshold = 0.0;

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
            enemyDistanceScore += params.enemyDistanceWeight / dist;
        }
    }
    score += enemyDistanceScore;

    // 計算與隊友的距離得分
    double allyDistanceScore = 0;
    for (const Ship& allyShip : ships) {
        if (!allyShip.isDead() && &allyShip != &ship) {
            double dist = move.distanceTo(allyShip.getPosition());
            allyDistanceScore += params.allyDistanceWeight / dist;
        }
    }
    score += allyDistanceScore;

    // 計算格檔和被瞄準得分
    int blockCount = 0, targetCount = 0;
    for (const Ship& enemyShip : enemy.getShips()) {
        if (!enemyShip.isDead()) {
            for (const Ship& allyShip : ships) {
                if (!allyShip.isDead() && &allyShip != &ship) {
                    Ray ray(enemyShip.getPosition(), allyShip.getPosition());
                    auto [distance, t] = ray.distanceAndProjection(move);
                    if (distance < EPSILON && t > EPSILON && t < 1.0) {
                        ++blockCount;
                    }
                }
            }

            // 檢查是否被直接瞄準
            Ray ray(enemyShip.getPosition(), move);
            bool isTarget = true;
            auto [distance, t] = ray.distanceAndProjection(move);
            if (distance < EPSILON && t > EPSILON) {
                // 檢查是否有其他船隻（包括敵人和隊友）擋住射線
                // 檢查敵方船隻
                for (const Ship& otherShip : enemy.getShips()) {
                    if (!otherShip.isDead() && &otherShip != &enemyShip) {
                        auto [blockDist, blockT] =
                            ray.distanceAndProjection(otherShip.getPosition());
                        if (blockDist < EPSILON && blockT > EPSILON && blockT < t) {
                            isTarget = false;
                            break;
                        }
                    }
                }

                // 檢查友方船隻
                if (isTarget) {
                    for (const Ship& allyShip : ships) {
                        if (!allyShip.isDead() && &allyShip != &ship) {
                            auto [blockDist, blockT] =
                                ray.distanceAndProjection(allyShip.getPosition());
                            if (blockDist < EPSILON && blockT > EPSILON && blockT < t) {
                                isTarget = false;
                                break;
                            }
                        }
                    }
                }

                if (isTarget) ++targetCount;
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
        std::vector<Position> damageArea = missile.getDamageArea(target);

        // 計算可能傷害到的敵人價值
        double potentialDamage = 0;
        for (const Ship& enemyShip : enemy.getShips()) {
            if (!enemyShip.isDead()) {
                std::vector<Position> moves =
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
};

class Game {
public:
    Game() : player1(true), player2(false), round(0) {
        map.resize(MAP_SIZE, std::vector<char>(MAP_SIZE, '.'));
    }

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
                if (ship->useMissile(type)) {
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
                if (ship->useMissile(type)) {
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
        std::vector<Position> damageArea = missile.getDamageArea(target);

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

// 參數實驗函數
void runParameterExperiment() {
    const int EXPERIMENT_ROUNDS = 10;
    std::vector<std::pair<std::string, StrategyParams>> paramSets = {
        {"Aggressive", StrategyParams(1.0, 1.2, 0.8, -0.5, 0.7, -0.2,0)},
        {"Defensive", StrategyParams(1.2, 0.8, 1.2, -1.2, 0.4, -0.5,3)},
        {"Balanced", StrategyParams(1.0, 1.0, 1.0, -1.0, 0.5, -0.3,1)},
        {"Missile-focused", StrategyParams(0.8, 1.5, 0.7, -0.8, 0.6, -0.2,1)},
        {"Health-focused", StrategyParams(1.5, 0.7, 1.0, -1.0, 0.5, -0.4,1)}
    };

    // 修改: 分別記錄作為P1和P2時的統計資料
    struct DetailedStats {
        int wins = 0;
        int totalShips = 0;
        int totalHealth = 0;
        int totalRounds = 0;
        double totalDuration = 0.0;
        // 新增更詳細的統計
        int totalMissilesUsed = 0;
        int totalDamageDealt = 0;
        int survivalRounds = 0;  // 存活回合數
    };

    struct ExperimentResult {
        DetailedStats p1Stats;
        DetailedStats p2Stats;
        int draws = 0;
    };

    // 創建結果矩陣
    std::vector<std::vector<ExperimentResult>> results(
        paramSets.size(), std::vector<ExperimentResult>(paramSets.size()));

    // 執行實驗並收集詳細資料
    for (size_t i = 0; i < paramSets.size(); ++i) {
        for (size_t j = 0; j < paramSets.size(); ++j) {


            std::cout << "Testing P1:" << paramSets[i].first << " vs P2:"
                     << paramSets[j].first << "...\n";

            for (int k = 0; k < EXPERIMENT_ROUNDS; ++k) {
                Game game(paramSets[i].second, paramSets[j].second);
                auto result = game.run();

                // 無論勝負都記錄雙方資料
                results[i][j].p1Stats.totalShips += result.p1Ships;
                results[i][j].p1Stats.totalHealth += result.p1Health;
                results[i][j].p1Stats.totalRounds += result.rounds;
                results[i][j].p1Stats.totalDuration += result.duration;

                results[i][j].p2Stats.totalShips += result.p2Ships;
                results[i][j].p2Stats.totalHealth += result.p2Health;
                results[i][j].p2Stats.totalRounds += result.rounds;
                results[i][j].p2Stats.totalDuration += result.duration;

                if (result.winner == 1) {
                    results[i][j].p1Stats.wins++;
                } else if (result.winner == 2) {
                    results[i][j].p2Stats.wins++;
                } else {
                    results[i][j].draws++;
                }
            }
        }
    }

    // 輸出整體分析
    std::cout << "\nOverall Strategy Analysis:\n";
    std::cout << "========================\n\n";

    for (size_t i = 0; i < paramSets.size(); ++i) {
        std::cout << "\nStrategy: " << paramSets[i].first << "\n";
        std::cout << "--------------------------------\n";

        // 計算作為P1時的總體表現
        int totalP1Games = 0;
        int totalP1Wins = 0;
        double avgP1Ships = 0;
        double avgP1Health = 0;

        // 計算作為P2時的總體表現
        int totalP2Games = 0;
        int totalP2Wins = 0;
        double avgP2Ships = 0;
        double avgP2Health = 0;

        for (size_t j = 0; j < paramSets.size(); ++j) {
            if (i == j) continue;

            // 作為P1的統計
            totalP1Games += EXPERIMENT_ROUNDS;
            totalP1Wins += results[i][j].p1Stats.wins;
            avgP1Ships += results[i][j].p1Stats.totalShips;
            avgP1Health += results[i][j].p1Stats.totalHealth;

            // 作為P2的統計
            totalP2Games += EXPERIMENT_ROUNDS;
            totalP2Wins += results[j][i].p2Stats.wins;
            avgP2Ships += results[j][i].p2Stats.totalShips;
            avgP2Health += results[j][i].p2Stats.totalHealth;
        }

        // 輸出作為P1的表現
        std::cout << "As Player 1:\n";
        std::cout << "  Win Rate: " << std::fixed << std::setprecision(1)
                  << (static_cast<double>(totalP1Wins) / totalP1Games * 100) << "%\n";
        std::cout << "  Average Ships Remaining: "
                  << (avgP1Ships / totalP1Games) << "\n";
        std::cout << "  Average Health Remaining: "
                  << (avgP1Health / totalP1Games) << "\n";

        // 輸出作為P2的表現
        std::cout << "As Player 2:\n";
        std::cout << "  Win Rate: " << std::fixed << std::setprecision(1)
                  << (static_cast<double>(totalP2Wins) / totalP2Games * 100) << "%\n";
        std::cout << "  Average Ships Remaining: "
                  << (avgP2Ships / totalP2Games) << "\n";
        std::cout << "  Average Health Remaining: "
                  << (avgP2Health / totalP2Games) << "\n";
    }

    // 輸出詳細對戰統計
    std::cout << "\nDetailed Matchup Statistics:\n";
    std::cout << "==========================\n\n";

    for (size_t i = 0; i < paramSets.size(); ++i) {
        for (size_t j = 0; j < paramSets.size(); ++j) {
            if (i == j) continue;

            const auto& res = results[i][j];
            int totalGames = EXPERIMENT_ROUNDS;

            std::cout << "\nMatchup: " << paramSets[i].first
                     << "(P1) vs " << paramSets[j].first << "(P2)\n";
            std::cout << "----------------------------------------\n";

            // Player 1 統計
            std::cout << "Player 1 (" << paramSets[i].first << "):\n";
            std::cout << "  Wins: " << res.p1Stats.wins << " ("
                     << std::fixed << std::setprecision(1)
                     << (static_cast<double>(res.p1Stats.wins) / totalGames * 100)
                     << "%)\n";
            std::cout << "  Average Ships: "
                     << static_cast<double>(res.p1Stats.totalShips) / totalGames
                     << "\n";
            std::cout << "  Average Health: "
                     << static_cast<double>(res.p1Stats.totalHealth) / totalGames
                     << "\n";

            // Player 2 統計
            std::cout << "Player 2 (" << paramSets[j].first << "):\n";
            std::cout << "  Wins: " << res.p2Stats.wins << " ("
                     << std::fixed << std::setprecision(1)
                     << (static_cast<double>(res.p2Stats.wins) / totalGames * 100)
                     << "%)\n";
            std::cout << "  Average Ships: "
                     << static_cast<double>(res.p2Stats.totalShips) / totalGames
                     << "\n";
            std::cout << "  Average Health: "
                     << static_cast<double>(res.p2Stats.totalHealth) / totalGames
                     << "\n";

            // 整體對戰統計
            std::cout << "Match Statistics:\n";
            std::cout << "  Draws: " << res.draws << " ("
                     << (static_cast<double>(res.draws) / totalGames * 100)
                     << "%)\n";
            std::cout << "  Average rounds: "
                     << static_cast<double>(res.p1Stats.totalRounds) / totalGames
                     << "\n";
            std::cout << "  Average duration: "
                     << res.p1Stats.totalDuration / totalGames
                     << " seconds\n";
        }
    }
}
int main() {
    std::cout << "Naval Battle Game Simulation\n";
    std::cout << "============================\n\n";

    runParameterExperiment();

    return 0;
}