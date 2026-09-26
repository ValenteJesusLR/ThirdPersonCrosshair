#include "Crosshair.h"

#include "Settings.h"

namespace Crosshair
{
	namespace
	{
		using Value = Scaleform::GFx::Value;

		constexpr auto kLayerName = "TPC_Crosshair";

		// What we hold on to between frames. Never destroyed: releasing a
		// Scaleform value after its movie is gone would crash. When the HUD
		// movie changes, the old state is abandoned rather than freed.
		struct State
		{
			Scaleform::GFx::Movie* movie{ nullptr };
			Value                  vanilla;   // the vanilla crosshair clip
			Value                  faded;     // what we fade: vanilla, or its HideChild
			Value                  layer;     // our sprite: outline + fill
			Value                  fill;      // the part that takes the HUD tint
			Value                  ticks;     // vanilla's four lines: ...CrosshairBase_mc.CrosshairTicks_mc
			Value                  clips;     // vanilla's centre shapes, the aiming dot among them
			double                 fadedAlpha{ 1.0 };
			bool                   isFaded{ false };
			bool                   shown{ false };
			bool                   found{ false };
			bool                   custom{ false };     // drawing our own, not reusing the ticks
			bool                   diagnosed{ false };  // logged our sprite once
			bool                   gaveUp{ false };  // logged "not found" once
		};
		State* g_state = new State();

		// Debug: last logged player state, to log only changes.
		std::int64_t g_lastSignature{ -1 };

		// Set by the menu event, read every frame.
		std::atomic_bool g_scopeOpen{ false };

		class MenuWatcher final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (std::string_view{ a_event.menuName.c_str() } == "ScopeMenu"sv) {
					g_scopeOpen = a_event.opening;
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		std::optional<double> ToNumber(const Value& a_value)
		{
			if (a_value.IsNumber()) {
				return a_value.GetNumber();
			}
			if (a_value.IsInt()) {
				return static_cast<double>(a_value.GetInt());
			}
			if (a_value.IsUInt()) {
				return static_cast<double>(a_value.GetUInt());
			}
			return std::nullopt;
		}

		std::optional<double> NumberMember(const Value& a_object, std::string_view a_name)
		{
			Value value;
			if (!a_object.IsObject() || !a_object.GetMember(a_name, &value)) {
				return std::nullopt;
			}
			return ToNumber(value);
		}

		std::string StringMember(const Value& a_object, std::string_view a_name)
		{
			Value value;
			return a_object.IsObject() && a_object.GetMember(a_name, &value) && value.IsString() ? value.GetString() : "";
		}

		bool IsHidden(const Value& a_clip)
		{
			Value visible;
			return a_clip.GetMember("visible", &visible) && visible.IsBoolean() && !visible.GetBoolean();
		}

		// A clip is on screen when neither it nor anything above it is hidden.
		bool OnScreen(const Value& a_clip)
		{
			Value node = a_clip;
			for (int depth = 0; depth < 16 && node.IsDisplayObject(); ++depth) {
				if (IsHidden(node)) {
					return false;
				}
				Value parent;
				if (!node.GetMember("parent", &parent)) {
					return false;
				}
				node = parent;
			}
			return true;
		}

		bool FindByName(Value a_node, std::string_view a_name, int a_depth, Value& a_out)
		{
			const auto count = NumberMember(a_node, "numChildren").value_or(0);
			for (std::int32_t i = 0; i < static_cast<std::int32_t>(count); ++i) {
				Value index{ i };
				Value child;
				if (!a_node.Invoke("getChildAt", &child, &index, 1) || !child.IsDisplayObject()) {
					continue;
				}
				if (StringMember(child, "name") == a_name) {
					a_out = child;
					return true;
				}
				if (a_depth > 0 && FindByName(child, a_name, a_depth - 1, a_out)) {
					return true;
				}
			}
			return false;
		}

		// ---- game state ----------------------------------------------------

		struct PlayerView
		{
			std::int32_t camera{ -1 };
			std::int32_t weaponState{ -1 };
			std::int32_t gunState{ -1 };
			bool         scope{ false };
		};

