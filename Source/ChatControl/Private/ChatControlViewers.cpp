// Things that make viewers visible in the world: greetings, ranks, named machines and pets that stay in the save.
#include "ChatControlSubsystem.h"
#include "ChatControl.h"
#include "ChatText.h"
#include "FGCharacterPlayer.h"
#include "FGGameState.h"
#include "Creature/FGCreature.h"
#include "Buildables/FGBuildableFactory.h"
#include "Components/WidgetComponent.h"
#include "FGVehicle.h"
#include "EngineUtils.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

using ChatText::Tr;

namespace
{
	const FColor PetColor(120, 255, 120);
	const FColor BuildingColor(255, 210, 90);
	const FColor WelcomeColor(255, 255, 255);

	/** Machines and vehicles (tractors, trucks, explorers, trains) can carry a viewer's name */
	bool IsNameable(const AActor* Actor)
	{
		return Actor && (Actor->IsA<AFGBuildableFactory>() || Actor->IsA<AFGVehicle>());
	}

	FText NameableDisplayName(const AActor* Actor)
	{
		if (const AFGVehicle* Vehicle = Cast<AFGVehicle>(Actor))
		{
			return Vehicle->mDisplayName;
		}
		const AFGBuildable* Building = Cast<AFGBuildable>(Actor);
		return Building ? Building->mDisplayName : FText::FromString(Actor->GetName());
	}

	/** Height of the name tag above a machine: just over its top */
	float TagHeightFor(const AActor* Actor)
	{
		FVector Origin, Extent;
		Actor->GetActorBounds(true, Origin, Extent);
		return FMath::Clamp(Origin.Z + Extent.Z - Actor->GetActorLocation().Z + 120.f, 150.f, 3000.f);
	}
}

void UChatControlSubsystem::WelcomeViewer(const FString& ViewerName)
{
	UWorld* World = GetGameWorld();
	AFGCharacterPlayer* Player = FindPlayer();
	if (!Settings.bWelcomeEnabled || !World || !Player)
	{
		return;
	}
	// A few meters in front of the camera, slightly to the side so it does not block the view
	FVector EyeLocation;
	FRotator EyeRotation;
	Player->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	const FRotator Flat(0.f, EyeRotation.Yaw, 0.f);
	const FVector Location = EyeLocation + Flat.Vector() * 450.f + FRotationMatrix(Flat).GetUnitAxis(EAxis::Y) * FMath::FRandRange(-150.f, 150.f) + FVector(0, 0, 60.f);
	AActor* Greeting = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Location));
	if (!Greeting)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Greeting);
	Greeting->SetRootComponent(Root);
	Root->RegisterComponent();
	Greeting->SetActorLocation(Location);
	AttachNameTag(Greeting, GetTagText(ViewerName), GetNameColor(ViewerName, WelcomeColor), 0.f);
	AttachNameTag(Greeting, Tr(TEXT("o7 joined the stream"), TEXT("o7 заходит на стрим")).ToString(), WelcomeColor, -45.f);
	Greeting->SetLifeSpan(FMath::Max(2.f, Settings.WelcomeSeconds));
	TrackSpawned(Greeting);
}

FString UChatControlSubsystem::GetRank(int32 ChipsSpent) const
{
	if (!Settings.bRanksEnabled)
	{
		return FString();
	}
	TArray<TPair<int32, FString>> Ranks;
	TArray<FString> Entries;
	Settings.Ranks.ParseIntoArray(Entries, TEXT(";"));
	for (const FString& Entry : Entries)
	{
		FString Threshold, Name;
		if (Entry.Split(TEXT("="), &Threshold, &Name) && !Name.TrimStartAndEnd().IsEmpty())
		{
			Ranks.Add({ FCString::Atoi(*Threshold), Name.TrimStartAndEnd() });
		}
	}
	if (Ranks.Num() == 0)
	{
		Ranks = {
			{ 0,     Tr(TEXT("Pioneer"), TEXT("Пионер")).ToString() },
			{ 200,   Tr(TEXT("Engineer"), TEXT("Инженер")).ToString() },
			{ 1000,  Tr(TEXT("Senior Engineer"), TEXT("Старший инженер")).ToString() },
			{ 3000,  Tr(TEXT("Project Lead"), TEXT("Руководитель проекта")).ToString() },
			{ 10000, Tr(TEXT("FICSIT Director"), TEXT("Директор FICSIT")).ToString() },
		};
	}
	Ranks.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });
	FString Result;
	for (const auto& Rank : Ranks)
	{
		if (ChipsSpent >= Rank.Key)
		{
			Result = Rank.Value;
		}
	}
	return Result;
}

void UChatControlSubsystem::CheckRankUp(const FString& ViewerKey, int32 SpentBefore)
{
	const FChatViewerRecord* Record = Economy.Find(ViewerKey);
	if (!Record)
	{
		return;
	}
	const FString NewRank = GetRank(Record->ChipsSpent);
	if (!NewRank.IsEmpty() && NewRank != GetRank(SpentBefore))
	{
		Announce(FText::Format(Tr(TEXT("{0} is now {1}!"), TEXT("{0} получает звание: {1}!")), FText::FromString(Record->DisplayName), FText::FromString(NewRank)));
	}
}

