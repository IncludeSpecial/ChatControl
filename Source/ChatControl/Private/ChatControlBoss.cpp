// Titans: boss fights with a random boss, random traits, three stages, a health bar and rewards.
#include "ChatControlSubsystem.h"
#include "ChatControl.h"
#include "ChatText.h"
#include "ChatEffects.h"
#include "FGCharacterPlayer.h"
#include "FGHealthComponent.h"
#include "Equipment/FGJetPack.h"
#include "Equipment/FGHoverPack.h"
#include "Creature/FGCreature.h"
#include "Creature/Enemy/FGCrabHatcher.h"
#include "AI/FGCreatureController.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Styling/CoreStyle.h"
#include "Dom/JsonObject.h"

using ChatText::Tr;

namespace
{
	const TCHAR* const Hog = TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/Char_Hog.Char_Hog_C");
	const TCHAR* const NuclearHog = TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/NuclearHog/Char_NuclearHog.Char_NuclearHog_C");
	const TCHAR* const SmallStinger = TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/SmallStinger/Char_Stinger_Child.Char_Stinger_Child_C");
	const TCHAR* const ForestSpitter = TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForest/Char_SpitterForestSmall.Char_SpitterForestSmall_C");
	const TCHAR* const RedSpitter = TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForestRed/Char_SpitterForestRedSmall.Char_SpitterForestRedSmall_C");
	const TCHAR* const DesertSpitter = TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterDesert/Char_SpitterDesertSmall.Char_SpitterDesertSmall_C");
	const TCHAR* const AquaticSpitter = TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterAquatic/Char_SpitterAquaticSmall.Char_SpitterAquaticSmall_C");
	const TCHAR* const CrabHatcher = TEXT("/Game/FactoryGame/Character/Creature/Enemy/CrabHatcher/Char_CrabHatcher.Char_CrabHatcher_C");

	/** How a boss fights, which decides what its stages do */
	enum class EBossKind : uint8 { Melee, Nuclear, Swarm, Spitter, Hatcher, Peaceful };

	struct FBossType
	{
		const TCHAR* Id;
		const TCHAR* English;
		const TCHAR* Russian;
		/** Russian name is feminine, so traits agree with it */
		bool bFeminine;
		const TCHAR* Class;
		float Scale;
		/** Times the creature's own health */
		float Health;
		EBossKind Kind;
		int32 Weight;
		const TCHAR* Minions[2];
	};

