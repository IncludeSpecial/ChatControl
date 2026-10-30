// Local data endpoint for OBS widgets: http://localhost:<port>/state returns JSON,
// and http://localhost:<port>/obs/<file> serves the widget files themselves.
#include "ChatControlSubsystem.h"
#include "ChatControl.h"
#include "Twitch/TwitchIrcConnector.h"
#include "YouTube/YouTubeChatConnector.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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
	// OBS blocks local-file pages from reaching localhost, so the widgets are served from here too
	OverlayFilesRoute = Router->BindRoute(FHttpPath(TEXT("/obs")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateLambda([WeakThis](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
		{
			OnComplete(WeakThis.IsValid() ? WeakThis->ServeOverlayFile(Request.RelativePath.GetPath())
				: FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound));
			return true;
		}));
	OverlayRouter = Router;
	FHttpServerModule::Get().StartAllListeners();
	UE_LOG(LogChatControl, Log, TEXT("Overlay: http://localhost:%d/state, widgets from %s"), Settings.OverlayPort, *GetOverlayFolder());
}

void UChatControlSubsystem::StopOverlay()
{
	if (TSharedPtr<IHttpRouter> Router = OverlayRouter.Pin(); Router && OverlayRoute.IsValid())
	{
		Router->UnbindRoute(OverlayRoute);
		Router->UnbindRoute(OverlayFilesRoute);
	}
	OverlayRoute.Reset();
	OverlayFilesRoute.Reset();
	OverlayRouter.Reset();
}

FString UChatControlSubsystem::GetOverlayFolder() const
{
	FString Folder = Settings.OverlayFolder.TrimStartAndEnd().TrimQuotes();
	if (Folder.IsEmpty())
	{
		Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ChatControl"), TEXT("Overlay"));
	}
	return FPaths::ConvertRelativePathToFull(Folder);
}

TUniquePtr<FHttpServerResponse> UChatControlSubsystem::ServeOverlayFile(const FString& RelativePath) const
{
	const FString Folder = GetOverlayFolder();
	FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(Folder, FGenericPlatformHttp::UrlDecode(RelativePath)));
	FPaths::NormalizeFilename(Path);
	FString Root = Folder;
	FPaths::NormalizeDirectoryName(Root);
	TArray<uint8> Bytes;
	// Only files inside the widget folder, never "../" outside it
	if (!Path.StartsWith(Root + TEXT("/")) || !FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
	{
		return FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound);
	}
	static const TMap<FString, FString> Types = {
		{ TEXT("html"), TEXT("text/html; charset=utf-8") }, { TEXT("js"), TEXT("text/javascript; charset=utf-8") },
		{ TEXT("css"), TEXT("text/css; charset=utf-8") }, { TEXT("json"), TEXT("application/json") },
		{ TEXT("png"), TEXT("image/png") }, { TEXT("jpg"), TEXT("image/jpeg") }, { TEXT("svg"), TEXT("image/svg+xml") },
		{ TEXT("webp"), TEXT("image/webp") }, { TEXT("gif"), TEXT("image/gif") }, { TEXT("ttf"), TEXT("font/ttf") },
		{ TEXT("otf"), TEXT("font/otf") }, { TEXT("woff2"), TEXT("font/woff2") }, { TEXT("mp3"), TEXT("audio/mpeg") },
		{ TEXT("wav"), TEXT("audio/wav") }, { TEXT("webm"), TEXT("video/webm") }, { TEXT("mp4"), TEXT("video/mp4") } };
	const FString* Type = Types.Find(FPaths::GetExtension(Path).ToLower());
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(MoveTemp(Bytes), Type ? *Type : TEXT("application/octet-stream"));
	Response->Headers.Add(TEXT("Cache-Control"), { TEXT("no-store") });
	return Response;
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
		if (bPollMode && PollVotes.IsValidIndex(i))
		{
			Count = PollVotes[i];
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
	Chaos->SetBoolField(TEXT("youtube"), YouTube.IsValid() && YouTube->IsConnected());
	Root->SetObjectField(TEXT("chaos"), Chaos);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}
