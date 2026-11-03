#include "ChatControlSubsystem.h"
#include "ChatControl.h"
#include "Twitch/TwitchIrcConnector.h"
#include "YouTube/YouTubeChatConnector.h"
#include "Twitch/TwitchApi.h"
#include "ChatControlModConfig.h"
#include "Configuration/ConfigManager.h"
#include "FGChatManager.h"
#include "FGCharacterPlayer.h"
#include "Creature/FGCreature.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Buildables/FGBuildableTradingPost.h"
#include "Components/WidgetComponent.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "ChatText.h"

using ChatText::Tr;

bool UChatControlSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Chat is handled by whoever runs the game logic; a dedicated server has no streamer
	return !IsRunningDedicatedServer();
}

void UChatControlSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Settings = FChatControlSettings::LoadOrCreate();
	ChatText::SetLanguage(Settings.Language);
	Economy.Load();
	Effects = ChatEffects::CreateAll();

	// The in-game settings page overrides the basic options from Settings.json
	ConfigManager = Collection.InitializeDependency<UConfigManager>();
	if (ConfigManager)
	{
		UChatControlModConfig::Register(ConfigManager, Settings);
		if (UChatControlModConfig::ApplyTo(ConfigManager, Settings))
		{
			Settings.Save();
		}
	}

	const double Now = FPlatformTime::Seconds();
	NextChipsPayout = Now + Settings.ChipsIntervalSeconds;
	NextEconomySave = Now + 60.0;
	NextConfigCheck = Now + 1.0;
	LastTriggerTime = Now;
	SessionStartTime = Now;
	NextVoteTime = Now + Settings.VoteIntervalMinutes * 60.0;

	RestartConnectors();
	StartOverlay();

	bInitialized = true;
	UE_LOG(LogChatControl, Log, TEXT("ChatControl initialized, settings: %s"), *FChatControlSettings::GetSettingsPath());
}

void UChatControlSubsystem::Deinitialize()
{
	bInitialized = false;
	StopOverlay();
	// Destroying the connector stops and joins its thread before IncomingEvents goes away
	Twitch.Reset();
	for (auto& Pair : TimedEffects)
	{
		Pair.Value.Revert();
	}
	TimedEffects.Empty();
	Economy.Save();
	SaveSessionSummary();
	Super::Deinitialize();
}

void UChatControlSubsystem::RestartConnectors()
{
	// Destroying the connector stops and joins its thread
	Twitch.Reset();
	YouTube.Reset();
	TwitchApi.Reset();
	bTwitchApiWasReady = false;
	if (Settings.bEnabled && !Settings.TwitchClientId.IsEmpty())
	{
		TWeakObjectPtr<UChatControlSubsystem> WeakThis = this;
		TwitchApi = MakeShared<FTwitchApi>(Settings.TwitchClientId, Settings.TwitchToken, Settings.TwitchRefreshToken, IncomingEvents,
			[WeakThis](const FString& Access, const FString& Refresh)
			{
				if (!WeakThis.IsValid())
				{
					return;
				}
				WeakThis->Settings.TwitchToken = Access;
				WeakThis->Settings.TwitchRefreshToken = Refresh;
				WeakThis->Settings.Save();
				if (Access.IsEmpty())
				{
					WeakThis->Announce(Tr(TEXT("Twitch: login expired, a new code is coming"), TEXT("Twitch: вход истёк, сейчас придёт новый код")));
				}
			},
			[WeakThis](const FString& Code, const FString& Url)
			{
				if (!WeakThis.IsValid())
				{
					return;
				}
				WeakThis->Announce(FText::Format(Tr(TEXT("Twitch login: open {0} and enter the code {1}"), TEXT("Вход в Twitch: открой {0} и введи код {1}")),
					FText::FromString(Url), FText::FromString(Code)));
				// The activation page accepts the code in the link, so usually one click is enough
				FPlatformProcess::LaunchURL(*(Url + TEXT("?device-code=") + Code), nullptr, nullptr);
			});
	}
	if (Settings.bEnabled && !Settings.YouTubeApiKey.IsEmpty() && !Settings.YouTubeChannel.IsEmpty())
	{
		YouTube = MakeShared<FYouTubeChatConnector>(Settings.YouTubeApiKey, Settings.YouTubeChannel, Settings.YouTubePollSeconds, IncomingEvents);
	}
	if (Settings.bEnabled && !Settings.TwitchChannel.IsEmpty())
	{
		Twitch = MakeShared<FTwitchIrcConnector>(Settings.TwitchChannel, IncomingEvents);
		Twitch->Start();
	}
}

void UChatControlSubsystem::TickConfig(double Now)
{
	if (!ConfigManager || Now < NextConfigCheck)
	{
		return;
	}
	NextConfigCheck = Now + 1.0;

	const FString OldChannel = Settings.TwitchChannel;
	const FString OldYouTube = Settings.YouTubeApiKey + Settings.YouTubeChannel;
	const FString OldClientId = Settings.TwitchClientId;
	const bool bWasEnabled = Settings.bEnabled;
	const int32 OldPort = Settings.OverlayPort;
	const bool bOverlayWasEnabled = Settings.bOverlayEnabled;
	bool bPortRejected = false;
	if (!UChatControlModConfig::ApplyTo(ConfigManager, Settings, &bPortRejected))
	{
		return;
	}
	if (bPortRejected)
	{
		Announce(FText::Format(Tr(TEXT("That OBS port can't be used. Back to {0}."), TEXT("Такой порт для OBS использовать нельзя. Вернул {0}.")), FText::AsNumber(Settings.OverlayPort, &FNumberFormattingOptions::DefaultNoGrouping())));
	}
	if (Settings.OverlayPort != OldPort || Settings.bOverlayEnabled != bOverlayWasEnabled)
	{
		StopOverlay();
		StartOverlay();
	}
	Settings.Save();
	ChatText::SetLanguage(Settings.Language);
	if (Settings.TwitchChannel != OldChannel || Settings.bEnabled != bWasEnabled || Settings.YouTubeApiKey + Settings.YouTubeChannel != OldYouTube || Settings.TwitchClientId != OldClientId)
	{
		RestartConnectors();
	}
	UE_LOG(LogChatControl, Log, TEXT("Settings changed in the mod config menu"));
	RewardSyncTime = Now + 5.0;
}

