#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include <iostream>
#include <iomanip>

// 常數定義
const int MAP_SIZE = 256;
const int MAX_ROUNDS = 100;

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
};

// 飛彈類別
class Missile {
public:
    enum Type { CROSS, SQUARE };

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
    Type type;

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

    std::vector<Position> getPossibleMoves() const {
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

    bool isValidPosition(const Position& pos) const {
        return pos.x >= 0 && pos.x < MAP_SIZE && pos.y >= 0 && pos.y < MAP_SIZE;
    }
};

// 玩家類別
class Player {
public:
    Player(bool isFirst)
        : isFirstPlayer(isFirst),
          params(isFirst) {
        if(isFirst) {
            // 初始化三種不同的船艦，包含兩種飛彈的數量
            for(int i=0; i<2; i++) {
                ships.push_back(Ship(1, 2, 0, 3)); // 快速船
            }
            for (int i=0; i<2; i++) {
                ships.push_back(Ship(2, 3, 4, 2)); // 中型船
            }
            for (int i=0; i<4; i++) {
                ships.push_back(Ship(3, 4, 5, 4)); // 重型船
            }

        }
        else {

            for(int i=0; i<3; i++) {
                ships.push_back(Ship(1, 2, 0, 3)); // 快速船
            }
            for (int i=0; i<3; i++) {
                ships.push_back(Ship(2, 3, 4, 2)); // 中型船
            }
            for (int i=0; i<3; i++) {
                ships.push_back(Ship(3, 4, 5, 4)); // 重型船
            }
        }

    }

