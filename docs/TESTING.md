# First in-game test

The code was written without a game to run it on. These are the things only the game can confirm, in the order to check them. Set `Debug=1` in the INI (it ships off) and keep the log open: `Documents\My Games\Fallout4\F4SE\ThirdPersonCrosshair.log`.

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
- If aiming logs `crosshair off`, note the `gunState` value you see and report it. The check lives in `Wanted()` in `src/Crosshair.cpp`. `ShowWhen=drawn` is a workaround until then.

## 3. Finding the vanilla crosshair

The first time you aim, look for one of these:

- `crosshair: found the vanilla crosshair at ...`: good.
- `vanilla crosshair not found in HUDMenu`: the clip has another name in this HUD. Search the HUD's swf in JPEXS for the crosshair clip, then put its path in `CrosshairPath`.

Once found, the log dumps the clip and its children as `debug: vanilla.<child> visible=... alpha=... frame=... label=...`. The dump repeats whenever the aiming state changes.

## 4. Look

While aiming in third person:

- [ ] Our crosshair shows, centred where you shoot. If it is off-centre, compare `x=`/`y=` with `bounds=` in the dump and use `OffsetX`/`OffsetY`.
- [ ] The vanilla dot is gone.
- [ ] Colour matches the HUD. If it stays white while the HUD is tinted, the tint sits on a child of the clip: check which dump lines show a `tint=` other than `(1.00,1.00,1.00 +0,0,0)`, or set `Color=` to an RGB value.
- [ ] Letting go of aim hides ours and brings the vanilla reticle back.

## 5. Only the dot, not the whole reticle

`HideVanilla` fades the whole vanilla clip. If something you want to keep lives in that clip, such as a hit marker from another mod, find the dump line that is `visible=true` only while aiming. That is the dot. Put its name in `HideChild`.

## 6. Where it should stay hidden

- [ ] First person, hip fire and iron sights.
- [ ] Scoped weapons (the scope overlay opens).
- [ ] VATS, Pip-Boy, dialogue, workshop.
- [ ] Crosshair turned off in Settings > Display (with `FollowVanillaVisibility=1`).
- [ ] Power armor, third person aim: should show.

## 7. With FallUI - HUD

Repeat 3 and 4 with FallUI - HUD installed. If its crosshair lives elsewhere, the name search should still find it. Otherwise add its path to `CrosshairPath`, ahead of the vanilla one.
