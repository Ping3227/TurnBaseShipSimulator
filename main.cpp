#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include <iostream>

// 常數定義
const int MAP_SIZE = 40;
const int MAX_ROUNDS = 100;
const double HEALTH_WEIGHT = -1.0;
const double MISSILE_WEIGHT = 0.8;
const double BLOCK_WEIGHT = 1.2;
const double TARGET_WEIGHT = -1.0;
const double ENEMY_DISTANCE_WEIGHT = 0.5;
const double ALLY_DISTANCE_WEIGHT = 0.3;

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

private:
    Type type;

    bool isValidPosition(const Position& pos) const {
        return pos.x >= 0 && pos.x < MAP_SIZE && pos.y >= 0 && pos.y < MAP_SIZE;
    }
};

// 船艦類別
class Ship {
public:
    Ship(int maxHp, int moveRange, int missiles)
        : maxHealth(maxHp), health(maxHp), moveRange(moveRange),
          remainingMissiles(missiles) {}

    bool isDead() const { return health <= 0; }
    int getHealth() const { return health; }
    int getMoveRange() const { return moveRange; }
    int getMissiles() const { return remainingMissiles; }
    Position getPosition() const { return position; }

    void setPosition(const Position& pos) { position = pos; }
    void takeDamage(int damage) { health = std::max(0, health - damage); }
    bool useMissile() {
        if (remainingMissiles > 0) {
            --remainingMissiles;
            return true;
        }
        return false;
    }

    double getValue() const {
        return (health * HEALTH_WEIGHT +
                remainingMissiles * MISSILE_WEIGHT);
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
    int remainingMissiles;
    Position position;

    bool isValidPosition(const Position& pos) const {
        return pos.x >= 0 && pos.x < MAP_SIZE && pos.y >= 0 && pos.y < MAP_SIZE;
    }
};

// 玩家類別
class Player {
public:
    Player(bool isFirst) : isFirstPlayer(isFirst) {
        // 初始化三種不同的船艦
        ships.push_back(Ship(3, 2, 3)); // 快速船
        ships.push_back(Ship(4, 1, 4)); // 中型船
        ships.push_back(Ship(5, 1, 3)); // 重型船
    }

    void placeShips() {
        std::random_device rd;
        std::mt19937 gen(rd());

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
                    placed = true;
                }
            }
        }
    }

    Position chooseMovePosition(Ship& ship, const Player& enemy) {
        std::vector<Position> moves = ship.getPossibleMoves();
        Position bestMove = ship.getPosition();
        double bestScore = -std::numeric_limits<double>::infinity();

        for (const Position& move : moves) {
            if (!canMoveTo(move)) continue;

            double score = evaluateMove(move, ship, enemy);
            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
            }
        }

        return bestMove;
    }

    Position chooseAttackPosition(const Ship& ship, const Player& enemy) {
        Position bestTarget(0, 0);
        double bestScore = -std::numeric_limits<double>::infinity();

        for (int x = 0; x < MAP_SIZE; ++x) {
            for (int y = 0; y < MAP_SIZE; ++y) {
                Position target(x, y);
                double score = evaluateAttack(target, enemy);
                if (score > bestScore) {
                    bestScore = score;
                    bestTarget = target;
                }
            }
        }

        return bestScore > 0 ? bestTarget : Position(-1, -1);
    }

    const std::vector<Ship>& getShips() const { return ships; }
    bool isDefeated() const {
        return std::all_of(ships.begin(), ships.end(),
                          [](const Ship& s) { return s.isDead(); });
    }

