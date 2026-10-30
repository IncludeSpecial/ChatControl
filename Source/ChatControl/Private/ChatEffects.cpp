#include "ChatEffects.h"
#include "ChatControl.h"
#include "ChatControlSettings.h"
#include "ChatControlSubsystem.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Creature/FGCreature.h"

#define LOCTEXT_NAMESPACE "ChatControl"

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

	/** Spawned creatures despawn after this long, so chat can't flood the save with enemies */
	constexpr float CreatureLifeSpan = 300.f;

	/**
	 * Finds ground on a ring around the player at roughly the player's height and in line of sight,
	 * so creatures don't end up on tree tops, roofs or behind walls.
	 */
	bool FindSpawnPoint(UWorld* World, const AActor* Player, FVector& OutLocation)
	{
		const FVector Center = Player->GetActorLocation();
		FCollisionQueryParams Params(TEXT("ChatControlSpawn"), false, Player);
		for (int32 Attempt = 0; Attempt < 16; ++Attempt)
		{
			const float Angle = FMath::FRandRange(0.f, 2.f * PI);
			// Start close and widen the ring if nothing fits
			const float Distance = FMath::FRandRange(600.f, 900.f) + Attempt * 50.f;
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
	int32 SpawnCreatures(const FChatEffectContext& Ctx, const TCHAR* (&Pool)[N], int32 Count)
	{
		int32 Spawned = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			UClass* CreatureClass = LoadClass<APawn>(nullptr, Pool[FMath::RandRange(0, N - 1)]);
			FVector Location;
			if (!CreatureClass || !FindSpawnPoint(Ctx.World, Ctx.Player, Location))
			{
				continue;
			}
			// Face the player so they notice them and aggro quickly
			const FRotator Facing((Ctx.Player->GetActorLocation() - Location).GetSafeNormal2D().Rotation());
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
			APawn* Creature = Ctx.World->SpawnActor<APawn>(CreatureClass, Location, Facing, Params);
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
			Creature->SetLifeSpan(CreatureLifeSpan);
			UE_LOG(LogChatControl, Log, TEXT("Spawned %s %.0f m from player, enabled=%d, controller=%s"),
				*CreatureClass->GetName(), FVector::Dist(Location, Ctx.Player->GetActorLocation()) / 100.f,
				FGCreature ? FGCreature->GetIsEnabled() : -1, *GetNameSafe(Creature->GetController()));
			++Spawned;
		}
		if (Spawned == 0)
		{
			UE_LOG(LogChatControl, Warning, TEXT("No creatures spawned (no ground found or classes failed to load)"));
		}
		return Spawned;
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
		const int32 Amount = FMath::Max(1, FMath::RoundToInt(UFGItemDescriptor::GetStackSize(ItemClass) * StackFraction));
		const int32 Added = Inventory->AddStack(FInventoryStack(Amount, ItemClass), true);
		if (Added <= 0)
		{
			Ctx.Subsystem->Announce(FText::Format(LOCTEXT("GiftInventoryFull", "{0} tried to send you a gift, but your inventory is full!"), FText::FromString(Ctx.ViewerName)));
			return false;
		}
		Ctx.Subsystem->Announce(FText::Format(LOCTEXT("Gift", "{0} sent you {1} x {2}!"),
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
		Ctx.Subsystem->Announce(FText::Format(LOCTEXT("Hi", "{0} waves at you!"), FText::FromString(Ctx.ViewerName)));
		return true;
	});

	Add(TEXT("gift"), false, [](const FChatEffectContext& Ctx)
	{
		return GiveRandomItem(Ctx, BasicGiftPool, 0.5f);
	});

	Add(TEXT("biggift"), false, [](const FChatEffectContext& Ctx)
	{
		return GiveRandomItem(Ctx, AdvancedGiftPool, 0.5f);
	});

	Add(TEXT("jump"), true, [](const FChatEffectContext& Ctx)
	{
		return BoostJumpsTemporarily(Ctx, 2.5f,
			FText::Format(LOCTEXT("Jump", "{0} gave you super jump for {1} s!"), FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
	});

	Add(TEXT("moon"), true, [](const FChatEffectContext& Ctx)
	{
		return ScaleGravityTemporarily(Ctx, 0.25f,
			FText::Format(LOCTEXT("Moon", "{0} switched on moon gravity for {1} s!"), FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
	});

	Add(TEXT("heavy"), true, [](const FChatEffectContext& Ctx)
	{
		return ScaleGravityTemporarily(Ctx, 3.f,
			FText::Format(LOCTEXT("Heavy", "{0} made you heavy for {1} s!"), FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
	});

	Add(TEXT("ambush"), true, [](const FChatEffectContext& Ctx)
	{
		const int32 Spawned = SpawnCreatures(Ctx, SmallCreaturePool, FMath::Max(1, Ctx.Settings->Amount));
		if (Spawned > 0)
		{
			Ctx.Subsystem->Announce(FText::Format(LOCTEXT("Ambush", "{0} sent an ambush: {1} enemies incoming!"), FText::FromString(Ctx.ViewerName), FText::AsNumber(Spawned)));
		}
		return Spawned > 0;
	});

	Add(TEXT("boss"), true, [](const FChatEffectContext& Ctx)
	{
		const int32 Spawned = SpawnCreatures(Ctx, BossCreaturePool, FMath::Max(1, Ctx.Settings->Amount));
		if (Spawned > 0)
		{
			Ctx.Subsystem->Announce(FText::Format(LOCTEXT("Boss", "{0} summoned a BOSS! Run!"), FText::FromString(Ctx.ViewerName)));
		}
		return Spawned > 0;
	});

	return Effects;
}

#undef LOCTEXT_NAMESPACE
