#include "ChatEffects.h"
#include "ChatControl.h"
#include "ChatControlSettings.h"
#include "ChatControlSubsystem.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	bool FindSpawnPoint(UWorld* World, const AActor* Player, const FChatControlSettings& Settings, FVector& OutLocation)
	{
		const FVector Center = Player->GetActorLocation();
		FCollisionQueryParams Params(TEXT("ChatControlSpawn"), false, Player);
		for (int32 Attempt = 0; Attempt < 16; ++Attempt)
		{
			const float Angle = FMath::FRandRange(0.f, 2.f * PI);
			// Start close and widen the ring if nothing fits
			const float MinDistance = FMath::Max(1.f, Settings.SpawnDistanceMin) * 100.f;
			const float MaxDistance = FMath::Max(Settings.SpawnDistanceMin, Settings.SpawnDistanceMax) * 100.f;
			const float Distance = FMath::FRandRange(MinDistance, MaxDistance) + Attempt * 50.f;
			const FVector Probe = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance;

			// The spot must be visible from the player at chest height
			FHitResult Blocked;
			if (World->LineTraceSingleByChannel(Blocked, Center, Probe, ECC_Visibility, Params))
			{
				continue;
			}
			FHitResult Ground;
			if (World->LineTraceSingleByChannel(Ground, Probe, Probe - FVector(0, 0, 800.f), ECC_Visibility, Params))
			{
				OutLocation = Ground.ImpactPoint + FVector(0, 0, 120.f);
				return true;
			}
		}
		return false;
	}

	template <int32 N>
	int32 SpawnCreatures(UChatControlSubsystem* Subsystem, UWorld* World, const AFGCharacterPlayer* Player, const FChatControlSettings& Settings, const TCHAR* (&Pool)[N], int32 Count, const FString& ViewerName)
	{
		int32 Spawned = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			UClass* CreatureClass = LoadClass<APawn>(nullptr, Pool[FMath::RandRange(0, N - 1)]);
			FVector Location;
			if (!CreatureClass || !FindSpawnPoint(World, Player, Settings, Location))
			{
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
				Subsystem->AttachNameTag(Creature, ViewerName, FColor(255, 90, 60), 160.f);
			}
			UE_LOG(LogChatControl, Log, TEXT("Spawned %s %.0f m from player, enabled=%d, controller=%s"),
				*CreatureClass->GetName(), FVector::Dist(Location, Player->GetActorLocation()) / 100.f,
				FGCreature ? FGCreature->GetIsEnabled() : -1, *GetNameSafe(Creature->GetController()));
			++Spawned;
		}
		if (Spawned == 0)
		{
			UE_LOG(LogChatControl, Warning, TEXT("No creatures spawned (no ground found or classes failed to load)"));
		}
		return Spawned;
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
			UWorld* World = Subsystem->GetGameWorld();
			const AFGCharacterPlayer* Player = Subsystem->FindPlayer();
			if (!World || !Player)
			{
				return;
			}
			const int32 Spawned = SpawnCreatures(Subsystem, World, Player, Subsystem->GetSettings(), Pool, Count, Viewer);
			if (Spawned > 0)
			{
				Subsystem->Announce(FText::Format(Arrival, FText::AsNumber(Spawned), FText::FromString(Viewer)));
			}
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
		Ctx.Subsystem->AttachNameTag(Sign, Ctx.ViewerName, FColor(250, 149, 73), 220.f);
		Ctx.Subsystem->AttachNameTag(Sign, Message, FColor::White, 170.f);
		Sign->SetLifeSpan(FMath::Max(10.f, Ctx.Settings->Duration));
		Ctx.Subsystem->TrackSpawned(Sign);
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

	return Effects;
}