private:
    bool isFirstPlayer;
    std::vector<Ship> ships;

    bool canPlaceShip(const Position& pos) const {
        for (const Ship& ship : ships) {
            if (!ship.isDead() && ship.getPosition().distanceTo(pos) < 2) {
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
                       const Player& enemy) {
        double score = 0;

        // 計算與敵人的距離得分
        for (const Ship& enemyShip : enemy.getShips()) {
            if (!enemyShip.isDead()) {
                double dist = move.distanceTo(enemyShip.getPosition());
                score += ENEMY_DISTANCE_WEIGHT *dist*ship.getValue();
            }
        }

        // 計算與隊友的距離得分
        for (const Ship& allyShip : ships) {
            if (!allyShip.isDead() && &allyShip != &ship) {
                double dist = move.distanceTo(allyShip.getPosition());
                score += ALLY_DISTANCE_WEIGHT *dist*ship.getValue();
            }
        }

        // 計算格檔和被瞄準得分
        int blockCount = 0, targetCount = 0;
        for (const Ship& enemyShip : enemy.getShips()) {
            if (!enemyShip.isDead()) {
                // 簡化的格檔判定
                for (const Ship& allyShip : ships) {
                    if (!allyShip.isDead() && &allyShip != &ship) {
                        Position allyPos = allyShip.getPosition();
                        if (isInLine(enemyShip.getPosition(), allyPos, move)) {
                            ++blockCount;
                        }
                    }
                }

                // 被瞄準判定
                if (isInLine(enemyShip.getPosition(), move, move)) {
                    ++targetCount;
                }
            }
        }

        score += blockCount * BLOCK_WEIGHT * ship.getValue();
        score += targetCount * TARGET_WEIGHT * ship.getValue();

        return score;
    }

    double evaluateAttack(const Position& target, const Player& enemy) {
        double score = 0;

        for (const Ship& enemyShip : enemy.getShips()) {
            if (!enemyShip.isDead()) {
                std::vector<Position> moves = enemyShip.getPossibleMoves();
                double moveCount = moves.size();

                for (const Position& move : moves) {
                    if (isInLine(target, move, move)) {
                        score += enemyShip.getValue() / moveCount;
                    }
                }
            }
        }

        // 檢查是否會誤傷隊友
        for (const Ship& allyShip : ships) {
            if (!allyShip.isDead()) {
                if (isInLine(target, allyShip.getPosition(),
                            allyShip.getPosition())) {
                    return -std::numeric_limits<double>::infinity();
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

// 遊戲類別
class Game {
public:
    Game() : player1(true), player2(false), round(0) {
        // 初始化遊戲地圖
        map.resize(MAP_SIZE, std::vector<char>(MAP_SIZE, '.'));
    }

    void run() {
        // 放置船艦
        player1.placeShips();
        player2.placeShips();
        updateMap();
        printStatus();

        // 主遊戲循環
        while (!isGameOver()) {
            // 玩家1的回合
            playTurn(player1, player2);
            if (isGameOver()) break;

            // 玩家2的回合
            playTurn(player2, player1);

            ++round;
            updateMap();
            printStatus();
        }

        announceWinner();
    }

private:
    Player player1, player2;
    std::vector<std::vector<char>> map;
    int round;

    void playTurn(Player& current, Player& enemy) {
        // 選擇攻擊位置
        for (const Ship& ship : current.getShips()) {
            if (!ship.isDead()) {
                Position target = current.chooseAttackPosition(ship, enemy);
                if (target.x != -1) {
                    // 處理攻擊
                    handleAttack(target, enemy);
                }
            }
        }

        // 移動船艦
        for (Ship& ship : const_cast<std::vector<Ship>&>(current.getShips())) {
            if (!ship.isDead()) {
                Position newPos = current.chooseMovePosition(ship, enemy);
                ship.setPosition(newPos);
            }
        }
    }

    void handleAttack(const Position& target, Player& enemy) {
        Missile missile(Missile::CROSS); // 使用十字範圍飛彈
        auto damageArea = missile.getDamageArea(target);

        for (const Position& pos : damageArea) {
            for (Ship& ship : const_cast<std::vector<Ship>&>(enemy.getShips())) {
                if (!ship.isDead() && ship.getPosition() == pos) {
                    ship.takeDamage(1);
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
        std::cout << "Round " << round << "\n\n";

        // 打印地圖
        for (const auto& row : map) {
            for (char cell : row) {
                std::cout << cell << ' ';
            }
            std::cout << '\n';
        }
        std::cout << '\n';

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
                         << ", Missiles=" << ship.getMissiles()
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
            if (!ship.isDead() && ship.getMissiles() > 0) {
                player1HasMissiles = true;
                break;
            }
        }

        for (const Ship& ship : player2.getShips()) {
            if (!ship.isDead() && ship.getMissiles() > 0) {
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

        // 判定勝負
        if (p1Ships > p2Ships || (p1Ships == p2Ships && p1Health > p2Health)) {
            std::cout << "Player 1 Wins!\n";
        } else if (p2Ships > p1Ships || (p1Ships == p2Ships && p2Health > p1Health)) {
            std::cout << "Player 2 Wins!\n";
        } else {
            std::cout << "It's a Draw!\n";
        }
    }
};

// Raycasting 輔助類別
class Raycaster {
public:
    static bool hasLineOfSight(const Position& from, const Position& to,
                             const std::vector<std::vector<char>>& map) {
        int x0 = from.x;
        int y0 = from.y;
        int x1 = to.x;
        int y1 = to.y;

        int dx = std::abs(x1 - x0);
        int dy = std::abs(y1 - y0);
        int x = x0;
        int y = y0;

        int n = 1 + dx + dy;
        int x_inc = (x1 > x0) ? 1 : -1;
        int y_inc = (y1 > y0) ? 1 : -1;
        int error = dx - dy;
        dx *= 2;
        dy *= 2;

        for (; n > 0; --n) {
            if (x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE) {
                if (map[y][x] != '.') {
                    if (x != x1 || y != y1) return false;
                }
            }

            if (error > 0) {
                x += x_inc;
                error -= dy;
            } else {
                y += y_inc;
                error += dx;
            }
        }

        return true;
    }
};

int main() {
    std::cout << "Naval Battle Game Simulation\n";
    std::cout << "============================\n\n";

    // 創建並運行遊戲
    Game game;
    game.run();

    return 0;
}