	const FBossType BossTypes[] = {
		{ TEXT("hogking"),  TEXT("Hog King"),        TEXT("Король кабанов"),     false, TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/AlphaHog/Char_AlphaHog.Char_AlphaHog_C"), 1.4f, 12.f, EBossKind::Melee, 10, { Hog, Hog } },
		{ TEXT("cliff"),    TEXT("Cliff Tyrant"),    TEXT("Скальный тиран"),     false, TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/CliffHog/Char_CliffHog.Char_CliffHog_C"), 1.35f, 12.f, EBossKind::Melee, 8, { Hog, Hog } },
		{ TEXT("nuclear"),  TEXT("Nuclear Hog"),     TEXT("Ядерный кабан"),      false, NuclearHog, 1.4f, 9.f, EBossKind::Nuclear, 6, { NuclearHog, Hog } },
		{ TEXT("queen"),    TEXT("Stinger Queen"),   TEXT("Королева жал"),       true,  TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/BigStinger/Char_EliteStinger.Char_EliteStinger_C"), 1.35f, 9.f, EBossKind::Swarm, 10, { SmallStinger, SmallStinger } },
		{ TEXT("stinger"),  TEXT("Stinger Lord"),    TEXT("Повелитель жал"),     false, TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/Char_Stinger.Char_Stinger_C"), 1.5f, 12.f, EBossKind::Swarm, 6, { SmallStinger, SmallStinger } },
		{ TEXT("green"),    TEXT("Green Matriarch"), TEXT("Зелёный матриарх"),   false, TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForest/Char_SpitterForestAlpha.Char_SpitterForestAlpha_C"), 1.35f, 9.f, EBossKind::Spitter, 7, { ForestSpitter, DesertSpitter } },
		{ TEXT("fire"),     TEXT("Fire Matriarch"),  TEXT("Огненный матриарх"),  false, TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForestRed/Char_SpitterForestRedAlpha.Char_SpitterForestRedAlpha_C"), 1.35f, 9.f, EBossKind::Spitter, 7, { RedSpitter, ForestSpitter } },
		{ TEXT("sand"),     TEXT("Sand Matriarch"),  TEXT("Песчаный матриарх"),  false, TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterDesert/Char_SpitterDesertAlpha.Char_SpitterDesertAlpha_C"), 1.35f, 9.f, EBossKind::Spitter, 7, { DesertSpitter, RedSpitter } },
		{ TEXT("deep"),     TEXT("Deep Matriarch"),  TEXT("Глубинный матриарх"), false, TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterAquatic/Char_SpitterAquaticAlpha.Char_SpitterAquaticAlpha_C"), 1.35f, 9.f, EBossKind::Spitter, 5, { AquaticSpitter, ForestSpitter } },
		{ TEXT("crabs"),    TEXT("Nest Mother"),     TEXT("Матка гнёзд"),        true,  TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/Char_Stinger.Char_Stinger_C"), 1.3f, 10.f, EBossKind::Hatcher, 7, { CrabHatcher, SmallStinger } },
		{ TEXT("giraffe"),  TEXT("Sky Giraffe"),     TEXT("Небесный жираф"),     false, TEXT("/Game/FactoryGame/Character/Creature/Wildlife/SpaceGiraffe/Char_SpaceGiraffe.Char_SpaceGiraffe_C"), 2.0f, 6.f, EBossKind::Peaceful, 4, { nullptr, nullptr } },
		{ TEXT("beetle"),   TEXT("Colossal Beetle"), TEXT("Жук-колосс"),         false, TEXT("/Game/FactoryGame/Character/Creature/Wildlife/Beetle/Char_Beetle.Char_Beetle_C"), 2.5f, 15.f, EBossKind::Peaceful, 3, { nullptr, nullptr } },
		{ TEXT("strider"),  TEXT("Giant Strider"),   TEXT("Гигантский страус"),  false, TEXT("/Game/FactoryGame/Character/Creature/Wildlife/NonFlyingBird/Char_NonFlyingBird.Char_NonFlyingBird_C"), 2.0f, 8.f, EBossKind::Peaceful, 3, { nullptr, nullptr } },
	};

	enum EBossTrait : uint32
	{
		Armored = 1 << 0, Swift = 1 << 1, Colossal = 1 << 2, Undying = 1 << 3, Summoning = 1 << 4,
		Nocturnal = 1 << 5, Misty = 1 << 6, Sparking = 1 << 7, Bloodthirsty = 1 << 8, Rabid = 1 << 9,
		Radioactive = 1 << 10, Jetpack = 1 << 11,
	};

	struct FBossTraitInfo
	{
		EBossTrait Trait;
		const TCHAR* English;
		const TCHAR* RussianMasculine;
		const TCHAR* RussianFeminine;
		/** Makes sense for peaceful wildlife too */
		bool bPeaceful;
		/** Goes after the boss name: "Hog King with a Jetpack" */
		bool bSuffix;
	};

	const FBossTraitInfo BossTraits[] = {
		{ Armored,      TEXT("Armored"),      TEXT("Бронированный"), TEXT("Бронированная"), true,  false },
		{ Swift,        TEXT("Swift"),        TEXT("Стремительный"), TEXT("Стремительная"), false, false },
		{ Colossal,     TEXT("Colossal"),     TEXT("Исполинский"),   TEXT("Исполинская"),   true,  false },
		{ Undying,      TEXT("Undying"),      TEXT("Живучий"),       TEXT("Живучая"),       true,  false },
		{ Summoning,    TEXT("Summoning"),    TEXT("Призывающий"),   TEXT("Призывающая"),   false, false },
		{ Nocturnal,    TEXT("Nocturnal"),    TEXT("Ночной"),        TEXT("Ночная"),        true,  false },
		{ Misty,        TEXT("Misty"),        TEXT("Туманный"),      TEXT("Туманная"),      true,  false },
		{ Sparking,     TEXT("Sparking"),     TEXT("Искрящий"),      TEXT("Искрящая"),      false, false },
		{ Bloodthirsty, TEXT("Bloodthirsty"), TEXT("Кровожадный"),   TEXT("Кровожадная"),   false, false },
		{ Rabid,        TEXT("Rabid"),        TEXT("Бешеный"),       TEXT("Бешеная"),       false, false },
		{ Radioactive,  TEXT("Radioactive"),  TEXT("Радиоактивный"), TEXT("Радиоактивная"), true,  false },
		{ Jetpack,      TEXT("with a Jetpack"), TEXT("с реактивным ранцем"), TEXT("с реактивным ранцем"), true, true },
	};

	const TCHAR* const TitanKey = TEXT("system:titan");

	/** The jetpack's fuel is protected; never instantiated */
	struct FJetPackAccess : AFGJetPack
	{
		static float GetFuel(const AFGJetPack* Pack) { return Pack->*(&FJetPackAccess::mCurrentFuel); }
		static void SetFuel(AFGJetPack* Pack, float Fuel) { (Pack->*(&FJetPackAccess::SetCurrentFuel))(Fuel); }
	};

	/** A game mesh strapped on a bone of the boss, Size centimeters along its longest side; no collision */
	UStaticMeshComponent* AddJetpackPart(USkeletalMeshComponent* Body, FName Bone, const TCHAR* MeshPath, const FVector& Location, const FQuat& Rotation, float Size)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
		if (!Mesh)
		{
			UE_LOG(LogChatControl, Warning, TEXT("Titan jetpack: mesh not found: %s"), MeshPath);
			return nullptr;
		}
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Body->GetOwner());
		Part->SetStaticMesh(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->SetupAttachment(Body, Bone);
		Part->RegisterComponent();
		Part->SetWorldLocationAndRotation(Location, Rotation);
		Part->SetWorldScale3D(FVector(Size / FMath::Max(1.0, 2.0 * Mesh->GetBounds().BoxExtent.GetMax())));
		return Part;
	}

	/** Ground on a ring around the player, traced from above; no fit test, for small static things like nests */
	bool FindGroundNear(UWorld* World, const AActor* Player, float RadiusMin, float RadiusMax, FVector& OutLocation)
	{
		FCollisionQueryParams Params(TEXT("ChatControlNest"), false, Player);
		for (int32 Attempt = 0; Attempt < 12; ++Attempt)
		{
			const float Angle = FMath::FRandRange(0.f, 2.f * PI);
			const FVector Probe = Player->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * FMath::FRandRange(RadiusMin, RadiusMax);
			FHitResult Ground;
			if (World->LineTraceSingleByChannel(Ground, Probe + FVector(0, 0, 400.f), Probe - FVector(0, 0, 1500.f), ECC_Visibility, Params))
			{
				OutLocation = Ground.ImpactPoint;
				return true;
			}
		}
		return false;
	}

	/** On foot or falling: not in a vehicle, on a zipline or in a hypertube, where throwing would break things */
	bool CanThrowPlayer(const AFGCharacterPlayer* Player)
	{
		const UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
		return !Player->IsDrivingVehicle() && Movement && (Movement->IsMovingOnGround() || Movement->IsFalling());
	}

	/** The first solid thing under Location; Location itself over a void */
	FVector GroundBelow(UWorld* World, const FVector& Location, const AActor* IgnoreBoss, const AActor* IgnorePlayer)
	{
		FHitResult Ground;
		FCollisionQueryParams Query(NAME_None, false, IgnoreBoss);
		Query.AddIgnoredActor(IgnorePlayer);
		return World && World->LineTraceSingleByChannel(Ground, Location + FVector(0.f, 0.f, 300.f), Location - FVector(0.f, 0.f, 100000.f), ECC_WorldStatic, Query)
			? Ground.ImpactPoint : Location;
	}

	/** Damage that fades with distance from Center and throws the player away from it */
	void Shockwave(AFGCharacterPlayer* Player, const FVector& Center, float Radius, float Damage, float Push, AActor* Causer)
	{
		const float Distance = FVector::Dist2D(Player->GetActorLocation(), Center);
		if (Distance > Radius || FMath::Abs(Player->GetActorLocation().Z - Center.Z) > Radius || !Player->GetHealthComponent())
		{
			return;
		}
		const float Falloff = 1.f - 0.6f * Distance / Radius;
		Player->GetHealthComponent()->TakeDamage(Player, Damage * Falloff, GetDefault<UDamageType>(), nullptr, Causer);
		if (CanThrowPlayer(Player))
		{
			const FVector Away = (Player->GetActorLocation() - Center).GetSafeNormal2D();
			Player->LaunchCharacter(Away * Push * Falloff + FVector(0, 0, Push * 0.5f), true, true);
		}
	}

	/** Full aggro on the player, so a freshly spawned creature attacks instead of standing around */
	void ProvokeCreature(APawn* Pawn, AActor* Player)
	{
		const AFGCreature* Creature = Cast<AFGCreature>(Pawn);
		AFGCreatureController* Controller = Pawn ? Cast<AFGCreatureController>(Pawn->GetController()) : nullptr;
		if (Creature && Controller && Player)
		{
			Controller->SetAggroForTarget(Player, FMath::Max(1.f, Creature->GetMaximumAggroLevel()), true);
		}
	}
}

bool UChatControlSubsystem::StartTitan(const FString& ViewerKey, const FString& ViewerName, const FChatEffectSettings& Effect, const FString& TestPicks)
{
	if (BossFight.bActive || IsInSafeZone() || !FindPlayer())
	{
		return false;
	}

	// Pick the boss: by id for "!test titan crabs", otherwise weighted random
	TArray<FString> Picks;
	TestPicks.ParseIntoArrayWS(Picks);
	int32 Type = INDEX_NONE;
	for (int32 i = 0; i < UE_ARRAY_COUNT(BossTypes); ++i)
	{
		if (Picks.ContainsByPredicate([&](const FString& Pick) { return Pick.Equals(BossTypes[i].Id, ESearchCase::IgnoreCase); }))
		{
			Type = i;
		}
	}
	if (Type == INDEX_NONE)
	{
		int32 Total = 0;
		for (const FBossType& Boss : BossTypes)
		{
			Total += Boss.Kind == EBossKind::Peaceful && !Settings.bPeacefulTitans ? 0 : Boss.Weight;
		}
		int32 Roll = FMath::RandRange(1, FMath::Max(1, Total));
		for (int32 i = 0; i < UE_ARRAY_COUNT(BossTypes) && Type == INDEX_NONE; ++i)
		{
			Roll -= BossTypes[i].Kind == EBossKind::Peaceful && !Settings.bPeacefulTitans ? 0 : BossTypes[i].Weight;
			Type = Roll <= 0 ? i : INDEX_NONE;
		}
		Type = FMath::Max(0, Type);
	}
	const FBossType& Boss = BossTypes[Type];

	// 0..Amount random traits, at least one; "!test titan queen jetpack" picks them instead
	uint32 Traits = 0;
	TArray<const FBossTraitInfo*> Pool;
	TArray<const FBossTraitInfo*> Picked;
	for (const FBossTraitInfo& Info : BossTraits)
	{
		if (Picks.ContainsByPredicate([&Info](const FString& Pick) { return FString(Info.English).Contains(Pick); }))
		{
			Picked.Add(&Info);
			Traits |= Info.Trait;
		}
		if (Boss.Kind != EBossKind::Peaceful || Info.bPeaceful)
		{
			Pool.Add(&Info);
		}
	}
	const int32 TraitCount = Picked.Num() > 0 ? 0 : FMath::RandRange(1, FMath::Clamp(Effect.Amount, 1, 3));
	for (int32 i = 0; i < TraitCount && Pool.Num() > 0; ++i)
	{
		const FBossTraitInfo* Info = Pool[FMath::RandRange(0, Pool.Num() - 1)];
		Pool.Remove(Info);
		Picked.Add(Info);
		Traits |= Info->Trait;
	}

	// "Bloodthirsty Armored Hog King" / "Кровожадный Бронированный Король кабанов"
	TArray<FString> Words;
	TArray<FString> Suffixes;
	for (const FBossTraitInfo* Info : Picked)
	{
		(Info->bSuffix ? Suffixes : Words).Add(ChatText::IsRussian() ? (Boss.bFeminine ? Info->RussianFeminine : Info->RussianMasculine) : Info->English);
	}
	Words.Add(ChatText::IsRussian() ? Boss.Russian : Boss.English);
	Words.Append(Suffixes);

	BossFight = FBossFight();
	BossFight.bActive = true;
	BossFight.bArriving = true;
	BossFight.Type = Type;
	BossFight.Traits = Traits;
	BossFight.Title = FString::Join(Words, TEXT(" "));
	BossFight.ViewerKey = ViewerKey;
	BossFight.ViewerName = ViewerName;
	BossFight.EndTime = FPlatformTime::Seconds() + FMath::Max(60.f, Effect.Duration);

	const float Delay = FMath::FRandRange(Effect.DelayMin, FMath::Max(Effect.DelayMin, Effect.DelayMax));
	Announce(FText::Format(Tr(TEXT("TITAN! {0} summons {1}. It arrives in {2} s - get ready!"), TEXT("ТИТАН! {0} призывает: {1}. Появится через {2} с - готовься!")),
		FText::FromString(ViewerName), FText::FromString(BossFight.Title), FText::AsNumber(FMath::RoundToInt(Delay))));
	TWeakObjectPtr<UChatControlSubsystem> WeakSelf = this;
	RunDelayed(Delay, [WeakSelf] { if (WeakSelf.IsValid()) { WeakSelf->SpawnTitan(); } });
	return true;
}

void UChatControlSubsystem::SpawnTitan()
{
	if (!BossFight.bActive || !BossFight.bArriving)
	{
		return;
	}
	UWorld* World = GetGameWorld();
	AFGCharacterPlayer* Player = FindPlayer();
	const FBossType& Boss = BossTypes[BossFight.Type];
	UClass* BossClass = LoadClass<APawn>(nullptr, Boss.Class);
	if (!World || !Player || !BossClass)
	{
		UE_LOG(LogChatControl, Warning, TEXT("Titan: cannot spawn %s"), Boss.Class);
		EndBossFight(false);
		return;
	}

	// Above ~1.6x creatures get stuck: the AI navigation and attack areas are made for their normal size
	const float MaxScale = Boss.Kind == EBossKind::Peaceful ? 2.6f : 1.6f;
	const float Scale = FMath::Min(MaxScale, Boss.Scale * ((BossFight.Traits & Colossal) ? 1.15f : 1.f));
	float Radius = 60.f, HalfHeight = 90.f;
	if (const ACharacter* Default = Cast<ACharacter>(BossClass->GetDefaultObject()); Default && Default->GetCapsuleComponent())
	{
		Default->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);
	}
	// The spot has to fit the scaled-up boss, not the normal creature
	FVector Location;
	if (!ChatEffects::FindCreatureSpot(World, Player, Settings, Radius * Scale, HalfHeight * Scale, Location))
	{
		if (++BossFight.SpawnTries < 4)
		{
			UE_LOG(LogChatControl, Log, TEXT("Titan: no room yet, retrying in 3 s"));
			TWeakObjectPtr<UChatControlSubsystem> WeakSelf = this;
			RunDelayed(3.f, [WeakSelf] { if (WeakSelf.IsValid()) { WeakSelf->SpawnTitan(); } });
			return;
		}
		Announce(FText::Format(Tr(TEXT("{0} found no room to land and left."), TEXT("{0}: титану негде приземлиться, он уходит.")), FText::FromString(BossFight.Title)));
		EndBossFight(false);
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FRotator Facing((Player->GetActorLocation() - Location).GetSafeNormal2D().Rotation());
	APawn* Pawn = World->SpawnActor<APawn>(BossClass, Location, Facing, Params);
	if (!Pawn)
	{
		EndBossFight(false);
		return;
	}
	if (!Pawn->GetController())
	{
		Pawn->SpawnDefaultController();
	}
	Pawn->SetActorScale3D(FVector(Scale));
	AFGCreature* Creature = Cast<AFGCreature>(Pawn);
	if (Creature)
	{
		Creature->SetIsPersistent(true);
	}
	Pawn->SetLifeSpan(BossFight.EndTime - FPlatformTime::Seconds() + 60.0);
	TrackSpawned(Pawn);

	UFGHealthComponent* Health = Creature ? Creature->GetHealthComponent() : nullptr;
	if (Health)
	{
		const float Multiplier = Boss.Health * FMath::Max(0.1f, Settings.Effects.FindRef(TEXT("titan")).Strength)
			* ((BossFight.Traits & Armored) ? 1.6f : 1.f) * (Settings.Difficulty <= 1 ? 0.75f : Settings.Difficulty >= 3 ? 1.5f : 1.f);
		const float Before = Health->GetMaxHealth();
		Health->SetMaxHealth(Before * Multiplier);
		Health->ResetHealth();
		UE_LOG(LogChatControl, Log, TEXT("Titan %s: scale %.1f, health %.0f -> max %.0f, current %.0f"),
			Boss.Id, Scale, Before, Health->GetMaxHealth(), Health->GetCurrentHealth());
	}

	const double Now = FPlatformTime::Seconds();
	BossFight.bArriving = false;
	BossFight.Boss = Pawn;
	BossFight.Health = Health;
	BossFight.MaxHealth = Health ? FMath::Max(1.f, Health->GetMaxHealth()) : 1.f;
	BossFight.LastHealth = Health ? Health->GetCurrentHealth() : 0.f;
	BossFight.PlayerLastHealth = Player->GetHealthComponent() ? Player->GetHealthComponent()->GetCurrentHealth() : 0.f;
	BossFight.BaseDilation = (BossFight.Traits & Swift) ? 1.15f : 1.f;
	BossFight.LastDamageTime = Now;
	BossFight.LastPlayerHitTime = Now;
	BossFight.NextSummonTime = Now + (Boss.Kind == EBossKind::Hatcher ? 20.0 : 30.0);
	BossFight.NextRegenTime = Now + 1.0;
	Pawn->CustomTimeDilation = BossFight.BaseDilation;
	if (const ACharacter* BodyOwner = Cast<ACharacter>(Pawn); BodyOwner && BodyOwner->GetMesh())
	{
		const FVector BodyExtent = BodyOwner->GetMesh()->CalcBounds(FTransform::Identity).BoxExtent;
		BossFight.BodyHalfLength = FMath::Max(BodyExtent.X, BodyExtent.Y) * Scale;
		UE_LOG(LogChatControl, Log, TEXT("Titan: body half length %.0f cm"), BossFight.BodyHalfLength);
	}
	ProvokeCreature(Pawn, Player);
	AttachBossBar(Pawn);

	Announce(FText::Format(Tr(TEXT("{0} is here! Defeat it before it defeats you."), TEXT("{0} уже здесь! Бей первым!")), FText::FromString(BossFight.Title)));
	if (Boss.Kind == EBossKind::Peaceful)
	{
		Announce(Tr(TEXT("It carries loot - but it stomps anyone who gets too close!"), TEXT("Титан несёт лут, но топчет всех, кто подойдёт близко!")));
	}
	// Traits that change the world for the whole fight
	auto RunWorldEffect = [this](const TCHAR* EffectId)
	{
		if (const FChatEffect* WorldEffect = Effects.Find(EffectId))
		{
			ExecuteEffect(*WorldEffect, TitanKey, BossFight.Title);
		}
	};
	if (BossFight.Traits & Radioactive)
	{
		Announce(Tr(TEXT("It glows with radiation - keep your distance!"), TEXT("От титана фонит радиацией - держи дистанцию!")));
	}
	if (BossFight.Traits & Jetpack)
	{
		Announce(Tr(TEXT("It wears a jetpack - watch the sky!"), TEXT("У титана реактивный ранец - следи за небом!")));
		BossFight.NextJetTime = Now + 5.0;
		ACharacter* Character = Cast<ACharacter>(Pawn);
		USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
		if (Body && Character->GetCapsuleComponent())
		{
			// On the highest bone of the back half: it moves with the body and stays on the corpse
			const float BodyRadius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
			const FTransform ActorTransform = Pawn->GetActorTransform();
			FName BackBone = NAME_None;
			FVector BackLocation = Pawn->GetActorLocation() + FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			double BestHeight = -UE_BIG_NUMBER;
			TArray<FName> Bones;
			Body->GetBoneNames(Bones);
			for (const FName& Bone : Bones)
			{
				// A limb can be the highest bone, and the jetpack would wave around with it
				const FString BoneName = Bone.ToString();
				bool bLimb = false;
				for (const TCHAR* Limb : { TEXT("pincer"), TEXT("claw"), TEXT("leg"), TEXT("arm"), TEXT("foot"), TEXT("hand"), TEXT("toe"), TEXT("finger"), TEXT("tail"), TEXT("jaw"), TEXT("mandible"), TEXT("wing"), TEXT("antenna"), TEXT("horn"), TEXT("tusk"), TEXT("stinger"), TEXT("neck") })
				{
					bLimb |= BoneName.Contains(Limb);
				}
				if (bLimb)
				{
					continue;
				}
				const FVector BoneLocation = Body->GetBoneLocation(Bone);
				const FVector Local = ActorTransform.InverseTransformPositionNoScale(BoneLocation);
				if (Local.X < BodyRadius * 0.2f && Local.X > -BodyRadius * 1.2f && FMath::Abs(Local.Y) < BodyRadius * 0.5f && Local.Z > BestHeight)
				{
					BestHeight = Local.Z;
					BackBone = Bone;
					BackLocation = BoneLocation;
				}
			}
			const FQuat ActorRotation = Pawn->GetActorQuat();
			const FVector PackLocation = BackLocation + FVector(0.f, 0.f, 30.f);
			AddJetpackPart(Body, BackBone, TEXT("/Game/FactoryGame/Equipment/JetPack/Mesh/Jetpack_Groundmesh.Jetpack_Groundmesh"),
				PackLocation, ActorRotation, FMath::Max(130.f, BodyRadius));
			// Blows down and back, past the body instead of through it
			const FVector Exhaust = (Pawn->GetActorForwardVector() * -0.5f - FVector::UpVector * 0.87f).GetSafeNormal();
			FString FlameShape;
			for (const float Side : { -1.f, 1.f })
			{
				// The jetpack's own flame mesh, with its own material
				UStaticMeshComponent* Flame = AddJetpackPart(Body, BackBone, TEXT("/Game/FactoryGame/VFX/Mesh/SM_Cylinder_Taper_01.SM_Cylinder_Taper_01"),
					PackLocation + Pawn->GetActorRightVector() * Side * BodyRadius * 0.3f - Pawn->GetActorForwardVector() * BodyRadius * 0.3f, ActorRotation, FMath::Max(250.f, BodyRadius * 1.6f));
				if (Flame)
				{
					// Aim the mesh's longest side, in the direction it grows from its pivot
					const FBoxSphereBounds MeshBounds = Flame->GetStaticMesh()->GetBounds();
					const int32 Axis = MeshBounds.BoxExtent.X >= MeshBounds.BoxExtent.Y && MeshBounds.BoxExtent.X >= MeshBounds.BoxExtent.Z ? 0 : MeshBounds.BoxExtent.Y >= MeshBounds.BoxExtent.Z ? 1 : 2;
					FVector Along = FVector::ZeroVector;
					Along[Axis] = MeshBounds.Origin[Axis] >= 0.0 ? 1.0 : -1.0;
					Flame->SetWorldRotation(FQuat::FindBetweenNormals(Along, Exhaust));
					// The mesh is about as wide as it is long: a flame is long and narrow
					FVector FlameScale = Flame->GetComponentScale() * 0.3;
					FlameScale[Axis] /= 0.3;
					Flame->SetWorldScale3D(FlameScale);
					Flame->SetVisibility(false);
					BossFight.JetFlameScale = Flame->GetRelativeScale3D();
					BossFight.JetFlames.Add(Flame);
					FlameShape = MeshBounds.ToString();
				}
			}
			UE_LOG(LogChatControl, Log, TEXT("Titan jetpack: on bone %s, %d flames, flame mesh %s"), *BackBone.ToString(), BossFight.JetFlames.Num(), *FlameShape);
		}
	}
	if (BossFight.Traits & Nocturnal)
	{
		RunWorldEffect(TEXT("night"));
	}
	if (BossFight.Traits & Misty)
	{
		RunWorldEffect(TEXT("fog"));
	}
}

void UChatControlSubsystem::AttachBossBar(APawn* Boss)
{
	FVector Origin, Extent;
	Boss->GetActorBounds(true, Origin, Extent);
	const float Scale = FMath::Max(0.1f, Boss->GetActorScale3D().Z);
	const float WorldHeight = Origin.Z + Extent.Z - Boss->GetActorLocation().Z + ((BossFight.Traits & Jetpack) ? 240.f : 140.f);

	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 34);
	TitleFont.OutlineSettings.OutlineSize = 3;
	TitleFont.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
	FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22);
	SmallFont.OutlineSettings.OutlineSize = 2;
	SmallFont.OutlineSettings.OutlineColor = TitleFont.OutlineSettings.OutlineColor;

	TWeakObjectPtr<UChatControlSubsystem> WeakSelf = this;
	TWeakObjectPtr<UFGHealthComponent> WeakHealth = BossFight.Health;
	const bool bPeaceful = BossTypes[BossFight.Type].Kind == EBossKind::Peaceful;
	const FLinearColor BarColor = bPeaceful ? FLinearColor(0.35f, 0.85f, 0.4f) : FLinearColor(0.98f, 0.45f, 0.15f);

	UWidgetComponent* Bar = NewObject<UWidgetComponent>(Boss);
	Bar->SetupAttachment(Boss->GetRootComponent());
	// The boss is scaled up; keep the bar at normal size and place it above its head in world units
	Bar->SetUsingAbsoluteScale(true);
	Bar->SetRelativeLocation(FVector(0, 0, WorldHeight / Scale));
	Bar->SetWidgetSpace(EWidgetSpace::World);
	Bar->SetDrawAtDesiredSize(true);
	Bar->SetPivot(FVector2D(0.5f, 1.f));
	Bar->SetTwoSided(true);
	Bar->SetBlendMode(EWidgetBlendMode::Transparent);
	Bar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bar->SetCastShadow(false);
	Bar->SetSlateWidget(SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(BossFight.Title)).Font(TitleFont).ColorAndOpacity(BarColor).Justification(ETextJustify::Center)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4)
		[
			SNew(STextBlock).Text(FText::Format(Tr(TEXT("from {0}"), TEXT("от {0}")), FText::FromString(BossFight.ViewerName)))
				.Font(SmallFont).ColorAndOpacity(GetNameColor(BossFight.ViewerName, FColor::White))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 6)
		[
			SNew(SBorder).Padding(3).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.75f))
			[
				SNew(SBox).WidthOverride(460.f).HeightOverride(26.f)
				[
					SNew(SProgressBar).FillColorAndOpacity(BarColor)
						.Percent_Lambda([WeakHealth]() -> TOptional<float>
						{
							return WeakHealth.IsValid() && WeakHealth->GetMaxHealth() > 0.f ? WeakHealth->GetCurrentHealth() / WeakHealth->GetMaxHealth() : 0.f;
						})
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock).Font(SmallFont).ColorAndOpacity(FLinearColor::White)
				.Text_Lambda([WeakSelf, WeakHealth]()
				{
					const int32 Stage = WeakSelf.IsValid() ? WeakSelf->BossFight.Stage : 1;
					const int32 Percent = WeakHealth.IsValid() && WeakHealth->GetMaxHealth() > 0.f ? FMath::CeilToInt(WeakHealth->GetCurrentHealth() / WeakHealth->GetMaxHealth() * 100.f) : 0;
					return FText::Format(Tr(TEXT("Stage {0}/3 - {1}%"), TEXT("Стадия {0}/3 - {1}%")), FText::AsNumber(Stage), FText::AsNumber(Percent));
				})
		]);
	Bar->RegisterComponent();
	NameTags.Add(Bar);
	BossFight.Bar = Bar;
}

void UChatControlSubsystem::TickBoss(double Now)
{
	if (!BossFight.bActive || BossFight.bArriving)
	{
		return;
	}
	APawn* Boss = BossFight.Boss.Get();
	UFGHealthComponent* Health = BossFight.Health.Get();
	AFGCharacterPlayer* Player = FindPlayer();
	const FBossType& Type = BossTypes[BossFight.Type];
	// The game may remove the corpse on the tick it dies: almost no health left last tick counts as a kill
	const bool bKilled = Health ? Health->IsDead() : BossFight.LastHealth > 0.f && BossFight.LastHealth <= BossFight.MaxHealth * 0.05f;
	if (!bKilled && (!Boss || !Health))
	{
		// Removed by !panic, unloaded with the world, or its lifespan ran out
		EndBossFight(false);
		return;
	}

	if (bKilled)
	{
		const FVector LootLocation = Boss ? Boss->GetActorLocation() : Player ? Player->GetActorLocation() + Player->GetActorForwardVector() * 300.f : FVector::ZeroVector;
		Announce(FText::Format(Tr(TEXT("VICTORY! {0} is defeated!"), TEXT("ПОБЕДА! Титан повержен: {0}!")), FText::FromString(BossFight.Title)));
		const int32 Loot = Type.Kind == EBossKind::Peaceful ? 10 : 6;
		// Killed in the air, the boss's location is still up there
		if (Player && ChatEffects::DropLootCrate(GetGameWorld(), Player, GroundBelow(GetGameWorld(), LootLocation, Boss, Player) + FVector(0, 0, 100.f), Loot, 0.6f))
		{
			Announce(Tr(TEXT("It dropped a crate full of loot!"), TEXT("Из титана выпал ящик с лутом!")));
		}
		if (Settings.TitanRewardChips > 0)
		{
			const int32 Paid = Economy.PayActiveViewers(Now, Settings.ActiveWindowSeconds, Settings.TitanRewardChips);
			if (Paid > 0)
			{
				Announce(FText::Format(Tr(TEXT("Chat celebrates: every active viewer gets {0} chips!"), TEXT("Чат празднует: каждый активный зритель получает {0} чипов!")),
					FText::AsNumber(Settings.TitanRewardChips)));
			}
		}
		EndBossFight(false);
		return;
	}
	if (Player && !Player->IsAliveAndWell())
	{
		Announce(FText::Format(Tr(TEXT("{0} defeated the streamer! {1} gets {2} chips for the summon."), TEXT("Стример проиграл: {0}! {1} получает {2} чипов за призыв.")),
			FText::FromString(BossFight.Title), FText::FromString(BossFight.ViewerName), FText::AsNumber(Settings.TitanCallerChips)));
		if (Settings.TitanCallerChips > 0 && !BossFight.ViewerKey.StartsWith(TEXT("system:")))
		{
			Economy.AddChips(BossFight.ViewerKey, BossFight.ViewerName, Settings.TitanCallerChips);
		}
		Boss->SetLifeSpan(15.f);
		EndBossFight(false);
		return;
	}
	if (Now >= BossFight.EndTime)
	{
		Announce(FText::Format(Tr(TEXT("{0} got bored and left."), TEXT("{0}: титану стало скучно, он уходит.")), FText::FromString(BossFight.Title)));
		EndBossFight(true);
		return;
	}

	const float Current = Health->GetCurrentHealth();
	if (Current < BossFight.LastHealth - 0.01f)
	{
		BossFight.LastDamageTime = Now;
	}

	// Stages at 66% and 33%, each fires once
	const float Fraction = Current / BossFight.MaxHealth;
	const int32 Stage = Fraction <= 0.33f ? 3 : Fraction <= 0.66f ? 2 : 1;
	if (Stage > BossFight.Stage)
	{
		for (int32 Next = BossFight.Stage + 1; Next <= Stage; ++Next)
		{
			BossFight.Stage = Next;
			EnterBossStage(Next);
		}
	}

	// Traits
	float Dilation = BossFight.BaseDilation * (BossFight.Stage >= 3 && Type.Kind != EBossKind::Peaceful ? 1.25f : BossFight.Stage == 2 && Type.Kind == EBossKind::Melee ? 1.1f : 1.f);
	if (BossFight.Traits & Rabid)
	{
		Dilation *= 1.f + (1.f - Fraction) * 0.3f;
	}
	// Faster animations than this make attacks miss
	Dilation = FMath::Min(Dilation, 1.45f);
	// !dance freezes creatures with a tiny time dilation; don't undo it
	if (Boss->CustomTimeDilation >= 0.01f)
	{
		Boss->CustomTimeDilation = Dilation;
	}

	if ((BossFight.Traits & Undying) && Now >= BossFight.NextRegenTime)
	{
		BossFight.NextRegenTime = Now + 1.0;
		if (Now - BossFight.LastDamageTime > 6.0 && Current < BossFight.MaxHealth)
		{
			Health->Heal(BossFight.MaxHealth * 0.015f);
		}
	}
	if (UFGHealthComponent* PlayerHealth = Player ? Player->GetHealthComponent() : nullptr)
	{
		const float PlayerHealthNow = PlayerHealth->GetCurrentHealth();
		if (PlayerHealthNow < BossFight.PlayerLastHealth)
		{
			BossFight.LastPlayerHitTime = Now;
		}
		if ((BossFight.Traits & Bloodthirsty) && PlayerHealthNow < BossFight.PlayerLastHealth)
		{
			// Every hit on the streamer heals the boss
			Health->Heal((BossFight.PlayerLastHealth - PlayerHealthNow) * 0.05f * BossFight.MaxHealth / FMath::Max(1.f, PlayerHealth->GetMaxHealth()));
		}
		BossFight.PlayerLastHealth = PlayerHealthNow;
	}
	if (Player && Now >= BossFight.NextAggroTime)
	{
		BossFight.NextAggroTime = Now + (Type.Kind == EBossKind::Peaceful ? 1.5 : 3.0);
		ProvokeCreature(Boss, Player);
		BossFight.Minions.RemoveAll([](const TWeakObjectPtr<APawn>& Minion) { return !Minion.IsValid(); });
		for (const TWeakObjectPtr<APawn>& Minion : BossFight.Minions)
		{
			ProvokeCreature(Minion.Get(), Player);
		}
		if (Type.Kind == EBossKind::Peaceful)
		{
			// Wildlife has no attack: walk it to the player instead
			if (AAIController* Controller = Cast<AAIController>(Boss->GetController()))
			{
				Controller->MoveToActor(Player, 150.f);
			}
		}
	}
	if (Type.Kind == EBossKind::Peaceful && Player)
	{
		ACharacter* Beast = Cast<ACharacter>(Boss);
		float Radius = 150.f, HalfHeight = 100.f;
		if (Beast && Beast->GetCapsuleComponent())
		{
			Beast->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);
		}
		const float DamageScale = Settings.Difficulty >= 3 ? 1.5f : Settings.Difficulty <= 1 ? 0.6f : 1.f;
		const float Distance = FVector::Dist2D(Player->GetActorLocation(), Boss->GetActorLocation());

		// Stomp: hurts and throws the player back when they stand too close
		if (!BossFight.bJetFlying && Now >= BossFight.NextStompTime)
		{
			BossFight.NextStompTime = Now + (BossFight.Stage >= 3 ? 1.2 : 2.0);
			Shockwave(Player, Boss->GetActorLocation(), Radius + 550.f, 15.f * DamageScale, 900.f, Boss);
		}

		// Charge: a running jump straight at the player
		if (Beast && !BossFight.bJetFlying && Now >= BossFight.NextChargeTime && Distance > 800.f && Distance < 3500.f && Beast->GetCharacterMovement() && Beast->GetCharacterMovement()->IsMovingOnGround())
		{
			BossFight.NextChargeTime = Now + (BossFight.Stage >= 3 ? 6.0 : 9.0);
			const FVector Toward = (Player->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
			Beast->LaunchCharacter(Toward * FMath::Min(2200.f, Distance * 1.6f) + FVector(0, 0, 350.f), true, true);
		}

		// Leap slam from stage 2: up into the air, lands next to the player with a shockwave
		UCharacterMovementComponent* Movement = Beast ? Beast->GetCharacterMovement() : nullptr;
		if (Movement && BossFight.bLeaping && Now - BossFight.LeapStartTime > 0.4 && Movement->IsMovingOnGround())
		{
			BossFight.bLeaping = false;
			Shockwave(Player, Boss->GetActorLocation(), Radius + 900.f, 30.f * DamageScale, 1300.f, Boss);
		}
		else if (Movement && !BossFight.bLeaping && !BossFight.bJetFlying && BossFight.Stage >= 2 && Now >= BossFight.NextLeapTime && Distance < 4000.f && Movement->IsMovingOnGround())
		{
			BossFight.NextLeapTime = Now + (BossFight.Stage >= 3 ? 10.0 : 14.0);
			BossFight.bLeaping = true;
			BossFight.LeapStartTime = Now;
			// About 1.3 s in the air: horizontal speed covers the distance, vertical speed brings it back down
			const float Gravity = FMath::Abs(Movement->GetGravityZ());
			const float AirTime = 1.3f;
			const FVector Toward = Player->GetActorLocation() - Boss->GetActorLocation();
			Beast->LaunchCharacter(FVector(Toward.X, Toward.Y, 0.f) / AirTime + FVector(0, 0, 0.5f * Gravity * AirTime), true, true);
		}
	}
	const ACharacter* BossBody = Cast<ACharacter>(Boss);
	if (Player && Settings.bTitanPushPlayer && !BossFight.bJetFlying && Now >= BossFight.NextShoveTime && BossBody && BossBody->GetCapsuleComponent() && CanThrowPlayer(Player))
	{
		// The scaled body is bigger than its collision: keep the player from ending up inside it, with the camera in the model.
		// The body is long, so the zone follows it, but stays inside the reach of its bites: mesh bounds are looser than the body
		float Radius = 100.f, HalfHeight = 100.f;
		BossBody->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);
		const FVector Offset = Player->GetActorLocation() - Boss->GetActorLocation();
		if (FMath::Abs(FVector::DotProduct(Offset, Boss->GetActorForwardVector())) < FMath::Max(BossFight.BodyHalfLength * 0.7f, Radius) + 60.f
			&& FMath::Abs(FVector::DotProduct(Offset, Boss->GetActorRightVector())) < Radius + 60.f && FMath::Abs(Offset.Z) < HalfHeight + 200.f)
		{
			BossFight.NextShoveTime = Now + 0.3;
			// Being shoved by an angry titan hurts: otherwise it could never land a bite on someone it keeps pushing away
			const AFGCreature* BossCreature = Cast<AFGCreature>(Boss);
			if (Type.Kind != EBossKind::Peaceful && !(BossCreature && BossCreature->IsStunned()) && Now >= BossFight.NextBodyHitTime && Player->GetHealthComponent())
			{
				BossFight.NextBodyHitTime = Now + 1.5;
				Player->GetHealthComponent()->TakeDamage(Player, 10.f * (Settings.Difficulty >= 3 ? 1.5f : Settings.Difficulty <= 1 ? 0.6f : 1.f), GetDefault<UDamageType>(), nullptr, Boss);
			}
			const FVector Away = Offset.IsNearlyZero() ? Boss->GetActorForwardVector() : Offset.GetSafeNormal2D();
			Player->LaunchCharacter(Away * 700.f + FVector(0.f, 0.f, 150.f), true, false);
		}
	}
	if (Player)
	{
		// Most titans only hit on the ground: staying in the air burns the jetpack's fuel, shuts down the hover pack and hurts
		AFGEquipment* BackItem = Player->GetEquipmentInSlot(EEquipmentSlot::ES_BACK);
		AFGHoverPack* HoverPack = Cast<AFGHoverPack>(BackItem);
		AFGJetPack* JetPack = Cast<AFGJetPack>(BackItem);
		const bool bHovering = HoverPack && HoverPack->IsHoverActive();
		BossFight.bPlayerInAir = !Player->IsDrivingVehicle() && (bHovering || (Player->GetCharacterMovement() && Player->GetCharacterMovement()->IsFalling()));
		if (JetPack && JetPack->IsThrusting())
		{
			BossFight.LastThrustTime = Now;
		}
		// Only powered flight counts: not a fall, a parachute, a jump pad or being thrown by a shockwave
		const bool bFlying = BossFight.bPlayerInAir && (bHovering || Now - BossFight.LastThrustTime < 2.0);
		// A quick touch of the ground doesn't start the count over
		if (BossFight.bPlayerInAir)
		{
			BossFight.PlayerGroundedSince = 0.0;
		}
		else if (BossFight.PlayerGroundedSince <= 0.0)
		{
			BossFight.PlayerGroundedSince = Now;
		}
		else if (Now - BossFight.PlayerGroundedSince >= 1.5)
		{
			BossFight.PlayerFlyingSince = 0.0;
		}
		if (bFlying && BossFight.PlayerFlyingSince <= 0.0)
		{
			BossFight.PlayerFlyingSince = Now;
		}
		if (Settings.bTitanJamFlight && bFlying && Now - BossFight.PlayerFlyingSince >= Settings.TitanFlightGraceSeconds && Now >= BossFight.NextJamTime
			&& FVector::Dist(Player->GetActorLocation(), Boss->GetActorLocation()) < 15000.f)
		{
			BossFight.NextJamTime = Now + 2.0;
			if (JetPack)
			{
				FJetPackAccess::SetFuel(JetPack, FJetPackAccess::GetFuel(JetPack) * 0.5f);
			}
			if (HoverPack)
			{
				HoverPack->SetHoverMode(EHoverPackMode::HPM_Inactive);
			}
			if (Player->GetHealthComponent())
			{
				Player->GetHealthComponent()->TakeDamage(Player, 5.f * (Settings.Difficulty >= 3 ? 1.5f : Settings.Difficulty <= 1 ? 0.6f : 1.f), GetDefault<UDamageType>(), nullptr, Boss);
			}
			if (!BossFight.bJamAnnounced)
			{
				BossFight.bJamAnnounced = true;
				Announce(Tr(TEXT("The titan jams your flight: the longer you stay in the air, the worse it gets!"), TEXT("Титан глушит полёт: чем дольше ты в воздухе, тем хуже!")));
			}
		}
	}
	if ((BossFight.Traits & Radioactive) && Player && Now >= BossFight.NextRadiationTime)
	{
		// Radiation: the closer, the more it hurts; no push
		BossFight.NextRadiationTime = Now + 1.0;
		const float Distance = FVector::Dist(Player->GetActorLocation(), Boss->GetActorLocation());
		if (Distance < 1000.f && Player->GetHealthComponent())
		{
			const float Damage = FMath::Lerp(6.f, 1.5f, Distance / 1000.f) * (Settings.Difficulty >= 3 ? 1.5f : Settings.Difficulty <= 1 ? 0.6f : 1.f);
			Player->GetHealthComponent()->TakeDamage(Player, Damage, GetDefault<UDamageType>(), nullptr, Boss);
		}
	}
	const bool bHasJetpack = (BossFight.Traits & Jetpack) != 0;
	if ((bHasJetpack || Settings.bTitanLeapToPlayer) && Player)
	{
		// A player on a tower above the boss, or the boss left on top of one, can't reach each other on foot:
		// after a while any titan makes the same flight, just without the jetpack
		const FVector Gap = Player->GetActorLocation() - Boss->GetActorLocation();
		if (Player->IsDrivingVehicle() || FMath::Abs(Gap.Z) < 500.f || Gap.Size2D() > 6000.f)
		{
			BossFight.PlayerOutOfReachSince = 0.0;
		}
		else if (BossFight.PlayerOutOfReachSince <= 0.0)
		{
			BossFight.PlayerOutOfReachSince = Now;
		}
		// So can a boss with no way through a wall or a factory: it may keep walking around, what counts is that it gets no closer.
		// Spitters keep their distance on purpose, to shoot
		if (Now >= BossFight.NextStuckCheckTime)
		{
			BossFight.NextStuckCheckTime = Now + 8.0;
			const float GapNow = Gap.Size();
			BossFight.bBossStuck = Type.Kind != EBossKind::Spitter && !Player->IsDrivingVehicle() && !BossFight.bJetFlying && GapNow > 1200.f
				&& BossFight.LastStuckGap > 0.f && GapNow > BossFight.LastStuckGap - 300.f;
			BossFight.LastStuckGap = GapNow;
		}
		// Jetpack: the engines fire up, it climbs, hangs over the player, dives at where they stood and needs a moment to recover
		ACharacter* Flyer = Cast<ACharacter>(Boss);
		UCharacterMovementComponent* Movement = Flyer ? Flyer->GetCharacterMovement() : nullptr;
		auto ShowFlames = [this](bool bShow)
		{
			for (const TWeakObjectPtr<UStaticMeshComponent>& Flame : BossFight.JetFlames)
			{
				if (Flame.IsValid())
				{
					Flame->SetVisibility(bShow);
				}
			}
		};
		const float Distance = FVector::Dist2D(Player->GetActorLocation(), Boss->GetActorLocation());
		// Whatever keeps the player safe, a roof of a machine, a wall or good footwork: a titan that hasn't hurt them for a while comes from above
		const bool bHarmless = Now - BossFight.LastPlayerHitTime >= Settings.TitanLeapSeconds * 2.f;
		bool bTimeToFly = bHasJetpack
			? Now >= BossFight.NextJetTime || (Distance > 2000.f && Now >= BossFight.NextJetChaseTime) || (BossFight.bBossStuck && Now >= BossFight.NextJetChaseTime)
			: Now >= BossFight.NextJetTime && (bHarmless || BossFight.bBossStuck || (BossFight.PlayerOutOfReachSince > 0.0 && Now - BossFight.PlayerOutOfReachSince >= Settings.TitanLeapSeconds));
		if (bTimeToFly && !BossFight.bJetFlying)
		{
			// No room to hang over a player under a roof or in a cave, and the safe zone stays safe from the sky: it comes on foot
			FHitResult Cover;
			FCollisionQueryParams CoverQuery(NAME_None, false, Boss);
			CoverQuery.AddIgnoredActor(Player);
			const FVector Head = Player->GetActorLocation() + FVector(0.f, 0.f, 100.f);
			const bool bSafeZone = IsInSafeZone();
			if (bSafeZone || GetGameWorld()->LineTraceSingleByChannel(Cover, Head, Head + FVector(0.f, 0.f, 1800.f), ECC_WorldStatic, CoverQuery))
			{
				bTimeToFly = false;
				BossFight.NextJetTime = BossFight.NextJetChaseTime = Now + 2.0;
				if (!BossFight.bJetGrounded)
				{
					BossFight.bJetGrounded = true;
					UE_LOG(LogChatControl, Log, TEXT("Titan jetpack: stays on the ground, player is %s"), bSafeZone ? TEXT("in the safe zone") : *FString::Printf(TEXT("under %s"), Cover.GetActor() ? *Cover.GetActor()->GetName() : TEXT("something")));
				}
			}
			else
			{
				BossFight.bJetGrounded = false;
			}
		}
		// !dance freezes creatures with a tiny time dilation; a frozen boss stays on the ground
		if (Movement && !BossFight.bJetFlying && bTimeToFly && Boss->CustomTimeDilation >= 0.01f)
		{
			if (!bHasJetpack && !BossFight.bLeapAnnounced)
			{
				BossFight.bLeapAnnounced = true;
				Announce(Tr(TEXT("You can't hide from the titan: it is coming for you!"), TEXT("От титана не спрячешься: он идёт за тобой!")));
			}
			BossFight.PlayerOutOfReachSince = 0.0;
			BossFight.bBossStuck = false;
			// A dive that misses gives the player a new count, not a dive every few seconds
			BossFight.LastPlayerHitTime = Now;
			BossFight.bJetFlying = true;
			BossFight.bJetDiving = false;
			BossFight.bJetRammed = false;
			BossFight.JetStartTime = Now;
			BossFight.JetFrom = Boss->GetActorLocation();
			BossFight.JetClimbTime = FMath::Clamp(Distance / 2500.f, 1.2f, 2.5f);
			if (AAIController* Controller = Cast<AAIController>(Boss->GetController()))
			{
				Controller->StopMovement();
			}
			ShowFlames(true);
			UE_LOG(LogChatControl, Log, TEXT("Titan jetpack: takes off, %.0f m to the player, movement mode was %d"), Distance / 100.f, static_cast<int32>(Movement->MovementMode));
		}
		if (Movement && BossFight.bJetFlying)
		{
			float Radius = 100.f, HalfHeight = 100.f;
			Flyer->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);
			const double IgniteTime = 0.8, DiveTime = 0.55;
			const double HoverTime = BossFight.Stage >= 3 ? 0.8 : 1.2;
			const double Elapsed = Now - BossFight.JetStartTime;
			const FVector PlayerLocation = Player->GetActorLocation();
			const FVector Hover = PlayerLocation + (BossFight.JetFrom - PlayerLocation).GetSafeNormal2D() * 500.f + FVector(0.f, 0.f, 1800.f);
			FVector Position = Boss->GetActorLocation();
			float Pitch = 0.f;
			float DiveAlpha = 0.f;
			if (Elapsed < IgniteTime)
			{
				// Shakes on the ground with the engines on: the warning
				Position = BossFight.JetFrom + FMath::VRand() * 6.f;
			}
			else if (Elapsed < IgniteTime + BossFight.JetClimbTime)
			{
				const float Climb = static_cast<float>((Elapsed - IgniteTime) / BossFight.JetClimbTime);
				Position = FMath::Lerp(BossFight.JetFrom, Hover, 1.f - FMath::Square(1.f - Climb));
				Pitch = 20.f;
			}
			else if (Elapsed < IgniteTime + BossFight.JetClimbTime + HoverTime)
			{
				Position = FMath::VInterpTo(Position, Hover, GetGameWorld()->GetDeltaSeconds(), 3.f);
			}
			else
			{
				if (!BossFight.bJetDiving)
				{
					// The target is locked here: moving away now dodges the hit
					BossFight.bJetDiving = true;
					BossFight.JetDiveStartTime = Now;
					BossFight.JetFrom = Position;
					const FVector Approach = (Position - PlayerLocation).GetSafeNormal2D();
					// A player high in the air gets rammed up there, where they will be by then; flying straight doesn't dodge it
					BossFight.bJetIntercept = Settings.bTitanJamFlight && BossFight.bPlayerInAir
						&& PlayerLocation.Z - GroundBelow(GetGameWorld(), PlayerLocation, Boss, Player).Z > 400.f;
					if (BossFight.bJetIntercept)
					{
						// Never below the player: a falling one would lead it into the ground
						const FVector Velocity = Player->GetVelocity();
						BossFight.JetTo = PlayerLocation + FVector(Velocity.X, Velocity.Y, FMath::Max(Velocity.Z, 0.0)) * DiveTime + Approach * (Radius + 100.f);
					}
					else
					{
						// Next to the player, unless that is off the edge of the pillar or ledge they stand on: then right onto it
						const FVector PlayerGround = GroundBelow(GetGameWorld(), PlayerLocation, Boss, Player);
						const FVector BesideGround = GroundBelow(GetGameWorld(), PlayerLocation + Approach * (Radius + 300.f), Boss, Player);
						BossFight.JetTo = (PlayerGround.Z - BesideGround.Z > 300.f ? PlayerGround : BesideGround) + FVector(0.f, 0.f, HalfHeight + 20.f);
						GetGameWorld()->FindTeleportSpot(Boss, BossFight.JetTo, Boss->GetActorRotation());
					}
				}
				DiveAlpha = FMath::Clamp(static_cast<float>((Now - BossFight.JetDiveStartTime) / DiveTime), 0.f, 1.f);
				Position = FMath::Lerp(BossFight.JetFrom, BossFight.JetTo, DiveAlpha * DiveAlpha);
				Pitch = -45.f;
			}
			if (Movement->MovementMode != MOVE_Flying)
			{
				Movement->SetMovementMode(MOVE_Flying);
			}
			Movement->StopMovementImmediately();
			Boss->SetActorLocationAndRotation(Position, FRotator(Pitch, (PlayerLocation - Position).Rotation().Yaw, 0.f));
			for (const TWeakObjectPtr<UStaticMeshComponent>& Flame : BossFight.JetFlames)
			{
				if (Flame.IsValid())
				{
					Flame->SetRelativeScale3D(BossFight.JetFlameScale * FMath::FRandRange(0.85f, 1.15f));
				}
			}
			if (DiveAlpha >= 1.f && BossFight.bJetIntercept)
			{
				const bool bRammed = FVector::Dist(Position, PlayerLocation) < Radius + 500.f;
				if (bRammed && Player->GetHealthComponent())
				{
					// Hurts more and throws the player down
					Player->GetHealthComponent()->TakeDamage(Player, 30.f * (Settings.Difficulty >= 3 ? 1.5f : Settings.Difficulty <= 1 ? 0.6f : 1.f), GetDefault<UDamageType>(), nullptr, Boss);
					if (AFGHoverPack* HoverPack = Cast<AFGHoverPack>(Player->GetEquipmentInSlot(EEquipmentSlot::ES_BACK)))
					{
						HoverPack->SetHoverMode(EHoverPackMode::HPM_Inactive);
					}
					if (CanThrowPlayer(Player))
					{
						Player->LaunchCharacter(FVector(0.f, 0.f, -1000.f), true, true);
					}
				}
				UE_LOG(LogChatControl, Log, TEXT("Titan jetpack: ram %s, %.0f m from the player, player health now %.0f"), bRammed ? TEXT("hit") : TEXT("missed"), FVector::Dist(Position, PlayerLocation) / 100.f,
					Player->GetHealthComponent() ? Player->GetHealthComponent()->GetCurrentHealth() : 0.f);
				// Carries on down to the ground; a free fall could cost the boss health. A player it just hit is spared the landing's shockwave too
				BossFight.bJetRammed = bRammed;
				BossFight.bJetIntercept = false;
				BossFight.JetFrom = Position;
				BossFight.JetTo = GroundBelow(GetGameWorld(), Position, Boss, Player) + FVector(0.f, 0.f, HalfHeight + 20.f);
				GetGameWorld()->FindTeleportSpot(Boss, BossFight.JetTo, Boss->GetActorRotation());
				BossFight.JetDiveStartTime = Now;
			}
			else if (DiveAlpha >= 1.f)
			{
				BossFight.bJetFlying = false;
				BossFight.NextJetTime = Now + (BossFight.Stage >= 3 ? 8.0 : 12.0);
				BossFight.NextJetChaseTime = Now + 4.0;
				// Without a jetpack the wait is for being out of reach, which the timer above already measures; a stuck boss must not sit on a tower for 12 s
				if (!bHasJetpack)
				{
					BossFight.NextJetTime = Now + 4.0;
				}
				BossFight.NextStuckCheckTime = Now + 8.0;
				BossFight.LastStuckGap = 0.f;
				Movement->SetMovementMode(MOVE_Falling);
				Boss->SetActorRotation(FRotator(0.f, Boss->GetActorRotation().Yaw, 0.f));
				ShowFlames(false);
				if (!BossFight.bJetRammed)
				{
					Shockwave(Player, Position, Radius + 900.f, 25.f * (Settings.Difficulty >= 3 ? 1.5f : Settings.Difficulty <= 1 ? 0.6f : 1.f), 1200.f, Boss);
				}
				// The hard landing is the window to hit it
				if (AFGCreature* Creature = Cast<AFGCreature>(Boss))
				{
					Creature->BeginStun(2.f);
				}
				UE_LOG(LogChatControl, Log, TEXT("Titan jetpack: landed %.0f m from the player, stunned: %d, player health now %.0f"), FVector::Dist(Position, PlayerLocation) / 100.f, Cast<AFGCreature>(Boss) && Cast<AFGCreature>(Boss)->IsStunned() ? 1 : 0,
					Player->GetHealthComponent() ? Player->GetHealthComponent()->GetCurrentHealth() : 0.f);
			}
		}
	}
	const bool bPeriodicSummons = (BossFight.Traits & Summoning) || Type.Kind == EBossKind::Hatcher;
	if (bPeriodicSummons && Type.Kind != EBossKind::Peaceful && Now >= BossFight.NextSummonTime)
	{
		BossFight.NextSummonTime = Now + (Type.Kind == EBossKind::Hatcher ? (BossFight.Stage >= 2 ? 12.0 : 20.0) : 30.0);
		SummonMinions(2);
	}
	BossFight.LastHealth = Health->GetCurrentHealth();
}

void UChatControlSubsystem::EnterBossStage(int32 Stage)
{
	const FBossType& Type = BossTypes[BossFight.Type];
	const FText Title = FText::FromString(BossFight.Title);
	auto RunWorldEffect = [this](const TCHAR* EffectId)
	{
		if (const FChatEffect* WorldEffect = Effects.Find(EffectId))
		{
			ExecuteEffect(*WorldEffect, TitanKey, BossFight.Title);
		}
	};

	if (Type.Kind == EBossKind::Peaceful)
	{
		// Loot titans shed loot as they get hurt
		if (AFGCharacterPlayer* Player = FindPlayer(); Player && BossFight.Boss.IsValid())
		{
			ChatEffects::DropLootCrate(GetGameWorld(), Player, GroundBelow(GetGameWorld(), BossFight.Boss->GetActorLocation(), BossFight.Boss.Get(), Player) + FVector(0, 0, 100.f), 3, 0.4f);
		}
		Announce(FText::Format(Tr(TEXT("{0} drops some loot! Stage {1}/3"), TEXT("{0} роняет лут! Стадия {1}/3")), Title, FText::AsNumber(Stage)));
		Announce(Stage == 2
			? FText::Format(Tr(TEXT("{0} learned to leap - watch the ground!"), TEXT("{0} научился прыгать - следи за землёй!")), Title)
			: FText::Format(Tr(TEXT("{0} goes wild: faster charges and stomps!"), TEXT("{0} в бешенстве: таран и топот чаще!")), Title));
		return;
	}

	if (Stage == 2)
	{
		switch (Type.Kind)
		{
		case EBossKind::Melee:
			Announce(FText::Format(Tr(TEXT("{0} is enraged and calls its pack!"), TEXT("{0} в ярости и зовёт стаю!")), Title));
			SummonMinions(3);
			break;
		case EBossKind::Nuclear:
			Announce(FText::Format(Tr(TEXT("{0} digs up glowing helpers - they explode!"), TEXT("{0} выкапывает светящихся помощников - они взрываются!")), Title));
			SummonMinions(2);
			break;
		case EBossKind::Swarm:
			Announce(FText::Format(Tr(TEXT("{0} releases a swarm!"), TEXT("{0} выпускает рой!")), Title));
			SummonMinions(6);
			break;
		case EBossKind::Spitter:
			Announce(FText::Format(Tr(TEXT("{0} calls spitters of other elements!"), TEXT("{0} зовёт плевак других стихий!")), Title));
			SummonMinions(3);
			break;
		case EBossKind::Hatcher:
			Announce(FText::Format(Tr(TEXT("{0} hatches faster!"), TEXT("{0} вылупляет потомство быстрее!")), Title));
			SummonMinions(2);
			break;
		default:
			break;
		}
		if (BossFight.Traits & Sparking)
		{
			RunWorldEffect(TEXT("blackout"));
		}
	}
	else if (Stage == 3)
	{
		Announce(FText::Format(Tr(TEXT("LAST STAGE! {0} goes all in!"), TEXT("ПОСЛЕДНЯЯ СТАДИЯ! {0} идёт ва-банк!")), Title));
		switch (Type.Kind)
		{
		case EBossKind::Spitter:
			RunWorldEffect(TEXT("night"));
			RunWorldEffect(TEXT("fog"));
			SummonMinions(2);
			break;
		case EBossKind::Swarm:
		case EBossKind::Melee:
			SummonMinions(4);
			break;
		default:
			SummonMinions(2);
			break;
		}
	}
}

int32 UChatControlSubsystem::SummonMinions(int32 Count)
{
	UWorld* World = GetGameWorld();
	AFGCharacterPlayer* Player = FindPlayer();
	const FBossType& Type = BossTypes[BossFight.Type];
	if (!World || !Player || !Type.Minions[0])
	{
		return 0;
	}
	const int32 Total = FMath::Max(1, FMath::RoundToInt(Count * Settings.GetCreatureMultiplier()));
	int32 Spawned = 0;
	for (int32 i = 0; i < Total; ++i)
	{
		const TCHAR* Path = Type.Minions[FMath::RandRange(0, 1)];
		UClass* AnyClass = LoadClass<AActor>(nullptr, Path ? Path : Type.Minions[0]);
		if (AnyClass && AnyClass->IsChildOf(AFGCrabHatcher::StaticClass()))
		{
			// A crab nest on the ground that bursts into flying crabs when the player is near
			FVector NestLocation;
			// No ground around a player up in the air or on a tower: then around the boss
			if (FindGroundNear(World, Player, 500.f, 900.f, NestLocation) || (BossFight.Boss.IsValid() && FindGroundNear(World, BossFight.Boss.Get(), 500.f, 900.f, NestLocation)))
			{
				FActorSpawnParameters NestParams;
				NestParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AFGCrabHatcher* Nest = World->SpawnActor<AFGCrabHatcher>(AnyClass, NestLocation, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), NestParams))
				{
					Nest->SetLifeSpan(FMath::Max(10.f, Settings.CreatureLifeSpanSeconds));
					Nest->RegisterHostilePlayer(Player);
					TrackSpawned(Nest);
					++Spawned;
				}
			}
			continue;
		}
		UClass* MinionClass = AnyClass && AnyClass->IsChildOf(APawn::StaticClass()) ? AnyClass : nullptr;
		float Radius = 60.f, HalfHeight = 90.f;
		if (const ACharacter* Default = MinionClass ? Cast<ACharacter>(MinionClass->GetDefaultObject()) : nullptr; Default && Default->GetCapsuleComponent())
		{
			Default->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);
		}
		FVector Location;
		if (!MinionClass || !(ChatEffects::FindCreatureSpot(World, Player, Settings, Radius, HalfHeight, Location)
			|| (BossFight.Boss.IsValid() && ChatEffects::FindCreatureSpot(World, BossFight.Boss.Get(), Settings, Radius, HalfHeight, Location))))
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
		APawn* Minion = World->SpawnActor<APawn>(MinionClass, Location, (Player->GetActorLocation() - Location).GetSafeNormal2D().Rotation(), Params);
		if (!Minion)
		{
			continue;
		}
		if (!Minion->GetController())
		{
			Minion->SpawnDefaultController();
		}
		if (AFGCreature* Creature = Cast<AFGCreature>(Minion))
		{
			Creature->SetIsPersistent(true);
		}
		Minion->SetLifeSpan(FMath::Max(10.f, Settings.CreatureLifeSpanSeconds));
		TrackSpawned(Minion);
		ProvokeCreature(Minion, Player);
		BossFight.Minions.Add(Minion);
		++Spawned;
	}
	UE_LOG(LogChatControl, Log, TEXT("Titan: %d minion(s) summoned"), Spawned);
	return Spawned;
}

