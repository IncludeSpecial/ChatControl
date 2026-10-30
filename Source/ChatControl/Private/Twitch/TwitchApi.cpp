#include "TwitchApi.h"
#include "ChatControl.h"
#include "HttpModule.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

const TCHAR* FTwitchApi::Scopes = TEXT("moderator:read:followers channel:read:redemptions channel:manage:redemptions channel:read:polls channel:manage:polls");

namespace
{
	const FString Helix = TEXT("https://api.twitch.tv/helix/");

	FString ToJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}

	const TArray<TSharedPtr<FJsonValue>>* DataOf(const TSharedPtr<FJsonObject>& Json)
	{
		const TArray<TSharedPtr<FJsonValue>>* Data = nullptr;
		return Json && Json->TryGetArrayField(TEXT("data"), Data) ? Data : nullptr;
	}
}

FTwitchApi::FTwitchApi(const FString& InClientId, const FString& InToken, const FString& InRefresh, TQueue<FChatEvent, EQueueMode::Mpsc>& InOutput,
	FOnTokens InOnTokens, FOnLoginCode InOnLoginCode)
	: ClientId(InClientId.TrimStartAndEnd())
	, Token(InToken.TrimStartAndEnd())
	, Refresh(InRefresh.TrimStartAndEnd())
	, OnTokens(MoveTemp(InOnTokens))
	, OnLoginCode(MoveTemp(InOnLoginCode))
	, Output(InOutput)
{
}

namespace
{
	FString FormBody(const TMap<FString, FString>& Fields)
	{
		TArray<FString> Parts;
		for (const auto& Pair : Fields)
		{
			Parts.Add(Pair.Key + TEXT("=") + FGenericPlatformHttp::UrlEncode(Pair.Value));
		}
		return FString::Join(Parts, TEXT("&"));
	}
}

void FTwitchApi::StartDeviceLogin()
{
	bAuthInFlight = true;
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Http = FHttpModule::Get().CreateRequest();
	Http->SetURL(TEXT("https://id.twitch.tv/oauth2/device"));
	Http->SetVerb(TEXT("POST"));
	Http->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	Http->SetContentAsString(FormBody({ { TEXT("client_id"), ClientId }, { TEXT("scopes"), Scopes } }));
	TWeakPtr<FTwitchApi> WeakSelf = AsShared();
	Http->OnProcessRequestComplete().BindLambda([WeakSelf](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
	{
		TSharedPtr<FTwitchApi> Self = WeakSelf.Pin();
		if (!Self)
		{
			return;
		}
		Self->bAuthInFlight = false;
		TSharedPtr<FJsonObject> Json;
		if (bOk && Response && Response->GetResponseCode() == 200 &&
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Json) && Json)
		{
			Self->DeviceCode = Json->GetStringField(TEXT("device_code"));
			Self->DeviceInterval = FMath::Max(2.0, Json->GetNumberField(TEXT("interval")));
			Self->DeviceExpires = FPlatformTime::Seconds() + Json->GetNumberField(TEXT("expires_in"));
			Self->NextDevicePoll = FPlatformTime::Seconds() + Self->DeviceInterval;
			Self->OnLoginCode(Json->GetStringField(TEXT("user_code")), Json->GetStringField(TEXT("verification_uri")));
			return;
		}
		UE_LOG(LogChatControl, Warning, TEXT("Twitch login: could not start (%s). Check the Client ID and that the app type is Public."),
			Response ? *Response->GetContentAsString().Left(200) : TEXT("no response"));
		Self->NextValidate = FPlatformTime::Seconds() + 120.0;
	});
	Http->ProcessRequest();
}