void UChatControlSubsystem::TickTwitchApi(double Now)
{
	if (!TwitchApi)
	{
		return;
	}
	TwitchApi->SetFollowsEnabled(Settings.bFollowsEnabled);
	TwitchApi->Tick(Now);
	const bool bReady = TwitchApi->IsReady();
	if (bReady && !bTwitchApiWasReady)
	{
		Announce(FText::Format(Tr(TEXT("Twitch: logged in as {0}"), TEXT("Twitch: вход выполнен, {0}")), FText::FromString(TwitchApi->GetLogin())));
		RewardSyncTime = Now;
	}
	bTwitchApiWasReady = bReady;
	if (bReady && RewardSyncTime > 0.0 && Now >= RewardSyncTime)
	{
		RewardSyncTime = 0.0;
		SyncTwitchRewards();
	}
}

void UChatControlSubsystem::SyncTwitchRewards()
{
	TMap<FString, TPair<FString, int32>> Wanted;
	if (Settings.bAutoRewards)
	{
		for (const auto& Pair : Settings.Effects)
		{
			if (Pair.Value.bEnabled && Pair.Value.ChannelPointsCost > 0 && Effects.Contains(Pair.Key))
			{
				Wanted.Add(Pair.Key, { FString::Printf(TEXT("ChatControl: !%s"), *Pair.Key), Pair.Value.ChannelPointsCost });
			}
		}
	}
	TWeakObjectPtr<UChatControlSubsystem> WeakThis = this;
	TwitchApi->SyncRewards(Wanted, Settings.AutoRewardIds, [WeakThis](const TMap<FString, FString>& Created)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		FChatControlSettings& S = WeakThis->Settings;
		for (const auto& Pair : S.AutoRewardIds)
		{
			S.RewardEffects.Remove(Pair.Value);
		}
		S.AutoRewardIds = Created;
		for (const auto& Pair : Created)
		{
			S.RewardEffects.Add(Pair.Value, Pair.Key);
		}
		S.Save();
		WeakThis->Announce(FText::Format(Tr(TEXT("Twitch: {0} channel point rewards are ready"), TEXT("Twitch: награды за баллы канала готовы ({0})")), FText::AsNumber(Created.Num())));
	});
}

void UChatControlSubsystem::RunDelayed(float Delay, TFunction<void()> Action)
{
	DelayedActions.Add({ FPlatformTime::Seconds() + Delay, MoveTemp(Action) });
}

void UChatControlSubsystem::TickDelayedActions(double Now)
{
	// Run due actions after removing them, so an action may safely schedule another one
	TArray<TFunction<void()>> Due;
	for (int32 i = DelayedActions.Num() - 1; i >= 0; --i)
	{
		if (Now >= DelayedActions[i].Key)
		{
			Due.Add(MoveTemp(DelayedActions[i].Value));
			DelayedActions.RemoveAt(i);
		}
	}
	for (int32 i = Due.Num() - 1; i >= 0; --i)
	{
		Due[i]();
	}
}

UWorld* UChatControlSubsystem::GetGameWorld() const
{
	return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

TStatId UChatControlSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UChatControlSubsystem, STATGROUP_Tickables);
}

void UChatControlSubsystem::Tick(float DeltaTime)
{
	const double Now = FPlatformTime::Seconds();

	if (YouTube)
	{
		YouTube->Tick(Now);
	}
	TickTwitchApi(Now);

	FChatEvent Event;
	while (IncomingEvents.Dequeue(Event))
	{
		HandleEvent(Event);
	}

	if (const AFGCharacterPlayer* Player = FindPlayer())
	{
		const bool bDead = !Player->IsAliveAndWell();
		if (bDead)
		{
			LastDeathTime = Now;
			SessionDeaths += bPlayerWasDead ? 0 : 1;
		}
		bPlayerWasDead = bDead;
	}

	TickConfig(Now);
	TickWorldData(Now);
	TickEconomy(Now);
	TickTimedEffects(Now);
	TickDelayedActions(Now);
	TickBoss(Now);
	TickVotes(Now);
	TickNameTags();
	TickChaosQueue(Now);
	TickAutoChaos(Now);
}

void UChatControlSubsystem::Announce(const FText& Message) const
{
	UE_LOG(LogChatControl, Log, TEXT("%s"), *Message.ToString());
	PushOverlayEvent(Message.ToString());
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (AFGChatManager* Chat = World ? AFGChatManager::Get(World) : nullptr)
	{
		FChatMessageStruct ChatMessage;
		ChatMessage.MessageType = EFGChatMessageType::CMT_CustomMessage;
		ChatMessage.MessageSender = FText::FromString(TEXT("ChatControl"));
		ChatMessage.MessageSenderColor = FLinearColor(0.57f, 0.27f, 1.f); // Twitch purple
		ChatMessage.MessageText = Message;
		// The chat sorts by timestamp: without it the message lands at the top of history and never shows as new
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			ChatMessage.ServerTimeStamp = GameState->GetServerWorldTimeSeconds();
		}
		Chat->AddChatMessageToReceived(ChatMessage);
	}
}

void UChatControlSubsystem::StartTimedEffect(const FString& Slot, float Duration, TFunction<void()> Maintain, TFunction<void()> Revert)
{
	if (FTimedEffect* Active = TimedEffects.Find(Slot))
	{
		Active->Revert();
	}
	Maintain();
	TimedEffects.Add(Slot, FTimedEffect{ FPlatformTime::Seconds() + Duration, MoveTemp(Maintain), MoveTemp(Revert) });
}

