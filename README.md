[![Join the Half Sword Enhancer Discord](https://img.shields.io/badge/Discord-Join%20the%20community-5865F2?style=flat-square&logo=discord&logoColor=white)](https://discord.gg/x3KmgsQYMp)

# Half Sword Enhancer

[![Experimental build](https://img.shields.io/badge/download-experimental%20build-yellow?style=flat-square)](https://github.com/lambor590/Half-Sword-Enhancer/releases/download/experimental-latest/HSEnhancer.zip)
[![Total downloads](https://img.shields.io/github/downloads/lambor590/Half-Sword-Enhancer/total?style=flat-square&color=green)](https://github.com/lambor590/Half-Sword-Enhancer/releases)
[![License: source-available](https://img.shields.io/badge/license-source--available-orange?style=flat-square)](LICENSE)

Half Sword Enhancer adds a searchable in-game menu to Half Sword. You can customize your character, spawn NPCs and equipment, edit the world, and build scenarios.

**[Download the experimental build](https://github.com/lambor590/Half-Sword-Enhancer/releases/download/experimental-latest/HSEnhancer.zip)** · [Experimental release page](https://github.com/lambor590/Half-Sword-Enhancer/releases/tag/experimental-latest) · [Nexus Mods](https://www.nexusmods.com/halfsword/mods/26)

> Use the latest experimental build for the full game. The latest stable release is outdated and incompatible.
>
> For Half Sword Demo, [install v0.5.2 manually](#half-sword-demo-v052). It is the last compatible version.

## Features

| Area | What you can do |
|---|---|
| Player | Adjust your body, health, movement, combat, abilities, and saved game values. |
| World | Control game speed, gravity, maps, NPC behavior, the free camera, world objects, lighting, atmosphere, and the sky. |
| Spawn | Find and spawn items, weapons, armor, and customized NPCs. |
| Equipment | Build custom weapons and armor, assemble complete loadouts, and save presets. |
| Settings | Assign keyboard or mouse shortcuts, tune graphics, customize the interface, and replace supported textures. |

The menu has help text and shortcut notifications, and saves your settings. Most editors let you save and reuse presets for characters, equipment, and scenarios.

## Installation

**The launcher is optional for the full game.** After installation, start the game through Steam without the launcher. [README.txt](README.txt) in the ZIP's root has installation and uninstall instructions.

### Full game on Windows

Download the [experimental ZIP](https://github.com/lambor590/Half-Sword-Enhancer/releases/download/experimental-latest/HSEnhancer.zip). Close the game and extract the complete archive. Choose one installation method.

#### With the launcher

Run `HSEnhancerLauncher.exe` and follow the prompts. The launcher finds the game and chooses the installation method based on whether you use UE4SS. It can also check for updates and start the game. Run it again to update or repair the mod. Do not run it from inside the ZIP.

#### Manual installation

1. Right-click Half Sword in Steam. Select Manage, then Browse local files. Open `HalfSwordUE5\Binaries\Win64`.
2. Copy `winmm.dll` and `HSEnhancer.dll` from `Manual Install` into `Win64`, next to `HalfSwordUE5-Win64-Shipping.exe`.

If you use UE4SS, take the files from `Manual Install` and replace step 2:

- Copy `HSEnhancer.dll` into `Win64` and `main.dll` into `Win64\ue4ss\Mods\HSEnhancer\dlls`. Create the folders if needed.
- Make sure `Win64\ue4ss\Mods\mods.txt` contains `HSEnhancer : 1`.
- Do not install HSE's `winmm.dll`. Remove it if you previously installed HSE without UE4SS.

### Half Sword Demo v0.5.2

[v0.5.2 is the final version for the demo](https://github.com/lambor590/Half-Sword-Enhancer/releases/tag/v0.5.2). **Do not install or update it with the launcher.** The launcher downloads newer versions that are incompatible with the demo.

1. Close the demo and [uninstall any other HSE version](#uninstalling).
2. Download and extract the [v0.5.2 ZIP](https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v0.5.2/HSEnhancer.zip). You can also get it on [Nexus Mods](https://www.nexusmods.com/halfsword/mods/26?tab=files). In the Files tab, open Old files and choose "HS Enhancer Manual Install DEMO", version 0.5.2.
3. Right-click Half Sword Demo in Steam. Select Manage, then Browse local files. Open `HalfSwordUE5\Binaries\Win64`.
4. Copy `dwmapi.dll` and `HSEnhancer.dll` from that ZIP next to `HalfSwordUE5-Win64-Shipping.exe`. Use both files from v0.5.2. The demo uses `dwmapi.dll` instead of `winmm.dll`.

### Linux and Steam Deck

For the full game on Linux or Steam Deck, use Proton and follow the community-maintained [Linux and Steam Deck guide](Linux-Guide.md).

## Getting started

1. Start Half Sword through Steam. The mod menu opens by default.
2. Press `Insert` to hide or show the menu.
3. Find an option in the sidebar or search bar. Hover over it for help, or assign a keyboard or mouse shortcut.

Change the menu key and interface behavior in the Interface section of Settings.

When Discord is running, HSE shows "Half Sword [Enhanced]" with the map and mode, such as "Playing Free Mode in Yard", plus the remaining enemies and session time. Labels come from the game. For maps without a published label, the activity shows only the mode.

Activity buttons link to [the HSE website](https://halfswordenhancer.com) and its Discord community. The HSE logo appears as a small badge. The main image and its hover text stay the same. Mod tools such as Free Camera do not change the activity. Toggle Discord Activity in the Interface section of Settings. It is on by default.

HSE saves your settings and presets in `%APPDATA%\Half Sword Enhancer\`.

## Requirements and compatibility

- Half Sword or Half Sword Demo on Steam, with the matching mod version listed above
- Windows 10 or 11, 64-bit
- DirectX 11 or DirectX 12
- [Microsoft Visual C++ Redistributable for x64](https://aka.ms/vc14/vc_redist.x64.exe)

## Troubleshooting

- If the menu does not appear, press `Insert`, check your mod version, and install the Visual C++ Redistributable. If needed, repeat the [installation steps for your game](#installation).
- If the game will not start, [uninstall HSE](#uninstalling), then reinstall the correct version using the steps above.
- If the launcher cannot find the game, right-click Half Sword in Steam and select Manage, then Browse local files. Give the launcher that folder.
- For help, join [Discord](https://discord.gg/x3KmgsQYMp). Include your HSE version, game version, and a short description of the problem.

## Uninstalling

These steps cover both installation methods. Deleting the ZIP or launcher does not uninstall the mod.

1. Close the game. Right-click Half Sword or Half Sword Demo in Steam. Select Manage, then Browse local files, and open `HalfSwordUE5\Binaries\Win64`.
2. Delete `HSEnhancer.dll`. Then remove the files listed for your installation:

| Installation | Also remove from `Win64` |
|---|---|
| Full game without UE4SS | HSE's `winmm.dll` |
| Demo v0.5.2 | HSE's `dwmapi.dll` |
| UE4SS | The `ue4ss\Mods\HSEnhancer` folder. Remove `HSEnhancer : 1` from `ue4ss\Mods\mods.txt`. |

Remove only HSE files. Keep UE4SS and other mods. You can then start the game through Steam.

Your settings and presets stay in `%APPDATA%\Half Sword Enhancer\`. Delete that folder only if you also want to erase them.

## Credits

- The Ghost created and develops the mod.
- digitalyeti contributed the Linux guide.

## License

Half Sword Enhancer is source-available, not open-source. The [license](LICENSE) permits personal use and private forks. It prohibits public redistribution, shared modifications, and commercial use.

Half Sword Enhancer is an unofficial community mod and is not affiliated with the developers or publishers of Half Sword.

&copy; 2026 The Ghost