		PlayerView Look()
		{
			PlayerView view;
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (!player || !camera) {
				return view;
			}
			// Read only, from the HUD's thread: no smart pointer copies (they
			// would take references on the camera's state from under the main
			// thread) and no menu lookups.
			if (const auto* state = camera->currentState.get()) {
				view.camera = static_cast<std::int32_t>(state->id.get());
			}
			// Signed bitfields: kFireSighted (8) in four bits reads back as -8.
			view.weaponState = static_cast<std::int32_t>(player->weaponState) & 0x7;
			view.gunState = static_cast<std::int32_t>(player->gunState) & 0xF;
			view.scope = g_scopeOpen;
			return view;
		}

		bool Wanted(const PlayerView& a_view)
		{
			if (a_view.camera != static_cast<std::int32_t>(RE::CameraState::k3rdPerson) || a_view.scope) {
				return false;
			}
			if (a_view.weaponState < static_cast<std::int32_t>(RE::WEAPON_STATE::kDrawn)) {
				return false;
			}
			if (Settings::Get().showWhen == ShowWhen::kDrawn) {
				return true;
			}
			return a_view.gunState == static_cast<std::int32_t>(RE::GUN_STATE::kSighted) ||
			       a_view.gunState == static_cast<std::int32_t>(RE::GUN_STATE::kFireSighted);
		}

		// ---- debug ---------------------------------------------------------

		void DumpClip(Value a_clip, const std::string& a_path, int a_depth)
		{
			Value bounds;
			Value parent;
			a_clip.GetMember("parent", &parent);
			a_clip.Invoke("getBounds", &bounds, &parent, 1);

			Value transform, tint;
			std::string tintText;
			if (a_clip.GetMember("transform", &transform) && transform.GetMember("colorTransform", &tint)) {
				tintText = std::format(" tint=({:.2f},{:.2f},{:.2f} +{},{},{})",
					NumberMember(tint, "redMultiplier").value_or(-1), NumberMember(tint, "greenMultiplier").value_or(-1),
					NumberMember(tint, "blueMultiplier").value_or(-1), NumberMember(tint, "redOffset").value_or(0),
					NumberMember(tint, "greenOffset").value_or(0), NumberMember(tint, "blueOffset").value_or(0));
			}

			REX::INFO("debug: {} visible={} alpha={:.2f} x={:.1f} y={:.1f} bounds=({:.1f},{:.1f} {:.1f}x{:.1f}) frame={} label='{}'{}",
				a_path, !IsHidden(a_clip), NumberMember(a_clip, "alpha").value_or(-1),
				NumberMember(a_clip, "x").value_or(0), NumberMember(a_clip, "y").value_or(0),
				NumberMember(bounds, "x").value_or(0), NumberMember(bounds, "y").value_or(0),
				NumberMember(bounds, "width").value_or(0), NumberMember(bounds, "height").value_or(0),
				NumberMember(a_clip, "currentFrame").value_or(-1), StringMember(a_clip, "currentLabel"), tintText);

			if (a_depth <= 0) {
				return;
			}
			const auto count = NumberMember(a_clip, "numChildren").value_or(0);
			for (std::int32_t i = 0; i < static_cast<std::int32_t>(count); ++i) {
				Value index{ i };
				Value child;
				if (a_clip.Invoke("getChildAt", &child, &index, 1) && child.IsDisplayObject()) {
					const auto name = StringMember(child, "name");
					if (name != kLayerName) {
						DumpClip(child, a_path + "." + name, a_depth - 1);
					}
				}
			}
		}

		void LogChanges(const PlayerView& a_view, bool a_wanted)
		{
			const std::int64_t signature = (static_cast<std::int64_t>(a_view.camera & 0xFF) << 24) |
			                               (static_cast<std::int64_t>(a_view.weaponState & 0xFF) << 16) |
			                               (static_cast<std::int64_t>(a_view.gunState & 0xFF) << 8) |
			                               (a_view.scope ? 2 : 0) | (a_wanted ? 1 : 0);
			if (signature == g_lastSignature) {
				return;
			}
			g_lastSignature = signature;
			REX::INFO("debug: camera={} weaponState={} gunState={} scope={} -> crosshair {}",
				a_view.camera, a_view.weaponState, a_view.gunState, a_view.scope, a_wanted ? "ON" : "off");
		}

