#include "ChatControlSubsystem.h"
#include "ChatControl.h"
#include "Twitch/TwitchIrcConnector.h"
#include "FGChatManager.h"
#include "FGCharacterPlayer.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "ChatControl"

bool UChatControlSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Chat is handled by whoever runs the game logic; a dedicated server has no streamer
	return !IsRunningDedicatedServer();
}

void UChatControlSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Settings = FChatControlSettings::LoadOrCreate();
	Economy.Load();
	Effects = ChatEffects::CreateAll();

	const double Now = FPlatformTime::Seconds();
	NextChipsPayout = Now + Settings.ChipsIntervalSeconds;
	NextEconomySave = Now + 60.0;
	LastTriggerTime = Now;

	if (Settings.bEnabled && !Settings.TwitchChannel.IsEmpty())
	{
		Twitch = MakeShared<FTwitchIrcConnector>(Settings.TwitchChannel, IncomingEvents);
		Twitch->Start();
	}

	bInitialized = true;
	UE_LOG(LogChatControl, Log, TEXT("ChatControl initialized, settings: %s"), *FChatControlSettings::GetSettingsPath());
}

void UChatControlSubsystem::Deinitialize()
{
	bInitialized = false;
	// Destroying the connector stops and joins its thread before IncomingEvents goes away
	Twitch.Reset();
	for (auto& Pair : TimedEffects)
	{
		Pair.Value.Revert();
	}
	TimedEffects.Empty();
	Economy.Save();
	Super::Deinitialize();
}

TStatId UChatControlSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UChatControlSubsystem, STATGROUP_Tickables);
}

void UChatControlSubsystem::Tick(float DeltaTime)
{
	const double Now = FPlatformTime::Seconds();

	FChatEvent Event;
	while (IncomingEvents.Dequeue(Event))
	{
		HandleEvent(Event);
	}

	if (const AFGCharacterPlayer* Player = FindPlayer(); Player && !Player->IsAliveAndWell())
	{
		LastDeathTime = Now;
	}

	TickEconomy(Now);
	TickTimedEffects(Now);
	TickChaosQueue(Now);
	TickAutoChaos(Now);
}

void UChatControlSubsystem::Announce(const FText& Message) const
{
	UE_LOG(LogChatControl, Log, TEXT("%s"), *Message.ToString());
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (AFGChatManager* Chat = World ? AFGChatManager::Get(World) : nullptr)
	{
		FChatMessageStruct ChatMessage;
		ChatMessage.MessageType = EFGChatMessageType::CMT_CustomMessage;
		ChatMessage.MessageSender = FText::FromString(TEXT("ChatControl"));
		ChatMessage.MessageSenderColor = FLinearColor(0.57f, 0.27f, 1.f); // Twitch purple
		ChatMessage.MessageText = Message;
		Chat->AddChatMessageToReceived(ChatMessage);
	}
}

void UChatControlSubsystem::StartTimedEffect(const FString& Slot, float Duration, TFunction<void()> Apply, TFunction<void()> Revert)
{
	if (FTimedEffect* Active = TimedEffects.Find(Slot))
	{
		Active->Revert();
	}
	Apply();
	TimedEffects.Add(Slot, FTimedEffect{ FPlatformTime::Seconds() + Duration, MoveTemp(Revert) });
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
		Announce(FText::Format(LOCTEXT("BitsReceived", "{0} cheered {1} bits and got {2} chips!"), Name, FText::AsNumber(Event.Amount), FText::AsNumber(Chips)));
		// A cheer message may also contain a command, now paid with the fresh chips
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
			Announce(FText::Format(LOCTEXT("UnknownReward", "Unknown channel points reward id {0}. Map it to an effect in Settings.json (RewardEffects)."), FText::FromString(Event.RewardId)));
		}
		break;
	}

	case EChatEventType::Subscription:
		Economy.AddChips(Key, Event.DisplayName, Settings.ChipsPerSub);
		Announce(FText::Format(LOCTEXT("Sub", "{0} subscribed and got {1} chips! Thank you!"), Name, FText::AsNumber(Settings.ChipsPerSub)));
		break;

	case EChatEventType::GiftSubs:
	{
		const int32 Chips = Settings.ChipsPerSub * Event.Amount;
		Economy.AddChips(Key, Event.DisplayName, Chips);
		Announce(FText::Format(LOCTEXT("GiftSubs", "{0} gifted {1} subs and got {2} chips!"), Name, FText::AsNumber(Event.Amount), FText::AsNumber(Chips)));
		break;
	}

	case EChatEventType::Raid:
	{
		Announce(FText::Format(LOCTEXT("Raid", "{0} is raiding with {1} viewers! Brace yourself!"), Name, FText::AsNumber(Event.Amount)));
		TArray<FString> Harmful;
		for (const auto& Pair : Effects)
		{
			const FChatEffectSettings* EffectSettings = Settings.Effects.Find(Pair.Key);
			if (Pair.Value.bHarmful && EffectSettings && EffectSettings->bEnabled)
			{
				Harmful.Add(Pair.Key);
			}
		}
		const int32 Waves = FMath::Clamp(Event.Amount / 5, 1, 5);
		for (int32 i = 0; i < Waves && Harmful.Num() > 0; ++i)
		{
			ChaosQueue.Add({ Harmful[FMath::RandRange(0, Harmful.Num() - 1)], Key, Event.DisplayName, 0 });
		}
		break;
	}
	}
}

