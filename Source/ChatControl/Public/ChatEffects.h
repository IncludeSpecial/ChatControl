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