void FTwitchApi::PollDeviceLogin()
{
	bAuthInFlight = true;
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Http = FHttpModule::Get().CreateRequest();
	Http->SetURL(TEXT("https://id.twitch.tv/oauth2/token"));
	Http->SetVerb(TEXT("POST"));
	Http->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	Http->SetContentAsString(FormBody({ { TEXT("client_id"), ClientId }, { TEXT("scopes"), Scopes }, { TEXT("device_code"), DeviceCode },
		{ TEXT("grant_type"), TEXT("urn:ietf:params:oauth:grant-type:device_code") } }));
	TWeakPtr<FTwitchApi> WeakSelf = AsShared();
	Http->OnProcessRequestComplete().BindLambda([WeakSelf](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
	{
		TSharedPtr<FTwitchApi> Self = WeakSelf.Pin();
		if (!Self)
		{
			return;
		}
		Self->bAuthInFlight = false;
		TSharedPtr<FJsonObject> Json;
		if (bOk && Response && Response->GetResponseCode() == 200 &&
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Json) && Json)
		{
			Self->DeviceCode.Empty();
			Self->AcceptTokens(Json);
			return;
		}
		// "authorization_pending" until the streamer confirms; keep polling until the code expires
		Self->NextDevicePoll = FPlatformTime::Seconds() + Self->DeviceInterval;
	});
	Http->ProcessRequest();
}

void FTwitchApi::RefreshToken()
{
	bAuthInFlight = true;
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Http = FHttpModule::Get().CreateRequest();
	Http->SetURL(TEXT("https://id.twitch.tv/oauth2/token"));
	Http->SetVerb(TEXT("POST"));
	Http->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	Http->SetContentAsString(FormBody({ { TEXT("client_id"), ClientId }, { TEXT("grant_type"), TEXT("refresh_token") }, { TEXT("refresh_token"), Refresh } }));
	TWeakPtr<FTwitchApi> WeakSelf = AsShared();
	Http->OnProcessRequestComplete().BindLambda([WeakSelf](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
	{
		TSharedPtr<FTwitchApi> Self = WeakSelf.Pin();
		if (!Self)
		{
			return;
		}
		Self->bAuthInFlight = false;
		TSharedPtr<FJsonObject> Json;
		if (bOk && Response && Response->GetResponseCode() == 200 &&
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Json) && Json)
		{
			Self->AcceptTokens(Json);
			return;
		}
		// Refresh token revoked: log in again
		Self->Token.Empty();
		Self->Refresh.Empty();
		Self->OnTokens(FString(), FString());
	});
	Http->ProcessRequest();
}

void FTwitchApi::AcceptTokens(const TSharedPtr<FJsonObject>& Json)
{
	Token = Json->GetStringField(TEXT("access_token"));
	Json->TryGetStringField(TEXT("refresh_token"), Refresh);
	OnTokens(Token, Refresh);
	NextValidate = 0.0;
}

void FTwitchApi::Request(const FString& Verb, const FString& Url, const FString& Body, TFunction<void(int32, TSharedPtr<FJsonObject>)> OnDone)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Http = FHttpModule::Get().CreateRequest();
	Http->SetURL(Url);
	Http->SetVerb(Verb);
	Http->SetHeader(TEXT("Client-Id"), ClientId);
	Http->SetHeader(TEXT("Authorization"), Url.StartsWith(TEXT("https://id.twitch.tv")) ? TEXT("OAuth ") + Token : TEXT("Bearer ") + Token);
	if (!Body.IsEmpty())
	{
		Http->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Http->SetContentAsString(Body);
	}
	TWeakPtr<FTwitchApi> WeakSelf = AsShared();
	Http->OnProcessRequestComplete().BindLambda([WeakSelf, OnDone, Verb, Url](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
	{
		if (!WeakSelf.IsValid())
		{
			return;
		}
		const int32 Code = bOk && Response ? Response->GetResponseCode() : 0;
		TSharedPtr<FJsonObject> Json;
		if (Response)
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Json);
			if (Code >= 300)
			{
				UE_LOG(LogChatControl, Warning, TEXT("Twitch API %s %s -> %d %s"), *Verb, *Url.Left(120), Code, *Response->GetContentAsString().Left(300));
			}
		}
		OnDone(Code, Json);
	});
	Http->ProcessRequest();
}

