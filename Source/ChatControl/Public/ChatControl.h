#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

CHATCONTROL_API DECLARE_LOG_CATEGORY_EXTERN(LogChatControl, Log, All);

class FChatControlModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
