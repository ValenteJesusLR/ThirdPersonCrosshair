#include "Crosshair.h"
#include "Settings.h"

namespace
{
	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type == F4SE::MessagingInterface::kGameDataReady) {
			static bool installed = false;
			if (!installed) {
				installed = true;
				Crosshair::Install();
			}
		}
	}
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	Settings::Get().Load();

	F4SE::GetMessagingInterface()->RegisterListener(OnMessage);

	REX::INFO("Third Person Crosshair loaded");
	return true;
}