void UChatControlSubsystem::HandleEvent(const FChatEvent& Event)
{
	if (!Settings.bEnabled || Settings.IgnoredUsers.Contains(Event.DisplayName.ToLower()))
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	const FString Key = Event.GetEconomyKey();
	const FText Name = FText::FromString(Event.DisplayName);
	if (Event.Type != EChatEventType::Raid && Event.Type != EChatEventType::Follow)
	{
		ViewerRoles.Add(Event.DisplayName.ToLower(), Event.bIsBroadcaster ? 4 : Event.bIsModerator ? 3 : Event.bIsVip ? 2 : Event.bIsSubscriber ? 1 : 0);
	}

	switch (Event.Type)
	{
	case EChatEventType::Message:
	{
		const FChatViewerRecord* Record = Economy.Find(Key);
		const bool bFirstMessage = !Record || !Record->bGreetedThisSession;
		Economy.OnViewerMessage(Key, Event.DisplayName, Now, Settings.FirstMessageBonus);
		if (bFirstMessage && !Event.bIsBroadcaster)
		{
			WelcomeViewer(Event.DisplayName);
		}
		HandleCommand(Event);
		break;
	}

	case EChatEventType::Bits:
	{
		Economy.OnViewerMessage(Key, Event.DisplayName, Now, Settings.FirstMessageBonus);
		const int32 Chips = Event.Amount * Settings.ChipsPerBit;
		Economy.AddChips(Key, Event.DisplayName, Chips);
		Announce(FText::Format(Tr(TEXT("{0} cheered {1} bits and got {2} chips!"), TEXT("{0} отправил(а) {1} бит и получил(а) {2} чипов!")), Name, FText::AsNumber(Event.Amount), FText::AsNumber(Chips)));
		// A cheer message may also contain a command, now paid with the fresh chips
		HandleCommand(Event);
		break;
	}

	case EChatEventType::SuperChat:
	{
		Economy.OnViewerMessage(Key, Event.DisplayName, Now, Settings.FirstMessageBonus);
		const int32 Chips = Event.Amount * Settings.ChipsPerSuperChatUnit;
		Economy.AddChips(Key, Event.DisplayName, Chips);
		Announce(FText::Format(Tr(TEXT("{0} sent a Super Chat and got {1} chips!"), TEXT("{0} отправил(а) Super Chat и получил(а) {1} чипов!")), Name, FText::AsNumber(Chips)));
		HandleCommand(Event);
		break;
	}

	case EChatEventType::Follow:
	{
		Economy.AddChips(Key, Event.DisplayName, Settings.ChipsPerFollow);
		Announce(FText::Format(Tr(TEXT("{0} followed and got {1} chips!"), TEXT("{0} фолловит канал и получает {1} чипов!")), Name, FText::AsNumber(Settings.ChipsPerFollow)));
		if (!Settings.FollowEffect.IsEmpty() && Effects.Contains(Settings.FollowEffect))
		{
			RequestEffect(Settings.FollowEffect, Key, Event.DisplayName, true);
		}
		break;
	}

	case EChatEventType::RewardRedeem:
	{
		Economy.OnViewerMessage(Key, Event.DisplayName, Now, Settings.FirstMessageBonus);
		if (const FString* EffectId = Settings.RewardEffects.Find(Event.RewardId))
		{
			RequestEffect(*EffectId, Key, Event.DisplayName, true);
		}
		else
		{
			Announce(FText::Format(Tr(TEXT("Unknown channel points reward id {0}. Map it to an effect in Settings.json (RewardEffects)."), TEXT("Неизвестная награда за баллы канала, ID {0}. Привяжи её к эффекту в Settings.json (RewardEffects).")), FText::FromString(Event.RewardId)));
		}
		break;
	}

	case EChatEventType::Subscription:
		Economy.AddChips(Key, Event.DisplayName, Settings.ChipsPerSub);
		Announce(FText::Format(Tr(TEXT("{0} subscribed and got {1} chips! Thank you!"), TEXT("{0} подписался(ась) и получил(а) {1} чипов! Спасибо!")), Name, FText::AsNumber(Settings.ChipsPerSub)));
		if (Settings.bPetOnSub)
		{
			SpawnPet(Key, Event.DisplayName);
		}
		break;

	case EChatEventType::GiftSubs:
	{
		const int32 Chips = Settings.ChipsPerSub * Event.Amount;
		Economy.AddChips(Key, Event.DisplayName, Chips);
		Announce(FText::Format(Tr(TEXT("{0} gifted {1} subs and got {2} chips!"), TEXT("{0} подарил(а) {1} подписок и получил(а) {2} чипов!")), Name, FText::AsNumber(Event.Amount), FText::AsNumber(Chips)));
		break;
	}

	case EChatEventType::Raid:
	{
		Announce(FText::Format(Tr(TEXT("{0} is raiding with {1} viewers! Brace yourself!"), TEXT("{0} врывается с рейдом на {1} зрителей! Держись!")), Name, FText::AsNumber(Event.Amount)));
		auto IsEnabled = [this](const TCHAR* EffectId)
		{
			const FChatEffectSettings* EffectSettings = Settings.Effects.Find(EffectId);
			return EffectSettings && EffectSettings->bEnabled && Effects.Contains(EffectId);
		};

		// One ambush per 5 raiders (max 5), plus a boss for big raids
		const int32 Waves = FMath::Clamp(Event.Amount / FMath::Max(1, Settings.RaidViewersPerAmbush), 1, FMath::Max(1, Settings.RaidMaxAmbushes));
		if (IsEnabled(TEXT("ambush")))
		{
			for (int32 i = 0; i < Waves; ++i)
			{
				ChaosQueue.Add({ TEXT("ambush"), Key, Event.DisplayName, 0 });
			}
		}
		// Big raids bring a titan, medium ones a boss
		if (Settings.RaidTitanMinViewers > 0 && Event.Amount >= Settings.RaidTitanMinViewers && IsEnabled(TEXT("titan")))
		{
			ChaosQueue.Add({ TEXT("titan"), Key, Event.DisplayName, 0 });
		}
		else if (Settings.RaidBossMinViewers > 0 && Event.Amount >= Settings.RaidBossMinViewers && IsEnabled(TEXT("boss")))
		{
			ChaosQueue.Add({ TEXT("boss"), Key, Event.DisplayName, 0 });
		}
		break;
	}
	}
}

