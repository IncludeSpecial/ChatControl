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
	/** How many things the effect spawns (creatures). 0 = not applicable */
	UPROPERTY() int32 Amount = 0;
	/** Effect-specific power: jump/gravity multiplier, share of a stack for gifts. 0 = not applicable */
	UPROPERTY() float Strength = 0.f;
	/** Random warning delay before the effect hits, seconds (creatures) */
	UPROPERTY() float DelayMin = 0.f;
	UPROPERTY() float DelayMax = 0.f;
	/** Can be picked by votes, Auto-Chaos and the pot */
	UPROPERTY() bool bRandomPool = true;
	/** Channel points price of the reward the mod creates for this effect (needs Twitch login). 0 = no reward */
	UPROPERTY() int32 ChannelPointsCost = 0;
};

/**
 * Stored as JSON in Saved/ChatControl/Settings.json.
 * The in-game settings page (Mods -> ChatControl) edits the same values.
 */
USTRUCT()
struct FChatControlSettings
{
	GENERATED_BODY()

	/** Bumped when new per-effect fields appear, so older files get their defaults filled in */
	UPROPERTY() int32 SettingsVersion = 0;

	UPROPERTY() bool bEnabled = true;
	/** Language of game chat messages and the settings page: "auto" (follows the game), "en" or "ru" */
	UPROPERTY() FString Language = TEXT("auto");

	// --- Twitch ---
	UPROPERTY() FString TwitchChannel = TEXT("swittyids");
	/** Chat logins to ignore (bots) */
	UPROPERTY() TArray<FString> IgnoredUsers = { TEXT("nightbot"), TEXT("streamelements"), TEXT("streamlabs"), TEXT("moobot"), TEXT("fossabot") };

	/** 1 = easy (pricier, fewer creatures), 2 = normal, 3 = hardcore (cheaper, more creatures) */
	UPROPERTY() int32 Difficulty = 2;

	// --- Twitch account (optional, unlocks follows, auto channel point rewards and native polls) ---
	/** Client ID of the streamer's own app from dev.twitch.tv/console */
	UPROPERTY() FString TwitchClientId;
	/** Tokens saved after logging in with a code at twitch.tv/activate. Never shown in the menu */
	UPROPERTY() FString TwitchToken;
	UPROPERTY() FString TwitchRefreshToken;
	UPROPERTY() bool bFollowsEnabled = true;
	UPROPERTY() int32 ChipsPerFollow = 50;
	/** Effect that runs for a new follower, free. Empty = none */
	UPROPERTY() FString FollowEffect = TEXT("hi");
	/** Create and handle channel point rewards for effects with ChannelPointsCost > 0 */
	UPROPERTY() bool bAutoRewards = true;
	/** Run votes as native Twitch polls instead of !1 / !2 / !3 */
	UPROPERTY() bool bTwitchPolls = false;

	// --- Notifications ---
	/** Announce the periodic chip payout in the game chat */
	UPROPERTY() bool bAnnouncePayouts = true;
	/** Write a summary file for each session */
	UPROPERTY() bool bSaveSessionFiles = true;

	// --- YouTube ---
	/** YouTube Data API v3 key from Google Cloud Console. Empty = YouTube off */
	UPROPERTY() FString YouTubeApiKey;
	/** Channel id (UC...), channel link, live video id or live link */
	UPROPERTY() FString YouTubeChannel;
	/** Seconds between chat polls; each poll costs 5 of the 10 000 free daily quota units */
	UPROPERTY() float YouTubePollSeconds = 10.f;
	/** Chips per whole currency unit of a Super Chat */
	UPROPERTY() int32 ChipsPerSuperChatUnit = 5;

	// --- Creatures ---
	/** No creatures while the player is within this many meters of the HUB. 0 = off */
	UPROPERTY() float SafeZoneRadius = 40.f;
	/** Spawned creatures despawn after this long, so chat can't flood the world with enemies */
	UPROPERTY() float CreatureLifeSpanSeconds = 300.f;
	/** Creatures appear on a ring around the player, in meters */
	UPROPERTY() float SpawnDistanceMin = 6.f;
	UPROPERTY() float SpawnDistanceMax = 10.f;