void FTwitchApi::Tick(double Now)
{
	if (ClientId.IsEmpty() || bAuthInFlight)
	{
		return;
	}
	if (Token.IsEmpty())
	{
		if (!DeviceCode.IsEmpty() && Now < DeviceExpires)
		{
			if (Now >= NextDevicePoll)
			{
				PollDeviceLogin();
			}
		}
		else if (Now >= NextValidate)
		{
			// No token, or the previous code expired: ask for a new code
			NextValidate = Now + 30.0;
			DeviceCode.Empty();
			StartDeviceLogin();
		}
		return;
	}
	// Twitch requires validating tokens hourly
	if (Now >= NextValidate && !bValidating)
	{
		NextValidate = Now + 3600.0;
		Validate();
	}
	if (!IsReady())
	{
		return;
	}
	if (bFollows && Now >= NextFollowers && !bFollowersInFlight)
	{
		NextFollowers = Now + 30.0;
		PollFollowers();
	}
	if (OwnedRewards.Num() > 0 && Now >= NextRedemptions && RedemptionsInFlight == 0)
	{
		NextRedemptions = Now + 4.0;
		PollRedemptions();
	}
	if (!PollId.IsEmpty() && Now >= NextPollCheck && !bPollCheckInFlight)
	{
		NextPollCheck = Now + 3.0;
		CheckPoll();
	}
}

void FTwitchApi::Validate()
{
	bValidating = true;
	Request(TEXT("GET"), TEXT("https://id.twitch.tv/oauth2/validate"), FString(), [this](int32 Code, TSharedPtr<FJsonObject> Json)
	{
		bValidating = false;
		if (Code == 200 && Json)
		{
			BroadcasterId = Json->GetStringField(TEXT("user_id"));
			Login = Json->GetStringField(TEXT("login"));
			UE_LOG(LogChatControl, Log, TEXT("Twitch API: logged in as %s"), *Login);
			return;
		}
		if (Code == 401)
		{
			BroadcasterId.Empty();
			if (!Refresh.IsEmpty())
			{
				RefreshToken();
			}
			else
			{
				Token.Empty();
				OnTokens(FString(), FString());
			}
		}
		else
		{
			// Network trouble: try again soon
			NextValidate = FPlatformTime::Seconds() + 60.0;
		}
	});
}

void FTwitchApi::PollFollowers()
{
	bFollowersInFlight = true;
	Request(TEXT("GET"), Helix + TEXT("channels/followers?first=20&broadcaster_id=") + BroadcasterId, FString(), [this](int32 Code, TSharedPtr<FJsonObject> Json)
	{
		bFollowersInFlight = false;
		const TArray<TSharedPtr<FJsonValue>>* Data = DataOf(Json);
		if (Code != 200 || !Data)
		{
			return;
		}
		// Newest first; on the first call only remember where the list starts
		const bool bFirst = LastFollowedAt.IsEmpty();
		FString Newest = LastFollowedAt;
		for (int32 i = Data->Num() - 1; i >= 0; --i)
		{
			const TSharedPtr<FJsonObject> Follow = (*Data)[i]->AsObject();
			const FString At = Follow->GetStringField(TEXT("followed_at"));
			if (!bFirst && At > LastFollowedAt)
			{
				FChatEvent Event;
				Event.Type = EChatEventType::Follow;
				Event.Platform = TEXT("twitch");
				Event.UserId = Follow->GetStringField(TEXT("user_id"));
				Event.DisplayName = Follow->GetStringField(TEXT("user_name"));
				Output.Enqueue(MoveTemp(Event));
			}
			Newest = At > Newest ? At : Newest;
		}
		LastFollowedAt = Newest.IsEmpty() ? TEXT("0") : Newest;
	});
}