void UChatControlSubsystem::HandleCommand(const FChatEvent& Event)
{
	FString Text = Event.Text.TrimStartAndEnd();
	if ((Event.Type == EChatEventType::Bits || Event.Type == EChatEventType::SuperChat) && !Text.StartsWith(TEXT("!")))
	{
		// Cheer messages start with "Cheer100 ...", so look for the command anywhere
		const int32 CommandStart = Text.Find(TEXT(" !"));
		Text = CommandStart != INDEX_NONE ? Text.RightChop(CommandStart + 1) : FString();
	}
	if (!Text.StartsWith(TEXT("!")))
	{
		return;
	}

	FString Command, Args;
	if (!Text.RightChop(1).Split(TEXT(" "), &Command, &Args))
	{
		Command = Text.RightChop(1);
	}
	Command = Command.ToLower();
	const FString RawArgs = Args.TrimStartAndEnd();
	Args = RawArgs.ToLower();

	const FString Key = Event.GetEconomyKey();
	const FText Name = FText::FromString(Event.DisplayName);

	if (Command == TEXT("chips") || Command == TEXT("чипы"))
	{
		const FChatViewerRecord* Record = Economy.Find(Key);
		const FString Rank = GetRank(Record ? Record->ChipsSpent : 0);
		Announce(Rank.IsEmpty()
			? FText::Format(Tr(TEXT("{0} has {1} chips"), TEXT("У {0} {1} чипов")), Name, FText::AsNumber(Record ? Record->Chips : 0))
			: FText::Format(Tr(TEXT("{0} ({2}) has {1} chips"), TEXT("У {0} ({2}) {1} чипов")), Name, FText::AsNumber(Record ? Record->Chips : 0), FText::FromString(Rank)));
		return;
	}

	if ((Command == TEXT("give") || Command == TEXT("передать")) && Settings.bTransfersEnabled)
	{
		HandleGiveCommand(Event, RawArgs);
		return;
	}

	if ((Command == TEXT("daily") || Command == TEXT("бонус")) && Settings.DailyChips > 0)
	{
		if (Economy.ClaimDaily(Key, Event.DisplayName, Settings.DailyChips))
		{
			Announce(FText::Format(Tr(TEXT("{0} took the daily bonus: +{1} chips. See you tomorrow!"), TEXT("{0} забирает ежедневный бонус: +{1} чипов. До завтра!")), Name, FText::AsNumber(Settings.DailyChips)));
		}
		else
		{
			Announce(FText::Format(Tr(TEXT("{0}, the daily bonus is already taken today."), TEXT("{0}, ежедневный бонус сегодня уже получен.")), Name));
		}
		return;
	}

	if (Command == TEXT("effects") || Command == TEXT("help"))
	{
		const double Now = FPlatformTime::Seconds();
		TArray<FString> Parts;
		for (const auto& Pair : Settings.Effects)
		{
			if (Pair.Value.bEnabled && Effects.Contains(Pair.Key))
			{
				const int32 Cost = GetScaledCost(Pair.Value.Cost, Now);
				Parts.Add(Cost > 0 ? FString::Printf(TEXT("!%s (%d)"), *Pair.Key, Cost) : FString::Printf(TEXT("!%s"), *Pair.Key));
			}
		}
		Announce(FText::Format(Tr(TEXT("Commands: {0}"), TEXT("Команды: {0}")), FText::FromString(FString::Join(Parts, TEXT(", ")))));
		return;
	}

	const bool bCanModerate = Event.bIsBroadcaster || Event.bIsModerator;

	if (Command == TEXT("pause") && bCanModerate)
	{
		bChaosPaused = !bChaosPaused;
		Announce(bChaosPaused
			? Tr(TEXT("Chaos paused. Harmful effects will wait in the queue."), TEXT("Хаос на паузе. Вредные эффекты ждут в очереди."))
			: Tr(TEXT("Chaos resumed!"), TEXT("Хаос снова в деле!")));
		return;
	}

	if (Command == TEXT("panic") && bCanModerate)
	{
		Panic();
		return;
	}

	if ((Command == TEXT("clear") || Command == TEXT("убрать")) && bCanModerate)
	{
		FString Viewer = RawArgs;
		Viewer.RemoveFromStart(TEXT("@"));
		const int32 Removed = ClearSigns(Viewer);
		Announce(Viewer.IsEmpty()
			? FText::Format(Tr(TEXT("Signs removed: {0}"), TEXT("Убрано табличек: {0}")), FText::AsNumber(Removed))
			: FText::Format(Tr(TEXT("Signs by {0} removed: {1}"), TEXT("Убрано табличек {0}: {1}")), FText::FromString(Viewer), FText::AsNumber(Removed)));
		return;
	}

	if (Command == TEXT("test") && Event.bIsBroadcaster)
	{
		HandleTestCommand(Event, Args);
		return;
	}

	if (HandleGameCommand(Event, Command, Args))
	{
		return;
	}

	if (Effects.Contains(Command))
	{
		RequestEffect(Command, Key, Event.DisplayName, false, RawArgs);
	}
}

void UChatControlSubsystem::HandleGiveCommand(const FChatEvent& Event, const FString& Args)
{
	const FText Name = FText::FromString(Event.DisplayName);
	auto Number = [](int32 Value) { return FText::AsNumber(Value); };

	// "!give nick 100", "!give @nick 100" or "!give 100 nick"
	TArray<FString> Parts;
	Args.ParseIntoArrayWS(Parts);
	FString Target;
	int32 Amount = 0;
	for (FString Part : Parts)
	{
		if (Part.IsNumeric() && Amount == 0)
		{
			Amount = FCString::Atoi(*Part);
		}
		else if (Target.IsEmpty())
		{
			Part.RemoveFromStart(TEXT("@"));
			Target = Part;
		}
	}
	if (Target.IsEmpty() || Amount <= 0)
	{
		Announce(FText::Format(Tr(TEXT("{0}, use: !give <nick> <amount>"), TEXT("{0}, так: !give <ник> <сколько>")), Name));
		return;
	}

	const double Now = FPlatformTime::Seconds();
	const FString FromKey = Event.GetEconomyKey();
	if (const double* Until = TransferCooldownUntil.Find(FromKey); Until && Now < *Until)
	{
		Announce(FText::Format(Tr(TEXT("{0}, next transfer in {1} s."), TEXT("{0}, следующий перевод через {1} с.")), Name, Number(FMath::CeilToInt(*Until - Now))));
		return;
	}
	if (Amount < Settings.TransferMin || (Settings.TransferMax > 0 && Amount > Settings.TransferMax))
	{
		Announce(FText::Format(Tr(TEXT("{0}, a transfer is {1} to {2} chips."), TEXT("{0}, перевод - от {1} до {2} чипов.")), Name, Number(Settings.TransferMin), Number(Settings.TransferMax)));
		return;
	}
	const FString ToKey = Economy.FindKeyByName(Target, Event.Platform);
	if (ToKey.IsEmpty() || Settings.IgnoredUsers.Contains(Target.ToLower()))
	{
		Announce(FText::Format(Tr(TEXT("{0}, {1} hasn't chatted here yet."), TEXT("{0}, {1} ещё не писал(а) в этот чат.")), Name, FText::FromString(Target)));
		return;
	}
	if (ToKey == FromKey)
	{
		Announce(FText::Format(Tr(TEXT("{0}, you can't give chips to yourself."), TEXT("{0}, себе передать нельзя.")), Name));
		return;
	}

	const int32 Fee = FMath::Clamp(FMath::RoundToInt(Amount * FMath::Clamp(Settings.TransferFeePercent, 0, 100) / 100.f), 0, Amount);
	int32 Left = 0;
	const FText ToName = FText::FromString(Economy.Find(ToKey) ? Economy.Find(ToKey)->DisplayName : Target);
	switch (Economy.Transfer(FromKey, ToKey, Amount, Fee, Settings.TransferSendPerDay, Settings.TransferReceivePerDay, Left))
	{
	case EChipTransfer::Done:
		TransferCooldownUntil.Add(FromKey, Now + Settings.TransferCooldownSeconds);
		Announce(Fee > 0
			? FText::Format(Tr(TEXT("{0} gave {1} {2} chips ({3} went to the pot)."), TEXT("{0} передаёт {1} {2} чипов ({3} ушло в копилку).")), Name, ToName, Number(Amount - Fee), Number(Fee))
			: FText::Format(Tr(TEXT("{0} gave {1} {2} chips."), TEXT("{0} передаёт {1} {2} чипов.")), Name, ToName, Number(Amount)));
		break;
	case EChipTransfer::NotEnoughChips:
		Announce(FText::Format(Tr(TEXT("{0}, you only have {1} chips."), TEXT("{0}, у тебя только {1} чипов.")), Name, Number(Left)));
		break;
	case EChipTransfer::SenderDailyLimit:
		Announce(FText::Format(Tr(TEXT("{0}, daily send limit: {1} more chips today."), TEXT("{0}, дневной лимит отправки: сегодня можно ещё {1} чипов.")), Name, Number(Left)));
		break;
	case EChipTransfer::ReceiverDailyLimit:
		Announce(FText::Format(Tr(TEXT("{0}, {1} can only receive {2} more chips today."), TEXT("{0}, {1} сегодня может получить ещё только {2} чипов.")), Name, ToName, Number(Left)));
		break;
	}
}