		// Debug: where our own sprite ended up, the first time it shows.
		void DumpLayer(State& a_state)
		{
			Value parent, stage, bounds, fillBounds;
			a_state.layer.GetMember("parent", &parent);
			const bool onStage = a_state.layer.GetMember("stage", &stage) && stage.IsDisplayObject();
			if (onStage) {
				a_state.layer.Invoke("getBounds", &bounds, &stage, 1);
			}
			a_state.fill.Invoke("getBounds", &fillBounds, &a_state.layer, 1);
			Value index;
			parent.Invoke("getChildIndex", &index, &a_state.layer, 1);

			REX::INFO("debug: our sprite: parent='{}' index={} of {} onStage={} visible={} alpha={:.2f} x={:.1f} y={:.1f}",
				StringMember(parent, "name"), ToNumber(index).value_or(-1), NumberMember(parent, "numChildren").value_or(-1),
				onStage, !IsHidden(a_state.layer), NumberMember(a_state.layer, "alpha").value_or(-1),
				NumberMember(a_state.layer, "x").value_or(0), NumberMember(a_state.layer, "y").value_or(0));
			REX::INFO("debug: our sprite: on screen at ({:.1f},{:.1f} {:.1f}x{:.1f}), drawing is {:.1f}x{:.1f}",
				NumberMember(bounds, "x").value_or(0), NumberMember(bounds, "y").value_or(0),
				NumberMember(bounds, "width").value_or(0), NumberMember(bounds, "height").value_or(0),
				NumberMember(fillBounds, "width").value_or(0), NumberMember(fillBounds, "height").value_or(0));
		}

		// ---- drawing -------------------------------------------------------

		void Rect(Value& a_graphics, double a_x, double a_y, double a_w, double a_h)
		{
			std::array<Value, 4> args{ Value(a_x), Value(a_y), Value(a_w), Value(a_h) };
			a_graphics.Invoke("drawRect", nullptr, args.data(), args.size());
		}

		// Draws the shape centred on 0,0, grown by a_grow on every side.
		void Shape(Value& a_sprite, std::uint32_t a_color, double a_grow)
		{
			const auto& s = Settings::Get();
			Value graphics;
			if (!a_sprite.GetMember("graphics", &graphics)) {
				return;
			}
			graphics.Invoke("clear");
			std::array<Value, 2> fill{ Value(a_color), Value(1.0) };
			graphics.Invoke("beginFill", nullptr, fill.data(), fill.size());

			const double t = s.thickness + a_grow * 2;
			const double half = t / 2;
			if (s.length > 0) {
				const double len = s.length + a_grow * 2;
				const double gap = s.gap - a_grow;
				Rect(graphics, -half, -gap - len, t, len);  // up
				Rect(graphics, -half, gap, t, len);         // down
				Rect(graphics, -gap - len, -half, len, t);  // left
				Rect(graphics, gap, -half, len, t);         // right
			}
			if (s.centerDot) {
				const double d = s.dotSize + a_grow * 2;
				Rect(graphics, -d / 2, -d / 2, d, d);
			}
			graphics.Invoke("endFill");
		}

		bool MakeSprite(Scaleform::GFx::Movie& a_movie, Value& a_out)
		{
			a_movie.CreateObject(&a_out, "flash.display.Sprite");
			if (!a_out.IsDisplayObject()) {
				return false;
			}
			a_out.SetMember("mouseEnabled", Value(false));
			a_out.SetMember("mouseChildren", Value(false));
			return true;
		}