void FTwitchApi::PollRedemptions()
{
	for (const auto& Pair : OwnedRewards)
	{
		const FString RewardId = Pair.Key;
		++RedemptionsInFlight;
		Request(TEXT("GET"), FString::Printf(TEXT("%schannel_points/custom_rewards/redemptions?status=UNFULFILLED&first=20&broadcaster_id=%s&reward_id=%s"), *Helix, *BroadcasterId, *RewardId), FString(),
			[this, RewardId](int32 Code, TSharedPtr<FJsonObject> Json)
		{
			--RedemptionsInFlight;
			const TArray<TSharedPtr<FJsonValue>>* Data = DataOf(Json);
			if (Code != 200 || !Data)
			{
				return;
			}
			for (const TSharedPtr<FJsonValue>& Value : *Data)
			{
				const TSharedPtr<FJsonObject> Redemption = Value->AsObject();
				const FString Id = Redemption->GetStringField(TEXT("id"));
				if (HandledRedemptions.Contains(Id))
				{
					continue;
				}
				HandledRedemptions.Add(Id);
				FChatEvent Event;
				Event.Type = EChatEventType::RewardRedeem;
				Event.Platform = TEXT("twitch");
				Event.UserId = Redemption->GetStringField(TEXT("user_id"));
				Event.DisplayName = Redemption->GetStringField(TEXT("user_name"));
				Event.RewardId = RewardId;
				Redemption->TryGetStringField(TEXT("user_input"), Event.Text);
				Output.Enqueue(MoveTemp(Event));

				// Mark it done on Twitch so it leaves the streamer's queue
				TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
				Body->SetStringField(TEXT("status"), TEXT("FULFILLED"));
				Request(TEXT("PATCH"), FString::Printf(TEXT("%schannel_points/custom_rewards/redemptions?id=%s&broadcaster_id=%s&reward_id=%s"), *Helix, *Id, *BroadcasterId, *RewardId),
					ToJson(Body), [](int32, TSharedPtr<FJsonObject>) {});
			}
		});
	}
}

void FTwitchApi::SyncRewards(const TMap<FString, TPair<FString, int32>>& Wanted, const TMap<FString, FString>& Known, TFunction<void(const TMap<FString, FString>&)> OnDone)
{
	if (!IsReady())
	{
		return;
	}
	TWeakPtr<FTwitchApi> WeakSelf = AsShared();
	Request(TEXT("GET"), Helix + TEXT("channel_points/custom_rewards?only_manageable_rewards=true&broadcaster_id=") + BroadcasterId, FString(),
		[this, WeakSelf, Wanted, Known, OnDone](int32 Code, TSharedPtr<FJsonObject> Json)
	{
		const TArray<TSharedPtr<FJsonValue>>* Data = DataOf(Json);
		if (Code != 200 || !Data)
		{
			// 403 = the channel has no channel points (not Affiliate/Partner)
			return;
		}
		TSet<FString> Existing;
		for (const TSharedPtr<FJsonValue>& Value : *Data)
		{
			Existing.Add(Value->AsObject()->GetStringField(TEXT("id")));
		}

		TSharedRef<TMap<FString, FString>> Result = MakeShared<TMap<FString, FString>>();
		TSharedRef<int32> Pending = MakeShared<int32>(1);
		auto Finish = [this, Result, Pending, OnDone]
		{
			if (--*Pending == 0)
			{
				OwnedRewards.Reset();
				for (const auto& Pair : *Result)
				{
					OwnedRewards.Add(Pair.Value, Pair.Key);
				}
				OnDone(*Result);
			}
		};

		for (const auto& Pair : Wanted)
		{
			const FString EffectId = Pair.Key;
			TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
			Body->SetStringField(TEXT("title"), Pair.Value.Key.Left(45));
			Body->SetNumberField(TEXT("cost"), FMath::Max(1, Pair.Value.Value));
			const FString* KnownId = Known.Find(EffectId);
			++*Pending;
			if (KnownId && Existing.Contains(*KnownId))
			{
				Existing.Remove(*KnownId);
				const FString Id = *KnownId;
				Request(TEXT("PATCH"), FString::Printf(TEXT("%schannel_points/custom_rewards?broadcaster_id=%s&id=%s"), *Helix, *BroadcasterId, *Id), ToJson(Body),
					[Result, EffectId, Id, Finish](int32 PatchCode, TSharedPtr<FJsonObject>) { if (PatchCode == 200) { Result->Add(EffectId, Id); } Finish(); });
			}
			else
			{
				Request(TEXT("POST"), Helix + TEXT("channel_points/custom_rewards?broadcaster_id=") + BroadcasterId, ToJson(Body),
					[Result, EffectId, Finish](int32 PostCode, TSharedPtr<FJsonObject> Created)
				{
					const TArray<TSharedPtr<FJsonValue>>* CreatedData = DataOf(Created);
					if (PostCode == 200 && CreatedData && CreatedData->Num() > 0)
					{
						Result->Add(EffectId, (*CreatedData)[0]->AsObject()->GetStringField(TEXT("id")));
					}
					Finish();
				});
			}
		}
		// Rewards the mod made earlier that are no longer wanted
		for (const FString& Id : Existing)
		{
			Request(TEXT("DELETE"), FString::Printf(TEXT("%schannel_points/custom_rewards?broadcaster_id=%s&id=%s"), *Helix, *BroadcasterId, *Id), FString(),
				[](int32, TSharedPtr<FJsonObject>) {});
		}
		Finish();
	});
}

