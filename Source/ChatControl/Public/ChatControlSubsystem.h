#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Containers/Queue.h"
#include "ChatEvent.h"
#include "ChatEconomy.h"
#include "ChatEffects.h"
#include "ChatControlSettings.h"
#include "ChatControlSubsystem.generated.h"

class FTwitchIrcConnector;
class AFGCharacterPlayer;

/**
 * Heart of the mod: receives chat events, runs the FICSIT Chips economy, cooldowns,
 * chaos budget and Auto-Chaos, and executes effects in the game world.
 * Lives for the whole game session, so the chat connection survives loading saves.
 */
UCLASS()
class CHATCONTROL_API UChatControlSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bInitialized; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }

	/** Shows a message in the in-game chat */
	void Announce(const FText& Message) const;

	/**
	 * Runs Apply now and Revert after Duration seconds.
	 * If an effect in the same slot is active, it is reverted first, so effects never stack.
	 */
	void StartTimedEffect(const FString& Slot, float Duration, TFunction<void()> Apply, TFunction<void()> Revert);

private:
	struct FPendingChaos
	{
		FString EffectId;
		FString ViewerKey;
		FString ViewerName;
		int32 PaidChips = 0;
	};

	struct FTimedEffect
	{
		double EndTime = 0.0;
		TFunction<void()> Revert;
	};

	void RestartConnectors();
	/** Picks up changes made in the in-game mod config menu */
	void TickConfig(double Now);

	void HandleEvent(const FChatEvent& Event);
	void HandleCommand(const FChatEvent& Event);
	/** Validates cooldowns and price, then runs or queues the effect. bFree skips the price (rewards, raids, Auto-Chaos). */
	void RequestEffect(const FString& EffectId, const FString& ViewerKey, const FString& ViewerName, bool bFree);
	/** Returns false if the effect could not run right now */
	bool ExecuteEffect(const FChatEffect& Effect, const FString& ViewerName);

	void TickEconomy(double Now);
	void TickChaosQueue(double Now);
	void TickAutoChaos(double Now);
	void TickTimedEffects(double Now);

	int32 GetScaledCost(int32 BaseCost, double Now) const;
	int32 GetChaosBudgetUsed(double Now) const;
	AFGCharacterPlayer* FindPlayer() const;

	FChatControlSettings Settings;
	FChatEconomy Economy;

	UPROPERTY()
	TObjectPtr<class UConfigManager> ConfigManager;
	double NextConfigCheck = 0.0;
	TMap<FString, FChatEffect> Effects;

	TQueue<FChatEvent, EQueueMode::Mpsc> IncomingEvents;
	TSharedPtr<FTwitchIrcConnector> Twitch;

	TArray<FPendingChaos> ChaosQueue;
	/** (time, weight) of harmful effects executed within the last minute */
	TArray<TPair<double, int32>> ChaosHistory;
	TMap<FString, FTimedEffect> TimedEffects;

	TMap<FString, double> GlobalCooldownUntil;
	/** Key: viewerKey|effectId */
	TMap<FString, double> ViewerCooldownUntil;

	double NextChipsPayout = 0.0;
	double NextEconomySave = 0.0;
	double LastTriggerTime = 0.0;
	double LastDeathTime = -1000.0;
	bool bChaosPaused = false;
	bool bInitialized = false;
};
