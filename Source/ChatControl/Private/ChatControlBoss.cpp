// Titans: boss fights with a random boss, random traits, three stages, a health bar and rewards.
#include "ChatControlSubsystem.h"
#include "ChatControl.h"
#include "ChatText.h"
#include "ChatEffects.h"
#include "FGCharacterPlayer.h"
#include "FGHealthComponent.h"
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
#include "Materials/MaterialInterface.h"
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

	/** A game mesh strapped on the boss, Size centimeters along its longest side; no collision */
	UStaticMeshComponent* AddJetpackPart(APawn* Pawn, const TCHAR* MeshPath, const FVector& Location, const FRotator& Rotation, float Size)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
		if (!Mesh)
		{
			UE_LOG(LogChatControl, Warning, TEXT("Titan jetpack: mesh not found: %s"), MeshPath);
			return nullptr;
		}
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Pawn);
		Part->SetStaticMesh(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->SetupAttachment(Pawn->GetRootComponent());
		Part->SetRelativeLocationAndRotation(Location, Rotation);
		Part->SetRelativeScale3D(FVector(Size / FMath::Max(1.0, 2.0 * Mesh->GetBounds().BoxExtent.GetMax())));
		Part->RegisterComponent();
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

	/** Damage that fades with distance from Center and throws the player away from it */
	void Shockwave(AFGCharacterPlayer* Player, const FVector& Center, float Radius, float Damage, float Push, AActor* Causer)
	{
		const float Distance = FVector::Dist2D(Player->GetActorLocation(), Center);
		if (Distance > Radius || !Player->GetHealthComponent())
		{
			return;
		}
		const float Falloff = 1.f - 0.6f * Distance / Radius;
		Player->GetHealthComponent()->TakeDamage(Player, Damage * Falloff, GetDefault<UDamageType>(), nullptr, Causer);
		const FVector Away = (Player->GetActorLocation() - Center).GetSafeNormal2D();
		Player->LaunchCharacter(Away * Push * Falloff + FVector(0, 0, Push * 0.5f), true, true);
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
	BossFight.NextSummonTime = Now + (Boss.Kind == EBossKind::Hatcher ? 20.0 : 30.0);
	BossFight.NextRegenTime = Now + 1.0;
	Pawn->CustomTimeDilation = BossFight.BaseDilation;
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
		const ACharacter* Character = Cast<ACharacter>(Pawn);
		if (const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr)
		{
			float BodyRadius = 100.f, BodyHalfHeight = 100.f;
			Capsule->GetUnscaledCapsuleSize(BodyRadius, BodyHalfHeight);
			AddJetpackPart(Pawn, TEXT("/Game/FactoryGame/Equipment/JetPack/Mesh/Jetpack_Groundmesh.Jetpack_Groundmesh"),
				FVector(-BodyRadius * 0.3f, 0.f, BodyHalfHeight + 20.f), FRotator::ZeroRotator, FMath::Max(120.f, BodyRadius * 1.4f));
			UMaterialInterface* FlameMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FactoryGame/VFX/Vehicles/Drone/MI/MI_Thruster_Drone_01.MI_Thruster_Drone_01"));
			for (const float Side : { -1.f, 1.f })
			{
				UStaticMeshComponent* Flame = AddJetpackPart(Pawn, TEXT("/Game/FactoryGame/VFX/Mesh/SM_VFX_Thruster_01.SM_VFX_Thruster_01"),
					FVector(-BodyRadius * 0.3f, Side * BodyRadius * 0.4f, BodyHalfHeight - 40.f), FRotator::ZeroRotator, FMath::Max(150.f, BodyRadius * 1.5f));
				if (Flame)
				{
					Flame->SetMaterial(0, FlameMaterial);
					Flame->SetVisibility(false);
					BossFight.JetFlames.Add(Flame);
				}
			}
			UE_LOG(LogChatControl, Log, TEXT("Titan jetpack: %d flames, flame material %s"), BossFight.JetFlames.Num(), FlameMaterial ? TEXT("found") : TEXT("not found"));
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
	const float WorldHeight = Origin.Z + Extent.Z - Boss->GetActorLocation().Z + 140.f;

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
		if (Player && ChatEffects::DropLootCrate(GetGameWorld(), Player, LootLocation + FVector(0, 0, 100.f), Loot, 0.6f))
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
		if (Now >= BossFight.NextStompTime)
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
	if ((BossFight.Traits & Jetpack) && Player)
	{
		// Jetpack: flies up and over to the player and lands next to them with a shockwave
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
		// !dance freezes creatures with a tiny time dilation; a frozen boss stays on the ground
		if (Movement && !BossFight.bJetFlying && Now >= BossFight.NextJetTime && Boss->CustomTimeDilation >= 0.01f)
		{
			BossFight.bJetFlying = true;
			BossFight.JetStartTime = Now;
			BossFight.JetFrom = Boss->GetActorLocation();
			const float Distance = FVector::Dist2D(Player->GetActorLocation(), BossFight.JetFrom);
			BossFight.JetDuration = FMath::Clamp(Distance / 1500.f, 2.f, 4.5f);
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
			const float Alpha = FMath::Clamp(static_cast<float>((Now - BossFight.JetStartTime) / BossFight.JetDuration), 0.f, 1.f);
			if (Alpha < 0.7f)
			{
				// Follows the player until it starts coming down, which leaves time to dodge
				const FVector Away = (BossFight.JetFrom - Player->GetActorLocation()).GetSafeNormal2D();
				BossFight.JetTo = Player->GetActorLocation() + Away * (Radius + 350.f)
					+ FVector(0.f, 0.f, HalfHeight - Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 30.f);
			}
			FVector Position = FMath::Lerp(BossFight.JetFrom, BossFight.JetTo, Alpha);
			Position.Z += 1500.f * FMath::Sin(PI * Alpha);
			if (Movement->MovementMode != MOVE_Flying)
			{
				Movement->SetMovementMode(MOVE_Flying);
			}
			Movement->StopMovementImmediately();
			Boss->SetActorLocationAndRotation(Position, FRotator(0.f, (Player->GetActorLocation() - Position).Rotation().Yaw, 0.f));
			if (Alpha >= 1.f)
			{
				BossFight.bJetFlying = false;
				BossFight.NextJetTime = Now + (BossFight.Stage >= 3 ? 5.0 : 8.0);
				Movement->SetMovementMode(MOVE_Falling);
				ShowFlames(false);
				Shockwave(Player, Position, Radius + 900.f, 20.f * (Settings.Difficulty >= 3 ? 1.5f : Settings.Difficulty <= 1 ? 0.6f : 1.f), 1100.f, Boss);
				UE_LOG(LogChatControl, Log, TEXT("Titan jetpack: landed %.0f m from the player"), FVector::Dist(Position, Player->GetActorLocation()) / 100.f);
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
			ChatEffects::DropLootCrate(GetGameWorld(), Player, BossFight.Boss->GetActorLocation() + FVector(0, 0, 100.f), 3, 0.4f);
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
			if (FindGroundNear(World, Player, 500.f, 900.f, NestLocation))
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
		if (!MinionClass || !ChatEffects::FindCreatureSpot(World, Player, Settings, Radius, HalfHeight, Location))
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
