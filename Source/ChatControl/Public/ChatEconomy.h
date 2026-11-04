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
	/** Local date (YYYY-MM-DD) of the last !daily */
	UPROPERTY() FString LastDaily;
	/** Local date (YYYY-MM-DD) the transfer counters below belong to */
	UPROPERTY() FString TransferDay;
	UPROPERTY() int32 SentToday = 0;
	UPROPERTY() int32 ReceivedToday = 0;

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
	/** The second pot, for a bigger effect */
	UPROPERTY() int32 Pot2 = 0;
};

enum class EChipTransfer : uint8
{
	Done,
	NotEnoughChips,
	SenderDailyLimit,
	ReceiverDailyLimit,
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
	/** Gives Amount once per calendar day; false if already claimed today */
	bool ClaimDaily(const FString& Key, const FString& DisplayName, int32 Amount);
	const FChatViewerRecord* FindByName(const FString& DisplayName) const;
	/** Economy key of a viewer by display name; a viewer on PreferredPlatform wins if the name exists on several */
	FString FindKeyByName(const FString& DisplayName, const FString& PreferredPlatform) const;
	/**
	 * Moves Amount chips from one viewer to another: the receiver gets Amount - Fee, the fee goes to the pot.
	 * Daily limits count chips sent and received per calendar day; 0 = no limit. OutLeft = what the blocking limit still allows.
	 */
	EChipTransfer Transfer(const FString& FromKey, const FString& ToKey, int32 Amount, int32 Fee, int32 SendLimit, int32 ReceiveLimit, int32& OutLeft);

	/** Gives Amount chips to everyone who wrote within ActiveWindow. Returns how many viewers were paid. */
	int32 PayActiveViewers(double Now, double ActiveWindow, int32 Amount);
	int32 CountActiveViewers(double Now, double ActiveWindow) const;

	bool IsDirty() const { return bDirty; }

	/** Index: 0 = the first pot, 1 = the second */
	int32 GetPot(int32 Index = 0) const { return Index == 0 ? Data.Pot : Data.Pot2; }
	void AddToPot(int32 Amount, int32 Index = 0)
	{
		int32& Pot = Index == 0 ? Data.Pot : Data.Pot2;
		Pot = FMath::Max(0, Pot + Amount);
		bDirty = true;
	}
	/** (display name, chips spent), highest first */
	TArray<TPair<FString, int32>> GetTopSpenders(int32 Count) const;
	/** (display name, chips) of viewers who wrote within ActiveWindow, richest first */
	TArray<TPair<FString, int32>> GetActiveBalances(double Now, double ActiveWindow, int32 Count) const;

private:
	static FString GetSavePath();

	FChatEconomyData Data;
	mutable bool bDirty = false;
};
