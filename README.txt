Half Sword Enhancer installation

This package is for the full game. For the demo, see the note below.
The launcher is optional. After installation, start the game through Steam.

Close the game and extract the complete ZIP. Choose one installation method.

With the launcher

Run HSEnhancerLauncher.exe and follow the prompts. Run it again to update or
repair the mod. Do not run it from inside the ZIP.

Manual installation

1. Right-click Half Sword in Steam. Select Manage, then Browse local files.
   Open HalfSwordUE5\Binaries\Win64.
2. Copy "winmm.dll" and "HSEnhancer.dll" from "Manual Install" into Win64,
   next to HalfSwordUE5-Win64-Shipping.exe.

If you use UE4SS, take the files from "Manual Install" and replace step 2:
- Copy "HSEnhancer.dll" into Win64 and "main.dll" into
  Win64\ue4ss\Mods\HSEnhancer\dlls. Create the folders if needed.
- Make sure Win64\ue4ss\Mods\mods.txt contains "HSEnhancer : 1".
- Do not install HSE's "winmm.dll". Remove it if you previously installed
  HSE without UE4SS.

How to uninstall

These steps cover both installation methods. Deleting the ZIP or launcher
does not uninstall the mod.

1. Close the game and open its Win64 folder as described above.
2. Delete "HSEnhancer.dll" and remove the files for your installation:
   - If you installed without UE4SS, delete HSE's "winmm.dll".
   - If you use UE4SS, delete the "ue4ss\Mods\HSEnhancer" folder.
     Remove "HSEnhancer : 1" from "ue4ss\Mods\mods.txt".
   Remove only HSE files. Keep UE4SS and other mods.

Settings and presets remain in %APPDATA%\Half Sword Enhancer\.
Delete that folder only if you also want to erase your settings and presets.

Half Sword Demo

Download v0.5.2 and follow the manual installation steps in its README:
https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v0.5.2/HSEnhancer.zip
Do not use the launcher for the demo. It downloads newer, incompatible versions.

Linux and Steam Deck

For the full game through Proton, follow the included Linux-Guide.md.
