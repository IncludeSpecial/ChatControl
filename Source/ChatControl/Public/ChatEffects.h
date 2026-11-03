#pragma once

#include "CoreMinimal.h"

class UWorld;
class AFGCharacterPlayer;
class UChatControlSubsystem;
class AActor;
struct FChatEffectSettings;
struct FChatControlSettings;

struct FChatEffectContext
{
	UWorld* World = nullptr;
	AFGCharacterPlayer* Player = nullptr;
	UChatControlSubsystem* Subsystem = nullptr;
	const FChatEffectSettings* Settings = nullptr;
	/** Who triggered it, for announcements */
	FString ViewerName;
	/** Economy key of who triggered it ("platform:id", or a system key for ADA, votes and the pot) */
	FString ViewerKey;
	/** Text after the command, original case (e.g. the message for !sign) */
	FString Args;
};

struct FChatEffect
{
	FString Id;
	/** Harmful effects go through the chaos budget, pause and death protection */
	bool bHarmful = false;
	/** Returns false if the effect could not be applied (chips are refunded) */
	TFunction<bool(const FChatEffectContext&)> Execute;
};

namespace ChatEffects
{
	/** All built-in effects, keyed by id (the chat command without "!") */
	TMap<FString, FChatEffect> CreateAll();

	/** A free spot on a ring around the player where a creature with this capsule fits */
	bool FindCreatureSpot(UWorld* World, const AActor* Player, const FChatControlSettings& Settings, float Radius, float HalfHeight, FVector& OutLocation);

	/** Drops a crate with random parts; AdvancedShare = chance of an advanced part per stack */
	bool DropLootCrate(UWorld* World, AActor* Player, const FVector& Location, int32 Stacks, float AdvancedShare);
}
