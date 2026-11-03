#pragma once

#include "CoreMinimal.h"
#include "Configuration/ModConfiguration.h"
#include "ChatControlModConfig.generated.h"

class UConfigManager;
struct FChatControlSettings;

/**
 * In-game settings page (Mods -> ChatControl).
 * The property tree is built in code from SML's config property widgets instead of a Blueprint asset,
 * so the defaults always match FChatControlSettings.
 */
UCLASS()
class CHATCONTROL_API UChatControlModConfig : public UModConfiguration
{
	GENERATED_BODY()

public:
	UChatControlModConfig();

	/** Builds the property tree with Defaults and registers it; SML then loads the user's saved values */
	static void Register(UConfigManager* ConfigManager, const FChatControlSettings& Defaults);

	/** Copies values from the live config into Settings. Returns true if anything changed. */
	/** bOutPortRejected is set when the OBS port in the menu was invalid and got reset to the default */
	static bool ApplyTo(UConfigManager* ConfigManager, FChatControlSettings& Settings, bool* bOutPortRejected = nullptr);

	static FConfigId GetId();
};
