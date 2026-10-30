// Chat games: votes, the shared chip pot, leaderboard and session stats.
#include "ChatControlSubsystem.h"
#include "ChatControl.h"
#include "ChatText.h"
#include "Twitch/TwitchApi.h"
#include "FGCharacterPlayer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

using ChatText::Tr;

bool UChatControlSubsystem::HandleGameCommand(const FChatEvent& Event, const FString& Command, const FString& Args)
{
	const FString Key = Event.GetEconomyKey();

	// Votes: "!1".."!3" or "!vote 2"
	int32 Choice = 0;
	if (Command == TEXT("1") || Command == TEXT("2") || Command == TEXT("3"))
	{
		Choice = FCString::Atoi(*Command);
	}
	else if (Command == TEXT("vote") || Command == TEXT("голос"))
	{
		Choice = FCString::Atoi(*Args);
	}
	if (Choice > 0)
	{
		if (bVoteActive && Choice <= VoteOptions.Num())
		{
			if (!Votes.Contains(Key) && Settings.VoteReward > 0)
			{
				Economy.AddChips(Key, Event.DisplayName, Settings.VoteReward);
			}
			Votes.Add(Key, Choice - 1);
		}
		return true;
	}

	if (Command == TEXT("pot") || Command == TEXT("копилка"))
	{
		HandlePotCommand(Event, Args);
		return true;
	}

	if (Command == TEXT("top") || Command == TEXT("топ"))
	{
		TArray<TPair<FString, int32>> Spenders = Economy.GetTopSpenders(3);
		TArray<FString> Parts;
		for (int32 i = 0; i < Spenders.Num(); ++i)
		{
			Parts.Add(FString::Printf(TEXT("%d. %s (%d)"), i + 1, *Spenders[i].Key, Spenders[i].Value));
		}
		Announce(Parts.Num() > 0
			? FText::Format(Tr(TEXT("Top supporters by chips spent: {0}"), TEXT("Топ по потраченным чипам: {0}")), FText::FromString(FString::Join(Parts, TEXT(", "))))
			: Tr(TEXT("Nobody has spent chips yet"), TEXT("Пока никто не тратил чипы")));
		return true;
	}

	if (Command == TEXT("stats") || Command == TEXT("статистика"))
	{
		Announce(GetSessionSummary());
		return true;
	}

	return false;
}

void UChatControlSubsystem::HandlePotCommand(const FChatEvent& Event, const FString& Args)
{
	const FString* GoalEffect = Settings.GoalEffect.IsEmpty() ? nullptr : &Settings.GoalEffect;
	if (!Settings.bGoalEnabled || !GoalEffect || !Effects.Contains(*GoalEffect) || Settings.GoalChips <= 0)
	{
		return;
	}
	const FString Key = Event.GetEconomyKey();
	const int32 Amount = FCString::Atoi(*Args);
	if (Amount > 0)
	{
		if (!Economy.TrySpend(Key, Amount))
		{
			const FChatViewerRecord* Record = Economy.Find(Key);
			Announce(FText::Format(Tr(TEXT("{0} doesn't have {1} chips (has {2})"), TEXT("У {0} нет {1} чипов (есть {2})")),
				FText::FromString(Event.DisplayName), FText::AsNumber(Amount), FText::AsNumber(Record ? Record->Chips : 0)));
			return;
		}
		Economy.AddToPot(Amount);
	}

	if (Economy.GetPot() >= Settings.GoalChips)
	{
		Economy.AddToPot(-Settings.GoalChips);
		Announce(FText::Format(Tr(TEXT("The pot is full! Chat unleashes !{0}!"), TEXT("Копилка собрана! Чат запускает !{0}!")), FText::FromString(*GoalEffect)));
		RequestEffect(*GoalEffect, TEXT("system:pot"), Tr(TEXT("Chat"), TEXT("Чат")).ToString(), true);
		return;
	}
	Announce(FText::Format(Tr(TEXT("Pot for !{0}: {1}/{2} chips. Chip in with !pot <amount>"), TEXT("Копилка на !{0}: {1}/{2} чипов. Скинуться: !pot <сколько>")),
		FText::FromString(*GoalEffect), FText::AsNumber(Economy.GetPot()), FText::AsNumber(Settings.GoalChips)));
}

