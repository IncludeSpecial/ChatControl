#include "ChatEffects.h"
#include "ChatControl.h"
#include "ChatControlSettings.h"
#include "ChatControlSubsystem.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Creature/FGCreature.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableGenerator.h"
#include "FGItemPickup_Spawnable.h"
#include "FGCrate.h"
#include "FGResourceSinkSubsystem.h"
#include "FGTimeSubsystem.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "ChatText.h"
#include "FGHealthComponent.h"
#include "Resources/FGConsumableDescriptor.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "UObject/UObjectIterator.h"

using ChatText::Tr;

namespace
{
	const TCHAR* BasicGiftPool[] = {
		TEXT("/Game/FactoryGame/Resource/RawResources/OreIron/Desc_OreIron.Desc_OreIron_C"),
		TEXT("/Game/FactoryGame/Resource/RawResources/OreCopper/Desc_OreCopper.Desc_OreCopper_C"),
		TEXT("/Game/FactoryGame/Resource/RawResources/Stone/Desc_Stone.Desc_Stone_C"),
		TEXT("/Game/FactoryGame/Resource/RawResources/Coal/Desc_Coal.Desc_Coal_C"),
		TEXT("/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate.Desc_IronPlate_C"),
		TEXT("/Game/FactoryGame/Resource/Parts/IronRod/Desc_IronRod.Desc_IronRod_C"),
		TEXT("/Game/FactoryGame/Resource/Parts/Wire/Desc_Wire.Desc_Wire_C"),
		TEXT("/Game/FactoryGame/Resource/Parts/Cable/Desc_Cable.Desc_Cable_C"),
		TEXT("/Game/FactoryGame/Resource/Parts/Cement/Desc_Cement.Desc_Cement_C"),
	};

	const TCHAR* AdvancedGiftPool[] = {
		TEXT("/Game/FactoryGame/Resource/Parts/IronPlateReinforced/Desc_IronPlateReinforced.Desc_IronPlateReinforced_C"),
		TEXT("/Game/FactoryGame/Resource/Parts/ModularFrame/Desc_ModularFrame.Desc_ModularFrame_C"),
		TEXT("/Game/FactoryGame/Resource/Parts/Rotor/Desc_Rotor.Desc_Rotor_C"),
		TEXT("/Game/FactoryGame/Resource/Parts/Motor/Desc_Motor.Desc_Motor_C"),
		TEXT("/Game/FactoryGame/Resource/Parts/CircuitBoard/Desc_CircuitBoard.Desc_CircuitBoard_C"),
		TEXT("/Game/FactoryGame/Resource/Parts/Computer/Desc_Computer.Desc_Computer_C"),
	};

	const TCHAR* SmallCreaturePool[] = {
		TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/Char_Hog.Char_Hog_C"),
		TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/SmallStinger/Char_Stinger_Child.Char_Stinger_Child_C"),
		TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForest/Char_SpitterForestSmall.Char_SpitterForestSmall_C"),
	};

	const TCHAR* BossCreaturePool[] = {
		TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/AlphaHog/Char_AlphaHog.Char_AlphaHog_C"),
		TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/BigStinger/Char_EliteStinger.Char_EliteStinger_C"),
		TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForest/Char_SpitterForestAlpha.Char_SpitterForestAlpha_C"),
	};

