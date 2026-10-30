#include "ChatControlSettings.h"
#include "ChatControl.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

static FChatEffectSettings MakeEffect(int32 Cost, float GlobalCooldown, float ViewerCooldown, float Duration, int32 ChaosWeight,
	float Strength = 0.f, int32 Amount = 0, float DelayMin = 0.f, float DelayMax = 0.f)
{
	FChatEffectSettings Effect;
	Effect.Cost = Cost;
	Effect.GlobalCooldown = GlobalCooldown;
	Effect.ViewerCooldown = ViewerCooldown;
	Effect.Duration = Duration;
	Effect.ChaosWeight = ChaosWeight;
	Effect.Strength = Strength;
	Effect.Amount = Amount;
	Effect.DelayMin = DelayMin;
	Effect.DelayMax = DelayMax;
	return Effect;
}

void FChatControlSettings::AddMissingEffectDefaults()
{
	//                                    cost  global  viewer  dur   chaos strength amount delay
	const TPair<const TCHAR*, FChatEffectSettings> Defaults[] = {
		// Free tier
		{ TEXT("hi"),       MakeEffect(0,   5.f,   120.f, 0.f,  0) },
		{ TEXT("gift"),     MakeEffect(0,   15.f,  300.f, 0.f,  0, 0.5f) },
		// Paid tier
		{ TEXT("biggift"),  MakeEffect(150, 30.f,  60.f,  0.f,  0, 0.5f) },
		{ TEXT("jump"),     MakeEffect(30,  30.f,  60.f,  20.f, 1, 2.5f) },
		{ TEXT("moon"),     MakeEffect(50,  45.f,  60.f,  30.f, 1, 0.25f) },
		{ TEXT("heavy"),    MakeEffect(60,  45.f,  60.f,  20.f, 2, 3.f) },
		// Creatures
		{ TEXT("ambush"),   MakeEffect(80,  60.f,  120.f, 0.f,  2, 0.f, 3, 3.f, 5.f) },
		{ TEXT("boss"),     MakeEffect(300, 180.f, 300.f, 0.f,  4, 0.f, 1, 3.f, 5.f) },
	};
	for (const auto& Pair : Defaults)
	{
		FChatEffectSettings* Existing = Effects.Find(Pair.Key);
		if (!Existing)
		{
			Effects.Add(Pair.Key, Pair.Value);
		}
		else if (SettingsVersion < 2)
		{
			// Fields added in version 2 are missing from older files and would load as 0
			Existing->Strength = Pair.Value.Strength;
			Existing->Amount = Pair.Value.Amount;
			Existing->DelayMin = Pair.Value.DelayMin;
			Existing->DelayMax = Pair.Value.DelayMax;
		}
	}
	SettingsVersion = CurrentVersion;
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
