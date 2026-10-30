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
	 * Runs Maintain now and every tick for Duration seconds, then Revert.
	 * If an effect in the same slot is active, it is reverted first, so effects never stack.
	 */
	void StartTimedEffect(const FString& Slot, float Duration, TFunction<void()> Maintain, TFunction<void()> Revert);

	/** The local player character, or null in menus and loading screens */
	AFGCharacterPlayer* FindPlayer() const;

	/** Runs Action on the game thread after Delay seconds (real time) */
	void RunDelayed(float Delay, TFunction<void()> Action);

	UWorld* GetGameWorld() const;
	const FChatControlSettings& GetSettings() const { return Settings; }

	/** Remembers an actor spawned by chat, so !panic can remove it */
	void TrackSpawned(AActor* Actor) { SpawnedActors.Add(Actor); }
	bool GetHubLocation(FVector& OutLocation) const;
	/** True while the player is close to the HUB and creature effects should wait */
	bool IsInSafeZone() const;
	/** Cancels everything chat started: queue, delayed spawns, timed effects, creatures */
	void Panic();

	/** Floating text above an actor that always faces the camera */
	class UTextRenderComponent* AttachNameTag(AActor* Target, const FString& Text, const FColor& Color, float Height);
	/** Spawns (or replaces) the viewer's named pet next to the player */
	bool SpawnPet(const FString& ViewerKey, const FString& ViewerName);

private:
	struct FPendingChaos
	{
		FString EffectId;
		FString ViewerKey;
		FString ViewerName;
		int32 PaidChips = 0;
		FString Args;
	};

	struct FTimedEffect
	{
		double EndTime = 0.0;
		TFunction<void()> Maintain;
		TFunction<void()> Revert;
	};

	void RestartConnectors();
	/** Picks up changes made in the in-game mod config menu */
	void TickConfig(double Now);

	void HandleEvent(const FChatEvent& Event);
	void HandleCommand(const FChatEvent& Event);
	/** Validates cooldowns and price, then runs or queues the effect. bFree skips the price (rewards, raids, Auto-Chaos). */
	void RequestEffect(const FString& EffectId, const FString& ViewerKey, const FString& ViewerName, bool bFree, const FString& Args = FString());
	/** Returns false if the effect could not run right now */
	bool ExecuteEffect(const FChatEffect& Effect, const FString& ViewerName, const FString& Args = FString());
	/** Keeps name tags readable by turning them to the camera */
	void TickNameTags();

	void TickEconomy(double Now);
	void TickChaosQueue(double Now);
	void TickAutoChaos(double Now);
	void TickTimedEffects(double Now);
	void TickDelayedActions(double Now);

	int32 GetScaledCost(int32 BaseCost, double Now) const;
	int32 GetChaosBudgetUsed(double Now) const;
	/** "!test <what> [amount]" from the streamer: simulates paid events without spending anything */
	void HandleTestCommand(const FChatEvent& Event, const FString& Args);

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
	TArray<TPair<double, TFunction<void()>>> DelayedActions;
	TArray<TWeakObjectPtr<AActor>> SpawnedActors;
	TArray<TWeakObjectPtr<class UTextRenderComponent>> NameTags;
	/** viewer key -> their pet */
	TMap<FString, TWeakObjectPtr<AActor>> Pets;

	TMap<FString, double> GlobalCooldownUntil;
	/** Key: viewerKey|effectId */
	TMap<FString, double> ViewerCooldownUntil;

	double NextChipsPayout = 0.0;
	double NextChaosAttempt = 0.0;
	double NextEconomySave = 0.0;
	double LastTriggerTime = 0.0;
	double LastDeathTime = -1000.0;
	bool bChaosPaused = false;
	bool bInitialized = false;
};