FString UChatControlSubsystem::GetTagText(const FString& ViewerName) const
{
	const FChatViewerRecord* Record = Economy.FindByName(ViewerName);
	const FString Rank = Record ? GetRank(Record->ChipsSpent) : FString();
	return Rank.IsEmpty() ? ViewerName : ViewerName + TEXT("<br>") + Rank;
}

void UChatControlSubsystem::SetupPet(APawn* Pet, const FString& ViewerKey, const FString& ViewerName, bool bAnnounce)
{
	// Like other creatures, a pet without a spawner gets culled right away unless it is persistent
	if (AFGCreature* Creature = Cast<AFGCreature>(Pet))
	{
		Creature->SetIsPersistent(true);
	}
	if (!Settings.bPetPermanent)
	{
		Pet->SetLifeSpan(FMath::Max(60.f, Settings.PetLifeSpanSeconds));
	}
	const bool bTamed = bAnnounce && FMath::FRand() < Settings.PetTamedChance && TryTamePet(Pet, FindPlayer());
	TryNamePet(Pet, ViewerName);
	AttachNameTag(Pet, GetTagText(ViewerName), GetNameColor(ViewerName, PetColor), 120.f);
	Pets.Add(ViewerKey, Pet);
	if (Settings.bPetPermanent)
	{
		FindOrAddMark(WorldData.Pets, ViewerKey, ViewerName).Location = Pet->GetActorLocation();
		SaveWorldData();
	}
	if (bAnnounce)
	{
		Announce(bTamed
			? FText::Format(Tr(TEXT("{0}'s pet came to visit - and it is already tame!"), TEXT("Питомец {0} пришёл в гости - и он уже ручной!")), FText::FromString(ViewerName))
			: FText::Format(Tr(TEXT("{0}'s pet came to visit!"), TEXT("Питомец {0} пришёл в гости!")), FText::FromString(ViewerName)));
	}
}

bool UChatControlSubsystem::NameBuilding(const FString& ViewerKey, const FString& ViewerName)
{
	UWorld* World = GetGameWorld();
	AFGCharacterPlayer* Player = FindPlayer();
	if (!World || !Player)
	{
		return false;
	}
	// The machine or vehicle under the crosshair, otherwise the nearest one within 20 m (vehicles first)
	AActor* Target = nullptr;
	FVector EyeLocation;
	FRotator EyeRotation;
	Player->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	if (APlayerController* Controller = World->GetFirstPlayerController())
	{
		Controller->GetPlayerViewPoint(EyeLocation, EyeRotation);
	}
	FHitResult Hit;
	FCollisionQueryParams Query(TEXT("ChatControlName"), false, Player);
	if (World->LineTraceSingleByChannel(Hit, EyeLocation, EyeLocation + EyeRotation.Vector() * 3000.f, ECC_Visibility, Query))
	{
		Target = IsNameable(Hit.GetActor()) ? Hit.GetActor() : nullptr;
	}
	if (!Target)
	{
		float Best = 2000.f;
		for (TActorIterator<AFGVehicle> It(World); It; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), Player->GetActorLocation());
			if (Distance < Best)
			{
				Best = Distance;
				Target = *It;
			}
		}
	}
	if (!Target)
	{
		float Best = 2000.f;
		for (TActorIterator<AFGBuildableFactory> It(World); It; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), Player->GetActorLocation());
			if (Distance < Best)
			{
				Best = Distance;
				Target = *It;
			}
		}
	}
	if (!Target)
	{
		Announce(FText::Format(Tr(TEXT("{0} wanted to sign a machine, but there is none nearby."), TEXT("{0} хотел(а) подписать машину, но рядом нет ни одной.")), FText::FromString(ViewerName)));
		return false;
	}
	// One machine per viewer: the name moves to the new one
	if (TWeakObjectPtr<UWidgetComponent>* OldTag = BuildingTags.Find(ViewerKey); OldTag && OldTag->IsValid())
	{
		(*OldTag)->DestroyComponent();
	}
	BuildingTags.Add(ViewerKey, AttachNameTag(Target, GetTagText(ViewerName), GetNameColor(ViewerName, BuildingColor), TagHeightFor(Target)));
	NamedBuildings.Add(ViewerKey, Target);
	FindOrAddMark(WorldData.Buildings, ViewerKey, ViewerName).Location = Target->GetActorLocation();
	SaveWorldData();
	Announce(FText::Format(Tr(TEXT("{0} signed this machine: {1}"), TEXT("{0} подписывает машину: {1}")), FText::FromString(ViewerName), NameableDisplayName(Target)));
	return true;
}

