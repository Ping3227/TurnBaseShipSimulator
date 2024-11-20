# Naval Battle Simulation

## Introduction

This project is a naval battle simulation that leverages reinforcement learning (RL) to optimize player strategies. It includes modules for managing ships, missile attacks, decision-making, RL agents (`QAgent`), and a full game simulator.

## Features

- **Ship Management**: Handles movement, health, and resource management for ships.
- **Missile Types**: Supports two missile types: Cross (`CROSS`) and Square (`SQUARE`) with distinct attack patterns.
- **Strategy Optimization**: Uses reinforcement learning to train optimal strategy parameters.
- **Multiple Strategies**: Includes predefined strategies like Aggressive, Defensive, and Balanced.
- **Simulation Results**: Provides detailed insights into match outcomes and strategy comparisons.

## File Structure

- **`main.cpp`**: Contains the main implementation for the simulation and all related logic.

### Key Classes and Modules

1. **`Ship`**: Represents a ship, including its attributes (health, movement range) and actions (moving, taking damage, and using missiles).
2. **`Missile`**: Manages attack logic and calculates damage areas based on missile types.
3. **`Player`**: Represents a player, encapsulating decision-making and fleet management.
4. **`Game`**: The core game loop, handling player turns, attacks, and victory conditions.
5. **`QAgent`**: An RL agent used for learning and adapting optimal strategy parameters.

## How to Run

### Run the Simulation

1. **Compile the code**:
   ```bash
   g++ -std=c++20 -o naval_simulation main.cpp
   ```
2. **Execute the program**:
   ```bash
   ./naval_simulation
   ```
   By default, the program runs a strategy comparison experiment. To run reinforcement learning training, uncomment the `runParameterExperiment()` function in `main()`.

### Modify Parameters

You can customize the following constants and parameters:
- `MAP_SIZE`: Adjusts the map's dimensions.
- `MAX_ROUNDS`: Sets the maximum number of game rounds.
- `VERBOSE_OUTPUT`: Enables or disables detailed console logs.
- `StrategyParams`: Customize strategy parameters for different behaviors.

## Example Output

The program will produce outputs showing:
- Game states for each round.
- Updates to RL agent parameters.
- Training results and win rate statistics for different strategies.

## Dependencies

This project only requires the C++ Standard Library, including:
- `<vector>`, `<cmath>`, `<algorithm>`, `<random>`, `<iostream>`, and others.

## Contributions

Contributions are welcome! Feel free to open issues or submit pull requests to improve the project.