	/**
	 * Finds ground on a ring around the player at roughly the player's height and in line of sight,
	 * so creatures don't end up on tree tops, roofs or behind walls.
	 */
	bool FindSpawnPoint(UWorld* World, const AActor* Player, const FChatControlSettings& Settings, float Radius, float HalfHeight, FVector& OutLocation)
	{
		const FVector Center = Player->GetActorLocation();
		FCollisionQueryParams Params(TEXT("ChatControlSpawn"), false, Player);
		const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Radius, HalfHeight);
		for (int32 Attempt = 0; Attempt < 40; ++Attempt)
		{
			const float Angle = FMath::FRandRange(0.f, 2.f * PI);
			// Start close and widen the ring if nothing fits
			const float MinDistance = FMath::Max(1.f, Settings.SpawnDistanceMin) * 100.f;
			const float MaxDistance = FMath::Max(Settings.SpawnDistanceMin, Settings.SpawnDistanceMax) * 100.f;
			const float Distance = FMath::FRandRange(MinDistance, MaxDistance) + Attempt * 50.f;
			const FVector Probe = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance;

			// Prefer spots visible from the player; among buildings that is often impossible, so drop it later
			FHitResult Blocked;
			if (Attempt < 20 && World->LineTraceSingleByChannel(Blocked, Center, Probe, ECC_Visibility, Params))
			{
				continue;
			}
			FHitResult Ground;
			if (!World->LineTraceSingleByChannel(Ground, Probe + FVector(0, 0, 300.f), Probe - FVector(0, 0, 1000.f), ECC_Visibility, Params))
			{
				continue;
			}
			// Big creatures (alphas) need the whole capsule to fit, not just a point
			const FVector Location = Ground.ImpactPoint + FVector(0, 0, HalfHeight + 30.f);
			if (World->OverlapAnyTestByChannel(Location, FQuat::Identity, ECC_Pawn, Capsule, Params))
			{
				continue;
			}
			OutLocation = Location;
			return true;
		}
		return false;
	}

	template <int32 N>
	int32 SpawnCreatures(UChatControlSubsystem* Subsystem, UWorld* World, const AFGCharacterPlayer* Player, const FChatControlSettings& Settings, const TCHAR* (&Pool)[N], int32 Count, const FString& ViewerName)
	{
		int32 Spawned = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			const TCHAR* ClassPath = Pool[FMath::RandRange(0, N - 1)];
			UClass* CreatureClass = LoadClass<APawn>(nullptr, ClassPath);
			if (!CreatureClass)
			{
				UE_LOG(LogChatControl, Warning, TEXT("Creature class failed to load: %s"), ClassPath);
				continue;
			}
			float Radius = 60.f, HalfHeight = 90.f;
			if (const ACharacter* Default = Cast<ACharacter>(CreatureClass->GetDefaultObject()); Default && Default->GetCapsuleComponent())
			{
				Default->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);
			}
			FVector Location;
			if (!FindSpawnPoint(World, Player, Settings, Radius, HalfHeight, Location))
			{
				UE_LOG(LogChatControl, Warning, TEXT("No free spot for %s (radius %.0f) near the player"), *CreatureClass->GetName(), Radius);
				continue;
			}
			// Face the player so they notice them and aggro quickly
			const FRotator Facing((Player->GetActorLocation() - Location).GetSafeNormal2D().Rotation());
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
			APawn* Creature = World->SpawnActor<APawn>(CreatureClass, Location, Facing, Params);
			if (!Creature)
			{
				continue;
			}
			if (!Creature->GetController())
			{
				Creature->SpawnDefaultController();
			}
			// Creatures without a spawner and without persistence get culled by the game right away
			AFGCreature* FGCreature = Cast<AFGCreature>(Creature);
			if (FGCreature)
			{
				FGCreature->SetIsPersistent(true);
			}
			Creature->SetLifeSpan(FMath::Max(10.f, Settings.CreatureLifeSpanSeconds));
			Subsystem->TrackSpawned(Creature);
			if (Settings.bCreatureNameTags)
			{
				Subsystem->AttachNameTag(Creature, Subsystem->GetTagText(ViewerName), Subsystem->GetNameColor(ViewerName, FColor(255, 90, 60)), 160.f);
			}
			UE_LOG(LogChatControl, Log, TEXT("Spawned %s %.0f m from player, enabled=%d, controller=%s"),
				*CreatureClass->GetName(), FVector::Dist(Location, Player->GetActorLocation()) / 100.f,
				FGCreature ? FGCreature->GetIsEnabled() : -1, *GetNameSafe(Creature->GetController()));
			++Spawned;
		}
		return Spawned;
	}

	/** Spawns what fits; whatever did not fit is retried a few seconds later (the player may move out of a tight spot) */
	template <int32 N>
	void SpawnWithRetry(UChatControlSubsystem* Subsystem, const TCHAR* (&Pool)[N], int32 Count, const FString& Viewer, const FText& Arrival, int32 TriesLeft)
	{
		UWorld* World = Subsystem->GetGameWorld();
		const AFGCharacterPlayer* Player = Subsystem->FindPlayer();
		const int32 Spawned = World && Player ? SpawnCreatures(Subsystem, World, Player, Subsystem->GetSettings(), Pool, Count, Viewer) : 0;
		if (Spawned > 0)
		{
			Subsystem->Announce(FText::Format(Arrival, FText::AsNumber(Spawned), FText::FromString(Viewer)));
		}
		const int32 Missing = Count - Spawned;
		if (Missing <= 0)
		{
			return;
		}
		if (TriesLeft <= 0)
		{
			Subsystem->Announce(FText::Format(Tr(TEXT("No room for creatures from {0} here - they gave up."), TEXT("Существам от {0} здесь не нашлось места - они ушли.")), FText::FromString(Viewer)));
			return;
		}
		UE_LOG(LogChatControl, Log, TEXT("%d creature(s) did not fit, retrying in 3 s"), Missing);
		Subsystem->RunDelayed(3.f, [Subsystem, &Pool, Missing, Viewer, Arrival, TriesLeft]
		{
			SpawnWithRetry(Subsystem, Pool, Missing, Viewer, Arrival, TriesLeft - 1);
		});
	}

	/**
	 * Warns the player, then spawns after a random delay (DelayMin..DelayMax) so there is a moment to react.
	 * Warning args: {0} viewer, {1} count, {2} seconds. Arrival args: {0} spawned count, {1} viewer.
	 */
	template <int32 N>
	bool SpawnCreaturesDelayed(const FChatEffectContext& Ctx, const TCHAR* (&Pool)[N], const FText& Warning, const FText& Arrival)
	{
		// Near the base creatures wait in the chaos queue until the player leaves
		if (Ctx.Subsystem->IsInSafeZone())
		{
			return false;
		}
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Ctx.Settings->Amount * Ctx.Subsystem->GetSettings().GetCreatureMultiplier()));
		const float Delay = FMath::Max(0.f, FMath::FRandRange(Ctx.Settings->DelayMin, FMath::Max(Ctx.Settings->DelayMin, Ctx.Settings->DelayMax)));
		UChatControlSubsystem* Subsystem = Ctx.Subsystem;
		const FString Viewer = Ctx.ViewerName;

		auto Spawn = [Subsystem, &Pool, Count, Viewer, Arrival]
		{
			SpawnWithRetry(Subsystem, Pool, Count, Viewer, Arrival, 3);
		};

		if (Delay < 0.1f)
		{
			Spawn();
			return true;
		}
		Subsystem->Announce(FText::Format(Warning, FText::FromString(Viewer), FText::AsNumber(Count), FText::AsNumber(FMath::RoundToInt(Delay))));
		Subsystem->RunDelayed(Delay, MoveTemp(Spawn));
		return true;
	}

	template <int32 N>
	bool GiveRandomItem(const FChatEffectContext& Ctx, const TCHAR* (&Pool)[N], float StackFraction)
	{
		UFGInventoryComponent* Inventory = Ctx.Player ? Ctx.Player->GetInventory() : nullptr;
		if (!Inventory)
		{
			return false;
		}
		TSubclassOf<UFGItemDescriptor> ItemClass = LoadClass<UFGItemDescriptor>(nullptr, Pool[FMath::RandRange(0, N - 1)]);
		if (!ItemClass)
		{
			UE_LOG(LogChatControl, Warning, TEXT("Gift item class failed to load"));
			return false;
		}
		const int32 Amount = FMath::Max(1, FMath::RoundToInt(UFGItemDescriptor::GetStackSize(ItemClass) * FMath::Clamp(StackFraction, 0.01f, 1.f)));
		const int32 Added = Inventory->AddStack(FInventoryStack(Amount, ItemClass), true);
		if (Added <= 0)
		{
			Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} tried to send you a gift, but your inventory is full!"), TEXT("{0} хотел(а) прислать подарок, но инвентарь полон!")), FText::FromString(Ctx.ViewerName)));
			return false;
		}
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} sent you {1} x {2}!"), TEXT("{0} прислал(а) тебе {1} x {2}!")),
			FText::FromString(Ctx.ViewerName), FText::AsNumber(Added), UFGItemDescriptor::GetItemName(ItemClass)));
		return true;
	}

	UCharacterMovementComponent* GetPlayerMovement(const UChatControlSubsystem* Subsystem)
	{
		const AFGCharacterPlayer* Player = Subsystem->FindPlayer();
		return Player ? Player->GetCharacterMovement() : nullptr;
	}

	void LogMovementDiagnostics(const FChatEffectContext& Ctx, const TCHAR* Effect)
	{
		const UCharacterMovementComponent* Movement = Ctx.Player->GetCharacterMovement();
		UE_LOG(LogChatControl, Log, TEXT("%s on %s: locally controlled=%d, mode=%d, gravityZ=%.1f, gravityScale=%.2f, jumpZ=%.1f"),
			Effect, *Ctx.Player->GetName(), Ctx.Player->IsLocallyControlled(), (int32)Movement->MovementMode,
			Movement->GetGravityZ(), Movement->GravityScale, Movement->JumpZVelocity);
	}

	/**
	 * Changes how strongly the player falls by adding vertical velocity every tick while airborne.
	 * Works on velocity directly because the game does not reliably use GravityScale for the player.
	 * 0.25 = moon, 3 = heavy.
	 */
	bool ScaleGravityTemporarily(const FChatEffectContext& Ctx, float Multiplier, const FText& Message)
	{
		if (!Ctx.Player || !Ctx.Player->GetCharacterMovement())
		{
			return false;
		}
		LogMovementDiagnostics(Ctx, TEXT("gravity"));
		UChatControlSubsystem* Subsystem = Ctx.Subsystem;
		UWorld* World = Ctx.World;
		Subsystem->StartTimedEffect(TEXT("gravity"), Ctx.Settings->Duration,
			[Subsystem, World, Multiplier]
			{
				UCharacterMovementComponent* Movement = GetPlayerMovement(Subsystem);
				if (Movement && Movement->IsFalling())
				{
					// GetGravityZ is negative; (1 - Multiplier) of it is added back (moon) or extra applied (heavy)
					Movement->Velocity.Z -= (1.f - Multiplier) * Movement->GetGravityZ() * World->GetDeltaSeconds();
				}
			},
			[] {});
		Subsystem->Announce(Message);
		return true;
	}

	/** Speeds the player up or slows them down through actor time dilation (movement, animations, jumps) */
	bool ScaleTimeTemporarily(const FChatEffectContext& Ctx, const FText& Message)
	{
		if (!Ctx.Player)
		{
			return false;
		}
		UChatControlSubsystem* Subsystem = Ctx.Subsystem;
		const float Dilation = FMath::Clamp(Ctx.Settings->Strength, 0.1f, 5.f);
		Subsystem->StartTimedEffect(TEXT("time"), Ctx.Settings->Duration,
			[Subsystem, Dilation]
			{
				if (AFGCharacterPlayer* Player = Subsystem->FindPlayer())
				{
					Player->CustomTimeDilation = Dilation;
				}
			},
			[Subsystem]
			{
				if (AFGCharacterPlayer* Player = Subsystem->FindPlayer())
				{
					Player->CustomTimeDilation = 1.f;
				}
			});
		Subsystem->Announce(Message);
		return true;
	}

	/** Teleports to ground near Center, trying a ring of the given radius; traces from high above so it works on uneven terrain */
	bool TeleportNear(AFGCharacterPlayer* Player, UWorld* World, const FVector& Center, float RadiusMin, float RadiusMax)
	{
		FCollisionQueryParams Params(TEXT("ChatControlTeleport"), false, Player);
		for (int32 Attempt = 0; Attempt < 16; ++Attempt)
		{
			const float Angle = FMath::FRandRange(0.f, 2.f * PI);
			const FVector Probe = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * FMath::FRandRange(RadiusMin, RadiusMax);
			FHitResult Ground;
			if (World->LineTraceSingleByChannel(Ground, Probe + FVector(0, 0, 5000.f), Probe - FVector(0, 0, 10000.f), ECC_Visibility, Params) &&
				Player->TeleportTo(Ground.ImpactPoint + FVector(0, 0, 150.f), Player->GetActorRotation()))
			{
				return true;
			}
		}
		return false;
	}

	/** Multiplies the vertical speed at the moment the player leaves the ground with upward speed (a jump) */
	bool BoostJumpsTemporarily(const FChatEffectContext& Ctx, float Multiplier, const FText& Message)
	{
		if (!Ctx.Player || !Ctx.Player->GetCharacterMovement())
		{
			return false;
		}
		LogMovementDiagnostics(Ctx, TEXT("jump"));
		UChatControlSubsystem* Subsystem = Ctx.Subsystem;
		TSharedRef<bool> bWasFalling = MakeShared<bool>(Ctx.Player->GetCharacterMovement()->IsFalling());
		Subsystem->StartTimedEffect(TEXT("jump"), Ctx.Settings->Duration,
			[Subsystem, bWasFalling, Multiplier]
			{
				UCharacterMovementComponent* Movement = GetPlayerMovement(Subsystem);
				if (!Movement)
				{
					return;
				}
				const bool bFalling = Movement->IsFalling();
				if (bFalling && !*bWasFalling && Movement->Velocity.Z > 100.f)
				{
					Movement->Velocity.Z *= Multiplier;
				}
				*bWasFalling = bFalling;
			},
			[] {});
		Ctx.Subsystem->Announce(Message);
		return true;
	}
}

