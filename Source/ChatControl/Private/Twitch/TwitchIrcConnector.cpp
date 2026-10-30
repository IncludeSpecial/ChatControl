#include "TwitchIrcConnector.h"
#include "ChatControl.h"
#include "HAL/RunnableThread.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "AddressInfoTypes.h"

namespace
{
	const TCHAR* TwitchHost = TEXT("irc.chat.twitch.tv");
	const TCHAR* TwitchPort = TEXT("6667");

	FString UnescapeTagValue(const FString& Value)
	{
		FString Result;
		Result.Reserve(Value.Len());
		for (int32 i = 0; i < Value.Len(); ++i)
		{
			if (Value[i] == TEXT('\\') && i + 1 < Value.Len())
			{
				switch (Value[++i])
				{
				case TEXT('s'): Result.AppendChar(TEXT(' ')); break;
				case TEXT(':'): Result.AppendChar(TEXT(';')); break;
				case TEXT('r'): Result.AppendChar(TEXT('\r')); break;
				case TEXT('n'): Result.AppendChar(TEXT('\n')); break;
				default: Result.AppendChar(Value[i]); break;
				}
			}
			else
			{
				Result.AppendChar(Value[i]);
			}
		}
		return Result;
	}

	/** Parsed IRCv3 line: "@tags :prefix COMMAND params :trailing" */
	struct FIrcMessage
	{
		TMap<FString, FString> Tags;
		FString Prefix;
		FString Command;
		TArray<FString> Params;
		FString Trailing;

		FString Tag(const TCHAR* Key) const
		{
			const FString* Found = Tags.Find(Key);
			return Found ? *Found : FString();
		}

		/** Login name from ":login!login@login.tmi.twitch.tv" */
		FString Login() const
		{
			FString Login;
			Prefix.Split(TEXT("!"), &Login, nullptr);
			return Login;
		}
	};

	FIrcMessage ParseIrc(FString Line)
	{
		FIrcMessage Msg;
		if (Line.StartsWith(TEXT("@")))
		{
			FString TagsPart;
			Line.Split(TEXT(" "), &TagsPart, &Line);
			TArray<FString> Pairs;
			TagsPart.RightChop(1).ParseIntoArray(Pairs, TEXT(";"));
			for (const FString& Pair : Pairs)
			{
				FString Key, Value;
				if (!Pair.Split(TEXT("="), &Key, &Value))
				{
					Key = Pair;
				}
				Msg.Tags.Add(Key, UnescapeTagValue(Value));
			}
		}
		if (Line.StartsWith(TEXT(":")))
		{
			Line.RightChop(1).Split(TEXT(" "), &Msg.Prefix, &Line);
		}
		FString Middle = Line;
		if (Line.Split(TEXT(" :"), &Middle, &Msg.Trailing))
		{
			// trailing parameter extracted
		}
		else if (Line.StartsWith(TEXT(":")))
		{
			Middle.Empty();
			Msg.Trailing = Line.RightChop(1);
		}
		Middle.ParseIntoArray(Msg.Params, TEXT(" "));
		if (Msg.Params.Num() > 0)
		{
			Msg.Command = Msg.Params[0];
			Msg.Params.RemoveAt(0);
		}
		return Msg;
	}

	void FillUser(const FIrcMessage& Msg, FChatEvent& Event)
	{
		Event.Platform = TEXT("twitch");
		Event.UserId = Msg.Tag(TEXT("user-id"));
		Event.DisplayName = Msg.Tag(TEXT("display-name"));
		if (Event.DisplayName.IsEmpty())
		{
			Event.DisplayName = Msg.Tag(TEXT("login"));
		}
		if (Event.DisplayName.IsEmpty())
		{
			Event.DisplayName = Msg.Login();
		}
		if (Event.UserId.IsEmpty())
		{
			Event.UserId = Event.DisplayName.ToLower();
		}
		const FString Badges = Msg.Tag(TEXT("badges"));
		Event.bIsBroadcaster = Badges.Contains(TEXT("broadcaster/"));
		Event.bIsModerator = Msg.Tag(TEXT("mod")) == TEXT("1");
	}
}

