# Naval Battle Simulation

## Introduction

This project simulates a naval battle where two players control fleets of ships with varying attributes. The game includes a mix of predefined strategies and reinforcement learning (RL)-trained strategies, enabling players to explore tactical decisions and evaluate their effectiveness.

## Features

- **Dynamic Fleet Composition**: Each player controls ships of varying sizes, health, movement ranges, and missile capabilities.
- **Missile Mechanics**: Two missile types (`CROSS` and `SQUARE`) with distinct attack patterns and range effects.
- **Advanced Decision-Making**: Players evaluate actions based on weights for health, missile resources, blocking opportunities, targeting, and distance strategies.
- **Reinforcement Learning (RL)**: The game includes a Q-learning agent (`QAgent`) to train optimized strategy parameters.
- **Predefined Strategies**: Includes static strategies (Aggressive, Defensive, Balanced) and RL-trained dynamic strategies.

## Default Configuration

The game features the following default ship configurations:

### Player 1
- **2 Small Ships**: 
  - **Health**: 1
  - **Movement Range**: 2 tiles (square shape distance)
  - **Missiles**: 3 `SQUARE`
- **2 Medium Ships**:
  - **Health**: 2
  - **Movement Range**: 3 tiles
  - **Missiles**: 4 `CROSS`, 2 `SQUARE`
- **4 Large Ships**:
  - **Health**: 3
  - **Movement Range**: 4 tiles
  - **Missiles**: 5 `CROSS`, 4 `SQUARE`

### Player 2
- **3 Small Ships**:
  - Same as Player 1
- **3 Medium Ships**:
  - Same as Player 1
- **3 Large Ships**:
  - Same as Player 1

### Missile Mechanics
- **`CROSS` Missile**: Deals damage in a cross-shaped pattern around the target (up, down, left, right).
- **`SQUARE` Missile**: Deals damage in a 3x3 square grid around the target.

Missiles can be blocked by both friendly and enemy ships based on their alignment and proximity to the target. Blocking considers the perpendicular distance from the missile's path.

## Game Mechanics

1. **Ship Placement**: Both players place their ships on opposite sides of the map.
2. **Turn Sequence**:
   - Player 1 attacks.
   - Player 2 moves.
   - Player 1's attacks are resolved.
   - Player 2 attacks.
   - Player 1 moves.
   - Player 2's attacks are resolved.
3. **Decision Variables**:
   - **Health and Missile Weights**: Influence the perceived value of a ship.
   - **Block Weight**: Assigns a score for blocking enemy attacks.
   - **Target Weight**: Assigns a score for prioritizing specific targets.
   - **Distance Weights**:
     - **Enemy Distance Weight**: Evaluates proximity to enemies.
     - **Ally Distance Weight**: Evaluates proximity to allies.
   - **Attack Threshold**: Determines the minimum score required to launch an attack.
4. **Behavior Simplifications**:
   - Ships are controlled sequentially by players.
   - Ships cannot predict each other's strategies.
   - Ships avoid friendly fire and maintain at least one tile distance from allies to prevent splash damage.
   - Ships do not prioritize missile types and instead use available resources as needed.

## Experiment Functions

### `runParameterExperiment`
- Trains RL agents using `QAgent` to optimize strategy parameters.
- Simulates thousands of games with varying strategies to identify the best-performing parameter set.

### `runDifferentStrategy`
- Conducts strategy comparisons using predefined parameters (e.g., Aggressive, Defensive, Balanced) alongside RL-trained strategies.
- Outputs win rates, remaining ship counts, and detailed performance metrics.
## Default Strategies and Parameters

The following predefined strategies are used in the `runDifferentStrategy` experiment:

### Strategy Configurations
Each strategy's parameters are listed in the following order:
`healthWeight`, `missileWeight`, `blockWeight`, `targetWeight`, `enemyDistanceWeight`, `allyDistanceWeight`, `attackThreshold`.

1. **Aggressive**: Focuses on offense with lower blocking and targeting priorities.  
   Parameters: `1.0, 1.0, 0.8, -0.5, 0.5, -0.5, 0`  

2. **Defensive**: Prioritizes blocking and targeting while maintaining a cautious attack threshold.  
   Parameters: `1.0, 1.0, 1.2, -1.5, 0.5, -0.5, 2`  

3. **Balanced**: A middle-ground approach between offense and defense.  
   Parameters: `1.0, 1.0, 1.0, -1.0, 0.5, -0.5, 1`  

4. **RL_Player1**: Parameters learned through RL training for Player 1.  
   Parameters: `0.653, 1.095, -1.189, 0.603, -0.772, 3.170, 0.574`  

5. **RL_Player2**: Parameters learned through RL training for Player 2.  
   Parameters: `1.226, 0.904, -0.882, 0.466, -0.903, 3.218, 0.793`  

### Observed Characteristics

1. **Aggressive**:
   - Performs relatively poorly in both attack and survival metrics due to low block and targeting weights.
   - Effective only in straightforward offensive scenarios.

2. **Defensive**:
   - Slightly better survival rates compared to Aggressive.
   - Stronger in preserving ships, but struggles against RL-trained strategies.

3. **Balanced**:
   - A well-rounded strategy but lacks the specialization to dominate in specific scenarios.
   - Achieves average win rates, outperforming Aggressive but lagging behind Defensive in defensive matchups.

4. **RL_Player1 and RL_Player2**:
   - RL-trained strategies consistently outperform predefined ones, showing significant adaptability and optimization.
   - RL_Player2 achieves the highest win rates, particularly as Player 1, with a balanced mix of attack and defensive behaviors.

## Observations from Strategy Experiments

1. **Player 1 Advantage**:
   - Player 1 has higher win rates across most scenarios due to its fleet composition and turn-order advantage.

2. **Predefined vs. RL Strategies**:
   - RL strategies dominate predefined ones, often achieving over 70% win rates in head-to-head matchups.
   - RL parameters adapt better to dynamic scenarios, optimizing the balance between blocking, targeting, and distance evaluation.

3. **Matchup-Specific Insights**:
   - Aggressive vs. Defensive: Defensive wins in most scenarios due to higher blocking and targeting weights.
   - RL vs. RL: The matchups are balanced, demonstrating the nuanced strength of different RL-trained parameters.

## How to Run

1. **Compile the code**:
   ```bash
   g++ -std=c++20 -o naval_simulation main.cpp
   ```
2. **Execute the program**:
   ```bash
   ./naval_simulation
   ```
   By default, the program runs the predefined strategy comparison (`runDifferentStrategy`). To enable RL training, uncomment the `runParameterExperiment()` function in the `main()` function.

## Example Outputs

The program outputs detailed game logs, including:
- Ship placements and attributes.
- Tactical decisions and attack resolutions.
- Training updates for RL agents and strategy evaluations.
- Win rates and performance summaries.

## Dependencies

This project only requires the C++ Standard Library, including:
- `<vector>`, `<cmath>`, `<algorithm>`, `<random>`, `<iostream>`, and others.