namespace
{
	/** Berries, nuts, mushrooms and medicine across all of the player's inventories, including the hand slot */
	int32 CountFood(const AActor* Player)
	{
		int32 Count = 0;
		TInlineComponentArray<UFGInventoryComponent*> Inventories(Player);
		for (const UFGInventoryComponent* Inventory : Inventories)
		{
			TArray<FInventoryStack> Stacks;
			Inventory->GetInventoryStacks(Stacks);
			for (const FInventoryStack& Stack : Stacks)
			{
				if (Stack.Item.GetItemClass() && Stack.Item.GetItemClass()->IsChildOf(UFGConsumableDescriptor::StaticClass()))
				{
					Count += Stack.NumItems;
				}
			}
		}
		return Count;
	}
}

TMap<FString, FChatEffect> ChatEffects::CreateAll()
{
	TMap<FString, FChatEffect> Effects;
	auto Add = [&Effects](const TCHAR* Id, bool bHarmful, TFunction<bool(const FChatEffectContext&)> Execute)
	{
		Effects.Add(Id, FChatEffect{ Id, bHarmful, MoveTemp(Execute) });
	};

	Add(TEXT("hi"), false, [](const FChatEffectContext& Ctx)
	{
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} waves at you!"), TEXT("{0} машет тебе!")), FText::FromString(Ctx.ViewerName)));
		return true;
	});

	Add(TEXT("gift"), false, [](const FChatEffectContext& Ctx)
	{
		return GiveRandomItem(Ctx, BasicGiftPool, Ctx.Settings->Strength);
	});

	Add(TEXT("biggift"), false, [](const FChatEffectContext& Ctx)
	{
		return GiveRandomItem(Ctx, AdvancedGiftPool, Ctx.Settings->Strength);
	});

	Add(TEXT("jump"), true, [](const FChatEffectContext& Ctx)
	{
		return BoostJumpsTemporarily(Ctx, Ctx.Settings->Strength,
			FText::Format(Tr(TEXT("{0} gave you super jump for {1} s!"), TEXT("{0} дарит тебе супер-прыжок на {1} с!")),
				FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
	});

	Add(TEXT("moon"), true, [](const FChatEffectContext& Ctx)
	{
		return ScaleGravityTemporarily(Ctx, Ctx.Settings->Strength,
			FText::Format(Tr(TEXT("{0} switched on moon gravity for {1} s!"), TEXT("{0} включает лунную гравитацию на {1} с!")),
				FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
	});

	Add(TEXT("heavy"), true, [](const FChatEffectContext& Ctx)
	{
		return ScaleGravityTemporarily(Ctx, Ctx.Settings->Strength,
			FText::Format(Tr(TEXT("{0} made you heavy for {1} s!"), TEXT("{0} делает тебя тяжёлым на {1} с!")),
				FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
	});

	Add(TEXT("speed"), true, [](const FChatEffectContext& Ctx)
	{
		return ScaleTimeTemporarily(Ctx,
			FText::Format(Tr(TEXT("{0} put you in fast-forward for {1} s!"), TEXT("{0} ускоряет тебя на {1} с!")),
				FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
	});

	Add(TEXT("slow"), true, [](const FChatEffectContext& Ctx)
	{
		return ScaleTimeTemporarily(Ctx,
			FText::Format(Tr(TEXT("{0} put you in slow motion for {1} s!"), TEXT("{0} замедляет тебя на {1} с!")),
				FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
	});

	Add(TEXT("fov"), true, [](const FChatEffectContext& Ctx)
	{
		APlayerController* Controller = Ctx.World->GetFirstPlayerController();
		APlayerCameraManager* Camera = Controller ? Controller->PlayerCameraManager.Get() : nullptr;
		if (!Camera)
		{
			return false;
		}
		TWeakObjectPtr<APlayerCameraManager> WeakCamera = Camera;
		const float TargetFOV = FMath::Clamp(Camera->GetFOVAngle() * Ctx.Settings->Strength, 20.f, 170.f);
		Ctx.Subsystem->StartTimedEffect(TEXT("fov"), Ctx.Settings->Duration,
			[WeakCamera, TargetFOV]
			{
				if (WeakCamera.IsValid())
				{
					WeakCamera->SetFOV(TargetFOV);
				}
			},
			[WeakCamera]
			{
				if (WeakCamera.IsValid())
				{
					WeakCamera->UnlockFOV();
				}
			});
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} bent your camera for {1} s!"), TEXT("{0} искривляет тебе камеру на {1} с!")),
			FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
		return true;
	});

	Add(TEXT("teleport"), true, [](const FChatEffectContext& Ctx)
	{
		const float Distance = FMath::Max(5.f, Ctx.Settings->Strength) * 100.f;
		if (!TeleportNear(Ctx.Player, Ctx.World, Ctx.Player->GetActorLocation(), Distance * 0.75f, Distance * 1.25f))
		{
			return false;
		}
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} teleported you somewhere!"), TEXT("{0} телепортирует тебя куда-то!")), FText::FromString(Ctx.ViewerName)));
		return true;
	});

	Add(TEXT("hub"), true, [](const FChatEffectContext& Ctx)
	{
		FVector Hub;
		if (!Ctx.Subsystem->GetHubLocation(Hub) || !TeleportNear(Ctx.Player, Ctx.World, Hub, 800.f, 1500.f))
		{
			return false;
		}
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} sent you back to the HUB!"), TEXT("{0} отправляет тебя обратно к ХАБу!")), FText::FromString(Ctx.ViewerName)));
		return true;
	});

	Add(TEXT("strike"), true, [](const FChatEffectContext& Ctx)
	{
		TArray<AFGBuildableManufacturer*> Working;
		for (TActorIterator<AFGBuildableManufacturer> It(Ctx.World); It; ++It)
		{
			if (!It->IsProductionPaused())
			{
				Working.Add(*It);
			}
		}
		if (Working.Num() == 0)
		{
			return false;
		}
		// Only machines paused by the strike are resumed, the player's own pauses stay untouched
		TArray<TWeakObjectPtr<AFGBuildableManufacturer>> Striking;
		const int32 Count = FMath::Min(Working.Num(), FMath::Max(1, Ctx.Settings->Amount));
		for (int32 i = 0; i < Count; ++i)
		{
			AFGBuildableManufacturer* Machine = Working[FMath::RandRange(i, Working.Num() - 1)];
			Working.Swap(i, Working.IndexOfByKey(Machine));
			Machine->SetIsProductionPaused(true);
			Striking.Add(Machine);
		}
		Ctx.Subsystem->StartTimedEffect(TEXT("strike"), Ctx.Settings->Duration, [] {},
			[Striking]
			{
				for (const TWeakObjectPtr<AFGBuildableManufacturer>& Machine : Striking)
				{
					if (Machine.IsValid() && Machine->IsProductionPaused())
					{
						Machine->SetIsProductionPaused(false);
					}
				}
			});
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} started a strike: {1} machines stop for {2} s!"), TEXT("{0} устраивает забастовку: {1} машин стоят {2} с!")),
			FText::FromString(Ctx.ViewerName), FText::AsNumber(Count), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
		return true;
	});

	Add(TEXT("blackout"), true, [](const FChatEffectContext& Ctx)
	{
		// Stopping the generators overloads the grid, so the fuse blows the normal way and has to be reset by hand
		TArray<TWeakObjectPtr<AFGBuildableGenerator>> Stopped;
		for (TActorIterator<AFGBuildableGenerator> It(Ctx.World); It; ++It)
		{
			if (!It->IsProductionPaused())
			{
				It->SetIsProductionPaused(true);
				Stopped.Add(*It);
			}
		}
		if (Stopped.Num() == 0)
		{
			return false;
		}
		Ctx.Subsystem->StartTimedEffect(TEXT("blackout"), Ctx.Settings->Duration, [] {},
			[Stopped]
			{
				for (const TWeakObjectPtr<AFGBuildableGenerator>& Generator : Stopped)
				{
					if (Generator.IsValid() && Generator->IsProductionPaused())
					{
						Generator->SetIsProductionPaused(false);
					}
				}
			});
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} pulled the plug! Generators are down for {1} s - check your fuses!"), TEXT("{0} выдёргивает рубильник! Генераторы стоят {1} с - проверь предохранители!")),
			FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
		return true;
	});

	Add(TEXT("underclock"), true, [](const FChatEffectContext& Ctx)
	{
		TArray<AFGBuildableManufacturer*> Machines;
		for (TActorIterator<AFGBuildableManufacturer> It(Ctx.World); It; ++It)
		{
			if (It->GetCanChangePotential() && It->GetPendingPotential() > Ctx.Settings->Strength)
			{
				Machines.Add(*It);
			}
		}
		if (Machines.Num() == 0)
		{
			return false;
		}
		const int32 Count = FMath::Min(Machines.Num(), FMath::Max(1, Ctx.Settings->Amount));
		TArray<TPair<TWeakObjectPtr<AFGBuildableManufacturer>, float>> Changed;
		for (int32 i = 0; i < Count; ++i)
		{
			Machines.Swap(i, FMath::RandRange(i, Machines.Num() - 1));
			Changed.Add({ Machines[i], Machines[i]->GetPendingPotential() });
			Machines[i]->SetPendingPotential(FMath::Clamp(Ctx.Settings->Strength, 0.01f, 1.f));
		}
		Ctx.Subsystem->StartTimedEffect(TEXT("underclock"), Ctx.Settings->Duration, [] {},
			[Changed]
			{
				for (const auto& Pair : Changed)
				{
					if (Pair.Key.IsValid())
					{
						Pair.Key->SetPendingPotential(Pair.Value);
					}
				}
			});
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} underclocked {1} machines for {2} s!"), TEXT("{0} замедляет {1} машин на {2} с!")),
			FText::FromString(Ctx.ViewerName), FText::AsNumber(Count), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
		return true;
	});

	Add(TEXT("rain"), false, [](const FChatEffectContext& Ctx)
	{
		TArray<FInventoryStack> Items;
		for (int32 i = 0; i < FMath::Max(1, Ctx.Settings->Amount); ++i)
		{
			const bool bAdvanced = FMath::FRand() < 0.3f;
			const TCHAR* Path = bAdvanced ? AdvancedGiftPool[FMath::RandRange(0, UE_ARRAY_COUNT(AdvancedGiftPool) - 1)] : BasicGiftPool[FMath::RandRange(0, UE_ARRAY_COUNT(BasicGiftPool) - 1)];
			if (TSubclassOf<UFGItemDescriptor> ItemClass = LoadClass<UFGItemDescriptor>(nullptr, Path))
			{
				const int32 Amount = FMath::Max(1, FMath::RoundToInt(UFGItemDescriptor::GetStackSize(ItemClass) * FMath::Clamp(Ctx.Settings->Strength, 0.01f, 1.f)));
				Items.Add(FInventoryStack(Amount, ItemClass));
			}
		}
		if (Items.Num() == 0)
		{
			return false;
		}
		const FVector Location = Ctx.Player->GetActorLocation() + Ctx.Player->GetActorForwardVector() * 300.f;
		AFGCrate* Crate = nullptr;
		AFGItemPickup_Spawnable::SpawnInventoryCrate(Ctx.World, Items, Location, { Ctx.Player }, Crate, EFGCrateType::CT_DismantleCrate);
		if (!Crate)
		{
			return false;
		}
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} dropped a supply crate in front of you!"), TEXT("{0} сбрасывает тебе ящик с припасами!")), FText::FromString(Ctx.ViewerName)));
		return true;
	});

	Add(TEXT("sink"), false, [](const FChatEffectContext& Ctx)
	{
		AFGResourceSinkSubsystem* Sink = AFGResourceSinkSubsystem::Get(Ctx.World);
		if (!Sink)
		{
			return false;
		}
		const int32 Coupons = FMath::Max(1, Ctx.Settings->Amount);
		Sink->AddResourceSinkCoupons(Coupons, false);
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} gave you {1} AWESOME Sink coupons!"), TEXT("{0} дарит тебе {1} купонов AWESOME Sink!")),
			FText::FromString(Ctx.ViewerName), FText::AsNumber(Coupons)));
		return true;
	});

	auto SetTime = [](const FChatEffectContext& Ctx, float Hours, const FText& Message)
	{
		AFGTimeOfDaySubsystem* Time = AFGTimeOfDaySubsystem::Get(Ctx.World);
		if (!Time)
		{
			return false;
		}
		Time->SetDaySeconds(Hours * 3600.f);
		Ctx.Subsystem->Announce(Message);
		return true;
	};

	Add(TEXT("night"), true, [SetTime](const FChatEffectContext& Ctx)
	{
		return SetTime(Ctx, 0.f, FText::Format(Tr(TEXT("{0} turned off the sun!"), TEXT("{0} выключает солнце!")), FText::FromString(Ctx.ViewerName)));
	});

	Add(TEXT("day"), false, [SetTime](const FChatEffectContext& Ctx)
	{
		return SetTime(Ctx, 12.f, FText::Format(Tr(TEXT("{0} brought the sun back!"), TEXT("{0} возвращает солнце!")), FText::FromString(Ctx.ViewerName)));
	});

	Add(TEXT("sign"), false, [](const FChatEffectContext& Ctx)
	{
		FString Message = Ctx.Args.Left(80);
		if (Message.IsEmpty())
		{
			return false;
		}
		// A floating sign on the ground in front of the player
		FCollisionQueryParams Params(TEXT("ChatControlSign"), false, Ctx.Player);
		const FVector Probe = Ctx.Player->GetActorLocation() + Ctx.Player->GetActorForwardVector() * 400.f;
		FHitResult Ground;
		const FVector Location = Ctx.World->LineTraceSingleByChannel(Ground, Probe + FVector(0, 0, 300.f), Probe - FVector(0, 0, 1000.f), ECC_Visibility, Params)
			? Ground.ImpactPoint : Probe;
		AActor* Sign = Ctx.World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Location));
		if (!Sign)
		{
			return false;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Sign);
		Sign->SetRootComponent(Root);
		Root->RegisterComponent();
		Sign->SetActorLocation(Location);
		Ctx.Subsystem->AttachNameTag(Sign, Ctx.ViewerName, Ctx.Subsystem->GetNameColor(Ctx.ViewerName, FColor(250, 149, 73)), 240.f);
		Ctx.Subsystem->AttachNameTag(Sign, Message, FColor::White, 170.f);
		Sign->SetLifeSpan(FMath::Max(10.f, Ctx.Settings->Duration));
		Ctx.Subsystem->TrackSpawned(Sign);
		Ctx.Subsystem->TrackSign(Sign, Ctx.ViewerName);
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} left a sign: {1}"), TEXT("{0} оставляет табличку: {1}")), FText::FromString(Ctx.ViewerName), FText::FromString(Message)));
		return true;
	});

	Add(TEXT("pet"), false, [](const FChatEffectContext& Ctx)
	{
		return Ctx.Subsystem->SpawnPet(TEXT("pet:") + Ctx.ViewerName.ToLower(), Ctx.ViewerName);
	});

	Add(TEXT("ambush"), true, [](const FChatEffectContext& Ctx)
	{
		return SpawnCreaturesDelayed(Ctx, SmallCreaturePool,
			Tr(TEXT("{0} is sending an ambush: {1} enemies in {2} s!"), TEXT("{0} готовит засаду: {1} врагов через {2} с!")),
			Tr(TEXT("Ambush! {0} enemies from {1}!"), TEXT("Засада! {0} врагов от {1}!")));
	});

	Add(TEXT("boss"), true, [](const FChatEffectContext& Ctx)
	{
		return SpawnCreaturesDelayed(Ctx, BossCreaturePool,
			Tr(TEXT("{0} is summoning a BOSS in {2} s!"), TEXT("{0} призывает БОССА через {2} с!")),
			Tr(TEXT("The BOSS from {1} is here! Run!"), TEXT("БОСС от {1} уже здесь! Беги!")));
	});

	Add(TEXT("fly"), true, [](const FChatEffectContext& Ctx)
	{
		if (!Ctx.Player->GetCharacterMovement())
		{
			return false;
		}
		UChatControlSubsystem* Subsystem = Ctx.Subsystem;
		const float TargetZ = Ctx.Player->GetActorLocation().Z + FMath::Clamp(Ctx.Settings->Strength, 1.f, 50.f) * 100.f;
		// Pull towards the hover height every tick; walking is switched to falling so the lift can start
		Subsystem->StartTimedEffect(TEXT("fly"), Ctx.Settings->Duration,
			[Subsystem, TargetZ]
			{
				AFGCharacterPlayer* Player = Subsystem->FindPlayer();
				UCharacterMovementComponent* Movement = Player ? Player->GetCharacterMovement() : nullptr;
				if (!Movement)
				{
					return;
				}
				if (Movement->IsMovingOnGround())
				{
					Movement->SetMovementMode(MOVE_Falling);
				}
				Movement->Velocity.Z = FMath::Clamp((TargetZ - Player->GetActorLocation().Z) * 3.f, -400.f, 800.f);
			},
			[Subsystem]
			{
				// A soft landing instead of fall damage
				Subsystem->StartTimedEffect(TEXT("softfall"), 8.f,
					[Subsystem]
					{
						AFGCharacterPlayer* Player = Subsystem->FindPlayer();
						UCharacterMovementComponent* Movement = Player ? Player->GetCharacterMovement() : nullptr;
						if (Movement && Movement->Velocity.Z < -500.f)
						{
							Movement->Velocity.Z = -500.f;
						}
					},
					[] {});
			});
		Subsystem->Announce(FText::Format(Tr(TEXT("{0} lifted you into the air for {1} s!"), TEXT("{0} поднимает тебя в воздух на {1} с!")),
			FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
		return true;
	});

	auto Resize = [](const FChatEffectContext& Ctx, const FText& Message)
	{
		UChatControlSubsystem* Subsystem = Ctx.Subsystem;
		const float Scale = FMath::Clamp(Ctx.Settings->Strength, 0.2f, 4.f);
		Subsystem->StartTimedEffect(TEXT("size"), Ctx.Settings->Duration,
			[Subsystem, Scale]
			{
				if (AFGCharacterPlayer* Player = Subsystem->FindPlayer(); Player && !Player->GetActorScale3D().Equals(FVector(Scale)))
				{
					Player->SetActorScale3D(FVector(Scale));
				}
			},
			[Subsystem]
			{
				if (AFGCharacterPlayer* Player = Subsystem->FindPlayer())
				{
					Player->SetActorScale3D(FVector::OneVector);
				}
			});
		Subsystem->Announce(Message);
		return true;
	};
	Add(TEXT("shrink"), true, [Resize](const FChatEffectContext& Ctx)
	{
		return Resize(Ctx, FText::Format(Tr(TEXT("{0} shrank you for {1} s!"), TEXT("{0} уменьшает тебя на {1} с!")),
			FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
	});
	Add(TEXT("giant"), true, [Resize](const FChatEffectContext& Ctx)
	{
		return Resize(Ctx, FText::Format(Tr(TEXT("{0} made you a giant for {1} s!"), TEXT("{0} делает тебя великаном на {1} с!")),
			FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
	});

	Add(TEXT("hungry"), true, [](const FChatEffectContext& Ctx)
	{
		UFGHealthComponent* Health = Ctx.Player->GetHealthComponent();
		if (!Health)
		{
			return false;
		}
		UChatControlSubsystem* Subsystem = Ctx.Subsystem;
		const float Damage = FMath::Max(0.5f, Ctx.Settings->Strength);
		// Health only goes down: natural regeneration is pushed back. Eating is noticed by food leaving the player's
		// inventories (backpack and hand slot). Never kills: stops at 10%.
		TSharedRef<float> Allowed = MakeShared<float>(Health->GetCurrentHealth());
		TSharedRef<int32> Food = MakeShared<int32>(CountFood(Ctx.Player));
		TSharedRef<double> NextBite = MakeShared<double>(FPlatformTime::Seconds() + 2.0);
		TSharedRef<bool> bFed = MakeShared<bool>(false);
		Subsystem->StartTimedEffect(TEXT("hungry"), Ctx.Settings->Duration,
			[Subsystem, Damage, Allowed, Food, NextBite, bFed]
			{
				AFGCharacterPlayer* Player = Subsystem->FindPlayer();
				UFGHealthComponent* Health = Player ? Player->GetHealthComponent() : nullptr;
				if (*bFed || !Health || Health->IsDead())
				{
					return;
				}
				const int32 FoodNow = CountFood(Player);
				if (FoodNow < *Food)
				{
					*bFed = true;
					Subsystem->Announce(Tr(TEXT("You ate something - the hunger is gone."), TEXT("Ты поел(а) - голод прошёл.")));
					return;
				}
				*Food = FoodNow;
				const float Floor = Health->GetMaxHealth() * 0.1f;
				const double Now = FPlatformTime::Seconds();
				if (Now >= *NextBite)
				{
					*NextBite = Now + 2.0;
					*Allowed = FMath::Max(Floor, *Allowed - Damage);
				}
				const float Current = Health->GetCurrentHealth();
				if (Current > *Allowed + 0.01f)
				{
					Health->TakeDamage(Player, Current - *Allowed, GetDefault<UDamageType>(), nullptr, nullptr);
				}
				else
				{
					// Took damage from something else: hunger continues from there
					*Allowed = Current;
				}
			},
			[] {});
		Subsystem->Announce(FText::Format(Tr(TEXT("{0} made you starving! Eat something to stop it."), TEXT("{0} морит тебя голодом! Съешь что-нибудь, чтобы это прекратить.")),
			FText::FromString(Ctx.ViewerName)));
		return true;
	});

	Add(TEXT("inventory"), true, [](const FChatEffectContext& Ctx)
	{
		UFGInventoryComponent* Inventory = Ctx.Player->GetInventory();
		if (!Inventory || Inventory->IsEmpty())
		{
			return false;
		}
		const int32 Size = Inventory->GetSizeLinear();
		TArray<FInventoryStack> Slots;
		Slots.SetNum(Size);
		for (int32 i = 0; i < Size; ++i)
		{
			Inventory->GetStackFromIndex(i, Slots[i]);
		}
		for (int32 i = Size - 1; i > 0; --i)
		{
			Slots.Swap(i, FMath::RandRange(0, i));
		}
		for (int32 i = 0; i < Size; ++i)
		{
			Inventory->RemoveAllFromIndex(i);
		}
		for (int32 i = 0; i < Size; ++i)
		{
			if (Slots[i].HasItems())
			{
				Inventory->AddStackToIndex(i, Slots[i], true);
			}
		}
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} shuffled your inventory!"), TEXT("{0} перемешивает твой инвентарь!")), FText::FromString(Ctx.ViewerName)));
		return true;
	});

	Add(TEXT("drop"), true, [](const FChatEffectContext& Ctx)
	{
		UFGInventoryComponent* Inventory = Ctx.Player->GetInventory();
		if (!Inventory || Inventory->IsEmpty())
		{
			return false;
		}
		TArray<int32> Filled;
		for (int32 i = 0; i < Inventory->GetSizeLinear(); ++i)
		{
			FInventoryStack Stack;
			if (Inventory->GetStackFromIndex(i, Stack) && Stack.HasItems())
			{
				Filled.Add(i);
			}
		}
		// Amount = stacks that fall out; Strength = how far away the crate lands (m)
		TArray<FInventoryStack> Dropped;
		for (int32 n = 0; n < FMath::Max(1, Ctx.Settings->Amount) && Filled.Num() > 0; ++n)
		{
			const int32 Index = Filled[FMath::RandRange(0, Filled.Num() - 1)];
			Filled.Remove(Index);
			FInventoryStack Stack;
			Inventory->GetStackFromIndex(Index, Stack);
			Inventory->RemoveAllFromIndex(Index);
			Dropped.Add(Stack);
		}
		const float Distance = FMath::Max(3.f, Ctx.Settings->Strength) * 100.f;
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		FVector Location = Ctx.Player->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * FMath::FRandRange(Distance * 0.75f, Distance * 1.25f);
		FHitResult Ground;
		FCollisionQueryParams Params(TEXT("ChatControlDrop"), false, Ctx.Player);
		if (Ctx.World->LineTraceSingleByChannel(Ground, Location + FVector(0, 0, 3000.f), Location - FVector(0, 0, 6000.f), ECC_Visibility, Params))
		{
			Location = Ground.ImpactPoint + FVector(0, 0, 50.f);
		}
		AFGCrate* Crate = nullptr;
		AFGItemPickup_Spawnable::SpawnInventoryCrate(Ctx.World, Dropped, Location, { Ctx.Player }, Crate, EFGCrateType::CT_DismantleCrate);
		if (!Crate)
		{
			// Never lose the player's things: put them back
			for (const FInventoryStack& Stack : Dropped)
			{
				Inventory->AddStack(Stack, true);
			}
			return false;
		}
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} knocked {1} stacks out of your pockets! The crate landed about {2} m away."), TEXT("{0} вытряхивает из карманов стаков: {1}! Ящик улетел примерно на {2} м.")),
			FText::FromString(Ctx.ViewerName), FText::AsNumber(Dropped.Num()), FText::AsNumber(FMath::RoundToInt(FVector::Dist(Location, Ctx.Player->GetActorLocation()) / 100.f))));
		return true;
	});

	Add(TEXT("fog"), true, [](const FChatEffectContext& Ctx)
	{
		UChatControlSubsystem* Subsystem = Ctx.Subsystem;
		TWeakObjectPtr<UWorld> World = Ctx.World;
		const float Density = FMath::Clamp(Ctx.Settings->Strength, 0.01f, 1.f);
		// The atmosphere updates fog from biome curves, so it is overridden every tick and released at the end
		Subsystem->StartTimedEffect(TEXT("fog"), Ctx.Settings->Duration,
			[World, Density]
			{
				for (TObjectIterator<UExponentialHeightFogComponent> It; It; ++It)
				{
					if (It->GetWorld() == World.Get())
					{
						It->SetFogDensity(Density);
						It->SetFogHeightFalloff(0.001f);
						It->SetStartDistance(0.f);
					}
				}
			},
			[] {});
		Subsystem->Announce(FText::Format(Tr(TEXT("{0} brought thick fog for {1} s!"), TEXT("{0} напускает густой туман на {1} с!")),
			FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
		return true;
	});

	Add(TEXT("dance"), false, [](const FChatEffectContext& Ctx)
	{
		const float Radius = FMath::Max(5.f, Ctx.Settings->Strength) * 100.f;
		TArray<TWeakObjectPtr<AActor>> Frozen;
		for (TActorIterator<AFGCreature> It(Ctx.World); It; ++It)
		{
			if (FVector::Dist(It->GetActorLocation(), Ctx.Player->GetActorLocation()) < Radius)
			{
				Frozen.Add(*It);
			}
		}
		if (Frozen.Num() == 0)
		{
			Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} wanted to stop the creatures, but there are none around."), TEXT("{0} хотел(а) остановить существ, но рядом никого нет.")), FText::FromString(Ctx.ViewerName)));
			return false;
		}
		Ctx.Subsystem->StartTimedEffect(TEXT("dance"), Ctx.Settings->Duration,
			[Frozen]
			{
				for (const TWeakObjectPtr<AActor>& Actor : Frozen)
				{
					if (Actor.IsValid())
					{
						Actor->CustomTimeDilation = 0.001f;
					}
				}
			},
			[Frozen]
			{
				for (const TWeakObjectPtr<AActor>& Actor : Frozen)
				{
					if (Actor.IsValid())
					{
						Actor->CustomTimeDilation = 1.f;
					}
				}
			});
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} froze {1} creatures for {2} s! Run!"), TEXT("{0} замораживает существ ({1}) на {2} с! Беги!")),
			FText::FromString(Ctx.ViewerName), FText::AsNumber(Frozen.Num()), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
		return true;
	});

	Add(TEXT("heal"), false, [](const FChatEffectContext& Ctx)
	{
		UFGHealthComponent* Health = Ctx.Player->GetHealthComponent();
		if (!Health || Health->IsDead())
		{
			return false;
		}
		Health->Heal(Health->GetMaxHealth());
		Ctx.Subsystem->Announce(FText::Format(Tr(TEXT("{0} healed you!"), TEXT("{0} лечит тебя!")), FText::FromString(Ctx.ViewerName)));
		return true;
	});

	Add(TEXT("name"), false, [](const FChatEffectContext& Ctx)
	{
		return Ctx.Subsystem->NameBuilding(Ctx.ViewerKey, Ctx.ViewerName);
	});

	return Effects;
}

