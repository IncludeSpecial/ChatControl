#pragma once

#include "CoreMinimal.h"
#include "ChatWorldData.generated.h"

/** Something a viewer left in a save: a named machine or a pet */
USTRUCT()
struct FChatWorldMark
{
	GENERATED_BODY()

	UPROPERTY() FString ViewerKey;
	UPROPERTY() FString ViewerName;
	UPROPERTY() FVector Location = FVector::ZeroVector;
};

/** Per-save data, stored in Saved/ChatControl/Worlds/<session>.json; actors are found again by location on load */
USTRUCT()
struct FChatWorldData
{
	GENERATED_BODY()

	UPROPERTY() TArray<FChatWorldMark> Buildings;
	UPROPERTY() TArray<FChatWorldMark> Pets;
};
