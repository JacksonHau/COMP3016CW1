# COMP3016CW1
## Overview
Top-Down Zombies is a 2D survival shooter built in C++ using SDL3 for the Immersive Game Technologies (COMP3016) module. The player must survive against endless waves of zombies, using multiple weapons and pickups while managing ammo and health. This project demonstrates OOP design, file-based asset loading, AI-driven enemy steering, and exception handling in an unmanaged environment.
## Gameplay Description
- **Objective**: Survive as long as possible against waves of zombies.
- Controls:
  - **WASD** – Move
  - **Mouse** – Aim
  - **Left Click** – Shoot
  - **1 / 2 / 3 or Scroll Wheel** – Switch weapons
  - **R** – Reload
- Weapons:
  -  **Pistol**: Infinite ammo, semi-auto
  -  **Shotgun**: Wide spread, low fire rate
  -  **Rifle**: High accuracy and rate of fire
- Pickups:
  - Ammo Crates refill all ammo
  - Health Boxes restore 1 heart
- **Waves**: Enemies spawn in increasing numbers each wave.
## Game Mechanics
- **Zombie AI:** Zombies steer toward the player using vector-based direction and velocity.  
- **Wave System:** Difficulty increases by spawning more zombies.
- **Pickup Spawning:** Ammo and health crates spawn randomly at timed intervals.  
- **Damage & Collision:** Circular hit detection between entities ensures accurate combat detection.  
- **Music System:** SDL3 audio stream toggles between menu and in-game music, stopping automatically on player death.  
- **Weapon System:**  
  - **Pistol** – Unlimited ammo, but you must **spam left-click** to fire (semi-automatic).  
  - **Shotgun & Rifle** – **Hold left-click** to continuously fire while ammo lasts (automatic).  
  - Reload manually with **R** when out of ammo, or collect **Ammo Crates** to instantly refill.
## Game Programming Patterns
- **Object-Oriented Design**: Abstract base Entity class inherited by **Player**, **Zombie**, **Bullet**, **AmmoCrate** and **HealthBox**.
- **Composition**: Each entity manages its own texture, update, and draw logic.
- **State Machine**: Game states include **Menu**, **Playing**, **Intermission**, and **GameOver**.
- **Resource Loading Pattern**: Centralised **load_any()** function attempts multiple file paths for assets.
- **Audio Management**: **MusicPlayer** class encapsulates audio streaming and track switching.
## Dependencies
- **Language**: C++
- **Libraries**:
  - SDL3
  - SDL3_image
  - SDL3_audio
  - CMake
- **Assets Folder**:
  - data/assets/ (images)
  - data/sounds/ (music)

## Use of AI
Generative AI tools (ChatGPT) were used to:
- Assist with code debugging and structuring game logic (OOP patterns, collision checks).
- Generate placeholder sprite and sound assets.
- Help refine this README.md wording and report structure.
All AI outputs were reviewed, modified, and integrated manually.

## Exception Handling & Testing
- Input validation for null textures, missing sounds, and failed asset loads.
- Fallback textures when assets are unavailable.
- Randomised spawn functions tested for valid coordinates.
- Extensive manual play-testing to ensure no crash on unexpected input.

## UML
![Image Alt](https://github.com/JacksonHau/COMP3016CW1/blob/main/UML.jpg?raw=true)

## Screenshots
![Image Alt](https://github.com/JacksonHau/COMP3016CW1/blob/main/Screenshot/Game%20Menu.jpg?raw=true)
![Image Alt](https://github.com/JacksonHau/COMP3016CW1/blob/main/Screenshot/Game%201.jpg?raw=true)
![Image Alt](https://github.com/JacksonHau/COMP3016CW1/blob/main/Screenshot/Game%202.jpg?raw=true)
![Image Alt](https://github.com/JacksonHau/COMP3016CW1/blob/main/Screenshot/Game%20Over.jpg?raw=true)
