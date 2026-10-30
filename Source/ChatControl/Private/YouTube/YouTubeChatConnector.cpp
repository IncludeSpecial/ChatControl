#include "YouTubeChatConnector.h"
#include "ChatControl.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* ApiRoot = TEXT("https://www.googleapis.com/youtube/v3/");

	/** Accepts a channel id (UC...), a video id, or a youtube.com / youtu.be link to either */
	void ParseTarget(FString Input, FString& OutChannel, FString& OutVideo)
	{
		Input.TrimStartAndEndInline();
		FString Value;
		if (Input.Split(TEXT("v="), nullptr, &Value) || Input.Split(TEXT("youtu.be/"), nullptr, &Value) || Input.Split(TEXT("/live/"), nullptr, &Value))
		{
			Value.Split(TEXT("&"), &Value, nullptr);
			Value.Split(TEXT("?"), &Value, nullptr);
			OutVideo = Value;
			return;
		}
		if (Input.Split(TEXT("/channel/"), nullptr, &Value))
		{
			Value.Split(TEXT("/"), &Value, nullptr);
			Input = Value;
		}
		if (Input.StartsWith(TEXT("UC")) && Input.Len() == 24)
		{
			OutChannel = Input;
		}
		else
		{
			OutVideo = Input;
		}
	}
}

FYouTubeChatConnector::FYouTubeChatConnector(const FString& InApiKey, const FString& InChannelOrVideo, float InPollSeconds, TQueue<FChatEvent, EQueueMode::Mpsc>& InOutput)
	: ApiKey(InApiKey.TrimStartAndEnd())
	, ChannelOrVideo(InChannelOrVideo)
	, PollSeconds(FMath::Max(5.f, InPollSeconds))
	, Output(InOutput)
{
	FString Channel;
	ParseTarget(ChannelOrVideo, Channel, VideoId);
	State = VideoId.IsEmpty() ? EState::FindVideo : EState::FindChat;
}

FYouTubeChatConnector::~FYouTubeChatConnector()
{
	if (ActiveRequest.IsValid())
	{
		ActiveRequest->OnProcessRequestComplete().Unbind();
		ActiveRequest->CancelRequest();
	}
}

void FYouTubeChatConnector::Tick(double Now)
{
	if (bRequestInFlight || Now < NextActionTime || ApiKey.IsEmpty())
	{
		return;
	}
	if (State == EState::Waiting)
	{
		State = AfterWait;
	}
	switch (State)
	{
	case EState::FindVideo: FindLiveVideo(); break;
	case EState::FindChat:  FindLiveChat(); break;
	case EState::Poll:      PollMessages(); break;
	default: break;
	}
}

void FYouTubeChatConnector::Retry(double Now, float Seconds, EState Next)
{
	State = EState::Waiting;
	AfterWait = Next;
	NextActionTime = Now + Seconds;
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> FYouTubeChatConnector::Get(const FString& Url, TFunction<void(TSharedPtr<FJsonObject>)> OnJson)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url + TEXT("&key=") + FGenericPlatformHttp::UrlEncode(ApiKey));
	Request->SetVerb(TEXT("GET"));
	TWeakPtr<FYouTubeChatConnector> WeakSelf = AsShared();
	Request->OnProcessRequestComplete().BindLambda([WeakSelf, OnJson](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
	{
		TSharedPtr<FYouTubeChatConnector> This = WeakSelf.Pin();
		if (!This)
		{
			return;
		}
		This->bRequestInFlight = false;
		This->ActiveRequest.Reset();
		TSharedPtr<FJsonObject> Json;
		if (bOk && Response.IsValid())
		{
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			FJsonSerializer::Deserialize(Reader, Json);
			if (Response->GetResponseCode() != 200)
			{
				UE_LOG(LogChatControl, Warning, TEXT("YouTube: HTTP %d %s"), Response->GetResponseCode(), *Response->GetContentAsString().Left(300));
				Json.Reset();
			}
		}
		OnJson(Json);
	});
	bRequestInFlight = true;
	ActiveRequest = Request;
	Request->ProcessRequest();
	return Request;
}

void FYouTubeChatConnector::FindLiveVideo()
{
	FString Channel, Video;
	ParseTarget(ChannelOrVideo, Channel, Video);
	if (Channel.IsEmpty())
	{
		Retry(FPlatformTime::Seconds(), 300.f, EState::FindVideo);
		return;
	}
	Get(FString(ApiRoot) + TEXT("search?part=id&eventType=live&type=video&channelId=") + Channel, [this](TSharedPtr<FJsonObject> Json)
	{
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (Json && Json->TryGetArrayField(TEXT("items"), Items) && Items->Num() > 0)
		{
			const TSharedPtr<FJsonObject> Id = (*Items)[0]->AsObject()->GetObjectField(TEXT("id"));
			VideoId = Id->GetStringField(TEXT("videoId"));
			State = EState::FindChat;
			return;
		}
		// Not live yet; searching is expensive (100 units), so check rarely
		UE_LOG(LogChatControl, Log, TEXT("YouTube: no live stream found, retrying in 3 min"));
		Retry(FPlatformTime::Seconds(), 180.f, EState::FindVideo);
	});
}

