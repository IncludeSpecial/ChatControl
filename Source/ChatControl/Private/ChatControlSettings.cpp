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
	TPair<const TCHAR*, FChatEffectSettings> Defaults[] = {
		// Free tier
		{ TEXT("hi"),       MakeEffect(0,   5.f,   120.f, 0.f,  0) },
		{ TEXT("gift"),     MakeEffect(0,   15.f,  300.f, 0.f,  0, 0.5f) },
		// Paid tier
		{ TEXT("biggift"),  MakeEffect(150, 30.f,  60.f,  0.f,  0, 0.5f) },
		{ TEXT("jump"),     MakeEffect(30,  30.f,  60.f,  20.f, 1, 2.5f) },
		{ TEXT("moon"),     MakeEffect(50,  45.f,  60.f,  30.f, 1, 0.25f) },
		{ TEXT("heavy"),    MakeEffect(60,  45.f,  60.f,  20.f, 2, 3.f) },
		{ TEXT("speed"),    MakeEffect(40,  30.f,  60.f,  20.f, 1, 2.f) },
		{ TEXT("slow"),     MakeEffect(50,  45.f,  60.f,  20.f, 2, 0.5f) },
		{ TEXT("fov"),      MakeEffect(40,  30.f,  60.f,  30.f, 1, 1.6f) },
		// Strength = distance in meters
		{ TEXT("teleport"), MakeEffect(100, 60.f,  180.f, 0.f,  2, 80.f) },
		{ TEXT("hub"),      MakeEffect(150, 120.f, 300.f, 0.f,  2) },
		// Factory: Amount = machines
		{ TEXT("strike"),   MakeEffect(120, 120.f, 180.f, 60.f, 3, 0.f, 5) },
		{ TEXT("underclock"), MakeEffect(120, 120.f, 180.f, 60.f, 3, 0.5f, 10) },
		{ TEXT("blackout"), MakeEffect(200, 300.f, 600.f, 15.f, 4) },
		// Gifts: rain Amount = stacks, sink Amount = coupons
		{ TEXT("rain"),     MakeEffect(100, 60.f,  120.f, 0.f,  0, 0.5f, 5) },
		{ TEXT("sink"),     MakeEffect(300, 300.f, 600.f, 0.f,  0, 0.f, 1) },
		// World
		{ TEXT("night"),    MakeEffect(60,  120.f, 120.f, 0.f,  1) },
		{ TEXT("day"),      MakeEffect(60,  120.f, 120.f, 0.f,  0) },
		// Viewers in the world: sign Duration = how long it stays
		{ TEXT("sign"),     MakeEffect(20,  10.f,  120.f, 600.f, 0) },
		{ TEXT("pet"),      MakeEffect(100, 30.f,  1800.f, 0.f, 0) },
		// Creatures
		{ TEXT("ambush"),   MakeEffect(80,  60.f,  120.f, 0.f,  2, 0.f, 3, 3.f, 5.f) },
		{ TEXT("boss"),     MakeEffect(300, 180.f, 300.f, 0.f,  4, 0.f, 1, 3.f, 5.f) },
	};
	// Free and personal effects make no sense as a random pick; channel point prices start at cost x 10
	const TSet<FString> NotRandom = { TEXT("hi"), TEXT("gift"), TEXT("sign"), TEXT("pet"), TEXT("day") };
	for (auto& Pair : Defaults)
	{
		Pair.Value.bRandomPool = !NotRandom.Contains(Pair.Key);
		Pair.Value.ChannelPointsCost = Pair.Value.Cost > 0 && Pair.Value.ChaosWeight > 0 ? Pair.Value.Cost * 10 : 0;
	}
	for (const auto& Pair : Defaults)
	{
		FChatEffectSettings* Existing = Effects.Find(Pair.Key);
		if (!Existing)
		{
			Effects.Add(Pair.Key, Pair.Value);
			continue;
		}
		if (Existing && SettingsVersion < 3)
		{
			Existing->bRandomPool = Pair.Value.bRandomPool;
			Existing->ChannelPointsCost = Pair.Value.ChannelPointsCost;
		}
		if (Existing && SettingsVersion < 2)
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