void UChatControlSubsystem::HandleTestCommand(const FChatEvent& Event, const FString& Args)
{
	FString What, AmountText;
	if (!Args.Split(TEXT(" "), &What, &AmountText))
	{
		What = Args;
	}
	const int32 Amount = FCString::Atoi(*AmountText);

	// Simulated events go through the normal handler, so the whole pipeline is tested
	FChatEvent Fake = Event;
	Fake.Text.Empty();
	if (What == TEXT("chips"))
	{
		const int32 Chips = Amount > 0 ? Amount : 1000;
		Economy.AddChips(Event.GetEconomyKey(), Event.DisplayName, Chips);
		Announce(FText::Format(Tr(TEXT("[test] {0} got {1} chips"), TEXT("[тест] {0} получает {1} чипов")), FText::FromString(Event.DisplayName), FText::AsNumber(Chips)));
	}
	else if (What == TEXT("bits"))
	{
		Fake.Type = EChatEventType::Bits;
		Fake.Amount = Amount > 0 ? Amount : 100;
		HandleEvent(Fake);
	}
	else if (What == TEXT("sub"))
	{
		Fake.Type = EChatEventType::Subscription;
		Fake.Amount = 1;
		HandleEvent(Fake);
	}
	else if (What == TEXT("gifts"))
	{
		Fake.Type = EChatEventType::GiftSubs;
		Fake.Amount = Amount > 0 ? Amount : 5;
		HandleEvent(Fake);
	}
	else if (What == TEXT("raid"))
	{
		Fake.Type = EChatEventType::Raid;
		Fake.Amount = Amount > 0 ? Amount : 10;
		HandleEvent(Fake);
	}
	else if (What == TEXT("welcome"))
	{
		WelcomeViewer(Event.DisplayName);
	}
	else if (What == TEXT("autochaos"))
	{
		LastTriggerTime = -Settings.AutoChaosIdleSeconds;
	}
	else if (const FChatEffect* Effect = Effects.Find(What))
	{
		if (!ExecuteEffect(*Effect, Event.GetEconomyKey(), Event.DisplayName, AmountText))
		{
			Announce(Tr(TEXT("[test] The effect can't run right now: creatures and titans wait away from the HUB, one titan at a time."),
				TEXT("[тест] Эффект сейчас не сработал: существа и титаны не появляются у ХАБа, а титан может быть только один.")));
		}
	}
	else if (static const TSet<FString> ChatCommands = { TEXT("chips"), TEXT("чипы"), TEXT("daily"), TEXT("бонус"), TEXT("effects"), TEXT("help"),
		TEXT("pause"), TEXT("panic"), TEXT("clear"), TEXT("убрать"), TEXT("vote"), TEXT("голос"), TEXT("pot"), TEXT("копилка"),
		TEXT("top"), TEXT("топ"), TEXT("stats"), TEXT("статистика"), TEXT("give"), TEXT("передать"), TEXT("1"), TEXT("2"), TEXT("3") }; ChatCommands.Contains(What))
	{
		// "!test clear" and the like: ordinary commands just run as if typed without !test
		Fake.Text = TEXT("!") + What + (AmountText.IsEmpty() ? FString() : TEXT(" ") + AmountText);
		HandleCommand(Fake);
	}
	else
	{
		Announce(Tr(TEXT("[test] chips [N], bits [N], sub, gifts [N], raid [N], welcome, autochaos, or an effect name"), TEXT("[тест] chips [N], bits [N], sub, gifts [N], raid [N], welcome, autochaos или название эффекта")));
	}
}

void UChatControlSubsystem::RequestEffect(const FString& EffectId, const FString& ViewerKey, const FString& ViewerName, bool bFree, const FString& Args)
{
	const FChatEffect* Effect = Effects.Find(EffectId);
	const FChatEffectSettings* EffectSettings = Settings.Effects.Find(EffectId);
	if (!Effect || !EffectSettings || !EffectSettings->bEnabled)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	const FString ViewerCooldownKey = ViewerKey + TEXT("|") + EffectId;

	// Paid redemptions (bits, channel points, raids) skip cooldowns: the viewer already paid
	if (!bFree)
	{
		if (const double* Until = GlobalCooldownUntil.Find(EffectId); Until && *Until > Now)
		{
			return;
		}
		if (const double* Until = ViewerCooldownUntil.Find(ViewerCooldownKey); Until && *Until > Now)
		{
			return;
		}
	}

	const int32 Cost = bFree ? 0 : GetScaledCost(EffectSettings->Cost, Now);
	const FChatViewerRecord* SpenderBefore = Economy.Find(ViewerKey);
	const int32 SpentBefore = SpenderBefore ? SpenderBefore->ChipsSpent : 0;
	if (Cost > 0 && !Economy.TrySpend(ViewerKey, Cost))
	{
		const FChatViewerRecord* Record = Economy.Find(ViewerKey);
		Announce(FText::Format(Tr(TEXT("{0} needs {1} chips for !{2} (has {3})"), TEXT("{0}: для !{2} нужно {1} чипов (есть {3})")),
			FText::FromString(ViewerName), FText::AsNumber(Cost), FText::FromString(EffectId), FText::AsNumber(Record ? Record->Chips : 0)));
		return;
	}
	if (Cost > 0)
	{
		CheckRankUp(ViewerKey, SpentBefore);
	}

	GlobalCooldownUntil.Add(EffectId, Now + EffectSettings->GlobalCooldown);
	ViewerCooldownUntil.Add(ViewerCooldownKey, Now + EffectSettings->ViewerCooldown);
	LastTriggerTime = Now;

	if (Effect->bHarmful)
	{
		ChaosQueue.Add({ EffectId, ViewerKey, ViewerName, Cost, Args });
		return;
	}

	if (!ExecuteEffect(*Effect, ViewerKey, ViewerName, Args) && Cost > 0)
	{
		Economy.AddChips(ViewerKey, ViewerName, Cost);
	}
}

