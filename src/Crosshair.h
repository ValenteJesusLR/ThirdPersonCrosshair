#pragma once

// The crosshair itself, drawn into the HUD movie at run time.
//
// Nothing here replaces a menu file. Every frame, after HUDMenu advances:
//   1. Decide whether the player is aiming a weapon in third person.
//   2. Find the vanilla crosshair clip (root.CenterGroup_mc.HUDCrosshair_mc,
//      or whatever the INI names), which also exists under FallUI - HUD.
//   3. Keep a sprite of our own next to it, at the same position, and show
//      it while fading the vanilla dot out. Hide it again when not aiming.
//
// Everything runs on the thread that advances the HUD, so Scaleform is safe
// to touch without queuing tasks.
namespace Crosshair
{
	// Patches HUDMenu::AdvanceMovie.
	void Install();
}
