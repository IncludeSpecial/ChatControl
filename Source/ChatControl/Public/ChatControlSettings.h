#pragma once

#include "CoreMinimal.h"
#include "ChatControlSettings.generated.h"

USTRUCT()
struct FChatEffectSettings
{
	GENERATED_BODY()

	UPROPERTY() bool bEnabled = true;
	/** Base price in chips. 0 = free command. Scaled by audience size. */
	UPROPERTY() int32 Cost = 0;
	/** Seconds before anyone can trigger this effect again */
	UPROPERTY() float GlobalCooldown = 10.f;
	/** Seconds before the same viewer can trigger this effect again */
	UPROPERTY() float ViewerCooldown = 60.f;
	/** Duration for timed effects, seconds */
	UPROPERTY() float Duration = 20.f;
	/** Chaos budget points consumed (harmful effects only) */
	UPROPERTY() int32 ChaosWeight = 1;
};

/**
 * Stored as JSON in Saved/ChatControl/Settings.json.
 * Will get an in-game ModConfig UI later; the file stays the source of truth for advanced options.
 */
USTRUCT()
struct FChatControlSettings
{
	GENERATED_BODY()

	UPROPERTY() bool bEnabled = true;

	// --- Twitch ---
	UPROPERTY() FString TwitchChannel = TEXT("swittyids");
	/** Chat logins to ignore (bots) */
	UPROPERTY() TArray<FString> IgnoredUsers = { TEXT("nightbot"), TEXT("streamelements"), TEXT("streamlabs"), TEXT("moobot"), TEXT("fossabot") };

	// --- FICSIT Chips economy ---
	UPROPERTY() int32 ChipsPerInterval = 10;
	UPROPERTY() float ChipsIntervalSeconds = 300.f;
	/** Viewer counts as active if they wrote something within this window */
	UPROPERTY() float ActiveWindowSeconds = 900.f;
	UPROPERTY() int32 FirstMessageBonus = 20;
	UPROPERTY() int32 ChipsPerBit = 2;
	UPROPERTY() int32 ChipsPerSub = 500;
	/** Extra price multiplier per active viewer above one, capped by MaxCostMultiplier */
	UPROPERTY() float CostScalePerViewer = 0.05f;
	UPROPERTY() float MaxCostMultiplier = 3.f;

	// --- Chaos limits ---
	UPROPERTY() int32 ChaosBudgetPerMinute = 4;
	/** Harmful effects are held back for this long after the player dies */
	UPROPERTY() float DeathProtectionSeconds = 60.f;

	// --- Auto-Chaos ---
	UPROPERTY() bool bAutoChaos = true;
	/** Trigger a random effect if chat triggered nothing for this long */
	UPROPERTY() float AutoChaosIdleSeconds = 600.f;

	/** Channel points reward id -> effect id. Unknown reward ids are printed to the game chat so they can be copied here. */
	UPROPERTY() TMap<FString, FString> RewardEffects;

	/** Effect id -> settings */
	UPROPERTY() TMap<FString, FChatEffectSettings> Effects;

	/** Fills Effects with defaults for any effect id that is missing */
	void AddMissingEffectDefaults();

	static FString GetSettingsPath();
	static FChatControlSettings LoadOrCreate();
	void Save() const;
};