void FYouTubeChatConnector::FindLiveChat()
{
	Get(FString(ApiRoot) + TEXT("videos?part=liveStreamingDetails&id=") + VideoId, [this](TSharedPtr<FJsonObject> Json)
	{
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		const TSharedPtr<FJsonObject>* Details = nullptr;
		if (Json && Json->TryGetArrayField(TEXT("items"), Items) && Items->Num() > 0 &&
			(*Items)[0]->AsObject()->TryGetObjectField(TEXT("liveStreamingDetails"), Details) &&
			(*Details)->TryGetStringField(TEXT("activeLiveChatId"), LiveChatId))
		{
			UE_LOG(LogChatControl, Log, TEXT("YouTube: connected to live chat of %s"), *VideoId);
			bFirstPage = true;
			PageToken.Empty();
			State = EState::Poll;
			return;
		}
		Retry(FPlatformTime::Seconds(), 60.f, EState::FindChat);
	});
}

void FYouTubeChatConnector::PollMessages()
{
	FString Url = FString(ApiRoot) + TEXT("liveChat/messages?part=snippet,authorDetails&maxResults=200&liveChatId=") + FGenericPlatformHttp::UrlEncode(LiveChatId);
	if (!PageToken.IsEmpty())
	{
		Url += TEXT("&pageToken=") + PageToken;
	}
	Get(Url, [this](TSharedPtr<FJsonObject> Json)
	{
		const double Now = FPlatformTime::Seconds();
		if (!Json)
		{
			// Stream ended or quota exceeded: look for the stream again later
			LiveChatId.Empty();
			FString Channel, Video;
			ParseTarget(ChannelOrVideo, Channel, Video);
			Retry(Now, 120.f, Video.IsEmpty() ? EState::FindVideo : EState::FindChat);
			return;
		}
		Json->TryGetStringField(TEXT("nextPageToken"), PageToken);
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!bFirstPage && Json->TryGetArrayField(TEXT("items"), Items))
		{
			for (const TSharedPtr<FJsonValue>& Item : *Items)
			{
				HandleItem(Item->AsObject());
			}
		}
		bFirstPage = false;
		const double ServerInterval = Json->GetNumberField(TEXT("pollingIntervalMillis")) / 1000.0;
		Retry(Now, FMath::Max<float>(PollSeconds, ServerInterval), EState::Poll);
	});
}

void FYouTubeChatConnector::HandleItem(const TSharedPtr<FJsonObject>& Item)
{
	const TSharedPtr<FJsonObject>* Snippet = nullptr;
	const TSharedPtr<FJsonObject>* Author = nullptr;
	if (!Item || !Item->TryGetObjectField(TEXT("snippet"), Snippet) || !Item->TryGetObjectField(TEXT("authorDetails"), Author))
	{
		return;
	}
	FChatEvent Event;
	Event.Platform = TEXT("youtube");
	Event.UserId = (*Author)->GetStringField(TEXT("channelId"));
	Event.DisplayName = (*Author)->GetStringField(TEXT("displayName"));
	Event.bIsBroadcaster = (*Author)->GetBoolField(TEXT("isChatOwner"));
	Event.bIsModerator = (*Author)->GetBoolField(TEXT("isChatModerator"));
	(*Snippet)->TryGetStringField(TEXT("displayMessage"), Event.Text);

	const FString Type = (*Snippet)->GetStringField(TEXT("type"));
	const TSharedPtr<FJsonObject>* Details = nullptr;
	if (Type == TEXT("textMessageEvent"))
	{
		Event.Type = EChatEventType::Message;
	}
	else if ((Type == TEXT("superChatEvent") && (*Snippet)->TryGetObjectField(TEXT("superChatDetails"), Details)) ||
		(Type == TEXT("superStickerEvent") && (*Snippet)->TryGetObjectField(TEXT("superStickerDetails"), Details)))
	{
		// Amount in whole currency units; the subsystem converts it to chips
		Event.Type = EChatEventType::SuperChat;
		Event.Amount = FMath::Max(1, FMath::RoundToInt((*Details)->GetNumberField(TEXT("amountMicros")) / 1000000.0));
		(*Details)->TryGetStringField(TEXT("userComment"), Event.Text);
	}
	else if (Type == TEXT("newSponsorEvent") || Type == TEXT("memberMilestoneChatEvent"))
	{
		Event.Type = EChatEventType::Subscription;
		Event.Amount = 1;
	}
	else if (Type == TEXT("membershipGiftingEvent") && (*Snippet)->TryGetObjectField(TEXT("membershipGiftingDetails"), Details))
	{
		Event.Type = EChatEventType::GiftSubs;
		Event.Amount = FMath::Max(1, (int32)(*Details)->GetNumberField(TEXT("giftMembershipsCount")));
	}
	else
	{
		return;
	}
	Output.Enqueue(MoveTemp(Event));
}