void FTwitchApi::StartPoll(const FString& Title, const TArray<FString>& Choices, int32 DurationSeconds, TFunction<void(const TArray<int32>&, bool)> OnUpdate)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("broadcaster_id"), BroadcasterId);
	Body->SetStringField(TEXT("title"), Title.Left(60));
	Body->SetNumberField(TEXT("duration"), FMath::Clamp(DurationSeconds, 15, 1800));
	TArray<TSharedPtr<FJsonValue>> ChoiceArray;
	for (const FString& Choice : Choices)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("title"), Choice.Left(25));
		ChoiceArray.Add(MakeShared<FJsonValueObject>(Item));
	}
	Body->SetArrayField(TEXT("choices"), ChoiceArray);

	const int32 NumChoices = Choices.Num();
	Request(TEXT("POST"), Helix + TEXT("polls"), ToJson(Body), [this, OnUpdate, NumChoices](int32 Code, TSharedPtr<FJsonObject> Json)
	{
		const TArray<TSharedPtr<FJsonValue>>* Data = DataOf(Json);
		if (Code != 200 || !Data || Data->Num() == 0)
		{
			TArray<int32> Failed;
			Failed.Init(-1, NumChoices);
			OnUpdate(Failed, true);
			return;
		}
		PollId = (*Data)[0]->AsObject()->GetStringField(TEXT("id"));
		PollCallback = OnUpdate;
		NextPollCheck = FPlatformTime::Seconds() + 3.0;
	});
}

void FTwitchApi::CheckPoll()
{
	bPollCheckInFlight = true;
	Request(TEXT("GET"), FString::Printf(TEXT("%spolls?broadcaster_id=%s&id=%s"), *Helix, *BroadcasterId, *PollId), FString(), [this](int32 Code, TSharedPtr<FJsonObject> Json)
	{
		bPollCheckInFlight = false;
		const TArray<TSharedPtr<FJsonValue>>* Data = DataOf(Json);
		if (Code != 200 || !Data || Data->Num() == 0 || !PollCallback)
		{
			return;
		}
		const TSharedPtr<FJsonObject> Poll = (*Data)[0]->AsObject();
		TArray<int32> Votes;
		for (const TSharedPtr<FJsonValue>& Choice : Poll->GetArrayField(TEXT("choices")))
		{
			Votes.Add((int32)Choice->AsObject()->GetNumberField(TEXT("votes")));
		}
		const FString Status = Poll->GetStringField(TEXT("status"));
		const bool bFinished = Status != TEXT("ACTIVE");
		TFunction<void(const TArray<int32>&, bool)> Callback = PollCallback;
		if (bFinished)
		{
			PollId.Empty();
			PollCallback = nullptr;
		}
		Callback(Votes, bFinished);
	});
}