bool UChatControlSubsystem::ExecuteEffect(const FChatEffect& Effect, const FString& ViewerKey, const FString& ViewerName, const FString& Args)
{
	FChatEffectContext Ctx;
	Ctx.World = GetGameWorld();
	Ctx.Player = FindPlayer();
	Ctx.Subsystem = this;
	Ctx.Settings = Settings.Effects.Find(Effect.Id);
	Ctx.ViewerKey = ViewerKey;
	Ctx.ViewerName = ViewerName;
	Ctx.Args = Args;

	if (!Ctx.World || !Ctx.Player || !Ctx.Settings)
	{
		return false;
	}
	if (!Effect.Execute(Ctx))
	{
		return false;
	}
	RecordSessionEffect(ViewerName);
	return true;
}

void UChatControlSubsystem::TickEconomy(double Now)
{
	if (Now >= NextChipsPayout)
	{
		NextChipsPayout = Now + Settings.ChipsIntervalSeconds;
		const int32 Paid = Economy.PayActiveViewers(Now, Settings.ActiveWindowSeconds, Settings.ChipsPerInterval);
		if (Paid > 0 && Settings.bAnnouncePayouts)
		{
			Announce(FText::Format(Tr(TEXT("{0} active viewers received {1} chips"), TEXT("Активные зрители ({0}) получили по {1} чипов")), FText::AsNumber(Paid), FText::AsNumber(Settings.ChipsPerInterval)));
		}
	}
	if (Now >= NextEconomySave)
	{
		NextEconomySave = Now + 60.0;
		if (Economy.IsDirty())
		{
			Economy.Save();
		}
	}
}

void UChatControlSubsystem::TickChaosQueue(double Now)
{
	ChaosHistory.RemoveAll([Now](const TPair<double, int32>& Entry) { return Now - Entry.Key >= 60.0; });

	if (ChaosQueue.Num() == 0 || bChaosPaused || Now - LastDeathTime < Settings.DeathProtectionSeconds || Now < NextChaosAttempt)
	{
		return;
	}
	NextChaosAttempt = Now + 0.5;

	// First request that fits the budget and can run now wins; others (e.g. creatures in the safe zone) keep waiting
	const int32 BudgetLeft = Settings.ChaosBudgetPerMinute - GetChaosBudgetUsed(Now);
	for (int32 i = 0; i < ChaosQueue.Num(); ++i)
	{
		const FPendingChaos& Pending = ChaosQueue[i];
		const FChatEffect* Effect = Effects.Find(Pending.EffectId);
		const FChatEffectSettings* EffectSettings = Settings.Effects.Find(Pending.EffectId);
		if (!Effect || !EffectSettings)
		{
			ChaosQueue.RemoveAt(i);
			return;
		}
		if (EffectSettings->ChaosWeight > BudgetLeft)
		{
			continue;
		}
		if (ExecuteEffect(*Effect, Pending.ViewerKey, Pending.ViewerName, Pending.Args))
		{
			ChaosHistory.Add({ Now, EffectSettings->ChaosWeight });
			ChaosQueue.RemoveAt(i);
			return;
		}
	}
}

void UChatControlSubsystem::TickAutoChaos(double Now)
{
	if (!Settings.bAutoChaos || Now - LastTriggerTime < Settings.AutoChaosIdleSeconds || !FindPlayer())
	{
		return;
	}
	LastTriggerTime = Now;

	TArray<FString> Candidates;
	for (const auto& Pair : Settings.Effects)
	{
		if (Pair.Value.bEnabled && Pair.Value.bRandomPool && Effects.Contains(Pair.Key))
		{
			Candidates.Add(Pair.Key);
		}
	}
	if (Candidates.Num() > 0)
	{
		Announce(Tr(TEXT("Chat is quiet... ADA decided to have some fun."), TEXT("В чате тишина... ADA решила развлечься.")));
		RequestEffect(Candidates[FMath::RandRange(0, Candidates.Num() - 1)], TEXT("system:ada"), TEXT("ADA"), true);
	}
}

void UChatControlSubsystem::TickTimedEffects(double Now)
{
	for (auto It = TimedEffects.CreateIterator(); It; ++It)
	{
		if (Now >= It.Value().EndTime)
		{
			It.Value().Revert();
			It.RemoveCurrent();
		}
		else
		{
			It.Value().Maintain();
		}
	}
}

int32 UChatControlSubsystem::GetScaledCost(int32 BaseCost, double Now) const
{
	const int32 Active = FMath::Max(1, Economy.CountActiveViewers(Now, Settings.ActiveWindowSeconds));
	const float Multiplier = FMath::Min(1.f + Settings.CostScalePerViewer * (Active - 1), Settings.MaxCostMultiplier);
	return FMath::RoundToInt(BaseCost * Multiplier * Settings.GetPriceMultiplier());
}

bool UChatControlSubsystem::GetHubLocation(FVector& OutLocation) const
{
	UWorld* World = GetGameWorld();
	for (TActorIterator<AFGBuildableTradingPost> It(World); World && It; ++It)
	{
		OutLocation = It->GetActorLocation();
		return true;
	}
	return false;
}

bool UChatControlSubsystem::IsInSafeZone() const
{
	FVector Hub;
	const AFGCharacterPlayer* Player = FindPlayer();
	return Settings.SafeZoneRadius > 0.f && Player && GetHubLocation(Hub) &&
		FVector::Dist2D(Player->GetActorLocation(), Hub) < Settings.SafeZoneRadius * 100.f;
}

