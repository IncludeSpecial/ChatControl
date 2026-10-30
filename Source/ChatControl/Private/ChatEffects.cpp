#include "ChatEffects.h"
#include "ChatControl.h"
#include "ChatControlSettings.h"
#include "ChatControlSubsystem.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	UCharacterMovementComponent* GetMovement(const FChatEffectContext& Ctx)
	{
		return Ctx.Player ? Ctx.Player->GetCharacterMovement() : nullptr;
	}

	/** Multiplies a movement property for the effect duration. Effects sharing a slot replace each other instead of stacking. */
	bool ScaleMovementTemporarily(const FChatEffectContext& Ctx, const FString& Slot, float UCharacterMovementComponent::* Property, float Multiplier, const FText& Message)
	{
		UCharacterMovementComponent* Movement = GetMovement(Ctx);
		if (!Movement)
		{
			return false;
		}
		TWeakObjectPtr<UCharacterMovementComponent> WeakMovement = Movement;
		// Read the original value when applying: the previous effect in this slot is reverted first
		TSharedRef<float> Original = MakeShared<float>(0.f);
		Ctx.Subsystem->StartTimedEffect(Slot, Ctx.Settings->Duration,
			[WeakMovement, Property, Original, Multiplier]
			{
				if (WeakMovement.IsValid())
				{
					*Original = WeakMovement.Get()->*Property;
					WeakMovement.Get()->*Property = *Original * Multiplier;
				}
			},
			[WeakMovement, Property, Original]
			{
				if (WeakMovement.IsValid()) { WeakMovement.Get()->*Property = *Original; }
			});
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
		return ScaleMovementTemporarily(Ctx, TEXT("jump"), &UCharacterMovementComponent::JumpZVelocity, 2.5f,
			FText::Format(LOCTEXT("Jump", "{0} gave you super jump for {1} s!"), FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
	});

	Add(TEXT("moon"), true, [](const FChatEffectContext& Ctx)
	{
		return ScaleMovementTemporarily(Ctx, TEXT("gravity"), &UCharacterMovementComponent::GravityScale, 0.25f,
			FText::Format(LOCTEXT("Moon", "{0} switched on moon gravity for {1} s!"), FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
	});

	Add(TEXT("heavy"), true, [](const FChatEffectContext& Ctx)
	{
		return ScaleMovementTemporarily(Ctx, TEXT("gravity"), &UCharacterMovementComponent::GravityScale, 3.f,
			FText::Format(LOCTEXT("Heavy", "{0} made you heavy for {1} s!"), FText::FromString(Ctx.ViewerName), FText::AsNumber(FMath::RoundToInt(Ctx.Settings->Duration))));
	});

	return Effects;
}

#undef LOCTEXT_NAMESPACE
