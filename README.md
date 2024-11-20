# Naval Battle Simulation

## Introduction

This project simulates a naval battle where two players control fleets of ships with varying attributes. The game includes a mix of predefined strategies and reinforcement learning (RL)-trained strategies, enabling players to explore tactical decisions and evaluate their effectiveness.

## Features

- **Dynamic Fleet Composition**: Each player controls ships of varying sizes, health, movement ranges, and missile capabilities.
- **Missile Mechanics**: Two missile types (`CROSS` and `SQUARE`) with distinct attack patterns and range effects.
- **Advanced Decision-Making**: Players evaluate actions based on weights for health, missile resources, blocking opportunities, targeting, and distance strategies.
- **Simplified Mechanics**: The simulation uses simplified assumptions to ensure computational efficiency while retaining strategic depth.
- **Reinforcement Learning (RL)**: The game includes a Q-learning agent (`QAgent`) to train optimized strategy parameters.

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

## How to Run

1. **Compile the code**:
   ```bash
   g++ -std=c++11 -o naval_simulation main.cpp
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

