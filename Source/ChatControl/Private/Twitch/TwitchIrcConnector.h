#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Containers/Queue.h"
#include "ChatEvent.h"

class FSocket;
class FRunnableThread;

/**
 * Reads Twitch chat anonymously over plain IRC (irc.chat.twitch.tv:6667).
 * No token needed: anonymous connections receive messages, bits, subs, raids and text-input channel point rewards.
 * Runs on its own thread and pushes parsed events into a queue drained by the game thread.
 */
class FTwitchIrcConnector : public FRunnable
{
public:
	FTwitchIrcConnector(const FString& InChannel, TQueue<FChatEvent, EQueueMode::Mpsc>& InOutput);
	virtual ~FTwitchIrcConnector() override;

	void Start();

	bool IsConnected() const { return bConnected; }

	// FRunnable
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	bool Connect();
	void Disconnect();
	bool SendLine(const FString& Line);
	void HandleLine(const FString& Line);

	FString Channel;
	TQueue<FChatEvent, EQueueMode::Mpsc>& Output;

	FSocket* Socket = nullptr;
	FRunnableThread* Thread = nullptr;
	TAtomic<bool> bStopRequested{false};
	TAtomic<bool> bConnected{false};
	/** Bytes received but not yet terminated by \r\n */
	TArray<uint8> PendingBytes;
};
