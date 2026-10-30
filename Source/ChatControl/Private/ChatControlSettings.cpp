#include "ChatControlSettings.h"
#include "ChatControl.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

static FChatEffectSettings MakeEffect(int32 Cost, float GlobalCooldown, float ViewerCooldown, float Duration, int32 ChaosWeight, int32 Amount = 0)
{
	FChatEffectSettings Effect;
	Effect.Cost = Cost;
	Effect.GlobalCooldown = GlobalCooldown;
	Effect.ViewerCooldown = ViewerCooldown;
	Effect.Duration = Duration;
	Effect.ChaosWeight = ChaosWeight;
	Effect.Amount = Amount;
	return Effect;
}

void FChatControlSettings::AddMissingEffectDefaults()
{
	const TPair<const TCHAR*, FChatEffectSettings> Defaults[] = {
		// Free tier
		{ TEXT("hi"),       MakeEffect(0,   5.f,  120.f, 0.f,  0) },
		{ TEXT("gift"),     MakeEffect(0,   15.f, 300.f, 0.f,  0) },
		// Paid tier
		{ TEXT("biggift"),  MakeEffect(150, 30.f, 60.f,  0.f,  0) },
		{ TEXT("jump"),     MakeEffect(30,  30.f, 60.f,  20.f, 1) },
		{ TEXT("moon"),     MakeEffect(50,  45.f, 60.f,  30.f, 1) },
		{ TEXT("heavy"),    MakeEffect(60,  45.f, 60.f,  20.f, 2) },
		// Creatures
		{ TEXT("ambush"),   MakeEffect(80,  60.f,  120.f, 0.f, 2, 3) },
		{ TEXT("boss"),     MakeEffect(300, 180.f, 300.f, 0.f, 4, 1) },
	};
	for (const auto& Pair : Defaults)
	{
		if (!Effects.Contains(Pair.Key))
		{
			Effects.Add(Pair.Key, Pair.Value);
		}
	}
}

FString FChatControlSettings::GetSettingsPath()
{
	return FPaths::ProjectSavedDir() / TEXT("ChatControl") / TEXT("Settings.json");
}

FChatControlSettings FChatControlSettings::LoadOrCreate()
{
	FChatControlSettings Settings;
	FString Json;
	if (FFileHelper::LoadFileToString(Json, *GetSettingsPath()))
	{
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Settings))
		{
			UE_LOG(LogChatControl, Error, TEXT("Failed to parse %s, using defaults"), *GetSettingsPath());
			Settings = FChatControlSettings();
		}
	}
	Settings.AddMissingEffectDefaults();
	// Write back so new options appear in the file after updates
	Settings.Save();
	return Settings;
}

void FChatControlSettings::Save() const
{
	FString Json;
	if (FJsonObjectConverter::UStructToJsonObjectString(*this, Json))
	{
		FFileHelper::SaveStringToFile(Json, *GetSettingsPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}
