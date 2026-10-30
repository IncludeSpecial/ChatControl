#include "ChatControl.h"

DEFINE_LOG_CATEGORY(LogChatControl);

void FChatControlModule::StartupModule()
{
	// All runtime logic lives in UChatControlSubsystem, which the engine creates automatically.
}

void FChatControlModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FChatControlModule, ChatControl)
