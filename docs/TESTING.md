# Checking it in game

Use this after a game update, with a different HUD mod, or when something looks wrong. These are the things only the game can confirm, in the order to check them. Set `Debug=1` in the INI (it ships off) and keep the log open: `Documents\My Games\Fallout4\F4SE\ThirdPersonCrosshair.log`.

## 1. It loads

After the main menu appears, the log should contain:

```
Third Person Crosshair loaded
crosshair: hooked HUDMenu
```

## 2. The aiming check

Load a save, draw a gun, switch to third person and aim. Each change logs a line like this:

```
debug: camera=8 weaponState=3 gunState=6 scope=false -> crosshair ON
```

- `camera=8` is third person, and `0` is first person.
- `gunState` should be **6** (sighted) or **8** (firing while sighted) while you aim.
- If aiming logs `crosshair off`, note the `gunState` value you see. The check lives in `Wanted()` in `src/Crosshair.cpp`. `ShowWhen=drawn` works around it.

## 3. Finding the vanilla crosshair

The first time you aim, look for one of these:

- `crosshair: found the vanilla crosshair at ...`: good.
- `vanilla crosshair not found in HUDMenu`: the clip has another name in this HUD. Search the HUD's swf in JPEXS for the crosshair clip, then put its path in `CrosshairPath`.

Once found, the log dumps the clip and its children once, as `debug: vanilla.<child> visible=... alpha=... frame=... label=...`. The lines are `CrosshairTicks_mc`; if a HUD mod has no clip by that name, the plugin logs a warning and falls back to `Style=custom`.

## 4. Look

While aiming in third person:

- [ ] The four vanilla lines show around the dot, in the HUD colour.
- [ ] They stay up while firing.
- [ ] With `CenterDot=0`, the dot is gone and only the lines remain.
- [ ] Letting go of aim returns to the normal hip-fire crosshair.

## 5. Style=custom

Experimental: in testing, the custom sprite did not appear on screen. With `Debug=1` the first time it shows, the log has two `debug: our sprite` lines: where it was added, whether it is on stage, and how big the drawing is. Those tell whether the sprite is off the display list, off screen, or empty.
## 6. Where it should stay hidden

- [ ] First person, hip fire and iron sights.
- [ ] Scoped weapons (the scope overlay opens).
- [ ] VATS, Pip-Boy, dialogue, workshop.
- [ ] Crosshair turned off in Settings > Display (with `FollowVanillaVisibility=1`).
- [ ] Power armor, third person aim: should show.

## 7. With FallUI - HUD

Repeat 3 and 4 with FallUI - HUD installed. If its crosshair lives elsewhere, the name search should still find it. Otherwise add its path to `CrosshairPath`, ahead of the vanilla one.