FTwitchIrcConnector::FTwitchIrcConnector(const FString& InChannel, TQueue<FChatEvent, EQueueMode::Mpsc>& InOutput)
	: Channel(InChannel.ToLower())
	, Output(InOutput)
{
	Channel.RemoveFromStart(TEXT("#"));
}

FTwitchIrcConnector::~FTwitchIrcConnector()
{
	if (Thread)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}
	Disconnect();
}

void FTwitchIrcConnector::Start()
{
	if (!Thread && !Channel.IsEmpty())
	{
		Thread = FRunnableThread::Create(this, TEXT("ChatControl_TwitchIrc"));
	}
}

void FTwitchIrcConnector::Stop()
{
	bStopRequested = true;
}

bool FTwitchIrcConnector::Connect()
{
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Sockets)
	{
		return false;
	}

	const FAddressInfoResult Resolved = Sockets->GetAddressInfo(TwitchHost, TwitchPort, EAddressInfoFlags::Default, NAME_None, ESocketType::SOCKTYPE_Streaming);
	if (Resolved.ReturnCode != SE_NO_ERROR || Resolved.Results.Num() == 0)
	{
		UE_LOG(LogChatControl, Warning, TEXT("Twitch: failed to resolve %s"), TwitchHost);
		return false;
	}

	const TSharedRef<FInternetAddr> Address = Resolved.Results[0].Address;
	Socket = Sockets->CreateSocket(NAME_Stream, TEXT("ChatControl Twitch IRC"), Address->GetProtocolType());
	if (!Socket)
	{
		return false;
	}
	if (!Socket->Connect(*Address))
	{
		UE_LOG(LogChatControl, Warning, TEXT("Twitch: failed to connect to %s"), *Address->ToString(true));
		Disconnect();
		return false;
	}

	PendingBytes.Reset();
	// Anonymous read-only login: any "justinfan<digits>" nick works without a token
	const bool bSent =
		SendLine(TEXT("CAP REQ :twitch.tv/tags twitch.tv/commands")) &&
		SendLine(TEXT("PASS SCHMOOPIIE")) &&
		SendLine(FString::Printf(TEXT("NICK justinfan%d"), FMath::RandRange(10000, 99999))) &&
		SendLine(FString::Printf(TEXT("JOIN #%s"), *Channel));
	if (!bSent)
	{
		Disconnect();
		return false;
	}

	UE_LOG(LogChatControl, Log, TEXT("Twitch: connected to #%s"), *Channel);
	bConnected = true;
	return true;
}

void FTwitchIrcConnector::Disconnect()
{
	bConnected = false;
	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}

bool FTwitchIrcConnector::SendLine(const FString& Line)
{
	if (!Socket)
	{
		return false;
	}
	const FTCHARToUTF8 Utf8(*(Line + TEXT("\r\n")));
	int32 Sent = 0;
	return Socket->Send(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), Sent) && Sent == Utf8.Length();
}

