#pragma once

#include "CoreMinimal.h"

enum class EChatEventType : uint8
{
	Message,       // regular chat message (may contain a !command)
	Bits,          // message with cheered bits
	RewardRedeem,  // channel points reward with text input
	Subscription,  // sub / resub
	GiftSubs,      // one or many gifted subs
	Raid,
};

/** Platform-independent chat event. Produced by connectors on a worker thread, consumed on the game thread. */
struct FChatEvent
{
	EChatEventType Type = EChatEventType::Message;

	/** "twitch", later "youtube" */
	FString Platform;
	/** Stable per-platform user id (used as economy key) */
	FString UserId;
	FString DisplayName;
	FString Text;

	/** Bits for Bits, gift count for GiftSubs, viewer count for Raid */
	int32 Amount = 0;
	/** Channel points reward id for RewardRedeem */
	FString RewardId;

	bool bIsBroadcaster = false;
	bool bIsModerator = false;

	FString GetEconomyKey() const { return Platform + TEXT(":") + UserId; }
};
