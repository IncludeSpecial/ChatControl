#pragma once

#include "CoreMinimal.h"

class UWorld;
class AFGCharacterPlayer;
class UChatControlSubsystem;
struct FChatEffectSettings;

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
}
