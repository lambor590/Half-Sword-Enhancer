# Changelog

## [v0.5.1] - 2025-08-10

### Added
- Enhanced DLL injection error handling
- Implemented launcher auto-update functionality

### Important Notes
- The launcher auto-update functionality was accidentally omitted during the v0.5.0 rewrite
- While the mod auto-updates in v0.5.0, the launcher itself will not auto-update
- v0.5.1 restores the launcher auto-update functionality that existed in previous versions

## [v0.5.0] - 2025-08-08

# Update 0.5

This update changes completely how the launcher works and adds quality of life improvements to both the launcher and the mod.

The launcher has been rewritten completely for better error handling, security, performance and functionality.

## Launcher
* The mod is no longer embedded in the executable, but is downloaded from GitHub.
* Executable size went from `~900 KB` to `~200 KB`.
* With the rewrite, antiviruses flagging the launcher are no longer an issue.
* You can now drag & drop any version of the mod to the launcher executable for an instant install!
* There's now a configuration file for the launcher. You can now decide if you want the launcher to check for updates or not.
  * You will be asked whether you want to check for updates automatically or not the first time you run it.
* A first-run dialog has been added to explain a bit how the mod and launcher works to the user.

## Mod
Most of the changes are code base improvements.

### New
* Added a notification system
  * Whenever you toggle or run a mod feature, it will display a notification. Will be improved with future updates.
* Added tooltips
  * Now, when you hover on a feature or parameter, a tooltip will show explaining what it does
* Added `Clear Objects`
  * Removes dropped armor and weapons from the map
* Added macros
  * You can now bind multiple features to the same key
  * The key duplication warning will give you the option to share the key, showing which features are already binded to the same key.
* Added `Unlock UE Console` in the Settings
* Added `Snap to Ground` option on NPC & item spawners (enabled by default)
* Added `Graphics` section in the Settings
  * This allows you to customize the game's graphics options individually, as the game does not yet allow you to do so.
  * For example: you can play at low graphics settings with 100% render scale so the game doesn't look blurry.
* Mouse movement input is now disabled when the menu is visible. This prevents you from moving the camera while you are interacting with the mod interface.

## [v0.4.2] - 2025-06-04

* Reduced antivirus detections significantly
* Even more memory optimizations

## [v0.4.1] - 2025-06-03

# Update 0.4.1

* Fixed DirectX 12 crashes in fullscreen mode
* Removed `Adjust Body Tonus`
* Added `Custom Body Tonus` instead
  * Option to set a custom body tonus multiplier
  * Option not to weaken
* Added `Ragdoll`
* Added `Enemy Ragdoll`
* Added `Enemy Infinite Consciousness`

## [v0.4.0] - 2025-06-02

# Update 0.4

## Launcher
* Check if it has write permissions before applying an update
  * This is probably why the auto-updater does not work for some people
* Significant memory & processing optimizations

## Spawner rewrite
* The spawner is now much more stable, robust, stronger, faster, smarter and BETTER.

Here's some details:
* Abyss items now spawn.
* You can now spawn abyss props, such as chests and coffins!
* Assets are now loaded by the spawner.
* Added an internal queue so it only spawns the desired actors when the game can.
* The spawner now runs in the game thread.
  * This fixes ALL those random crashes that happened when you spawned some items (e.g. game crashing when you only spawned two swords).
* Added `Long Falchion T3`.
* Removed `Cuirass A T1` as it no longer exists.

## Mod
* Fixed menu toggle key not working on Linux.
* Updated Windows SDK.
* Raised the maximum strength multiplier to 10.
* Added a Linux guide (thanks to digitalyeti).
* Now all player modifiers in the arena are re-applied constantly instead of just the start of a fight.
* Added mouse buttons support.
* Added `Destroy All Willies`.
  * Option to only destroy dead willies.
  * Option to disintegrate.
* Added `Clear Blood`.
  * You can set the amount of blood to be cleared.
* Added `Toggle Game Paused`.
  * This can be used in Photo Mode to unpause the game.
* Added `Infinite Consciousness`.
  * You can't be knocked out.
* Added `No Kick Cooldown`.
* Added `No Pain`.
  * This could be taken as a semi-invulnerability.
  * You can only die if your head/neck is damaged enough.
* Added `Enemy No Pain`.
  * Same as `No Pain` but it's only applied to other Willies.
* Revamped user interface.
* Added key conflict warning to choose to either replace, keep or share keys

## [v0.3.0] - 2025-04-20

# Update 0.3

## Launcher
* Improved error handling significantly, hopefully you will now get a detailed error message if anything fails.
* Optimized update process.
* Improved injection process.

## Mod
* Fixed features being instantly bound to `Unknown`.
* Fixed all non-working features, including invulnerability.
* Optimized crash prevention system a bit.
* Added `Hands Rigidity Multiplier` parameter in `Strength Multiplier`.
  * This makes your punches do more damage.
* Added all bosses and new items to NPC & item spawner.
* General performance improvements.
* Reorganized some item spawner categories.
* Added `Possess Nearest Willie`.
  * Note that you still feel your character damage, and if your possessed Willie dies, it will be as if you had died.
  * AI is automatically disabled and re-enabled.
  * Yes, you can literally become a boss.
* Added `Kill All Enemies`.
  * You can choose between kill or snap neck.
  * You can adjust the radius.
* Added `Toggle Enemy AI`.
  * You can adjust the radius.
* The mod now weights about 10% less.
* Added `Bodyguard` option in NPC spawner.

## Introducing a Launcher Alternative
* There are now two ways of using the mod
  * You can use the launcher
  * Or you can drop two files in the game folder, and that's it.

Works exactly the same as the launcher version.

* This new way of using the mod does not have auto-updates, but it has other benefits
  * Antiviruses don't flag it as malware.
  * You just launch the game like normal and the mod will be injected automatically.

## Introducing Persistent Mod Features
Are you tired of applying invulnerability every 2 minutes?
You don't know if a feature is active?

Features now persist across game sessions and fights.

## [v0.2.1] - 2025-04-04

* Added support for the new Half Sword demo.

## [v0.2.0] - 2025-03-27

* Improved the launcher stability.
* Reduced the program weight significantly.
* Reduced antiviruses flagging the program as a malware even more.

## [v0.1.1] - 2025-03-22

* Minor fixes.

## [v0.1.0] - 2025-03-22

* Initial release of the mod launcher.

---

## Release Notes Template

Para crear una nueva release, añade una sección siguiendo este formato:

```markdown
## [vX.X.X] - YYYY-MM-DD

### Added
- Nueva funcionalidad añadida

### Changed
- Funcionalidad existente modificada

### Fixed
- Bugs corregidos

### Removed
- Funcionalidad eliminada
```

### Categorías disponibles:
- **Added**: Nueva funcionalidad
- **Changed**: Cambios en funcionalidad existente  
- **Deprecated**: Funcionalidad que será eliminada
- **Removed**: Funcionalidad eliminada
- **Fixed**: Corrección de bugs
- **Security**: Cambios de seguridad