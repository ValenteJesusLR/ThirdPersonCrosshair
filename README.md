# Third Person Crosshair

A Fallout 4 F4SE plugin. When you aim a gun in third person, vanilla hides the crosshair lines and leaves only a small dot. This plugin brings the game's own four crosshair lines back while you aim, the same lines you see when firing from the hip, with the same colour and spread.

- **Standalone.** No FallUI and no replaced `HUDMenu.swf`. The plugin works on whatever HUD is loaded, so it sits alongside vanilla, FallUI - HUD, DEF_UI and similar mods.
- Keeps the vanilla centre dot by default (`CenterDot=0` removes it).
- Stays out of the way in first person, in scopes, in VATS, and whenever the game hides its own crosshair.

## Requirements

- Fallout 4, current Steam version (1.11.x)
- [F4SE](https://f4se.silverlock.org/) and [Address Library for F4SE Plugins](https://www.nexusmods.com/fallout4/mods/47327), both for your runtime

## Installing

Install with Vortex or MO2 from an archive laid out like this, or copy the files into `Data` by hand:

```
F4SE/Plugins/ThirdPersonCrosshair.dll
F4SE/Plugins/ThirdPersonCrosshair.ini   (from dist/)
```

Start the game through `f4se_loader.exe`.

## Settings

All in `F4SE/Plugins/ThirdPersonCrosshair.ini`:

| Setting | Default | What it does |
|---|---|---|
| `ShowWhen` | `aiming` | `aiming`: only while aiming in third person. `drawn`: whenever a weapon is out in third person. |
| `Style` | `vanilla` | `vanilla`: bring back the game's own lines. `custom`: draw a crosshair from `[Crosshair]` (experimental, see below). |
| `CenterDot` | `1` | Keep the vanilla aiming dot in the middle of the lines. |
| `FollowVanillaVisibility` | `1` | Stay hidden whenever the game hides its crosshair (crosshair off in Settings > Display, Survival, menus). |
| `CrosshairPath` | vanilla path | Where the crosshair lives in HUDMenu, for HUDs that move it. |
| `Debug` | `0` | Log aiming state changes and the crosshair's layout. |

## How it works

The plugin wraps `HUDMenu::AdvanceMovie` (vfunc 4 of the HUDMenu vtable). Every frame, after the HUD's own scripts have run:

1. **Is the player aiming in third person?** The camera state must be `k3rdPerson`, the weapon must be drawn, the actor's `gunState` must be `kSighted` or `kFireSighted`, and `ScopeMenu` must not be open. These are plain reads: no references are taken on engine objects from the HUD's thread, and the scope is tracked by a menu open/close listener.
2. **Where is the vanilla crosshair?** `root.CenterGroup_mc.HUDCrosshair_mc`, or the paths set in the INI, with a search by name as a fallback. Its lines are `CrosshairBase_mc.CrosshairTicks_mc` and the aiming dot is `CrosshairBase_mc.CrosshairClips_mc.Dot_Dot`.
3. **Show the lines.** The aiming state hides `CrosshairTicks_mc`; the plugin sets it visible again. Because this runs after the HUD scripts every frame, the lines are what gets drawn.

`Style=custom` instead draws a crosshair of its own from the `[Crosshair]` settings. That mode is experimental: in testing, its sprite did not appear on screen. With `Debug=1` it logs where the sprite ended up.

| File | Role |
|---|---|
| `src/Crosshair.*` | The HUD hook, the aiming check, finding the vanilla clip, showing the lines (or drawing the custom one) |
| `src/Settings.*` | INI reader |
| `src/main.cpp` | Plugin entry, installs the hook once game data is ready |

## Building

Prerequisites: Visual Studio 2022 or newer with the *Desktop development with C++* workload, [xmake](https://xmake.io) 3.0 or newer, and git.

```powershell
git clone --recursive https://github.com/ValenteJesusLR/ThirdPersonCrosshair.git
cd ThirdPersonCrosshair
xmake build
```

If you cloned without `--recursive`, run `git submodule update --init --recursive` first.

The DLL lands in `build/windows/x64/releasedbg/`. To have xmake install straight into your mod manager, set `XSE_FO4_MODS_PATH` to its mods folder (the Vortex staging folder or the MO2 `mods` folder) before configuring, or `XSE_FO4_GAME_PATH` to install into the game's `Data` folder. With Vortex, deploy again after each build.

## Troubleshooting

Set `Debug=1` and check `Documents\My Games\Fallout4\F4SE\ThirdPersonCrosshair.log`. [docs/TESTING.md](docs/TESTING.md) lists what to look for.

## Credits

Built on [CommonLibF4](https://github.com/libxse/commonlibf4).
