#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Interfaces/IHttpRequest.h"
#include "ChatEvent.h"

class FJsonObject;

/**
 * Twitch Helix API with the streamer's own token (all over plain HTTPS, no WebSocket needed):
 * new followers, channel point rewards created by the mod, native polls.
 * Runs on the game thread; HTTP callbacks arrive there too.
 */
class FTwitchApi : public TSharedFromThis<FTwitchApi>
{
public:
	/** Scopes requested by the login page */
	static const TCHAR* Scopes;

	/** Called with new (access, refresh) tokens so they can be saved */
	using FOnTokens = TFunction<void(const FString& Access, const FString& Refresh)>;
	/** Called when the streamer has to enter a code at the activation page */
	using FOnLoginCode = TFunction<void(const FString& UserCode, const FString& Url)>;

	FTwitchApi(const FString& InClientId, const FString& InToken, const FString& InRefresh, TQueue<FChatEvent, EQueueMode::Mpsc>& InOutput,
		FOnTokens InOnTokens, FOnLoginCode InOnLoginCode);

	void Tick(double Now);
	bool IsReady() const { return !BroadcasterId.IsEmpty(); }
	const FString& GetLogin() const { return Login; }

	void SetFollowsEnabled(bool bEnabled) { bFollows = bEnabled; }

	/**
	 * Makes Twitch rewards match the wanted ones (effect id -> (title, cost)): creates, updates and deletes rewards
	 * the mod owns. OnDone receives effect id -> reward id.
	 */
	void SyncRewards(const TMap<FString, TPair<FString, int32>>& Wanted, const TMap<FString, FString>& Known, TFunction<void(const TMap<FString, FString>&)> OnDone);

	/** Starts a poll; OnUpdate gets vote counts while it runs and bFinished = true at the end. Count -1 in the array = poll failed. */
	void StartPoll(const FString& Title, const TArray<FString>& Choices, int32 DurationSeconds, TFunction<void(const TArray<int32>& Votes, bool bFinished)> OnUpdate);

private:
	void Validate();
	/** Device Code Flow: ask Twitch for a code, then wait until the streamer confirms it */
	void StartDeviceLogin();
	void PollDeviceLogin();
	void RefreshToken();
	void AcceptTokens(const TSharedPtr<FJsonObject>& Json);
	void PollFollowers();
	void PollRedemptions();
	void CheckPoll();
	void Request(const FString& Verb, const FString& Url, const FString& Body, TFunction<void(int32 Code, TSharedPtr<FJsonObject> Json)> OnDone);

	FString ClientId;
	FString Token;
	FString Refresh;
	FOnTokens OnTokens;
	FOnLoginCode OnLoginCode;
	FString DeviceCode;
	double NextDevicePoll = 0.0;
	double DeviceInterval = 5.0;
	double DeviceExpires = 0.0;
	bool bAuthInFlight = false;
	TQueue<FChatEvent, EQueueMode::Mpsc>& Output;

	FString BroadcasterId;
	FString Login;
	bool bFollows = true;
	bool bValidating = false;

	double NextValidate = 0.0;
	double NextFollowers = 0.0;
	double NextRedemptions = 0.0;
	double NextPollCheck = 0.0;
	FString LastFollowedAt;
	bool bFollowersInFlight = false;
	int32 RedemptionsInFlight = 0;

	/** reward id -> effect id, rewards owned by the mod */
	TMap<FString, FString> OwnedRewards;
	/** Redemptions already handed to the game, so a slow PATCH does not run them twice */
	TSet<FString> HandledRedemptions;

	FString PollId;
	TFunction<void(const TArray<int32>&, bool)> PollCallback;
	bool bPollCheckInFlight = false;
};