		// Builds our sprite and puts it just above the vanilla clip.
		bool BuildLayer(State& a_state)
		{
			const auto& s = Settings::Get();
			Value parent;
			if (!a_state.vanilla.GetMember("parent", &parent) || !parent.IsDisplayObject()) {
				return false;
			}

			Value name{ kLayerName };
			Value existing;
			if (parent.Invoke("getChildByName", &existing, &name, 1) && existing.IsDisplayObject()) {
				parent.Invoke("removeChild", nullptr, &existing, 1);  // left over from a previous state
			}

			Value outline;
			if (!MakeSprite(*a_state.movie, a_state.layer) || !MakeSprite(*a_state.movie, outline) ||
				!MakeSprite(*a_state.movie, a_state.fill)) {
				REX::WARN("crosshair: could not create sprites");
				return false;
			}
			a_state.layer.SetMember("name", name);

			if (s.outline > 0) {
				Shape(outline, s.outlineColor, s.outline);
				outline.SetMember("alpha", Value(s.outlineAlpha));
				a_state.layer.Invoke("addChild", nullptr, &outline, 1);
			}
			// White takes the HUD tint exactly.
			Shape(a_state.fill, s.useHudColor ? 0xFFFFFFu : s.color, 0.0);
			a_state.layer.Invoke("addChild", nullptr, &a_state.fill, 1);
			a_state.layer.SetMember("alpha", Value(s.alpha));
			a_state.layer.SetMember("visible", Value(false));

			Value index;
			const auto at = parent.Invoke("getChildIndex", &index, &a_state.vanilla, 1) ? ToNumber(index) : std::nullopt;
			if (at) {
				std::array<Value, 2> args{ a_state.layer, Value(static_cast<std::int32_t>(*at) + 1) };
				parent.Invoke("addChildAt", nullptr, args.data(), args.size());
			} else {
				parent.Invoke("addChild", nullptr, &a_state.layer, 1);
			}
			return true;
		}

		// Gives our fill the vanilla crosshair's colour, without its alpha
		// (which we may have set to zero ourselves).
		void CopyTint(State& a_state)
		{
			Value from, fromTint, to, toTint;
			if (!a_state.vanilla.GetMember("transform", &from) || !from.GetMember("colorTransform", &fromTint) ||
				!a_state.fill.GetMember("transform", &to) || !to.GetMember("colorTransform", &toTint)) {
				return;
			}
			for (const auto* member : { "redMultiplier", "greenMultiplier", "blueMultiplier", "redOffset", "greenOffset", "blueOffset" }) {
				Value value;
				if (fromTint.GetMember(member, &value)) {
					toTint.SetMember(member, value);
				}
			}
			to.SetMember("colorTransform", toTint);
		}

		// ---- the vanilla clip ----------------------------------------------

		bool FindVanilla(State& a_state)
		{
			const auto& s = Settings::Get();
			for (const auto& path : s.crosshairPaths) {
				if (a_state.movie->GetVariable(&a_state.vanilla, path.c_str()) && a_state.vanilla.IsDisplayObject()) {
					REX::INFO("crosshair: found the vanilla crosshair at {}", path);
					return true;
				}
			}
			// Not where expected: look for the same clip name anywhere.
			Value root;
			if (a_state.movie->GetVariable(&root, "root")) {
				for (const auto& path : s.crosshairPaths) {
					const auto dot = path.rfind('.');
					const auto name = dot == std::string::npos ? path : path.substr(dot + 1);
					if (FindByName(root, name, 6, a_state.vanilla)) {
						REX::INFO("crosshair: found the vanilla crosshair by searching for {}", name);
						return true;
					}
				}
			}
			return false;
		}

		void ChooseFaded(State& a_state)
		{
			const auto& child = Settings::Get().hideChild;
			a_state.faded = a_state.vanilla;
			if (child.empty()) {
				return;
			}
			Value found;
			if (FindByName(a_state.vanilla, child, 4, found)) {
				a_state.faded = found;
			} else {
				REX::WARN("crosshair: HideChild '{}' not found under the vanilla crosshair, fading all of it", child);
			}
		}