uint32 FTwitchIrcConnector::Run()
{
	float ReconnectDelay = 2.f;
	double LastReceiveTime = 0.0;
	bool bPingSent = false;

	while (!bStopRequested)
	{
		if (!Socket)
		{
			if (!Connect())
			{
				// Exponential backoff up to one minute, but stay responsive to Stop()
				for (float Waited = 0.f; Waited < ReconnectDelay && !bStopRequested; Waited += 0.25f)
				{
					FPlatformProcess::Sleep(0.25f);
				}
				ReconnectDelay = FMath::Min(ReconnectDelay * 2.f, 60.f);
				continue;
			}
			ReconnectDelay = 2.f;
			LastReceiveTime = FPlatformTime::Seconds();
			bPingSent = false;
		}

		if (Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(250)))
		{
			uint8 Buffer[4096];
			int32 Read = 0;
			if (!Socket->Recv(Buffer, sizeof(Buffer), Read) || Read <= 0)
			{
				UE_LOG(LogChatControl, Warning, TEXT("Twitch: connection lost, reconnecting"));
				Disconnect();
				continue;
			}
			LastReceiveTime = FPlatformTime::Seconds();
			bPingSent = false;
			PendingBytes.Append(Buffer, Read);

			// Split complete lines; partial UTF-8 sequences stay in the buffer until the line ends
			int32 LineStart = 0;
			for (int32 i = 0; i + 1 < PendingBytes.Num(); ++i)
			{
				if (PendingBytes[i] == '\r' && PendingBytes[i + 1] == '\n')
				{
					const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(PendingBytes.GetData() + LineStart), i - LineStart);
					HandleLine(FString(Converted.Length(), Converted.Get()));
					LineStart = i + 2;
					++i;
				}
			}
			PendingBytes.RemoveAt(0, LineStart, EAllowShrinking::No);
		}
		else
		{
			// Detect silently dropped connections: Twitch pings every ~5 minutes
			const double Idle = FPlatformTime::Seconds() - LastReceiveTime;
			if (Idle > 360.0 && !bPingSent)
			{
				bPingSent = SendLine(TEXT("PING :chatcontrol"));
			}
			else if (Idle > 420.0)
			{
				UE_LOG(LogChatControl, Warning, TEXT("Twitch: no response, reconnecting"));
				Disconnect();
			}
		}
	}

	Disconnect();
	return 0;
}

void FTwitchIrcConnector::HandleLine(const FString& Line)
{
	const FIrcMessage Msg = ParseIrc(Line);

	if (Msg.Command == TEXT("PING"))
	{
		SendLine(TEXT("PONG :") + Msg.Trailing);
		return;
	}
	if (Msg.Command == TEXT("RECONNECT"))
	{
		Disconnect();
		return;
	}

	if (Msg.Command == TEXT("PRIVMSG"))
	{
		FChatEvent Event;
		FillUser(Msg, Event);
		Event.Text = Msg.Trailing;

		const int32 Bits = FCString::Atoi(*Msg.Tag(TEXT("bits")));
		const FString RewardId = Msg.Tag(TEXT("custom-reward-id"));
		if (Bits > 0)
		{
			Event.Type = EChatEventType::Bits;
			Event.Amount = Bits;
		}
		else if (!RewardId.IsEmpty())
		{
			Event.Type = EChatEventType::RewardRedeem;
			Event.RewardId = RewardId;
		}
		Output.Enqueue(MoveTemp(Event));
		return;
	}

	if (Msg.Command == TEXT("USERNOTICE"))
	{
		const FString MsgId = Msg.Tag(TEXT("msg-id"));
		FChatEvent Event;
		FillUser(Msg, Event);
		Event.Text = Msg.Trailing;

		if (MsgId == TEXT("sub") || MsgId == TEXT("resub"))
		{
			Event.Type = EChatEventType::Subscription;
			Event.Amount = 1;
		}
		else if (MsgId == TEXT("submysterygift"))
		{
			Event.Type = EChatEventType::GiftSubs;
			Event.Amount = FMath::Max(1, FCString::Atoi(*Msg.Tag(TEXT("msg-param-mass-gift-count"))));
		}
		else if (MsgId == TEXT("subgift") && Msg.Tag(TEXT("msg-param-community-gift-id")).IsEmpty())
		{
			// Individual gifts that belong to a mass gift are already counted by submysterygift
			Event.Type = EChatEventType::GiftSubs;
			Event.Amount = 1;
		}
		else if (MsgId == TEXT("raid"))
		{
			Event.Type = EChatEventType::Raid;
			Event.Amount = FCString::Atoi(*Msg.Tag(TEXT("msg-param-viewerCount")));
		}
		else
		{
			return;
		}
		Output.Enqueue(MoveTemp(Event));
	}
}
