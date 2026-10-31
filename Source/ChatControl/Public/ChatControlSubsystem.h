#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Containers/Queue.h"
#include "ChatEvent.h"
#include "ChatEconomy.h"
#include "ChatEffects.h"
#include "ChatControlSettings.h"
#include "ChatWorldData.h"
#include "HttpRouteHandle.h"
#include "ChatControlSubsystem.generated.h"

class FTwitchIrcConnector;

/** The lizard doggo used for viewer pets */
static const TCHAR* const PetClassPath = TEXT("/Game/FactoryGame/Character/Creature/Wildlife/SpaceRabbit/Char_SpaceRabbit.Char_SpaceRabbit_C");
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
	/** Remembers who left a sign, so !clear can remove it by nick */
	void TrackSign(AActor* Sign, const FString& ViewerName) { Signs.Emplace(Sign, ViewerName); }
	/** Removes all signs, or only the ones left by Viewer; returns how many were removed */
	int32 ClearSigns(const FString& Viewer);
	bool GetHubLocation(FVector& OutLocation) const;
	/** True while the player is close to the HUB and creature effects should wait */
	bool IsInSafeZone() const;
	/** Cancels everything chat started: queue, delayed spawns, timed effects, creatures */
	void Panic();

	/** Floating text above an actor that always faces the camera */
	class UWidgetComponent* AttachNameTag(AActor* Target, const FString& Text, const FColor& Color, float Height);
	/** Name tag color for a viewer: their chat role color, or Default for viewers without a role */
	FColor GetNameColor(const FString& ViewerName, const FColor& Default) const;
	/** Spawns (or replaces) the viewer's named pet next to the player, or at At when restoring a save */
	bool SpawnPet(const FString& ViewerKey, const FString& ViewerName, const FVector* At = nullptr);
	/** Name tag text: the viewer's name plus their rank on a second line */
	FString GetTagText(const FString& ViewerName) const;
	/** Puts the viewer's name over the machine the player looks at (or the nearest one); kept in the save */
	bool NameBuilding(const FString& ViewerKey, const FString& ViewerName);

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
	bool ExecuteEffect(const FChatEffect& Effect, const FString& ViewerKey, const FString& ViewerName, const FString& Args = FString());
	/** Keeps name tags readable by turning them to the camera */
	void TickNameTags();
	void TickTwitchApi(double Now);
	/** Creates/updates/deletes the channel point rewards the mod owns */
	void SyncTwitchRewards();

	// Chat games (ChatControlGames.cpp)
	/** Votes, pot, top and stats commands. Returns true if the command was handled. */
	bool HandleGameCommand(const FChatEvent& Event, const FString& Command, const FString& Args);
	void HandlePotCommand(const FChatEvent& Event, const FString& Args);
	void TickVotes(double Now);
	void FinishVote(double Now);
	void RecordSessionEffect(const FString& ViewerName);
	FText GetSessionSummary() const;
	void SaveSessionSummary() const;

	// OBS overlay (ChatControlOverlay.cpp)
	void StartOverlay();
	void StopOverlay();
	void PushOverlayEvent(const FString& Text) const;
	FString BuildOverlayJson() const;
	/** The streamer's widget folder (if set), then the widgets shipped with the mod */
	TArray<FString> GetOverlayFolders() const;
	TUniquePtr<struct FHttpServerResponse> ServeOverlayFile(const FString& RelativePath) const;

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
	TSharedPtr<class FYouTubeChatConnector> YouTube;
	TSharedPtr<class FTwitchApi> TwitchApi;
	bool bTwitchApiWasReady = false;
	/** 0 = no sync pending */
	double RewardSyncTime = 0.0;
	/** Votes run as a native Twitch poll; counts come from Twitch */
	bool bPollMode = false;
	TArray<int32> PollVotes;

	TArray<FPendingChaos> ChaosQueue;
	/** (time, weight) of harmful effects executed within the last minute */
	TArray<TPair<double, int32>> ChaosHistory;
	TMap<FString, FTimedEffect> TimedEffects;
	TArray<TPair<double, TFunction<void()>>> DelayedActions;
	TArray<TWeakObjectPtr<AActor>> SpawnedActors;
	TArray<TPair<TWeakObjectPtr<AActor>, FString>> Signs;

	bool bVoteActive = false;
	double VoteEndTime = 0.0;
	double NextVoteTime = 0.0;
	TArray<FString> VoteOptions;
	/** viewer key -> option index */
	TMap<FString, int32> Votes;

	TWeakPtr<class IHttpRouter> OverlayRouter;
	FHttpRouteHandle OverlayRoute;
	FHttpRouteHandle OverlayFilesRoute;
	/** (id, text) of recent announcements for the overlay feed; mutable because Announce is const */
	mutable TArray<TPair<int32, FString>> OverlayEvents;
	mutable int32 OverlayEventCounter = 0;

	double SessionStartTime = 0.0;
	int32 SessionEffectCount = 0;
	int32 SessionDeaths = 0;
	bool bPlayerWasDead = false;
	TMap<FString, int32> SessionEffectsByViewer;
	TArray<TWeakObjectPtr<class UWidgetComponent>> NameTags;
	/** viewer key -> their pet */
	TMap<FString, TWeakObjectPtr<AActor>> Pets;
	/** Last seen chat role per viewer name (lower case): 4 streamer, 3 moderator, 2 VIP, 1 subscriber */
	TMap<FString, uint8> ViewerRoles;
	/** Lizard doggo taming lives in blueprints, so it is found by name at runtime */
	bool TryTamePet(APawn* Pet, AActor* Owner) const;
	/** Writes the viewer's name into the doggo's own blueprint name (its built-in name tag) */
	bool TryNamePet(APawn* Pet, const FString& Name) const;

	// Viewers (ChatControlViewers.cpp)
	/** Shows a greeting with the viewer's name next to the player */
	void WelcomeViewer(const FString& ViewerName);
	/** Rank for lifetime chips spent; empty when ranks are off */
	FString GetRank(int32 ChipsSpent) const;
	/** Announces a new rank if spending moved the viewer up */
	void CheckRankUp(const FString& ViewerKey, int32 SpentBefore);
	/** Gives a pet its tag, persistence and maybe tameness */
	void SetupPet(APawn* Pet, const FString& ViewerKey, const FString& ViewerName, bool bAnnounce);
	void TickWorldData(double Now);
	void ApplyWorldData();
	void SaveWorldData();
	FChatWorldMark& FindOrAddMark(TArray<FChatWorldMark>& Marks, const FString& ViewerKey, const FString& ViewerName);

	FChatWorldData WorldData;
	FString WorldDataPath;
	TWeakObjectPtr<UWorld> LoadedWorld;
	bool bWorldApplied = false;
	double WorldApplyTime = 0.0;
	double NextWorldSave = 0.0;
	/** viewer key -> their named machine */
	TMap<FString, TWeakObjectPtr<AActor>> NamedBuildings;
	TMap<FString, TWeakObjectPtr<class UWidgetComponent>> BuildingTags;

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
