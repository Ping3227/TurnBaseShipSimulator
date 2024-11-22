#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <map>


const int MAP_SIZE = 256;
const int MAX_ROUNDS = 100;
const bool VERBOSE_OUTPUT = false;
const double EPSILON = 1e-6;

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
            healthWeight = -1.896;
            missileWeight =  1.594;
            blockWeight = -0.790;
            targetWeight = -1.110;
            enemyDistanceWeight = -0.478;
            allyDistanceWeight = -1.598;
            attackThreshold= -1.772;
        } else {
            healthWeight = -1.583;
            missileWeight =  1.054;
            blockWeight =   1.130;
            targetWeight = -1.348;
            enemyDistanceWeight = -0.641;
            allyDistanceWeight =  -0.301;
            attackThreshold= 0.108;
        }
    }

    StrategyParams(double hw, double mw, double bw, double tw, double ew, double aw,double at)
        : healthWeight(hw), missileWeight(mw), blockWeight(bw),
          targetWeight(tw), enemyDistanceWeight(ew), allyDistanceWeight(aw) ,attackThreshold(at){}
};


struct Position {
    int x, y;
    Position(int _x = 0, int _y = 0) : x(_x), y(_y) {}

    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }

    double distanceTo(const Position& other) const {
        return std::sqrt(std::pow(x - other.x, 2) + std::pow(y - other.y, 2));
    }


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

struct Ray {
    Position origin;
    Position direction;

    Ray(const Position& from, const Position& to)
        : origin(from), direction(to - from) {}

