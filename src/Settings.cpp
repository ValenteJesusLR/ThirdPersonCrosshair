#include "Settings.h"

namespace
{
	constexpr auto kPath = "Data/F4SE/Plugins/ThirdPersonCrosshair.ini"sv;

	std::string_view Trim(std::string_view a_text)
	{
		constexpr auto blank = " \t\r\n"sv;
		const auto first = a_text.find_first_not_of(blank);
		if (first == std::string_view::npos) {
			return {};
		}
		const auto last = a_text.find_last_not_of(blank);
		return a_text.substr(first, last - first + 1);
	}

	std::string Lower(std::string_view a_text)
	{
		std::string out{ a_text };
		for (auto& c : out) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return out;
	}

	// Accepts decimal and 0x-prefixed hex.
	std::optional<std::int64_t> ParseInt(std::string_view a_text)
	{
		const auto text = std::string(Trim(a_text));
		if (text.empty()) {
			return std::nullopt;
		}
		try {
			std::size_t used = 0;
			const auto value = std::stoll(text, &used, 0);
			return used == text.size() ? std::optional{ value } : std::nullopt;
		} catch (...) {
			return std::nullopt;
		}
	}

	std::optional<double> ParseDouble(std::string_view a_text)
	{
		const auto text = std::string(Trim(a_text));
		if (text.empty()) {
			return std::nullopt;
		}
		try {
			std::size_t used = 0;
			const auto value = std::stod(text, &used);
			return used == text.size() ? std::optional{ value } : std::nullopt;
		} catch (...) {
			return std::nullopt;
		}
	}

	std::vector<std::string> ParseStrings(std::string_view a_text)
	{
		std::vector<std::string> out;
		while (!a_text.empty()) {
			const auto comma = a_text.find(',');
			if (const auto item = Trim(a_text.substr(0, comma)); !item.empty()) {
				out.emplace_back(item);
			}
			if (comma == std::string_view::npos) {
				break;
			}
			a_text.remove_prefix(comma + 1);
		}
		return out;
	}

	using Ini = std::unordered_map<std::string, std::string>;  // "section.key" -> value

	Ini ReadIni()
	{
		Ini ini;
		std::ifstream file{ std::string(kPath) };
		if (!file) {
			REX::WARN("settings: {} not found, using defaults", kPath);
			return ini;
		}

		std::string section;
		std::string line;
		while (std::getline(file, line)) {
			auto text = Trim(line);
			if (const auto comment = text.find(';'); comment != std::string_view::npos) {
				text = Trim(text.substr(0, comment));
			}
			if (text.empty()) {
				continue;
			}
			if (text.front() == '[' && text.back() == ']') {
				section = Lower(Trim(text.substr(1, text.size() - 2)));
				continue;
			}
			const auto equals = text.find('=');
			if (equals == std::string_view::npos) {
				continue;
			}
			ini[section + "." + Lower(Trim(text.substr(0, equals)))] = std::string(Trim(text.substr(equals + 1)));
		}
		return ini;
	}
}

Settings& Settings::Get()
{
	static Settings singleton;
	return singleton;
}

void Settings::Load()
{
	crosshairPaths = { "root.CenterGroup_mc.HUDCrosshair_mc"s, "root.HUDCrosshair_mc"s };

	const auto ini = ReadIni();
	const auto get = [&](std::string_view a_key) -> const std::string* {
		const auto it = ini.find(std::string(a_key));
		return it != ini.end() ? &it->second : nullptr;
	};
	const auto getBool = [&](std::string_view a_key, bool& a_out) {
		if (const auto* value = get(a_key)) {
			if (const auto number = ParseInt(*value)) {
				a_out = *number != 0;
			} else {
				a_out = Lower(*value) == "true";
			}
		}
	};
	const auto getDouble = [&](std::string_view a_key, double& a_out, double a_min, double a_max) {
		if (const auto* value = get(a_key)) {
			if (const auto number = ParseDouble(*value)) {
				a_out = std::clamp(*number, a_min, a_max);
			}
		}
	};
	const auto getColor = [&](std::string_view a_key, std::uint32_t& a_out) {
		if (const auto* value = get(a_key)) {
			if (const auto number = ParseInt(*value)) {
				a_out = static_cast<std::uint32_t>(*number) & 0xFFFFFF;
			}
		}
	};

	getBool("general.debug", debug);
	if (const auto* value = get("general.showwhen")) {
		showWhen = Lower(*value) == "drawn" ? ShowWhen::kDrawn : ShowWhen::kAiming;
	}
	if (const auto* value = get("general.style")) {
		style = Lower(*value) == "custom" ? Style::kCustom : Style::kVanilla;
	}
	getBool("general.hidevanilla", hideVanilla);
	getBool("general.followvanillavisibility", followVanillaVisibility);
	if (const auto* value = get("general.hidechild")) {
		hideChild = *value;
	}
	if (const auto* value = get("general.crosshairpath")) {
		if (auto paths = ParseStrings(*value); !paths.empty()) {
			crosshairPaths = std::move(paths);
		}
	}

	getDouble("crosshair.length", length, 0.0, 200.0);
	getDouble("crosshair.gap", gap, 0.0, 200.0);
	getDouble("crosshair.thickness", thickness, 0.5, 50.0);
	getBool("crosshair.centerdot", centerDot);
	getDouble("crosshair.dotsize", dotSize, 0.5, 50.0);
	getDouble("crosshair.offsetx", offsetX, -1000.0, 1000.0);
	getDouble("crosshair.offsety", offsetY, -1000.0, 1000.0);

	if (const auto* value = get("crosshair.color")) {
		useHudColor = Lower(*value) == "hud";
		if (!useHudColor) {
			getColor("crosshair.color", color);
		}
	}
	getDouble("crosshair.alpha", alpha, 0.0, 1.0);
	getDouble("crosshair.outline", outline, 0.0, 20.0);
	getColor("crosshair.outlinecolor", outlineColor);
	getDouble("crosshair.outlinealpha", outlineAlpha, 0.0, 1.0);

	REX::INFO("settings: debug={} show={} style={} hideVanilla={} hideChild='{}' follow={} length={} gap={} thickness={} dot={} color={}",
		debug, showWhen == ShowWhen::kDrawn ? "drawn" : "aiming", style == Style::kCustom ? "custom" : "vanilla", hideVanilla, hideChild, followVanillaVisibility,
		length, gap, thickness, centerDot, useHudColor ? "hud"s : std::format("{:06X}", color));
}
