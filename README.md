[![Join the Half Sword Enhancer Discord](https://img.shields.io/badge/Discord-Join%20the%20community-5865F2?style=flat-square&logo=discord&logoColor=white)](https://discord.gg/x3KmgsQYMp)

# Half Sword Enhancer

[![Experimental build](https://img.shields.io/badge/download-experimental%20build-yellow?style=flat-square)](https://github.com/lambor590/Half-Sword-Enhancer/releases/download/experimental-latest/HSEnhancer.zip)
[![Total downloads](https://img.shields.io/github/downloads/lambor590/Half-Sword-Enhancer/total?style=flat-square&color=green)](https://github.com/lambor590/Half-Sword-Enhancer/releases)
[![License: source-available](https://img.shields.io/badge/license-source--available-orange?style=flat-square)](LICENSE)

Half Sword Enhancer is an all-in-one gameplay mod for **Half Sword**. It adds a fast, searchable in-game menu for customizing your character, creating NPCs and equipment, changing the world, and setting up your own scenarios.

**[Download the experimental build](https://github.com/lambor590/Half-Sword-Enhancer/releases/download/experimental-latest/HSEnhancer.zip)** · [Experimental release page](https://github.com/lambor590/Half-Sword-Enhancer/releases/tag/experimental-latest) · [Nexus Mods](https://www.nexusmods.com/halfsword/mods/26)

> The experimental build is currently required for the final version of Half Sword. The latest stable release is outdated and is not compatible with the current game version.

## Features

| Area | What you can do |
|---|---|
| **Player** | Adjust your body, health, movement, combat, abilities, and saved game values. |
| **World** | Control game speed, gravity, maps, NPC behavior, the free camera, world objects, lighting, atmosphere, and the sky. |
| **Spawn** | Find and spawn items, weapons, armor, and fully customized NPCs. |
| **Equipment** | Build custom weapons and armor, assemble complete loadouts, and save reusable presets. |
| **Settings** | Assign keyboard or mouse shortcuts, tune graphics, customize the interface, and replace supported textures. |

The interface includes section search, contextual help, shortcut notifications, and persistent settings. Most editors support presets, so your characters, equipment, and scenarios can be saved and reused.

## Installation

### Windows — recommended

1. Download the [experimental HSEnhancer.zip](https://github.com/lambor590/Half-Sword-Enhancer/releases/download/experimental-latest/HSEnhancer.zip).
2. Extract the complete archive to a folder. Do not run the launcher from inside the ZIP.
3. Run `HSEnhancerLauncher.exe`.
4. Follow the prompts. The launcher finds Half Sword, installs the mod, can check for updates, and can start the game when it finishes.

The launcher supports both standalone and UE4SS installations and selects the appropriate method automatically. Run it again whenever you want to update or repair the mod.

### Manual installation

The same download includes everything required for a manual setup. Open the `Manual Install` folder and follow [Manual_Install.txt](Manual_Install.txt).

When installing manually, use either the standalone or UE4SS method—never both at the same time.

### Linux and Steam Deck

Half Sword Enhancer can be used through Proton. Follow the community-maintained [Linux and Steam Deck guide](Linux-Guide.md).

## Getting started

1. Launch Half Sword normally. The mod interface is visible by default.
2. Press **Insert** to hide or show the interface.
3. Use the sidebar or search bar to find a feature.
4. Hover over an option for an explanation, or assign it a keyboard or mouse shortcut for quick access.

The menu key and other interface behavior can be changed under **Settings → Interface**.

Settings and presets are stored in:

```text
%APPDATA%\Half Sword Enhancer\
```

## Requirements and compatibility

- Half Sword or Half Sword Demo on Steam
- Windows 10 or 11, 64-bit
- DirectX 11 or DirectX 12
- [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vc14/vc_redist.x64.exe)

Linux and Steam Deck require Proton and the additional setup described in the [Linux guide](Linux-Guide.md).

## Troubleshooting

- **The menu does not appear:** confirm that you are using the latest experimental build, install the Visual C++ Redistributable, and run the launcher again.
- **The game fails to start after installation:** remove old HSE files using the uninstall instructions in [Manual_Install.txt](Manual_Install.txt), then perform a clean launcher installation.
- **The launcher cannot find the game:** in Steam, open **Half Sword → Manage → Browse local files**, then provide that folder when the launcher asks for it.
- **Still need help?** Join the [Discord community](https://discord.gg/x3KmgsQYMp) and include your HSE version, game version, and a short description of what happened.

## Uninstalling

Close the game and follow the uninstall section in [Manual_Install.txt](Manual_Install.txt). Your settings and presets are kept in `%APPDATA%\Half Sword Enhancer\`; remove that folder only if you also want to erase your personal configuration.

## Credits

- **The Ghost** — creator and developer
- **digitalyeti** — Linux guide contributor

## License

Half Sword Enhancer is **source-available, not open-source**. Personal use and private forks are permitted; public redistribution, shared modifications, and commercial use are not. See the full [license](LICENSE).

Half Sword Enhancer is an unofficial community mod and is not affiliated with the developers or publishers of Half Sword.

&copy; 2026 The Ghost
