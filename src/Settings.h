#pragma once

enum class ShowWhen : std::uint8_t
{
	kAiming,  // third person, gun raised to aim (where vanilla shows the dot)
	kDrawn    // third person, any time a weapon is out
};

enum class Style : std::uint8_t
{
	kVanilla,  // turn the game's own four crosshair lines back on
	kCustom    // draw our own crosshair from [Crosshair]
};

struct Settings
{
	bool debug{ false };

	ShowWhen showWhen{ ShowWhen::kAiming };
	Style    style{ Style::kVanilla };

	// Fade out the vanilla crosshair clip while ours is up.
	bool hideVanilla{ true };

	// When set, only this child of the vanilla clip is faded (the dot itself),
	// instead of the whole clip. The debug log lists the children.
	std::string hideChild;

	// Stay hidden while the game hides its own crosshair (crosshair turned off
	// in the settings, Survival, menus).
	bool followVanillaVisibility{ true };

	// Where to look for the vanilla crosshair in HUDMenu, in order.
	std::vector<std::string> crosshairPaths;

	// Looks, in HUD pixels.
	double length{ 10.0 };
	double gap{ 5.0 };
	double thickness{ 2.0 };
	bool   centerDot{ true };
	double dotSize{ 2.0 };
	double offsetX{ 0.0 };
	double offsetY{ 0.0 };

	// true: white, tinted to the HUD colour like the vanilla crosshair.
	bool          useHudColor{ true };
	std::uint32_t color{ 0xFFFFFF };
	double        alpha{ 1.0 };

	double        outline{ 1.0 };  // 0 turns it off
	std::uint32_t outlineColor{ 0x000000 };
	double        outlineAlpha{ 0.6 };

	[[nodiscard]] static Settings& Get();
	void Load();
};