void UChatControlSubsystem::TickVotes(double Now)
{
	if (bVoteActive)
	{
		// A Twitch poll ends through its callback; the timer is only a safety net
		if (Now >= VoteEndTime + (bPollMode ? 30.0 : 0.0))
		{
			FinishVote(Now);
		}
		return;
	}
	if (!Settings.bVotesEnabled || Now < NextVoteTime || !FindPlayer())
	{
		return;
	}

	TArray<FString> Candidates;
	for (const auto& Pair : Settings.Effects)
	{
		if (Pair.Value.bEnabled && Pair.Value.bRandomPool && Effects.Contains(Pair.Key))
		{
			Candidates.Add(Pair.Key);
		}
	}
	if (Candidates.Num() < 2)
	{
		NextVoteTime = Now + Settings.VoteIntervalMinutes * 60.0;
		return;
	}

	VoteOptions.Reset();
	Votes.Reset();
	while (VoteOptions.Num() < FMath::Min(3, Candidates.Num()))
	{
		VoteOptions.Add(Candidates[FMath::RandRange(0, Candidates.Num() - 1)]);
		Candidates.Remove(VoteOptions.Last());
	}
	bVoteActive = true;
	VoteEndTime = Now + FMath::Max(15.f, Settings.VoteDurationSeconds);
	bPollMode = Settings.bTwitchPolls && TwitchApi && TwitchApi->IsReady();
	PollVotes.Init(0, VoteOptions.Num());
	if (bPollMode)
	{
		TWeakObjectPtr<UChatControlSubsystem> WeakThis = this;
		TwitchApi->StartPoll(Tr(TEXT("What should happen to the streamer?"), TEXT("Что устроить стримеру?")).ToString(), VoteOptions,
			FMath::RoundToInt(Settings.VoteDurationSeconds), [WeakThis](const TArray<int32>& Counts, bool bFinished)
		{
			if (!WeakThis.IsValid() || !WeakThis->bVoteActive)
			{
				return;
			}
			if (Counts.Contains(-1))
			{
				// Polls need Affiliate/Partner; fall back to chat voting for this round
				WeakThis->bPollMode = false;
				WeakThis->Announce(Tr(TEXT("Twitch poll unavailable, vote in chat with !1 / !2 / !3"), TEXT("Опрос Twitch недоступен, голосуйте в чате: !1 / !2 / !3")));
				return;
			}
			WeakThis->PollVotes = Counts;
			if (bFinished)
			{
				WeakThis->FinishVote(FPlatformTime::Seconds());
			}
		});
		Announce(FText::Format(Tr(TEXT("VOTE in the Twitch poll! {0} s to decide"), TEXT("ГОЛОСОВАНИЕ в опросе Twitch! {0} с на выбор")),
			FText::AsNumber(FMath::RoundToInt(Settings.VoteDurationSeconds))));
		return;
	}

	TArray<FString> Parts;
	for (int32 i = 0; i < VoteOptions.Num(); ++i)
	{
		Parts.Add(FString::Printf(TEXT("!%d = %s"), i + 1, *VoteOptions[i]));
	}
	Announce(FText::Format(Tr(TEXT("VOTE! {0} - {1} s to decide"), TEXT("ГОЛОСОВАНИЕ! {0} - {1} с на выбор")),
		FText::FromString(FString::Join(Parts, TEXT(", "))), FText::AsNumber(FMath::RoundToInt(Settings.VoteDurationSeconds))));
}

void UChatControlSubsystem::FinishVote(double Now)
{
	bVoteActive = false;
	NextVoteTime = Now + FMath::Max(1, Settings.VoteIntervalMinutes) * 60.0;

	TArray<int32> Counts;
	Counts.SetNumZeroed(VoteOptions.Num());
	for (const auto& Pair : Votes)
	{
		Counts[Pair.Value]++;
	}
	if (bPollMode && PollVotes.Num() == Counts.Num())
	{
		Counts = PollVotes;
	}
	bPollMode = false;
	if (FMath::Max(Counts) <= 0)
	{
		Announce(Tr(TEXT("Nobody voted. Maybe next time!"), TEXT("Никто не проголосовал. В следующий раз!")));
		return;
	}
	const int32 Best = FMath::Max(Counts);
	TArray<int32> Winners;
	for (int32 i = 0; i < Counts.Num(); ++i)
	{
		if (Counts[i] == Best)
		{
			Winners.Add(i);
		}
	}
	const FString& Winner = VoteOptions[Winners[FMath::RandRange(0, Winners.Num() - 1)]];
	Announce(FText::Format(Tr(TEXT("Chat chose !{0} ({1} votes)!"), TEXT("Чат выбрал !{0} ({1} голосов)!")), FText::FromString(Winner), FText::AsNumber(Best)));
	RequestEffect(Winner, TEXT("system:vote"), Tr(TEXT("Chat"), TEXT("Чат")).ToString(), true);
}

void UChatControlSubsystem::RecordSessionEffect(const FString& ViewerName)
{
	++SessionEffectCount;
	SessionEffectsByViewer.FindOrAdd(ViewerName)++;
}

FText UChatControlSubsystem::GetSessionSummary() const
{
	const int32 Minutes = FMath::RoundToInt((FPlatformTime::Seconds() - SessionStartTime) / 60.0);
	FString TopViewer;
	int32 TopCount = 0;
	for (const auto& Pair : SessionEffectsByViewer)
	{
		if (Pair.Value > TopCount && !Pair.Key.Equals(TEXT("ADA")))
		{
			TopViewer = Pair.Key;
			TopCount = Pair.Value;
		}
	}
	return FText::Format(Tr(TEXT("Session: {0} min, {1} effects, {2} deaths, most active: {3} ({4})"),
		TEXT("Сессия: {0} мин, эффектов {1}, смертей {2}, самый активный: {3} ({4})")),
		FText::AsNumber(Minutes), FText::AsNumber(SessionEffectCount), FText::AsNumber(SessionDeaths),
		FText::FromString(TopViewer.IsEmpty() ? TEXT("-") : TopViewer), FText::AsNumber(TopCount));
}

void UChatControlSubsystem::SaveSessionSummary() const
{
	if (SessionEffectCount == 0 || !Settings.bSaveSessionFiles)
	{
		return;
	}
	FString Report = GetSessionSummary().ToString() + LINE_TERMINATOR LINE_TERMINATOR;
	TArray<TPair<FString, int32>> ByViewer;
	for (const auto& Pair : SessionEffectsByViewer)
	{
		ByViewer.Add({ Pair.Key, Pair.Value });
	}
	ByViewer.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Value > B.Value; });
	for (const auto& Pair : ByViewer)
	{
		Report += FString::Printf(TEXT("%s: %d") LINE_TERMINATOR, *Pair.Key, Pair.Value);
	}
	const FString Path = FPaths::ProjectSavedDir() / TEXT("ChatControl") / TEXT("Sessions") /
		FDateTime::Now().ToString(TEXT("Session_%Y-%m-%d_%H-%M.txt"));
	FFileHelper::SaveStringToFile(Report, *Path, FFileHelper::EEncodingOptions::ForceUTF8);
}
