# Half Sword Enhancer on Linux and Steam Deck

**Credits:** Thanks to digitalyeti for the original Linux guide.

This guide is for the final version of **Half Sword**, not the demo.

## Installation

1. Download `HSEnhancer.zip` from the [latest release](https://github.com/lambor590/Half-Sword-Enhancer/releases/latest).
2. Open the ZIP and locate these files:
   - `Manual Install/winmm.dll`
   - `Manual Install/HSEnhancer.dll`
3. In Steam, right-click **Half Sword**, then select **Manage → Browse local files**.
4. Open `HalfSwordUE5/Binaries/Win64`.
5. Copy `winmm.dll` and `HSEnhancer.dll` into that folder, next to `HalfSwordUE5-Win64-Shipping.exe`.

## Proton launch option

In Steam, right-click **Half Sword**, open **Properties → General**, and enter this in **Launch Options**:

```text
WINEDLLOVERRIDES="winmm=n,b" %command%
```

Launch Half Sword normally through Steam. Press **Insert** in-game to open the Half Sword Enhancer menu.