void UChatControlSubsystem::HandleCommand(const FChatEvent& Event)
{
	FString Text = Event.Text.TrimStartAndEnd();
	if (Event.Type == EChatEventType::Bits && !Text.StartsWith(TEXT("!")))
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
	Args = Args.TrimStartAndEnd().ToLower();

	const FString Key = Event.GetEconomyKey();
	const FText Name = FText::FromString(Event.DisplayName);

	if (Command == TEXT("chips") || Command == TEXT("чипы"))
	{
		const FChatViewerRecord* Record = Economy.Find(Key);
		Announce(FText::Format(LOCTEXT("Balance", "{0} has {1} chips"), Name, FText::AsNumber(Record ? Record->Chips : 0)));
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
		Announce(FText::Format(LOCTEXT("EffectList", "Commands: {0}"), FText::FromString(FString::Join(Parts, TEXT(", ")))));
		return;
	}

	const bool bCanModerate = Event.bIsBroadcaster || Event.bIsModerator;

	if (Command == TEXT("pause") && bCanModerate)
	{
		bChaosPaused = !bChaosPaused;
		Announce(bChaosPaused
			? LOCTEXT("Paused", "Chaos paused. Harmful effects will wait in the queue.")
			: LOCTEXT("Resumed", "Chaos resumed!"));
		return;
	}

	if (Command == TEXT("test") && Event.bIsBroadcaster)
	{
		if (const FChatEffect* Effect = Effects.Find(Args))
		{
			ExecuteEffect(*Effect, Event.DisplayName);
		}
		return;
	}

	if (Effects.Contains(Command))
	{
		RequestEffect(Command, Key, Event.DisplayName, false);
	}
}

void UChatControlSubsystem::RequestEffect(const FString& EffectId, const FString& ViewerKey, const FString& ViewerName, bool bFree)
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
		Announce(FText::Format(LOCTEXT("NotEnoughChips", "{0} needs {1} chips for !{2} (has {3})"),
			FText::FromString(ViewerName), FText::AsNumber(Cost), FText::FromString(EffectId), FText::AsNumber(Record ? Record->Chips : 0)));
		return;
	}

	GlobalCooldownUntil.Add(EffectId, Now + EffectSettings->GlobalCooldown);
	ViewerCooldownUntil.Add(ViewerCooldownKey, Now + EffectSettings->ViewerCooldown);
	LastTriggerTime = Now;

	if (Effect->bHarmful)
	{
		ChaosQueue.Add({ EffectId, ViewerKey, ViewerName, Cost });
		return;
	}

	if (!ExecuteEffect(*Effect, ViewerName) && Cost > 0)
	{
		Economy.AddChips(ViewerKey, ViewerName, Cost);
	}
}

bool UChatControlSubsystem::ExecuteEffect(const FChatEffect& Effect, const FString& ViewerName)
{
	FChatEffectContext Ctx;
	Ctx.World = GetGameInstance()->GetWorld();
	Ctx.Player = FindPlayer();
	Ctx.Subsystem = this;
	Ctx.Settings = Settings.Effects.Find(Effect.Id);
	Ctx.ViewerName = ViewerName;

	if (!Ctx.World || !Ctx.Player || !Ctx.Settings)
	{
		return false;
	}
	return Effect.Execute(Ctx);
}

void UChatControlSubsystem::TickEconomy(double Now)
{
	if (Now >= NextChipsPayout)
	{
		NextChipsPayout = Now + Settings.ChipsIntervalSeconds;
		const int32 Paid = Economy.PayActiveViewers(Now, Settings.ActiveWindowSeconds, Settings.ChipsPerInterval);
		if (Paid > 0)
		{
			Announce(FText::Format(LOCTEXT("Payout", "{0} active viewers received {1} chips"), FText::AsNumber(Paid), FText::AsNumber(Settings.ChipsPerInterval)));
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

	if (ChaosQueue.Num() == 0 || bChaosPaused || Now - LastDeathTime < Settings.DeathProtectionSeconds)
	{
		return;
	}

	const FPendingChaos& Next = ChaosQueue[0];
	const FChatEffect* Effect = Effects.Find(Next.EffectId);
	const FChatEffectSettings* EffectSettings = Settings.Effects.Find(Next.EffectId);
	if (!Effect || !EffectSettings)
	{
		ChaosQueue.RemoveAt(0);
		return;
	}
	if (GetChaosBudgetUsed(Now) + EffectSettings->ChaosWeight > Settings.ChaosBudgetPerMinute)
	{
		return;
	}
	// Keep the request queued while there is no player (loading screen, main menu)
	if (ExecuteEffect(*Effect, Next.ViewerName))
	{
		ChaosHistory.Add({ Now, EffectSettings->ChaosWeight });
		ChaosQueue.RemoveAt(0);
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
		Announce(LOCTEXT("AutoChaos", "Chat is quiet... ADA decided to have some fun."));
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
	}
}

int32 UChatControlSubsystem::GetScaledCost(int32 BaseCost, double Now) const
{
	const int32 Active = FMath::Max(1, Economy.CountActiveViewers(Now, Settings.ActiveWindowSeconds));
	const float Multiplier = FMath::Min(1.f + Settings.CostScalePerViewer * (Active - 1), Settings.MaxCostMultiplier);
	return FMath::RoundToInt(BaseCost * Multiplier);
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
	// Singleplayer: the only player character (also found while driving a vehicle)
	for (TActorIterator<AFGCharacterPlayer> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