UWidgetComponent* UChatControlSubsystem::AttachNameTag(AActor* Target, const FString& Text, const FColor& Color, float Height)
{
	if (!Target || !Target->GetRootComponent())
	{
		return nullptr;
	}
	// A Slate text in the world instead of a TextRender: the engine's TextRender font has no Cyrillic glyphs
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 40);
	Font.OutlineSettings.OutlineSize = 3;
	Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
	UWidgetComponent* Tag = NewObject<UWidgetComponent>(Target);
	Tag->SetupAttachment(Target->GetRootComponent());
	Tag->SetRelativeLocation(FVector(0, 0, Height));
	Tag->SetWidgetSpace(EWidgetSpace::World);
	Tag->SetDrawAtDesiredSize(true);
	Tag->SetPivot(FVector2D(0.5f, 0.5f));
	Tag->SetTwoSided(true);
	Tag->SetBlendMode(EWidgetBlendMode::Transparent);
	Tag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tag->SetCastShadow(false);
	Tag->SetSlateWidget(SNew(STextBlock)
		.Text(FText::FromString(Text.Replace(TEXT("<br>"), LINE_TERMINATOR)))
		.Font(Font)
		.ColorAndOpacity(FLinearColor(Color))
		.Justification(ETextJustify::Center));
	Tag->RegisterComponent();
	NameTags.Add(Tag);
	return Tag;
}

void UChatControlSubsystem::TickNameTags()
{
	if (NameTags.Num() == 0)
	{
		return;
	}
	UWorld* World = GetGameWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Controller || !Controller->PlayerCameraManager)
	{
		return;
	}
	const FVector Camera = Controller->PlayerCameraManager->GetCameraLocation();
	NameTags.RemoveAll([](const TWeakObjectPtr<UWidgetComponent>& Tag) { return !Tag.IsValid(); });
	for (const TWeakObjectPtr<UWidgetComponent>& Tag : NameTags)
	{
		// The widget faces +X, so point +X at the camera (yaw only, stays upright)
		FRotator Facing = (Camera - Tag->GetComponentLocation()).Rotation();
		Facing.Pitch = 0.f;
		Facing.Roll = 0.f;
		Tag->SetWorldRotation(Facing);
	}
}