void UChatControlSubsystem::EndBossFight(bool bDestroyBoss)
{
	if (bDestroyBoss && BossFight.Boss.IsValid())
	{
		BossFight.Boss->Destroy();
	}
	else if (BossFight.bJetFlying)
	{
		// The fight ended in the air: gravity takes over again
		if (ACharacter* Flyer = Cast<ACharacter>(BossFight.Boss.Get()); Flyer && Flyer->GetCharacterMovement())
		{
			Flyer->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			Flyer->SetActorRotation(FRotator(0.f, Flyer->GetActorRotation().Yaw, 0.f));
		}
	}
	// The capsule of a boss killed in the air stays up there, and what hangs on it would too
	if (BossFight.Bar.IsValid())
	{
		BossFight.Bar->DestroyComponent();
	}
	for (const TWeakObjectPtr<UStaticMeshComponent>& Flame : BossFight.JetFlames)
	{
		if (Flame.IsValid())
		{
			Flame->DestroyComponent();
		}
	}
	BossFight = FBossFight();
}

TSharedPtr<FJsonObject> UChatControlSubsystem::BuildBossJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	const UFGHealthComponent* Health = BossFight.Health.Get();
	Json->SetBoolField(TEXT("active"), BossFight.bActive);
	Json->SetBoolField(TEXT("arriving"), BossFight.bArriving);
	Json->SetStringField(TEXT("name"), BossFight.Title);
	Json->SetStringField(TEXT("viewer"), BossFight.ViewerName);
	Json->SetNumberField(TEXT("stage"), BossFight.Stage);
	Json->SetNumberField(TEXT("health"), Health ? Health->GetCurrentHealth() : 0.f);
	Json->SetNumberField(TEXT("maxHealth"), BossFight.MaxHealth);
	Json->SetBoolField(TEXT("peaceful"), BossFight.bActive && BossTypes[BossFight.Type].Kind == EBossKind::Peaceful);
	return Json;
}
