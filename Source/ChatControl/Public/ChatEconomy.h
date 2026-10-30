#pragma once

#include "CoreMinimal.h"
#include "ChatEconomy.generated.h"

USTRUCT()
struct FChatViewerRecord
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() int32 Chips = 0;
	/** Lifetime chips spent, for the leaderboard */
	UPROPERTY() int32 ChipsSpent = 0;

	// Runtime only (not saved)
	double LastMessageTime = -1.0;
	bool bGreetedThisSession = false;
};

USTRUCT()
struct FChatEconomyData
{
	GENERATED_BODY()

	/** Key: "platform:userId" */
	UPROPERTY() TMap<FString, FChatViewerRecord> Viewers;
	/** Shared chip pot chat fills together */
	UPROPERTY() int32 Pot = 0;
};

/** FICSIT Chips: viewers earn them by being active in chat and spend them on effects. Persisted between streams. */
class FChatEconomy
{
public:
	void Load();
	void Save() const;

	FChatViewerRecord& GetOrAdd(const FString& Key, const FString& DisplayName);
	const FChatViewerRecord* Find(const FString& Key) const { return Data.Viewers.Find(Key); }

	/** Registers a chat message. Returns the first-message bonus granted (0 if none). */
	int32 OnViewerMessage(const FString& Key, const FString& DisplayName, double Now, int32 FirstMessageBonus);

	void AddChips(const FString& Key, const FString& DisplayName, int32 Amount);
	bool TrySpend(const FString& Key, int32 Amount);

	/** Gives Amount chips to everyone who wrote within ActiveWindow. Returns how many viewers were paid. */
	int32 PayActiveViewers(double Now, double ActiveWindow, int32 Amount);
	int32 CountActiveViewers(double Now, double ActiveWindow) const;

	bool IsDirty() const { return bDirty; }

	int32 GetPot() const { return Data.Pot; }
	void AddToPot(int32 Amount) { Data.Pot = FMath::Max(0, Data.Pot + Amount); bDirty = true; }
	/** (display name, chips spent), highest first */
	TArray<TPair<FString, int32>> GetTopSpenders(int32 Count) const;

private:
	static FString GetSavePath();

	FChatEconomyData Data;
	mutable bool bDirty = false;
};