    void placeShips() {
        std::random_device rd;
        std::mt19937 gen(rd());

        std::cout << (isFirstPlayer ? "Player 1" : "Player 2")
                  << " placing ships:\n";

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
                    std::cout << "Ship " << i + 1 << " placed at ("
                              << x << "," << y << ")\n";
                    placed = true;
                }
            }
        }
        std::cout << "\n";
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

        std::cout << (isFirstPlayer ? "Player 1" : "Player 2")
                  << " evaluating moves for ship at ("
                  << ship.getPosition().x << ","
                  << ship.getPosition().y << "):\n";

        for (const Position& move : moves) {
            if (!canMoveTo(move)) continue;

            std::string explanation;
            double score = evaluateMove(move, ship, enemy, explanation);

            // std::cout << "  Move to (" << move.x << "," << move.y
            //           << "): score=" << std::fixed << std::setprecision(2)
            //           << score << " - " << explanation << "\n";

            if (score > best.score) {
                best = {move, score, explanation};
            }
        }

        std::cout << "Chosen move: (" << best.position.x << ","
                  << best.position.y << ") with score "
                  << best.score << "\n\n";

        return best;
    }

    struct AttackDecision {
        Position position;
        Missile::Type missileType;
        double score;
        std::string explanation;
    };

    AttackDecision chooseAttackPosition(const Ship& ship,
                                      const Player& enemy) {
        AttackDecision best{{-1, -1}, Missile::CROSS,
                           -std::numeric_limits<double>::infinity(),
                           "No valid attacks"};

        std::cout << (isFirstPlayer ? "Player 1" : "Player 2")
                  << " evaluating attacks for ship at ("
                  << ship.getPosition().x << ","
                  << ship.getPosition().y << "):\n";

        // 評估兩種飛彈類型
        for (int type = 0; type <= 1; ++type) {
            Missile::Type missileType =
                type == 0 ? Missile::CROSS : Missile::SQUARE;

            // 檢查是否還有該類型的飛彈
            if ((missileType == Missile::CROSS &&
                 ship.getCrossMissiles() == 0) ||
                (missileType == Missile::SQUARE &&
                 ship.getSquareMissiles() == 0)) {
                continue;
            }

            for (int x = 0; x < MAP_SIZE; ++x) {
                for (int y = 0; y < MAP_SIZE; ++y) {
                    Position target(x, y);
                    std::string explanation;
                    double score = evaluateAttack(target, enemy,
                                                missileType, explanation);

                    if (score > 0) {
                        // std::cout << "  Attack at (" << x << "," << y
                        //           << ") with "
                        //           << (missileType == Missile::CROSS ?
                        //               "CROSS" : "SQUARE")
                        //           << " missile: score=" << std::fixed
                        //           << std::setprecision(2) << score
                        //           << " - " << explanation << "\n";

                        if (score > best.score) {
                            best = {target, missileType, score, explanation};
                        }
                    }
                }
            }
        }

        if (best.score > 0) {
            std::cout << "Chosen attack: (" << best.position.x << ","
                      << best.position.y << ") with "
                      << (best.missileType == Missile::CROSS ?
                          "CROSS" : "SQUARE")
                      << " missile, score " << best.score << "\n\n";
        } else {
            std::cout << "No attack chosen\n\n";
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
        explanation += "Enemy distance score: " +
                      std::to_string(enemyDistanceScore) + "; ";

        // 計算與隊友的距離得分
        double allyDistanceScore = 0;
        for (const Ship& allyShip : ships) {
            if (!allyShip.isDead() && &allyShip != &ship) {
                double dist = move.distanceTo(allyShip.getPosition());
                allyDistanceScore += params.allyDistanceWeight / (dist + 1);
            }
        }
        score += allyDistanceScore;
        explanation += "Ally distance score: " +
                      std::to_string(allyDistanceScore) + "; ";

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

        double blockScore = blockCount * params.blockWeight *
                           ship.getValue(params);
        score += blockScore;
        explanation += "Block score: " + std::to_string(blockScore) + "; ";

        double targetScore = targetCount * params.targetWeight *
                            ship.getValue(params);
        score += targetScore;
        explanation += "Target score: " + std::to_string(targetScore);

        return score;
    }

    double evaluateAttack(const Position& target, const Player& enemy,
                         Missile::Type missileType, std::string& explanation) {
        double score = 0;
        explanation = "";

        Missile missile(missileType);
        auto damageArea = missile.getDamageArea(target);

        // 計算可能傷害到的敵人價值
        double potentialDamage = 0;
        for (const Ship& enemyShip : enemy.getShips()) {
            if (!enemyShip.isDead()) {
                std::vector<Position> moves = enemyShip.getPossibleMoves();
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
        explanation += "Potential damage: " +
                      std::to_string(potentialDamage) + "; ";

        // 檢查是否會誤傷隊友
        for (const Ship& allyShip : ships) {
            if (!allyShip.isDead()) {
                for (const Position& pos : damageArea) {
                    if (pos == allyShip.getPosition()) {
                        explanation += "Would damage ally";
                        return -std::numeric_limits<double>::infinity();
                    }
                }
            }
        }

        return score;
    }

    bool isInLine(const Position& from, const Position& to,
                  const Position& point) const {
        // 簡化的射線檢測
        double d1 = point.distanceTo(from);
        double d2 = point.distanceTo(to);
        double d3 = from.distanceTo(to);
        return std::abs(d1 + d2 - d3) < 0.1;
    }
};

class Game {
public:
    Game() : player1(true), player2(false), round(0) {
        // 初始化遊戲地圖
        map.resize(MAP_SIZE, std::vector<char>(MAP_SIZE, '.'));
    }

    void run() {
        std::cout << "Game Start!\n\n";

        // 1. 玩家1放置船艦
        std::cout << "Phase: Player 1 placing ships\n";
        player1.placeShips();
        updateMap();
        printStatus();

        // 2. 玩家2放置船艦
        std::cout << "Phase: Player 2 placing ships\n";
        player2.placeShips();
        updateMap();
        printStatus();

        // 主遊戲循環
        while (!isGameOver()) {
            ++round;
            std::cout << "\nRound " << round << " Start!\n\n";

            // 3. 玩家1選擇攻擊地點
            std::cout << "Phase: Player 1 choosing attack positions\n";
            std::vector<std::pair<Position, Missile::Type>> p1Attacks;
            for (const Ship& ship : player1.getShips()) {
                if (!ship.isDead()) {
                    auto decision = player1.chooseAttackPosition(ship, player2);
                    if (decision.score > 0) {
                        p1Attacks.emplace_back(decision.position,
                                             decision.missileType);
                    }
                }
            }

            // 4. 玩家2移動船艦
            std::cout << "Phase: Player 2 moving ships\n";
            for (Ship& ship : const_cast<std::vector<Ship>&>(player2.getShips())) {
                if (!ship.isDead()) {
                    auto decision = player2.chooseMovePosition(ship, player1);
                    ship.setPosition(decision.position);
                }
            }
            updateMap();
            printStatus();

            // 5. 玩家1攻擊觸發
            std::cout << "Phase: Player 1 attacks triggering\n";
            for (const auto& attack : p1Attacks) {
                handleAttack(attack.first, attack.second, player1, player2);
            }
            updateMap();
            printStatus();

            if (isGameOver()) break;

            // 6. 玩家2選擇攻擊地點
            std::cout << "Phase: Player 2 choosing attack positions\n";
            std::vector<std::pair<Position, Missile::Type>> p2Attacks;
            for (const Ship& ship : player2.getShips()) {
                if (!ship.isDead()) {
                    auto decision = player2.chooseAttackPosition(ship, player1);
                    if (decision.score > 0) {
                        p2Attacks.emplace_back(decision.position,
                                             decision.missileType);
                    }
                }
            }

            // 7. 玩家1移動船艦
            std::cout << "Phase: Player 1 moving ships\n";
            for (Ship& ship : const_cast<std::vector<Ship>&>(player1.getShips())) {
                if (!ship.isDead()) {
                    auto decision = player1.chooseMovePosition(ship, player2);
                    ship.setPosition(decision.position);
                }
            }
            updateMap();
            printStatus();

            // 8. 玩家2攻擊觸發
            std::cout << "Phase: Player 2 attacks triggering\n";
            for (const auto& attack : p2Attacks) {
                handleAttack(attack.first, attack.second, player2, player1);
            }
            updateMap();
            printStatus();
        }

        announceWinner();
    }

private:
    Player player1, player2;
    std::vector<std::vector<char>> map;
    int round;

    void handleAttack(const Position& target, Missile::Type missileType,
                     Player& attacker, Player& defender) {
        std::cout << "Attack at (" << target.x << "," << target.y << ") with "
                  << (missileType == Missile::CROSS ? "CROSS" : "SQUARE")
                  << " missile\n";

        Missile missile(missileType);
        auto damageArea = missile.getDamageArea(target);

        for (const Position& pos : damageArea) {
            for (Ship& ship : const_cast<std::vector<Ship>&>(defender.getShips())) {
                if (!ship.isDead() && ship.getPosition() == pos) {
                    ship.takeDamage(1);
                    std::cout << "Hit ship at (" << pos.x << "," << pos.y
                              << "), damage dealt: 1\n";
                }
            }
        }
    }

    void updateMap() {
        // 清空地圖
        for (auto& row : map) {
            std::fill(row.begin(), row.end(), '.');
        }

        // 更新玩家1的船艦
        for (const Ship& ship : player1.getShips()) {
            if (!ship.isDead()) {
                Position pos = ship.getPosition();
                map[pos.y][pos.x] = '1';
            }
        }

        // 更新玩家2的船艦
        for (const Ship& ship : player2.getShips()) {
            if (!ship.isDead()) {
                Position pos = ship.getPosition();
                map[pos.y][pos.x] = '2';
            }
        }
    }

    void printStatus() const {
        std::cout << "\nCurrent game state:\n";
        std::cout << "Round: " << round << "\n\n";
        // // 打印地圖
        // std::cout << "  ";
        // for (int i = 0; i < MAP_SIZE; ++i) {
        //     std::cout << i << ' ';
        // }
        // std::cout << "\n";
        //
        // for (int i = 0; i < MAP_SIZE; ++i) {
        //     std::cout << i << ' ';
        //     for (char cell : map[i]) {
        //         std::cout << cell << ' ';
        //     }
        //     std::cout << '\n';
        // }
        // std::cout << '\n';

        // 打印船艦狀態
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
        // 檢查回合數限制
        if (round >= MAX_ROUNDS) {
            return true;
        }

        // 檢查玩家船艦是否全滅
        if (player1.isDefeated() || player2.isDefeated()) {
            return true;
        }

        // 檢查飛彈是否用盡
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

    void announceWinner() const {
        std::cout << "\nGame Over!\n";
        std::cout << "Total Rounds: " << round << "\n\n";

        // 計算存活船艦數和總血量
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

        std::cout << "Player 1: " << p1Ships << " ships remaining, "
                  << "total HP: " << p1Health << "\n";
        std::cout << "Player 2: " << p2Ships << " ships remaining, "
                  << "total HP: " << p2Health << "\n\n";

        if (p1Ships > p2Ships || (p1Ships == p2Ships && p1Health > p2Health)) {
            std::cout << "Player 1 Wins!\n";
        } else if (p2Ships > p1Ships || (p1Ships == p2Ships && p2Health > p1Health)) {
            std::cout << "Player 2 Wins!\n";
        } else {
            std::cout << "It's a Draw!\n";
        }
    }
};

int main() {
    std::cout << "Naval Battle Game Simulation\n";
    std::cout << "============================\n\n";

    Game game;
    game.run();

    return 0;
}