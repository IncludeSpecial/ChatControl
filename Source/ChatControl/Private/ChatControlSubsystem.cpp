#include "ChatControlSubsystem.h"
#include "ChatControl.h"
#include "Twitch/TwitchIrcConnector.h"
#include "YouTube/YouTubeChatConnector.h"
#include "ChatControlModConfig.h"
#include "Configuration/ConfigManager.h"
#include "FGChatManager.h"
#include "FGCharacterPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Buildables/FGBuildableTradingPost.h"
#include "Components/TextRenderComponent.h"
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
	const bool bWasEnabled = Settings.bEnabled;
	if (!UChatControlModConfig::ApplyTo(ConfigManager, Settings))
	{
		return;
	}
	Settings.Save();
	ChatText::SetLanguage(Settings.Language);
	if (Settings.TwitchChannel != OldChannel || Settings.bEnabled != bWasEnabled || Settings.YouTubeApiKey + Settings.YouTubeChannel != OldYouTube)
	{
		RestartConnectors();
	}
	UE_LOG(LogChatControl, Log, TEXT("Settings changed in the mod config menu"));
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
	TickEconomy(Now);
	TickTimedEffects(Now);
	TickDelayedActions(Now);
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

	switch (Event.Type)
	{
	case EChatEventType::Message:
		Economy.OnViewerMessage(Key, Event.DisplayName, Now, Settings.FirstMessageBonus);
		HandleCommand(Event);
		break;

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
		if (Settings.RaidBossMinViewers > 0 && Event.Amount >= Settings.RaidBossMinViewers && IsEnabled(TEXT("boss")))
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
		Announce(FText::Format(Tr(TEXT("{0} has {1} chips"), TEXT("У {0} {1} чипов")), Name, FText::AsNumber(Record ? Record->Chips : 0)));
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
	else if (What == TEXT("autochaos"))
	{
		LastTriggerTime = -Settings.AutoChaosIdleSeconds;
	}
	else if (const FChatEffect* Effect = Effects.Find(What))
	{
		ExecuteEffect(*Effect, Event.DisplayName);
	}
	else
	{
		Announce(Tr(TEXT("[test] chips [N], bits [N], sub, gifts [N], raid [N], autochaos, or an effect name"), TEXT("[тест] chips [N], bits [N], sub, gifts [N], raid [N], autochaos или название эффекта")));
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
	if (Cost > 0 && !Economy.TrySpend(ViewerKey, Cost))
	{
		const FChatViewerRecord* Record = Economy.Find(ViewerKey);
		Announce(FText::Format(Tr(TEXT("{0} needs {1} chips for !{2} (has {3})"), TEXT("{0}: для !{2} нужно {1} чипов (есть {3})")),
			FText::FromString(ViewerName), FText::AsNumber(Cost), FText::FromString(EffectId), FText::AsNumber(Record ? Record->Chips : 0)));
		return;
	}

	GlobalCooldownUntil.Add(EffectId, Now + EffectSettings->GlobalCooldown);
	ViewerCooldownUntil.Add(ViewerCooldownKey, Now + EffectSettings->ViewerCooldown);
	LastTriggerTime = Now;

	if (Effect->bHarmful)
	{
		ChaosQueue.Add({ EffectId, ViewerKey, ViewerName, Cost, Args });
		return;
	}

	if (!ExecuteEffect(*Effect, ViewerName, Args) && Cost > 0)
	{
		Economy.AddChips(ViewerKey, ViewerName, Cost);
	}
}

bool UChatControlSubsystem::ExecuteEffect(const FChatEffect& Effect, const FString& ViewerName, const FString& Args)
{
	FChatEffectContext Ctx;
	Ctx.World = GetGameWorld();
	Ctx.Player = FindPlayer();
	Ctx.Subsystem = this;
	Ctx.Settings = Settings.Effects.Find(Effect.Id);
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
		if (Paid > 0)
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
		if (ExecuteEffect(*Effect, Pending.ViewerName, Pending.Args))
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
		if (Pair.Value.bEnabled && Pair.Value.Cost > 0 && Effects.Contains(Pair.Key))
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

UTextRenderComponent* UChatControlSubsystem::AttachNameTag(AActor* Target, const FString& Text, const FColor& Color, float Height)
{
	if (!Target || !Target->GetRootComponent())
	{
		return nullptr;
	}
	UTextRenderComponent* Tag = NewObject<UTextRenderComponent>(Target);
	Tag->SetupAttachment(Target->GetRootComponent());
	Tag->SetRelativeLocation(FVector(0, 0, Height));
	Tag->SetHorizontalAlignment(EHTA_Center);
	Tag->SetVerticalAlignment(EVRTA_TextCenter);
	Tag->SetWorldSize(40.f);
	Tag->SetTextRenderColor(Color);
	Tag->SetText(FText::FromString(Text));
	Tag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tag->SetCastShadow(false);
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
	NameTags.RemoveAll([](const TWeakObjectPtr<UTextRenderComponent>& Tag) { return !Tag.IsValid(); });
	for (const TWeakObjectPtr<UTextRenderComponent>& Tag : NameTags)
	{
		// Text faces +X, so point +X at the camera (yaw only, stays upright)
		FRotator Facing = (Camera - Tag->GetComponentLocation()).Rotation();
		Facing.Pitch = 0.f;
		Facing.Roll = 0.f;
		Tag->SetWorldRotation(Facing);
	}
}

bool UChatControlSubsystem::SpawnPet(const FString& ViewerKey, const FString& ViewerName)
{
	UWorld* World = GetGameWorld();
	AFGCharacterPlayer* Player = FindPlayer();
	if (!World || !Player)
	{
		return false;
	}
	UClass* PetClass = LoadClass<APawn>(nullptr, TEXT("/Game/FactoryGame/Character/Creature/Wildlife/SpaceRabbit/Char_SpaceRabbit.Char_SpaceRabbit_C"));
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
	const FVector Location = Player->GetActorLocation() + Player->GetActorRightVector() * 250.f;
	APawn* Pet = World->SpawnActor<APawn>(PetClass, Location, Player->GetActorRotation(), Params);
	if (!Pet)
	{
		return false;
	}
	if (!Pet->GetController())
	{
		Pet->SpawnDefaultController();
	}
	Pet->SetLifeSpan(FMath::Max(60.f, Settings.PetLifeSpanSeconds));
	AttachNameTag(Pet, ViewerName, FColor(120, 255, 120), 120.f);
	Pets.Add(ViewerKey, Pet);
	Announce(FText::Format(Tr(TEXT("{0}'s pet came to visit!"), TEXT("Питомец {0} пришёл в гости!")), FText::FromString(ViewerName)));
	return true;
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
	Announce(FText::Format(Tr(TEXT("PANIC! Chaos cancelled: {0} queued effects, {1} creatures removed."),
		TEXT("ПАНИКА! Хаос отменён: убрано {0} эффектов из очереди и {1} существ.")), FText::AsNumber(Removed), FText::AsNumber(Creatures)));
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

