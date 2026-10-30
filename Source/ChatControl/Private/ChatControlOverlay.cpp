// Local data endpoint for OBS widgets: http://localhost:<port>/state returns JSON.
// The widgets themselves are plain HTML files the streamer adds as OBS browser sources.
#include "ChatControlSubsystem.h"
#include "ChatControl.h"
#include "Twitch/TwitchIrcConnector.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

void UChatControlSubsystem::StartOverlay()
{
	if (!Settings.bOverlayEnabled || Settings.OverlayPort <= 0)
	{
		return;
	}
	TSharedPtr<IHttpRouter> Router = FHttpServerModule::Get().GetHttpRouter(Settings.OverlayPort, false);
	if (!Router)
	{
		UE_LOG(LogChatControl, Warning, TEXT("Overlay: port %d is unavailable"), Settings.OverlayPort);
		return;
	}
	TWeakObjectPtr<UChatControlSubsystem> WeakThis = this;
	OverlayRoute = Router->BindRoute(FHttpPath(TEXT("/state")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateLambda([WeakThis](const FHttpServerRequest&, const FHttpResultCallback& OnComplete)
		{
			const FString Json = WeakThis.IsValid() ? WeakThis->BuildOverlayJson() : TEXT("{}");
			TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(Json, TEXT("application/json; charset=utf-8"));
			// Widgets are opened as local files, so the browser needs CORS permission
			Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
			Response->Headers.Add(TEXT("Cache-Control"), { TEXT("no-store") });
			OnComplete(MoveTemp(Response));
			return true;
		}));
	OverlayRouter = Router;
	FHttpServerModule::Get().StartAllListeners();
	UE_LOG(LogChatControl, Log, TEXT("Overlay: http://localhost:%d/state"), Settings.OverlayPort);
}

void UChatControlSubsystem::StopOverlay()
{
	if (TSharedPtr<IHttpRouter> Router = OverlayRouter.Pin(); Router && OverlayRoute.IsValid())
	{
		Router->UnbindRoute(OverlayRoute);
	}
	OverlayRoute.Reset();
	OverlayRouter.Reset();
}

void UChatControlSubsystem::PushOverlayEvent(const FString& Text) const
{
	OverlayEvents.Add({ ++OverlayEventCounter, Text });
	if (OverlayEvents.Num() > 20)
	{
		OverlayEvents.RemoveAt(0);
	}
}

FString UChatControlSubsystem::BuildOverlayJson() const
{
	// Requests arrive on the game thread (the HTTP server is ticked by the engine), so state can be read directly
	const double Now = FPlatformTime::Seconds();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> EventsJson;
	for (const TPair<int32, FString>& Event : OverlayEvents)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetNumberField(TEXT("id"), Event.Key);
		Item->SetStringField(TEXT("text"), Event.Value);
		EventsJson.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(TEXT("events"), EventsJson);

	TSharedRef<FJsonObject> Vote = MakeShared<FJsonObject>();
	Vote->SetBoolField(TEXT("active"), bVoteActive);
	Vote->SetNumberField(TEXT("secondsLeft"), bVoteActive ? FMath::Max(0.0, VoteEndTime - Now) : 0.0);
	Vote->SetNumberField(TEXT("duration"), Settings.VoteDurationSeconds);
	TArray<TSharedPtr<FJsonValue>> Options;
	for (int32 i = 0; i < VoteOptions.Num(); ++i)
	{
		int32 Count = 0;
		for (const auto& Pair : Votes)
		{
			Count += Pair.Value == i ? 1 : 0;
		}
		TSharedRef<FJsonObject> Option = MakeShared<FJsonObject>();
		Option->SetStringField(TEXT("effect"), VoteOptions[i]);
		Option->SetNumberField(TEXT("votes"), Count);
		Options.Add(MakeShared<FJsonValueObject>(Option));
	}
	Vote->SetArrayField(TEXT("options"), Options);
	Root->SetObjectField(TEXT("vote"), Vote);

	TSharedRef<FJsonObject> Pot = MakeShared<FJsonObject>();
	Pot->SetBoolField(TEXT("enabled"), Settings.bGoalEnabled);
	Pot->SetStringField(TEXT("effect"), Settings.GoalEffect);
	Pot->SetNumberField(TEXT("current"), Economy.GetPot());
	Pot->SetNumberField(TEXT("goal"), Settings.GoalChips);
	Root->SetObjectField(TEXT("pot"), Pot);

	TArray<TSharedPtr<FJsonValue>> Top;
	for (const TPair<FString, int32>& Entry : Economy.GetTopSpenders(5))
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Entry.Key);
		Item->SetNumberField(TEXT("chips"), Entry.Value);
		Top.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(TEXT("top"), Top);

	TSharedRef<FJsonObject> Chaos = MakeShared<FJsonObject>();
	Chaos->SetNumberField(TEXT("queued"), ChaosQueue.Num());
	Chaos->SetBoolField(TEXT("paused"), bChaosPaused);
	Chaos->SetBoolField(TEXT("connected"), Twitch.IsValid() && Twitch->IsConnected());
	Root->SetObjectField(TEXT("chaos"), Chaos);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}