bool UChatControlSubsystem::SpawnPet(const FString& ViewerKey, const FString& ViewerName, const FVector* At)
{
	UWorld* World = GetGameWorld();
	AFGCharacterPlayer* Player = FindPlayer();
	if (!World || !Player)
	{
		return false;
	}
	UClass* PetClass = LoadClass<APawn>(nullptr, PetClassPath);
	if (!PetClass)
	{
		return false;
	}
	if (TWeakObjectPtr<AActor>* Old = Pets.Find(ViewerKey); Old && Old->IsValid())
	{
		(*Old)->Destroy();
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	FVector Location = At ? *At : Player->GetActorLocation() + Player->GetActorRightVector() * 250.f;
	FHitResult Ground;
	FCollisionQueryParams Query(TEXT("ChatControlPet"), false, Player);
	if (World->LineTraceSingleByChannel(Ground, Location + FVector(0, 0, 100.f), Location - FVector(0, 0, 500.f), ECC_Visibility, Query))
	{
		Location = Ground.ImpactPoint + FVector(0, 0, 60.f);
	}
	APawn* Pet = World->SpawnActor<APawn>(PetClass, Location, Player->GetActorRotation(), Params);
	if (!Pet)
	{
		UE_LOG(LogChatControl, Warning, TEXT("Pet failed to spawn"));
		return false;
	}
	if (!Pet->GetController())
	{
		Pet->SpawnDefaultController();
	}
	SetupPet(Pet, ViewerKey, ViewerName, At == nullptr);
	return true;
}

bool UChatControlSubsystem::TryNamePet(APawn* Pet, const FString& Name) const
{
	bool bNamed = false;
	// Only blueprint-declared members, so engine fields like actor labels are never touched
	for (TFieldIterator<FProperty> It(Pet->GetClass()); It; ++It)
	{
		const FString PropertyName = It->GetName();
		if (!Cast<UBlueprintGeneratedClass>(It->GetOwnerClass()) || !PropertyName.Contains(TEXT("Name")))
		{
			continue;
		}
		if (const FStrProperty* Str = CastField<FStrProperty>(*It))
		{
			Str->SetPropertyValue_InContainer(Pet, Name);
		}
		else if (const FTextProperty* Text = CastField<FTextProperty>(*It))
		{
			Text->SetPropertyValue_InContainer(Pet, FText::FromString(Name));
		}
		else
		{
			continue;
		}
		UE_LOG(LogChatControl, Log, TEXT("Pet: set %s::%s = %s"), *Pet->GetClass()->GetName(), *PropertyName, *Name);
		bNamed = true;
	}
	for (TFieldIterator<UFunction> It(Pet->GetClass()); It; ++It)
	{
		UFunction* Function = *It;
		const FString FunctionName = Function->GetName();
		if (!Cast<UBlueprintGeneratedClass>(Function->GetOwnerClass()) || !(FunctionName.Contains(TEXT("SetName")) || FunctionName.Contains(TEXT("Rename"))))
		{
			continue;
		}
		TArray<FProperty*> Params;
		for (TFieldIterator<FProperty> P(Function); P && P->HasAnyPropertyFlags(CPF_Parm); ++P)
		{
			Params.Add(*P);
		}
		if (Params.Num() != 1 || Params[0]->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
		{
			UE_LOG(LogChatControl, Log, TEXT("Pet: skipped %s (%d params)"), *FunctionName, Params.Num());
			continue;
		}
		TArray<uint8> Buffer;
		Buffer.SetNumZeroed(Function->ParmsSize);
		Params[0]->InitializeValue_InContainer(Buffer.GetData());
		if (const FStrProperty* Str = CastField<FStrProperty>(Params[0]))
		{
			Str->SetPropertyValue_InContainer(Buffer.GetData(), Name);
		}
		else if (const FTextProperty* Text = CastField<FTextProperty>(Params[0]))
		{
			Text->SetPropertyValue_InContainer(Buffer.GetData(), FText::FromString(Name));
		}
		else
		{
			continue;
		}
		Pet->ProcessEvent(Function, Buffer.GetData());
		Params[0]->DestroyValue_InContainer(Buffer.GetData());
		UE_LOG(LogChatControl, Log, TEXT("Pet: called %s(%s)"), *FunctionName, *Name);
		bNamed = true;
	}
	if (!bNamed)
	{
		UE_LOG(LogChatControl, Log, TEXT("Pet: no blueprint name field found, only the floating name tag is shown"));
	}
	return bNamed;
}

FColor UChatControlSubsystem::GetNameColor(const FString& ViewerName, const FColor& Default) const
{
	const uint8* Role = ViewerRoles.Find(ViewerName.ToLower());
	if (!Settings.bRoleColors || !Role || *Role == 0)
	{
		return Default;
	}
	const FString& Hex = *Role == 4 ? Settings.ColorBroadcaster : *Role == 3 ? Settings.ColorModerator : *Role == 2 ? Settings.ColorVip : Settings.ColorSubscriber;
	FString Clean = Hex.TrimStartAndEnd();
	Clean.RemoveFromStart(TEXT("#"));
	return Clean.Len() == 6 ? FColor::FromHex(Clean) : Default;
}

bool UChatControlSubsystem::TryTamePet(APawn* Pet, AActor* Owner) const
{
	// Look on the doggo and its AI controller for something like Tame(Owner) / SetFriendActor(Owner) / bIsTamed
	TArray<UObject*> Targets = { Pet, Pet->GetController() };
	bool bTamed = false;
	for (UObject* Target : Targets)
	{
		if (!Target)
		{
			continue;
		}
		for (TFieldIterator<UFunction> It(Target->GetClass()); It; ++It)
		{
			UFunction* Function = *It;
			const FString FunctionName = Function->GetName();
			const bool bLooksLikeTame = (FunctionName.Contains(TEXT("Tame")) || FunctionName.Contains(TEXT("Friend")))
				&& !FunctionName.StartsWith(TEXT("Is")) && !FunctionName.StartsWith(TEXT("Get")) && !FunctionName.Contains(TEXT("Untame"));
			if (!bLooksLikeTame)
			{
				continue;
			}
			// Only call simple signatures: no params, or a single actor param that gets the player
			TArray<FProperty*> Params;
			for (TFieldIterator<FProperty> P(Function); P && P->HasAnyPropertyFlags(CPF_Parm); ++P)
			{
				Params.Add(*P);
			}
			const bool bInput = Params.Num() == 1 && !Params[0]->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm);
			const FObjectPropertyBase* ActorParam = bInput ? CastField<FObjectPropertyBase>(Params[0]) : nullptr;
			const FBoolProperty* BoolParam = bInput ? CastField<FBoolProperty>(Params[0]) : nullptr;
			if (Params.Num() == 0 || BoolParam || (ActorParam && Owner->IsA(ActorParam->PropertyClass)))
			{
				TArray<uint8> Buffer;
				Buffer.SetNumZeroed(FMath::Max<int32>(1, Function->ParmsSize));
				if (ActorParam)
				{
					ActorParam->SetObjectPropertyValue_InContainer(Buffer.GetData(), Owner);
				}
				if (BoolParam)
				{
					// SetTamed(true)
					BoolParam->SetPropertyValue_InContainer(Buffer.GetData(), true);
				}
				Target->ProcessEvent(Function, Buffer.GetData());
				UE_LOG(LogChatControl, Log, TEXT("Pet: called %s::%s"), *Target->GetClass()->GetName(), *FunctionName);
				bTamed = true;
			}
			else
			{
				UE_LOG(LogChatControl, Log, TEXT("Pet: skipped %s::%s (%d params)"), *Target->GetClass()->GetName(), *FunctionName, Params.Num());
			}
		}
		for (TFieldIterator<FProperty> It(Target->GetClass()); It; ++It)
		{
			const FString PropertyName = It->GetName();
			if (const FBoolProperty* Flag = CastField<FBoolProperty>(*It); Flag && PropertyName.Contains(TEXT("Tame")))
			{
				Flag->SetPropertyValue_InContainer(Target, true);
				UE_LOG(LogChatControl, Log, TEXT("Pet: set %s::%s = true"), *Target->GetClass()->GetName(), *PropertyName);
				bTamed = true;
			}
			else if (const FObjectPropertyBase* Ref = CastField<FObjectPropertyBase>(*It); Ref && PropertyName.Contains(TEXT("Friend")) && Owner->IsA(Ref->PropertyClass))
			{
				Ref->SetObjectPropertyValue_InContainer(Target, Owner);
				UE_LOG(LogChatControl, Log, TEXT("Pet: set %s::%s = player"), *Target->GetClass()->GetName(), *PropertyName);
				bTamed = true;
			}
		}
	}
	if (!bTamed)
	{
		UE_LOG(LogChatControl, Warning, TEXT("Pet: no taming function or property found on %s"), *Pet->GetClass()->GetName());
	}
	return bTamed;
}

void UChatControlSubsystem::Panic()
{
	const int32 Removed = ChaosQueue.Num() + DelayedActions.Num();
	ChaosQueue.Empty();
	DelayedActions.Empty();
	for (auto& Pair : TimedEffects)
	{
		Pair.Value.Revert();
	}
	TimedEffects.Empty();
	int32 Creatures = 0;
	for (const TWeakObjectPtr<AActor>& Actor : SpawnedActors)
	{
		if (Actor.IsValid())
		{
			Actor->Destroy();
			++Creatures;
		}
	}
	SpawnedActors.Empty();
	Signs.Empty();
	EndBossFight(false);
	Announce(FText::Format(Tr(TEXT("PANIC! Chaos cancelled: {0} queued effects, {1} creatures removed."),
		TEXT("ПАНИКА! Хаос отменён: убрано {0} эффектов из очереди и {1} существ.")), FText::AsNumber(Removed), FText::AsNumber(Creatures)));
}

int32 UChatControlSubsystem::ClearSigns(const FString& Viewer)
{
	int32 Removed = 0;
	Signs.RemoveAll([&](const TPair<TWeakObjectPtr<AActor>, FString>& Entry)
	{
		if (!Entry.Key.IsValid())
		{
			return true;
		}
		if (!Viewer.IsEmpty() && !Entry.Value.Equals(Viewer, ESearchCase::IgnoreCase))
		{
			return false;
		}
		Entry.Key->Destroy();
		++Removed;
		return true;
	});
	return Removed;
}

int32 UChatControlSubsystem::GetChaosBudgetUsed(double Now) const
{
	int32 Used = 0;
	for (const auto& Entry : ChaosHistory)
	{
		if (Now - Entry.Key < 60.0)
		{
			Used += Entry.Value;
		}
	}
	return Used;
}

AFGCharacterPlayer* UChatControlSubsystem::FindPlayer() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld())
	{
		return nullptr;
	}
	// The character the local player controls
	APlayerController* Controller = World->GetFirstPlayerController();
	if (AFGCharacterPlayer* Character = Controller ? Cast<AFGCharacterPlayer>(Controller->GetPawn()) : nullptr)
	{
		return Character;
	}
	// While driving the controller possesses the vehicle; fall back to a locally controlled or the only character
	AFGCharacterPlayer* Fallback = nullptr;
	for (TActorIterator<AFGCharacterPlayer> It(World); It; ++It)
	{
		if (It->IsLocallyControlled())
		{
			return *It;
		}
		Fallback = Fallback ? Fallback : *It;
	}
	return Fallback;
}

