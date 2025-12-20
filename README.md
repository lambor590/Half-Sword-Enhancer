# Half Sword Enhancer

[![Latest Release](https://img.shields.io/github/v/release/lambor590/Half-Sword-Enhancer?style=flat-square&color=blue)](https://github.com/lambor590/Half-Sword-Enhancer/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/lambor590/Half-Sword-Enhancer/total?style=flat-square&color=green)](https://github.com/lambor590/Half-Sword-Enhancer/releases)
[![License](https://img.shields.io/badge/license-All%20Rights%20Reserved-red?style=flat-square)](https://github.com/lambor590/Half-Sword-Enhancer#license)

> A feature-rich, highly optimized mod for Half Sword with an intuitive in-game interface.

## Table of Contents

- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Features](#features)
  - [Gameplay](#gameplay)
  - [Entity Spawner](#entity-spawner)
  - [Settings](#settings)
- [Highlights](#highlights)
- [Troubleshooting & FAQ](#troubleshooting--faq)
- [Compatibility](#compatibility)
- [Community](#community)
- [Credits](#credits)
- [License](#license)

## Requirements

- **OS:** Built natively for Windows 10/11 (64-bit)
- **Game:** Half Sword Demo or Playtest
- **Linux/Steam Deck:** Supported via Proton — see [Linux Guide](Linux-Guide.md)
- Microsoft Visual C++ Redistributable for Visual Studio (x64) — [Download here](https://aka.ms/vc14/vc_redist.x64.exe)

## Installation

Choose between the **Launcher** (recommended) or **Manual Install**.

### Launcher

1. Download the launcher: [HS_Enhancer_Launcher.exe](https://github.com/lambor590/Half-Sword-Enhancer/releases/latest/download/HS_Enhancer_Launcher.exe)
2. Run it — the launcher will:
   - Check for updates automatically
   - Launch Half Sword if not already running
   - Inject the mod into the game
3. Run the launcher each time you want to use the mod.

### Manual Install

1. Download the ZIP: [HS_Enhancer.zip](https://github.com/lambor590/Half-Sword-Enhancer/releases/latest/download/HS_Enhancer.zip)
2. Extract both files into: `<Game Folder>\HalfSwordUE5\Binaries\Win64`
3. Launch the game normally — the mod loads automatically.

### Comparison

| Feature | Launcher | Manual Install |
|---------|----------|----------------|
| Setup | One-click | Copy 2 files to game folder |
| Auto-updates | Yes | No |
| Internet required | Only for update check | No |
| Antivirus alerts | Possible (injection) | Rare |

> **Note:** Both methods provide identical mod functionality. The mod does not modify any game files.

## Quick Start

### Opening the Menu

Press **INSERT** to toggle the mod interface (customizable in Settings).

### Default Keybinds

| Key | Action |
|-----|--------|
| INSERT | Toggle mod menu |
| I | Infinite Stamina |
| Z | Toggle Slow Motion |
| J | Jump |
| N | Spawn NPC |
| T | Save Loadout |
| P | Speed Multiplier |
| L | Toggle Custom Gravity |

All keybinds can be customized or unbound in the mod interface.

### Becoming a Giant

1. Go to **Entity Spawner** → **Spawn NPC** → **Config**
2. Set **Scale** (recommended: 1.70)
3. Increase **Distance Up** (~85) to avoid floor clipping
4. Spawn the NPC
5. Bind and use **Possess Nearest Willie** to control it

## Features

### Gameplay

#### World

| Feature | Description | Parameters |
|---------|-------------|------------|
| **Slow Motion** | Slows down game time | Speed: 1-99% |
| **Custom Gravity** | Applies custom gravity | Range: -3000 to 3000 |
| **Kill All Enemies** | Eliminates nearby enemies | Radius: 50-5000, Snap Neck option |
| **Toggle Enemy AI** | Pause/resume enemy behavior | Radius: 50-5000 |
| **Destroy All Willies** | Remove NPCs from world | Dead only, Disintegrate options |
| **Clear Blood** | Remove blood decals | Intensity: 0.0-1.0 |
| **Clear Objects** | Remove dropped items | Radius: 50-5000 |
| **Toggle Game Paused** | Pause/unpause simulation | Useful for photo mode |

#### Player

| Feature | Description | Parameters |
|---------|-------------|------------|
| **Infinite Stamina** | Keeps stamina full | — |
| **Infinite Consciousness** | Prevents knockout | — |
| **Enemy Infinite Consciousness** | Enemies can't be knocked out | — |
| **Save Loadout** | Save current equipment | Works in free mode |
| **Jump** | Launch into the air | Force: 1000-10000 |
| **Speed Multiplier** | Move faster | Run/Walk: 1x-100x |
| **Strength Multiplier** | Enhanced physical abilities | Strength, Grab Force, Hands Rigidity: 1x-10x |
| **Custom Body Tonus** | Control muscle tension | Multiplier: 1x-10x, No Weakening option |
| **Ragdoll** | Go completely limp | — |
| **Enemy Ragdoll** | Make all enemies ragdoll | — |
| **No Kick Cooldown** | Rapid kicking | — |
| **Invulnerability** | Immune to all damage | — |
| **No Pain** | Immune to pain effects | — |
| **Enemy No Pain** | Enemies immune to pain | — |
| **Get Up** | Force stand when knocked down | — |
| **Dash** | Quick forward movement | Force: 1000-10000 |
| **Possess Nearest Willie** | Control closest NPC | Toggle to switch back |

### Entity Spawner

#### NPC Spawner

Spawn any NPC type with full customization.

**Available NPC Types:**
- Regular Willie
- No Brain Willie
- Boss 1 through Boss 9 (including Baron)

**Spawn Options:**

| Option | Description |
|--------|-------------|
| Bodyguard | NPC joins your team |
| Snap to Ground | Auto-adjust spawn height |
| Distance Forward | Spawn distance: 100-500 |
| Distance Up | Height offset: 0-300 |
| Scale | Size multiplier: 0.1x-4.0x |
| Team | Team assignment: 0-9 |

#### Item Spawner

Spawn weapons, armor, and props with a searchable interface.

**Categories:**

| Category | Examples |
|----------|----------|
| **Weapons** | Swords (21), Maces (9), Axes (2), Polearms (23), Daggers (6), Tools (23), Shields (8), Improvised (4) |
| **Helmets** | Metal armets, sallets, barbutes, kettle helms, cloth hats (43 variants) |
| **Body Armor** | Cuirasses, breastplates, gambeson, doublets (25 variants) |
| **Arms** | Vambraces, chains (10 variants) |
| **Legs** | Cuisses, greaves, hosen (19 variants) |
| **Hands** | Gauntlets, half-gauntlets (7 variants) |
| **Feet** | Shoes, sabatons (6 variants) |
| **Neck** | Mail standards, bevors (5 variants) |
| **Shoulders** | Spaulders, pauldrons (7 variants) |
| **Waist** | Foulds (3 variants) |
| **Props** | Baskets, candles, coffins, chests, tables, skeletons (9 variants) |

**Spawn Options:**
- Distance Forward: 50-300
- Distance Up: 0-200
- Scale: 0.1x-5.0x
- Snap to Ground

### Settings

#### GUI

| Setting | Description |
|---------|-------------|
| Toggle GUI Key | Customize menu key (default: INSERT) |
| Unbind Key | Key to unbind features (default: DELETE) |
| Enable Notifications | Show on-screen notifications for actions |
| Enable Tooltips | Show helpful tooltips on hover |
| Unlock UE Console | Enable Unreal Engine console (F2) |

#### Graphics

Control individual graphics settings (useful since the game lacks this option).

| Setting | Options |
|---------|---------|
| Apply on Startup | Auto-apply saved settings |
| Render Scale | 1-200% |
| Shadow Quality | Low, Medium, High, Epic, Cinematic |
| Global Illumination Quality | Low, Medium, High, Epic, Cinematic |
| Reflection Quality | Low, Medium, High, Epic, Cinematic |
| Post Process Quality | Low, Medium, High, Epic, Cinematic |
| Effects Quality | Low, Medium, High, Epic, Cinematic |

## Highlights

- **In-game interface** — Toggle with INSERT, no alt-tabbing needed
- **Highly optimized** — Less than 1 FPS impact
- **Auto-updates** — Launcher keeps everything current
- **Dynamic keybinds** — Customize or unbind any feature
- **Macros** — Bind multiple features to the same key
- **Persistent settings** — All preferences auto-saved
- **Notifications** — Visual feedback for actions
- **Tooltips** — Hover for feature explanations
- **Crash prevention** — Built-in stability measures
- **DirectX 11 & 12** — Full support for both

## Troubleshooting & FAQ

### Game Crashes

#### Entity Spawner Crashes
- **Low FPS:** Your PC may not handle many entities
- **Physics overload:** Too many physics objects can crash the game

#### Jump / Custom Gravity Crashes
Certain armor combinations crash when hitting the ground with high force. This is a game bug.

### Known Limitations

#### Save Loadout
Uses a native game function that only works in free mode, not gauntlet.

#### Height Limit
An invisible trigger marks anyone who touches it as the loser. This is a game mechanic.

#### Strength Multiplier Issues
Values above 4x may cause balance problems. Recommended: 3-4x.

### Launcher Issues

#### Injection Fails
Antivirus may block injection. Solutions:
- Add launcher to antivirus exclusions
- Temporarily disable real-time protection

#### Fatal Error on Boot
1. Conflict with other mods. Remove from `<Game>\HalfSwordUE5\Binaries\Win64`:
- `ue4ss.dll`
- `dwmapi.dll` (if not from this mod)

2. Missing Visual C++ Redistributable. [Download Visual C++ Redistributable (x64)](https://aka.ms/vc14/vc_redist.x64.exe)
3. Check your antivirus.
4. Verify game files via Steam.

#### Auto-updater Not Working
Download manually from:
- [GitHub Launcher](https://github.com/lambor590/Half-Sword-Enhancer/releases/latest/download/HS_Enhancer_Launcher.exe)
- [GitHub Manual Install](https://github.com/lambor590/Half-Sword-Enhancer/releases/latest/download/HS_Enhancer.zip)
- [Nexus Mods](https://www.nexusmods.com/halfsword/mods/26)

## Compatibility

| Platform | Status |
|----------|--------|
| Windows 10/11 | Fully supported |
| Linux / Steam Deck | Supported via Proton ([Guide](Linux-Guide.md)) |
| DirectX 11 | Fully supported |
| DirectX 12 | Fully supported |

## Community

[![Discord](https://img.shields.io/badge/Discord-Join%20Server-5865F2?style=flat-square&logo=discord&logoColor=white)](https://discord.gg/x3KmgsQYMp)

- Vote on community polls
- Suggest new features
- Get help and report bugs
- Share videos and screenshots

## Credits

- **The Ghost** — Creator and developer
- **digitalyeti** — Linux guide contributor

## License

This project has no open-source license.

- Code copying, modification, and distribution are **not allowed**
- Commercial use and derivative works are **not allowed**
- Private use is **allowed**

The source code is available for transparency purposes only. All rights reserved.

&copy; 2025 The Ghost