FChatWorldMark& UChatControlSubsystem::FindOrAddMark(TArray<FChatWorldMark>& Marks, const FString& ViewerKey, const FString& ViewerName)
{
	for (FChatWorldMark& Mark : Marks)
	{
		if (Mark.ViewerKey == ViewerKey)
		{
			Mark.ViewerName = ViewerName;
			return Mark;
		}
	}
	FChatWorldMark& Mark = Marks.AddDefaulted_GetRef();
	Mark.ViewerKey = ViewerKey;
	Mark.ViewerName = ViewerName;
	return Mark;
}

void UChatControlSubsystem::TickWorldData(double Now)
{
	UWorld* World = GetGameWorld();
	const AFGGameState* GameState = World ? World->GetGameState<AFGGameState>() : nullptr;
	if (!GameState || !FindPlayer())
	{
		return;
	}
	if (LoadedWorld.Get() != World)
	{
		// A save was loaded: read what viewers left in it, and give the world a moment to settle
		LoadedWorld = World;
		WorldData = FChatWorldData();
		Pets.Empty();
		NamedBuildings.Empty();
		BuildingTags.Empty();
		const FString Session = FPaths::MakeValidFileName(GameState->GetSessionName().IsEmpty() ? TEXT("Default") : GameState->GetSessionName());
		WorldDataPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ChatControl"), TEXT("Worlds"), Session + TEXT(".json"));
		FString Json;
		if (FFileHelper::LoadFileToString(Json, *WorldDataPath))
		{
			FJsonObjectConverter::JsonObjectStringToUStruct(Json, &WorldData);
		}
		bWorldApplied = false;
		WorldApplyTime = Now + 5.0;
		return;
	}
	if (!bWorldApplied && Now >= WorldApplyTime)
	{
		bWorldApplied = true;
		ApplyWorldData();
	}
	if (bWorldApplied && Now >= NextWorldSave)
	{
		NextWorldSave = Now + 30.0;
		// Pets wander: remember where they are; forget pets that died and machines that were dismantled
		WorldData.Pets.RemoveAll([this](FChatWorldMark& Mark)
		{
			const TWeakObjectPtr<AActor>* Pet = Pets.Find(Mark.ViewerKey);
			if (!Pet || !Pet->IsValid())
			{
				return true;
			}
			Mark.Location = (*Pet)->GetActorLocation();
			return false;
		});
		WorldData.Buildings.RemoveAll([this](FChatWorldMark& Mark)
		{
			const TWeakObjectPtr<AActor>* Building = NamedBuildings.Find(Mark.ViewerKey);
			if (!Building || !Building->IsValid())
			{
				return true;
			}
			// Vehicles drive around
			Mark.Location = (*Building)->GetActorLocation();
			return false;
		});
		SaveWorldData();
	}
}

void UChatControlSubsystem::ApplyWorldData()
{
	UWorld* World = GetGameWorld();
	if (!World)
	{
		return;
	}
	for (const FChatWorldMark& Mark : WorldData.Buildings)
	{
		// Machines stand still; a vehicle is saved where it was at most 30 s after the last update
		AActor* Found = nullptr;
		float Best = 1500.f;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), Mark.Location);
			if (Distance < Best && IsNameable(*It) && (It->IsA<AFGVehicle>() || Distance < 100.f))
			{
				Best = Distance;
				Found = *It;
			}
		}
		if (Found)
		{
			NamedBuildings.Add(Mark.ViewerKey, Found);
			BuildingTags.Add(Mark.ViewerKey, AttachNameTag(Found, GetTagText(Mark.ViewerName), GetNameColor(Mark.ViewerName, BuildingColor), TagHeightFor(Found)));
		}
	}

	if (!Settings.bPetPermanent)
	{
		return;
	}
	// The game keeps persistent creatures in the save; adopt the doggo closest to where the pet was, or bring a new one
	UClass* PetClass = LoadClass<APawn>(nullptr, PetClassPath);
	TSet<APawn*> Adopted;
	const TArray<FChatWorldMark> Marks = WorldData.Pets;
	for (const FChatWorldMark& Mark : Marks)
	{
		APawn* Best = nullptr;
		float BestDistance = 3000.f;
		for (TActorIterator<APawn> It(World, PetClass); PetClass && It; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), Mark.Location);
			if (!Adopted.Contains(*It) && Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = *It;
			}
		}
		if (Best)
		{
			Adopted.Add(Best);
			SetupPet(Best, Mark.ViewerKey, Mark.ViewerName, false);
		}
		else
		{
			SpawnPet(Mark.ViewerKey, Mark.ViewerName, &Mark.Location);
		}
	}
	UE_LOG(LogChatControl, Log, TEXT("World data: %d named machines, %d pets (%d found in the save)"), NamedBuildings.Num(), Marks.Num(), Adopted.Num());
}

void UChatControlSubsystem::SaveWorldData()
{
	FString Json;
	if (!WorldDataPath.IsEmpty() && FJsonObjectConverter::UStructToJsonObjectString(WorldData, Json))
	{
		FFileHelper::SaveStringToFile(Json, *WorldDataPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}
