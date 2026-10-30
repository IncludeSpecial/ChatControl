#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Interfaces/IHttpRequest.h"
#include "ChatEvent.h"

/**
 * Reads YouTube live chat through the YouTube Data API v3 (needs an API key).
 * Finds the live stream of a channel (or uses a given video id), then polls liveChatMessages.
 * Driven from the game thread by Tick; HTTP callbacks also arrive on the game thread.
 * Quota: finding a stream by channel costs 100 units, each poll 5 units (10 000 units per day for free).
 */
class FYouTubeChatConnector : public TSharedFromThis<FYouTubeChatConnector>
{
public:
	FYouTubeChatConnector(const FString& InApiKey, const FString& InChannelOrVideo, float InPollSeconds, TQueue<FChatEvent, EQueueMode::Mpsc>& InOutput);
	~FYouTubeChatConnector();

	void Tick(double Now);
	bool IsConnected() const { return !LiveChatId.IsEmpty(); }

private:
	enum class EState : uint8 { FindVideo, FindChat, Poll, Waiting };

	void FindLiveVideo();
	void FindLiveChat();
	void PollMessages();
	void Retry(double Now, float Seconds, EState Next);
	void HandleItem(const TSharedPtr<class FJsonObject>& Item);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Get(const FString& Url, TFunction<void(TSharedPtr<class FJsonObject>)> OnJson);

	FString ApiKey;
	FString ChannelOrVideo;
	float PollSeconds;
	TQueue<FChatEvent, EQueueMode::Mpsc>& Output;

	EState State = EState::FindVideo;
	EState AfterWait = EState::FindVideo;
	double NextActionTime = 0.0;
	bool bRequestInFlight = false;
	FString VideoId;
	FString LiveChatId;
	FString PageToken;
	/** The first page holds old messages; they are skipped */
	bool bFirstPage = true;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
};
