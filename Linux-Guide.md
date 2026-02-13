## Half Sword Enhancer & Mouse Fix for Linux Guide
**Credits:** Thanks to digitalyeti for writing the guide.

This guide details installing the Half Sword Enhancer mod on Linux / SteamDeck and also includes a fix for the mouse cursor escaping the game window.

**Installing the Half Sword Enhancer**

1. **Download the zip:** [HSEnhancer.zip](https://github.com/lambor590/Half-Sword-Enhancer/releases/latest/download/HSEnhancer.zip)
2. **Extract DLLs:** Extract the following two DLL files from the downloaded ZIP archive:
* `winmm.dll`
* `HSEnhancer.dll`
3. **Locate Game Directory:** Navigate to the game's installation folder.

On desktop Linux the default location is: `/home/username/.steam/steam/steamapps/common/Half Sword Demo/HalfSwordUE5/Binaries/Win64`
*(Replace `username` with your actual username.)*

For SteamDeck the location is: `/home/deck/.steam/steam/steamapps/common/Half Sword Demo/HalfSwordUE5/Binaries/Win64`

3. **Place DLLs:** Copy both `winmm.dll` and `HSEnhancer.dll` into the `Binaries/Win64` folder. This should be next to the `HalfSwordUE5-Win64-Shipping.exe` executable.

**Configuring Steam Launch Options**

The next step depends on whether you want to use the mouse fix, the mod, or both.

**A. Easiest Method - Basic Install (No Mouse Fix)**

This method provides a universal way to install the mod, but does not address the mouse escape issues on desktop.

1. **Open Steam Library:** Right-click on "Half Sword Demo" in your Steam Library and select "Properties".
2. **Launch Options:** In the "General" tab, find the "Launch Options" field.
3. **Enter Command:** Add the following command to the Launch Options field:
`WINEDLLOVERRIDES="winmm,HSEnhancer=n" %command%`. This sets the environment variable to tell Wine / Proton that it should load our DLLs in the prefix. The `=n` indicates native-mode, which tells Wine to look in exe directory. `%command%` is where steam puts the game executable.

**B. Hardest Method - Fix Mouse Escape (With Mod)**

This method combines both the mod and the mouse fix. It requires extra steps to configure Wine outside of steam.

1. **Install Protontricks:** If you don't have it, install Protontricks. Instructions can be found online for your specific Linux distribution.
2. **Run Protontricks GUI:** Execute `protontricks --gui` in your terminal.
3. **Select Game:** Find "Half Sword Demo" in the list and click "OK".
4. **Select Wineprefix:** Select "Select the default wineprefix".
5. **Run Winecfg:** Select "Run winecfg".
6. **Libraries Tab:** In the "Libraries" tab, enter the following (without the `.dll` extension) in the "New override for library" field:
* `winmm`
* `HSEnhancer`
7. **Verify Override:** The list should now display `winmm` and `HSEnhancer` with a setting of `n,b` (native first, then built-in). This is the desired setting.
8. **Exit:** Exit all Protontricks windows.
9. **Install Gamescope:** If you are not on SteamDeck, you wont have gamescope automatically installed. **NOTE:** The gamescope package is currently bugged and requires recent patches to capture the mouse. More details in final section. 

10. **Steam Launch Options:** Now, add the following to the Steam Launch Options to:
`gamescope -w 1920 -h 1080 -r 144 --force-grab-cursor -- %command%`
* **Resolution:** Adjust `-w` (width) and `-h` (height) to match your monitor's resolution.
* **Refresh Rate:** Set `-r` to your monitor's refresh rate (e.g., 60, 144). Use 60 if unsure.
* **Fix:** Gamescope's `--force-grab-cursor` captures the mouse cursor and locks it to the game window. See the below section for troubleshooting.

**Cursor Escape Fix Issue & Workarounds**

Recent versions of Gamescope have a bug where `--force-grab-cursor` doesn't function correctly. These are the versions used by most package managers, so you have a few options in this case:

* **Downgrade Gamescope:** Downgrade to an earlier version of Gamescope where the grab cursor function works.
* **Build From Source:** The latest master branch has a fix for this issue. Build the latest version of Gamescope from source to get the fix.
* **Wait:** The fix will be pushed to package managers eventually. You can wait until the fix lands, but it could be a while.
