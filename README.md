# Half Sword Enhancer

>An easy to use and feature rich mod.

## Table of Contents
- [Installation](#installation)
- [Linux Guide](Linux-Guide.md)
- [Quick Start](#quick-start)
- [Main features](#main-features)
- [Detailed Features](#detailed-features)
- [Troubleshooting & FAQ](#troubleshooting--faq)
- [Join the Discord server!](#join-the-discord-server)
- [License](#license)

## Installation
Choose between the **Launcher** or **Manual install**.

### 🚀 Launcher
- Download the executable: [HS_Enhancer_Launcher.exe](https://github.com/lambor590/Half-Sword-Enhancer/releases/latest/download/HS_Enhancer_Launcher.exe)
- Run it. It will check for updates, launch the game if not open and inject the mod.
- Must be run each time you want to use the mod.

### 🛠️ Manual Install
- Download the zip: [HS_Enhancer.zip](https://github.com/lambor590/Half-Sword-Enhancer/releases/latest/download/HS_Enhancer.zip)
- Extract both files into `<Game Folder>\HalfSwordUE5\Binaries\Win64`.

### ⚖️ Differences
Both versions are identical in terms of mod functionality; only the installation process differs. The mod itself does not modify any game files.

| Feature           | Launcher                              | Manual Install                         |
|-------------------|---------------------------------------|----------------------------------------|
| Installation      | One-click: checks updates, launches game and injects mod | Copy two files into game folder       |
| Automatic updates | ✅ Yes                                | ❌ No                                  |
| Internet required | Only for update check (optional)      | Not required                           |

## Quick Start

### Toggling the menu
Press `INSERT` by default; you can change the keybind in settings.

### Becoming a giant
Spawn an NPC with custom scale then possess it:
1. Go to **Entity Spawner** → **Spawn NPC** → open **Config** → set **Scale**.
2. Recommended scale: `1.70`.
3. Increase **Distance Up** (~`85`) to avoid floor clipping.
4. Spawn the NPC.
5. Possess it. There's no default keybind for this, so you need to bind `Possess Nearest Willie` to a key.

## Main features
* Friendly and easy to use interface displayed in the game window itself.
  * Can be toggled by pressing `INSERT` by default.
* Highly optimized. It doesn't even take 1 FPS.
* Completely made from scratch, specifically for Half Sword.
* Crash prevention system.
* Auto-updates.
  * This may cause antivirus alerts.
* Dynamic keybinding system.
  * Change your keybinds whenever you want with just one click.
  * You can unbind a feature and it will be automatically disabled.
* Configuration system.
  * Everything you do is automatically saved.
* All features are configurable within the mod interface.
* Full DirectX 11 & 12 support.

## Detailed Features

### 🌍 World
- **Slow Motion**
  - **Speed**: the speed of the game when is enabled.
- **Custom Gravity**
  - **Gravity**: applies a custom gravity level.
- **Kill All Enemies**
  - **Radius**: kills any enemy within this distance.
  - **Snap Neck**: just another way to kill enemies.
- **Toggle Enemy AI**
  - **Radius**: toggles AI for enemies within this distance.

### 🧑‍🚀 Player
- **Infinite Stamina**: keeps your stamina bar full at all times.
- **Save Loadout**: saves your current weapons and clothes for next fight.
- **Jump**
  - **Force**: controls how high you jump.
- **Speed Multiplier**
  - **Run Speed Multiplier**: makes you run faster.
  - **Walk Speed Multiplier**: makes you walk faster.
- **Strength Multiplier**
  - **Strength Multiplier**: makes your body more rigid and responsive. If you have it too high (4+), your character will have a hard time to move.
  - **Grab Force Multiplier**: makes it harder for your hands to loose grip (you can still loose grip as it seems to have a random chance, not sure).
  - **Hands Rigidity Multiplier**: makes your punches hit harder.
- **Invulnerability**: makes you immune to all damage.
- **Get Up**: forces you to stand up when knocked down.
- **Dash**
  - **Force**: controls how fast you dash.
- **Possess Nearest Willie**: take control of the closest NPC.

### 🏹 Entities
- **NPC Spawner**
  - **NPC Type**: choose which NPC class to spawn.
  - **Distance Forward**: how far in front the NPC appears.
  - **Distance Up**: height offset for spawn position.
  - **Scale**: size multiplier for the spawned NPC.
  - **Team**: assign the NPC to a team number.
  - **Bodyguard**: it won't attack you.
- **Item Spawner**
  - **Category**: select the item category.
  - **Subcategory**: choose a specific item type.
  - **Forward Distance**: how far in front the item appears.
  - **Up Distance**: height offset for spawn position.
  - **Scale**: size multiplier for the spawned item.

And this is just the beginning, there's much more to come!

## Troubleshooting & FAQ

### Game crashes
These are crashes that are not under my control and I can't do anything to fix them, at least at the moment.

#### Entity Spawner
- Your PC can't handle it.
  - You are running too low on FPS.
- The game can't handle it.
  - The game is not optimized to have a lot of entities.
  - Physics can also crash the game.

#### `Jump` and `Custom Gravity`
Currently, certain clothing or armor combinations crash the game when they fall and hit the ground with enough force; this is a game bug.

### Issues

#### Save Loadout
There's a native function the mod uses to save your loadout. It only works in free mode, not in gauntlet. I'll implement a custom save later.

#### When I reach a certain height I lose the fight
An invisible trigger around the map marks anyone who touches it as the loser. I'll try to remove it in a future update.

#### I can't stand or run without falling
This is caused by the Strength Multiplier. If set too high (>4), your character may lose balance. Recommended value: 3–4.

### Launcher issues

#### Injection fails
Some antivirus software blocks the launcher. Add it to exclusions or disable real-time protection.

#### Fatal error on boot
Conflicts with Mass Clown's trainer. Remove `ue4ss.dll` and `dwmapi.dll` from the game binaries folder (`<Game Folder>\HalfSwordUE5\Binaries\Win64`).

#### There's an update available, but clicking `OK` does nothing
* The auto-updater is broken for some people.
* Download the manual install version or launcher from:
  * [Nexus Mods](https://www.nexusmods.com/halfsword/mods/26)
  * GitHub:
    * [Launcher](https://github.com/lambor590/Half-Sword-Enhancer/releases/latest/download/HS_Enhancer_Launcher.exe)
    * [Manual Install](https://github.com/lambor590/Half-Sword-Enhancer/releases/latest/download/HS_Enhancer.zip)

## Join the Discord server!
[Join the Discord server!](https://discord.gg/x3KmgsQYMp)

* Take part in community polls to make decisions!
* Suggest new ideas, changes or anything that comes to mind!
* Get help and report issues or bugs!
* Get early access to new features!
* Become a tester and help with development!
  * Please note that tester applications are currently closed.
* Share videos and screenshots with the community!

## License

This project has no license, which means:

- Code copying is not allowed
- Modification is not allowed
- Distribution is not allowed
- Commercial use is not allowed
- Creation of derivative works is not allowed
- Private use is allowed

The source code is available for transparency purposes only. All rights reserved.

© 2025 The Ghost