	// --- Viewers in the world ---
	/** A subscription brings the viewer's named pet */
	UPROPERTY() bool bPetOnSub = true;
	UPROPERTY() float PetLifeSpanSeconds = 1800.f;
	/** Show the viewer's name above creatures they sent */
	UPROPERTY() bool bCreatureNameTags = true;
	/** Chance (0..1) that a pet arrives already tamed */
	UPROPERTY() float PetTamedChance = 0.5f;
	/** Pets never leave and are kept in the save ("subscriber zoo"); otherwise they leave after PetLifeSpanSeconds */
	UPROPERTY() bool bPetPermanent = true;
	/** A viewer's first message in a session shows their name next to the player */
	UPROPERTY() bool bWelcomeEnabled = true;
	UPROPERTY() float WelcomeSeconds = 8.f;
	/** Chips for !daily, once per calendar day; 0 = off */
	UPROPERTY() int32 DailyChips = 50;
	/** Ranks by lifetime chips spent */
	UPROPERTY() bool bRanksEnabled = true;
	/** "spent=Name;spent=Name"; empty = built-in ranks in the current language */
	UPROPERTY() FString Ranks;
	/** Name tag colors by chat role (hex); viewers without a role keep the color of the thing they sent */
	UPROPERTY() bool bRoleColors = true;
	UPROPERTY() FString ColorBroadcaster = TEXT("#FF2A4A");
	UPROPERTY() FString ColorModerator = TEXT("#19E3FF");
	UPROPERTY() FString ColorVip = TEXT("#FF6AD5");
	UPROPERTY() FString ColorSubscriber = TEXT("#B57BFF");

	// --- Chat games ---
	UPROPERTY() bool bVotesEnabled = true;
	UPROPERTY() int32 VoteIntervalMinutes = 10;
	UPROPERTY() float VoteDurationSeconds = 60.f;
	/** Chips for taking part in a vote */
	UPROPERTY() int32 VoteReward = 5;
	/** Shared pot: when chat fills GoalChips together, GoalEffect runs */
	UPROPERTY() bool bGoalEnabled = true;
	UPROPERTY() FString GoalEffect = TEXT("boss");
	UPROPERTY() int32 GoalChips = 1000;

	// --- OBS overlay ---
	UPROPERTY() bool bOverlayEnabled = true;
	/** Widgets read http://localhost:<port>/state */
	UPROPERTY() int32 OverlayPort = 8787;
	/** Folder with widget files served at http://localhost:<port>/obs/; empty = Saved/ChatControl/Overlay */
	UPROPERTY() FString OverlayFolder;

	// --- Raids ---
	/** One ambush per this many raiders */
	UPROPERTY() int32 RaidViewersPerAmbush = 5;
	UPROPERTY() int32 RaidMaxAmbushes = 5;
	/** Raids with at least this many viewers also bring a boss. 0 = never */
	UPROPERTY() int32 RaidBossMinViewers = 30;

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
	/** effect id -> id of the reward the mod created for it on Twitch */
	UPROPERTY() TMap<FString, FString> AutoRewardIds;

	/** Effect id -> settings */
	UPROPERTY() TMap<FString, FChatEffectSettings> Effects;

	/** Fills Effects with defaults for any effect id that is missing, and new fields of older files */
	void AddMissingEffectDefaults();

	static constexpr int32 CurrentVersion = 3;

	float GetPriceMultiplier() const { return Difficulty <= 1 ? 1.5f : Difficulty >= 3 ? 0.6f : 1.f; }
	float GetCreatureMultiplier() const { return Difficulty <= 1 ? 0.5f : Difficulty >= 3 ? 2.f : 1.f; }

	static FString GetSettingsPath();
	static FChatControlSettings LoadOrCreate();
	void Save() const;
};