		void Fade(State& a_state, bool a_fade)
		{
			if (a_fade) {
				if (!a_state.isFaded) {
					a_state.fadedAlpha = NumberMember(a_state.faded, "alpha").value_or(1.0);
					a_state.isFaded = true;
				}
				// Every frame: the HUD's own script may set it back.
				a_state.faded.SetMember("alpha", Value(0.0));
			} else if (a_state.isFaded) {
				a_state.faded.SetMember("alpha", Value(a_state.fadedAlpha));
				a_state.isFaded = false;
			}
		}

		// ---- per frame -----------------------------------------------------

		void Update(RE::IMenu& a_hud)
		{
			const auto& s = Settings::Get();
			auto*       movie = a_hud.uiMovie.get();
			if (!movie) {
				return;
			}
			if (movie != g_state->movie) {
				if (g_state->movie) {
					REX::INFO("crosshair: the HUD movie changed, starting over");
					g_state = new State();  // the old one is abandoned on purpose
				}
				g_state->movie = movie;
			}
			auto& state = *g_state;

			const auto view = Look();
			const bool wanted = Wanted(view);
			if (s.debug) {
				LogChanges(view, wanted);
			}

			if (!state.found) {
				if (state.gaveUp) {
					return;
				}
				if (!wanted) {
					return;  // look for it the first time it matters
				}
				if (!FindVanilla(state)) {
					REX::WARN("crosshair: vanilla crosshair not found in HUDMenu; set CrosshairPath in the INI");
					state.gaveUp = true;
					return;
				}
				FindByName(state.vanilla, "CrosshairTicks_mc", 3, state.ticks);
				FindByName(state.vanilla, "CrosshairClips_mc", 3, state.clips);

				state.custom = s.style == Style::kCustom;
				if (!state.custom && !state.ticks.IsDisplayObject()) {
					REX::WARN("crosshair: this HUD has no CrosshairTicks_mc, drawing our own crosshair instead");
					state.custom = true;
				}
				if (state.custom) {
					ChooseFaded(state);
					if (!BuildLayer(state)) {
						state.gaveUp = true;
						return;
					}
				} else {
					state.faded = state.clips;  // only faded when CenterDot=0
				}
				state.found = true;
				if (s.debug) {
					DumpClip(state.vanilla, "vanilla", 3);
				}
			}

			const bool show = wanted && (!s.followVanillaVisibility || OnScreen(state.vanilla));

			if (!state.custom) {
				// The aiming state hides the ticks and shows the dot. Every
				// frame, after the HUD's script, put the ticks back.
				if (show) {
					state.ticks.SetMember("visible", Value(true));
				}
				if (state.faded.IsDisplayObject()) {
					Fade(state, show && !s.centerDot);
				}
				return;
			}

			if (show) {
				state.layer.SetMember("x", Value(NumberMember(state.vanilla, "x").value_or(0) + s.offsetX));
				state.layer.SetMember("y", Value(NumberMember(state.vanilla, "y").value_or(0) + s.offsetY));
				if (s.useHudColor) {
					CopyTint(state);
				}
			}
			if (show != state.shown) {
				state.layer.SetMember("visible", Value(show));
				state.shown = show;
				if (show && s.debug && !state.diagnosed) {
					state.diagnosed = true;
					DumpLayer(state);
				}
			}
			Fade(state, show && s.hideVanilla);
		}

		// ---- the hook ------------------------------------------------------

		using AdvanceFn = void(RE::IMenu*, float, std::uint64_t);
		AdvanceFn* g_advance{ nullptr };

		void Advance(RE::IMenu* a_self, float a_timeDelta, std::uint64_t a_time)
		{
			g_advance(a_self, a_timeDelta, a_time);
			// After the HUD's own scripts ran, so our changes are what gets drawn.
			Update(*a_self);
		}
	}

	void Install()
	{
		static MenuWatcher watcher;
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->RegisterSink<RE::MenuOpenCloseEvent>(&watcher);
		}

		REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE::HUDMenu[0] };
		g_advance = reinterpret_cast<AdvanceFn*>(vtable.write_vfunc(4, &Advance));  // IMenu::AdvanceMovie
		REX::INFO("crosshair: hooked HUDMenu");
	}
}