    std::pair<double, double> distanceAndProjection(const Position& point) const {
        Position v = point - origin;
        double t = v.dot(direction) / direction.dot(direction);


        Position projection = origin + direction * t;
        double distance = point.distanceTo(projection);

        return {distance, t};
    }
};



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
            const int dx[] = {0, 1, 0, -1, 0};
            const int dy[] = {0, 0, 1, 0, -1};
            for (int i = 0; i < 5; ++i) {
                Position pos(target.x + dx[i], target.y + dy[i]);
                if (isValidPosition(pos)) {
                    area.push_back(pos);
                }
            }
        } else {
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

            if (score > best.score ) {
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
        std::vector<Position> potentialTargets;
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

        // evaluate all potential attack position
        for (const auto& [target, baseValue] : allEnemyPositions) {
            Ray ray(ship.getPosition(), target);
            bool pathBlocked = false;
            std::vector<Position> targetsOnRay;
            double totalScore = 0;

            // check the potential for each attack choice
            for (const auto& [enemyPos, enemyValue] : allEnemyPositions) {
                auto [distance, t] = ray.distanceAndProjection(enemyPos);
                if (t > EPSILON && distance < EPSILON) {
                    targetsOnRay.push_back(enemyPos);
                    totalScore += enemyValue;
                }
            }

            // check if the path is blocked by allies
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

                    if (score > best.score && score>params.attackThreshold) {
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


    double enemyDistanceScore = 0;
    for (const Ship& enemyShip : enemy.getShips()) {
        if (!enemyShip.isDead()) {
            double dist = move.distanceTo(enemyShip.getPosition());
            enemyDistanceScore += params.enemyDistanceWeight / dist;
        }
    }
    score += enemyDistanceScore;
    //std::cout<<"score after calculating enemy distance score: "<<score<<std::endl;

    double allyDistanceScore = 0;
    for (const Ship& allyShip : ships) {
        if (!allyShip.isDead() && &allyShip != &ship) {
            double dist = move.distanceTo(allyShip.getPosition());
            allyDistanceScore += params.allyDistanceWeight / dist;
        }
    }
    score += allyDistanceScore;
    //std::cout<<"score after calculating ally distance score: "<<score<<std::endl;

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

            // check whether others block enemy ship
            Ray ray(enemyShip.getPosition(), move);
            bool isTarget = true;
            auto [distance, t] = ray.distanceAndProjection(move);
            if (distance < EPSILON && t > EPSILON) {
                // enemy blocks
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

                // allies block
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
    //std::cout<<"score after calculating block and target score: "<<score<<std::endl;
    return score;
}

    double evaluateAttack(const Position& target, const Player& enemy,
                         Missile::Type missileType, std::string& explanation) {
        double score = 0;
        explanation = "";

        Missile missile(missileType);
        std::vector<Position> damageArea = missile.getDamageArea(target);

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

        // check if the attack is possibly blocked by allies
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

            // player 1 attack phase
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

            // player 2 move phase
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

            // player 1 attack trigger phase
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

            // player 2 attack phase
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

            // player 1 move phase
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

            // player 2 attack trigger phase
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
        int p1shipDestroyed=0;
        int p2shipDestroyed=0;

        for (const Ship& ship : player1.getShips()) {
            if (!ship.isDead()) {
                ++p1Ships;
                p1Health += ship.getHealth();
            }
            else {
                p1shipDestroyed++;
            }
        }

        for (const Ship& ship : player2.getShips()) {
            if (!ship.isDead()) {
                ++p2Ships;
                p2Health += ship.getHealth();
            }
            else {
                p2shipDestroyed++;
            }
        }

        int winner;
        if (p1shipDestroyed < p2shipDestroyed || (p1shipDestroyed == p2shipDestroyed && p1Health > p2Health)) {
            winner = 1;
        } else if (p2shipDestroyed < p1shipDestroyed || (p1shipDestroyed == p2shipDestroyed && p2Health > p1Health)) {
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

class QAgent {
private:
    struct State {
        std::vector<double> params;

        bool operator<(const State& other) const {
            for (size_t i = 0; i < params.size(); i++) {
                if (std::abs(params[i] - other.params[i]) > 0.01) {
                    return params[i] < other.params[i];
                }
            }
            return false;
        }
    };

    std::map<State, double> qTable;
    std::vector<double> currentBestParams;
    double bestQValue;
    double learningRate;
    double discountFactor;
    double explorationRate;
    std::mt19937 rng;
    std::string agentName;
    bool isFirstPlayer;
    int updateCount;

public:
    QAgent(std::string name, bool isFirst, double lr = 0.1, double gamma = 0.95, double epsilon = 0.3)
        : agentName(name), isFirstPlayer(isFirst), learningRate(lr),
          discountFactor(gamma), explorationRate(epsilon), updateCount(0) {
        std::random_device rd;
        rng = std::mt19937(rd());

        StrategyParams initial(isFirst);
        currentBestParams = {
            initial.healthWeight,
            initial.missileWeight,
            initial.blockWeight,
            initial.targetWeight,
            initial.enemyDistanceWeight,
            initial.allyDistanceWeight,
            initial.attackThreshold
        };
        bestQValue = 0.0;

        State initialState{currentBestParams};
        qTable[initialState] = bestQValue;
    }

    StrategyParams getAction() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        if (dist(rng) < explorationRate) {
            std::vector<double> newParams(7);

            for (size_t i = 0; i < 7; i++) {
                double baseValue = currentBestParams[i];
                double variation = 0.5;
                newParams[i] = baseValue + (dist(rng) * 2 - 1) * variation;



            }

            return StrategyParams(
                newParams[0], newParams[1], newParams[2],
                newParams[3], newParams[4], newParams[5], newParams[6]
            );
        }

        return StrategyParams(
            currentBestParams[0], currentBestParams[1], currentBestParams[2],
            currentBestParams[3], currentBestParams[4], currentBestParams[5],
            currentBestParams[6]
        );
    }

    void update(const StrategyParams& params, double reward) {
        State state{std::vector<double>{
            params.healthWeight,
            params.missileWeight,
            params.blockWeight,
            params.targetWeight,
            params.enemyDistanceWeight,
            params.allyDistanceWeight,
            params.attackThreshold
        }};


        if (qTable.find(state) == qTable.end()) {
            qTable[state] = reward;
        } else {
            qTable[state] = (1 - learningRate) * qTable[state] +
                           learningRate * reward;
        }


        if (qTable[state] > bestQValue) {
            bestQValue = qTable[state];
            currentBestParams = state.params;
            updateCount++;

            std::cout << "\n" << agentName << " - Update #" << updateCount << ":\n";
            std::cout << "New Q-Value: " << bestQValue << "\n";
            printCurrentState(state);
        }
    }


    void printCurrentState(const State& state) const {
        std::cout << "Parameters:\n";
        std::cout << "Health Weight: " << std::fixed << std::setprecision(3)
                 << state.params[0] << "\n";
        std::cout << "Missile Weight: " << state.params[1] << "\n";
        std::cout << "Block Weight: " << state.params[2] << "\n";
        std::cout << "Target Weight: " << state.params[3] << "\n";
        std::cout << "Enemy Distance Weight: " << state.params[4] << "\n";
        std::cout << "Ally Distance Weight: " << state.params[5] << "\n";
        std::cout << "Attack Threshold: " << state.params[6] << "\n";
    }

    void printBestParameters() const {
        std::cout << "\n" << agentName << " Best Parameters:\n";
        std::cout << "Health Weight: " << std::fixed << std::setprecision(3)
                 << currentBestParams[0] << "\n";
        std::cout << "Missile Weight: " << currentBestParams[1] << "\n";
        std::cout << "Block Weight: " << currentBestParams[2] << "\n";
        std::cout << "Target Weight: " << currentBestParams[3] << "\n";
        std::cout << "Enemy Distance Weight: " << currentBestParams[4] << "\n";
        std::cout << "Ally Distance Weight: " << currentBestParams[5] << "\n";
        std::cout << "Attack Threshold: " << currentBestParams[6] << "\n";
        std::cout << "Q-Value: " << bestQValue << "\n";
        std::cout << "Total Updates: " << updateCount << "\n";
    }

    void decay_exploration(double decay_rate = 0.997) {
        explorationRate *= decay_rate;
        explorationRate = std::max(0.05, explorationRate);
    }

    double getExplorationRate() const { return explorationRate; }
};

void runParameterExperiment() {
    const int TRAINING_EPISODES = 1000;
    const int LOG_INTERVAL = 50;

    QAgent p1Agent("Player 1", true, 0.1, 0.95, 0.5);
    QAgent p2Agent("Player 2", false, 0.1, 0.95, 0.5);

    int windowSize = LOG_INTERVAL;
    int p1WinsInWindow = 0;
    int p2WinsInWindow = 0;
    std::vector<double> p1WinRates;
    std::vector<double> p2WinRates;

    std::cout << "Starting RL training for " << TRAINING_EPISODES << " episodes\n";

    for (int episode = 0; episode < TRAINING_EPISODES; ++episode) {
        auto p1Params = p1Agent.getAction();
        auto p2Params = p2Agent.getAction();

        Game game(p1Params, p2Params);
        auto result = game.run();

        if (result.winner == 1) p1WinsInWindow++;
        else if (result.winner == 2) p2WinsInWindow++;


        double p1Reward = (result.p1Ships * 10.0) +
                         (result.p1Health * 1.0) +
                         (result.winner == 1 ? 200.0 : -50) -
                         (result.p2Ships * 40.0) -
                         (result.p2Health * 1.5);

        double p2Reward = (result.p2Ships * 10.0) +
                         (result.p2Health * 1.0) +
                         (result.winner == 2 ? 200.0 : -50) -
                         (result.p1Ships * 40.0) -
                         (result.p1Health * 1.5);


        if (result.p1Ships > result.p2Ships) p1Reward *= 1.2;
        if (result.p2Ships > result.p1Ships) p2Reward *= 1.2;

        p1Agent.update(p1Params, p1Reward);
        p2Agent.update(p2Params, p2Reward);

        p1Agent.decay_exploration();
        p2Agent.decay_exploration();

        if ((episode + 1) % LOG_INTERVAL == 0) {
            double p1WinRate = static_cast<double>(p1WinsInWindow) / windowSize;
            double p2WinRate = static_cast<double>(p2WinsInWindow) / windowSize;

            p1WinRates.push_back(p1WinRate);
            p2WinRates.push_back(p2WinRate);

            std::cout << "\n========== Episode " << episode + 1 << "/"
                     << TRAINING_EPISODES << " ==========\n";
            std::cout << "Exploration Rates - P1: " << std::fixed
                     << std::setprecision(3) << p1Agent.getExplorationRate()
                     << ", P2: " << p2Agent.getExplorationRate() << "\n";
            std::cout << "Recent Win Rates - P1: " << (p1WinRate * 100)
                     << "%, P2: " << (p2WinRate * 100) << "%\n";

            p1Agent.printBestParameters();
            p2Agent.printBestParameters();

            p1WinsInWindow = 0;
            p2WinsInWindow = 0;
        }
    }

    std::cout << "\n===== Training Complete =====\n";
    std::cout << "\nFinal Parameters:\n";
    p1Agent.printBestParameters();
    p2Agent.printBestParameters();
}
void runDifferentStrategy() {
    const int EXPERIMENT_ROUNDS = 20;
    std::vector<std::pair<std::string, StrategyParams>> paramSets = {
         {"Aggressive", StrategyParams(-1.0, 1.0, 0.8, -0.5, 0.5, -0.5,0)}, // relative less block and  target weight
         {"Defensive", StrategyParams(-1.0, 1.0, 1.2, -1.5, 0.5, -0.5,2)}, // relative high block and target weight
        {"Balanced", StrategyParams(-1.0, 1.0, 1.0, -1.0, 0.5, -0.5,1)},  // normal
        {"RL_Player2", StrategyParams( -1.583,   1.054, 1.130,  -1.348, -0.641,  -0.301, 0.108)}, // defensive
        {"RL_Player1", StrategyParams(-0.955, 0.655, 0.288, -1.263, 0.074, -0.530,-0.175)},// counter
        {"RL_Player1_v2", StrategyParams(-1.896,-1.594, 0.790, 1.110, 0.478, 1.598,1.772)},
        {"RL_Player2_v2", StrategyParams(-1.685, 0.782, 0.741, -1.793, -0.753, -0.492, 0.482)}


    };


    struct DetailedStats {
        int wins = 0;
        int totalShips = 0;
        int totalHealth = 0;
        int totalRounds = 0;
        double totalDuration = 0.0;

        int totalMissilesUsed = 0;
        int totalDamageDealt = 0;
        int survivalRounds = 0;
    };

    struct ExperimentResult {
        DetailedStats p1Stats;
        DetailedStats p2Stats;
        int draws = 0;
    };


    std::vector<std::vector<ExperimentResult>> results(
        paramSets.size(), std::vector<ExperimentResult>(paramSets.size()));


    for (size_t i = 0; i < paramSets.size(); ++i) {
        for (size_t j = 0; j < paramSets.size(); ++j) {


            std::cout << "Testing P1:" << paramSets[i].first << " vs P2:"
                     << paramSets[j].first << "...\n";

            for (int k = 0; k < EXPERIMENT_ROUNDS; ++k) {
                Game game(paramSets[i].second, paramSets[j].second);
                auto result = game.run();


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


    std::cout << "\nOverall Strategy Analysis:\n";
    std::cout << "========================\n\n";

    for (size_t i = 0; i < paramSets.size(); ++i) {
        std::cout << "\nStrategy: " << paramSets[i].first << "\n";
        std::cout << "--------------------------------\n";


        int totalP1Games = 0;
        int totalP1Wins = 0;
        double avgP1Ships = 0;
        double avgP1Health = 0;


        int totalP2Games = 0;
        int totalP2Wins = 0;
        double avgP2Ships = 0;
        double avgP2Health = 0;

        for (size_t j = 0; j < paramSets.size(); ++j) {



            totalP1Games += EXPERIMENT_ROUNDS;
            totalP1Wins += results[i][j].p1Stats.wins;
            avgP1Ships += results[i][j].p1Stats.totalShips;
            avgP1Health += results[i][j].p1Stats.totalHealth;


            totalP2Games += EXPERIMENT_ROUNDS;
            totalP2Wins += results[j][i].p2Stats.wins;
            avgP2Ships += results[j][i].p2Stats.totalShips;
            avgP2Health += results[j][i].p2Stats.totalHealth;
        }


        std::cout << "As Player 1:\n";
        std::cout << "  Win Rate: " << std::fixed << std::setprecision(1)
                  << (static_cast<double>(totalP1Wins) / totalP1Games * 100) << "%\n";
        std::cout << "  Average Ships Remaining: "
                  << (avgP1Ships / totalP1Games) << "\n";
        std::cout << "  Average Health Remaining: "
                  << (avgP1Health / totalP1Games) << "\n";


        std::cout << "As Player 2:\n";
        std::cout << "  Win Rate: " << std::fixed << std::setprecision(1)
                  << (static_cast<double>(totalP2Wins) / totalP2Games * 100) << "%\n";
        std::cout << "  Average Ships Remaining: "
                  << (avgP2Ships / totalP2Games) << "\n";
        std::cout << "  Average Health Remaining: "
                  << (avgP2Health / totalP2Games) << "\n";
    }

    std::cout << "\nDetailed Matchup Statistics:\n";
    std::cout << "==========================\n\n";

    for (size_t i = 0; i < paramSets.size(); ++i) {
        for (size_t j = 0; j < paramSets.size(); ++j) {


            const auto& res = results[i][j];
            int totalGames = EXPERIMENT_ROUNDS;

            std::cout << "\nMatchup: " << paramSets[i].first
                     << "(P1) vs " << paramSets[j].first << "(P2)\n";
            std::cout << "----------------------------------------\n";

            // Player 1
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

            // player 2
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

            // overall
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
    std::cout << "Naval Battle Game RL Training\n";
    std::cout << "============================\n\n";

    //runParameterExperiment();
    runDifferentStrategy();
    return 0;
}