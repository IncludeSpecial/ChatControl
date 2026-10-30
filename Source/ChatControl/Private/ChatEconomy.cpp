#include "ChatEconomy.h"
#include "ChatControl.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString FChatEconomy::GetSavePath()
{
	return FPaths::ProjectSavedDir() / TEXT("ChatControl") / TEXT("Economy.json");
}

void FChatEconomy::Load()
{
	FString Json;
	if (FFileHelper::LoadFileToString(Json, *GetSavePath()) &&
		!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Data))
	{
		UE_LOG(LogChatControl, Error, TEXT("Failed to parse %s"), *GetSavePath());
	}
	bDirty = false;
}

void FChatEconomy::Save() const
{
	FString Json;
	if (FJsonObjectConverter::UStructToJsonObjectString(Data, Json))
	{
		FFileHelper::SaveStringToFile(Json, *GetSavePath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		bDirty = false;
	}
}

FChatViewerRecord& FChatEconomy::GetOrAdd(const FString& Key, const FString& DisplayName)
{
	FChatViewerRecord& Record = Data.Viewers.FindOrAdd(Key);
	if (!DisplayName.IsEmpty() && Record.DisplayName != DisplayName)
	{
		Record.DisplayName = DisplayName;
		bDirty = true;
	}
	return Record;
}

int32 FChatEconomy::OnViewerMessage(const FString& Key, const FString& DisplayName, double Now, int32 FirstMessageBonus)
{
	FChatViewerRecord& Record = GetOrAdd(Key, DisplayName);
	Record.LastMessageTime = Now;
	if (!Record.bGreetedThisSession)
	{
		Record.bGreetedThisSession = true;
		Record.Chips += FirstMessageBonus;
		bDirty = true;
		return FirstMessageBonus;
	}
	return 0;
}

void FChatEconomy::AddChips(const FString& Key, const FString& DisplayName, int32 Amount)
{
	GetOrAdd(Key, DisplayName).Chips += Amount;
	bDirty = true;
}

bool FChatEconomy::TrySpend(const FString& Key, int32 Amount)
{
	FChatViewerRecord* Record = Data.Viewers.Find(Key);
	if (!Record || Record->Chips < Amount)
	{
		return false;
	}
	Record->Chips -= Amount;
	Record->ChipsSpent += Amount;
	bDirty = true;
	return true;
}

int32 FChatEconomy::PayActiveViewers(double Now, double ActiveWindow, int32 Amount)
{
	int32 Paid = 0;
	for (auto& Pair : Data.Viewers)
	{
		if (Pair.Value.LastMessageTime >= 0.0 && Now - Pair.Value.LastMessageTime <= ActiveWindow)
		{
			Pair.Value.Chips += Amount;
			++Paid;
		}
	}
	bDirty |= Paid > 0;
	return Paid;
}

int32 FChatEconomy::CountActiveViewers(double Now, double ActiveWindow) const
{
	int32 Count = 0;
	for (const auto& Pair : Data.Viewers)
	{
		if (Pair.Value.LastMessageTime >= 0.0 && Now - Pair.Value.LastMessageTime <= ActiveWindow)
		{
			++Count;
		}
	}
	return Count;
}
