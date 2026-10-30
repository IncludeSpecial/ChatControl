#include "ChatControlModConfig.h"
#include "ChatControl.h"
#include "ChatControlSettings.h"
#include "Configuration/ConfigManager.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyInteger.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "Configuration/Properties/ConfigPropertyString.h"
#include "Configuration/Properties/WidgetExtension/CP_Section.h"
#include "Internationalization/Culture.h"

namespace
{
	const TCHAR* WidgetPath = TEXT("/SML/Interface/UI/Menu/Mods/ConfigProperties/");

	/** Minimal two-language text until the mod gets a proper localization target */
	FText Tr(const TCHAR* English, const TCHAR* Russian)
	{
		const bool bRussian = FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName() == TEXT("ru");
		return FText::FromString(bRussian ? Russian : English);
	}

	/** SML's C++ property classes have no widgets; the BP_ConfigProperty* subclasses provide them */
	template <typename TProperty>
	TProperty* NewProperty(UObject* Outer, const TCHAR* BlueprintName, const FText& Name, const FText& Tooltip)
	{
		UClass* Class = LoadClass<TProperty>(nullptr, *FString::Printf(TEXT("%s%s.%s_C"), WidgetPath, BlueprintName, BlueprintName));
		if (!Class)
		{
			UE_LOG(LogChatControl, Warning, TEXT("Config widget class %s not found, falling back to native class"), BlueprintName);
			Class = TProperty::StaticClass();
		}
		TProperty* Property = NewObject<TProperty>(Outer, Class);
		Property->DisplayName = Name;
		Property->Tooltip = Tooltip;
		return Property;
	}

	/** The section widget defaults to a horizontal list without a header, which squeezes everything into one row */
	UConfigPropertySection* NewSection(UObject* Outer, const FText& Name, bool bHasHeader)
	{
		UConfigPropertySection* Section = NewProperty<UConfigPropertySection>(Outer, TEXT("BP_ConfigPropertySection"), Name, FText::GetEmpty());
		if (UCP_Section* Widget = Cast<UCP_Section>(Section))
		{
			Widget->WidgetType = ECP_SectionWidgetType::CPS_Vertical;
			Widget->HasHeader = bHasHeader;
			Widget->HeaderText = Name;
		}
		return Section;
	}

	UConfigPropertySection* AddSection(UConfigPropertySection* Parent, const FString& Key, const FText& Name)
	{
		UConfigPropertySection* Section = NewSection(Parent, Name, true);
		Parent->SectionProperties.Add(Key, Section);
		return Section;
	}

	void AddBool(UConfigPropertySection* Section, const FString& Key, bool Default, const FText& Name, const FText& Tooltip = FText::GetEmpty())
	{
		UConfigPropertyBool* Property = NewProperty<UConfigPropertyBool>(Section, TEXT("BP_ConfigPropertyBool"), Name, Tooltip);
		Property->DefaultValue = Property->Value = Default;
		Section->SectionProperties.Add(Key, Property);
	}

	void AddInt(UConfigPropertySection* Section, const FString& Key, int32 Default, const FText& Name, const FText& Tooltip = FText::GetEmpty())
	{
		UConfigPropertyInteger* Property = NewProperty<UConfigPropertyInteger>(Section, TEXT("BP_ConfigPropertyInteger"), Name, Tooltip);
		Property->DefaultValue = Property->Value = Default;
		Section->SectionProperties.Add(Key, Property);
	}

	void AddFloat(UConfigPropertySection* Section, const FString& Key, float Default, const FText& Name, const FText& Tooltip = FText::GetEmpty())
	{
		UConfigPropertyFloat* Property = NewProperty<UConfigPropertyFloat>(Section, TEXT("BP_ConfigPropertyFloat"), Name, Tooltip);
		Property->DefaultValue = Property->Value = Default;
		Section->SectionProperties.Add(Key, Property);
	}

	void AddString(UConfigPropertySection* Section, const FString& Key, const FString& Default, const FText& Name, const FText& Tooltip = FText::GetEmpty())
	{
		UConfigPropertyString* Property = NewProperty<UConfigPropertyString>(Section, TEXT("BP_ConfigPropertyString"), Name, Tooltip);
		Property->DefaultValue = Property->Value = Default;
		Section->SectionProperties.Add(Key, Property);
	}

	const UConfigPropertySection* GetSection(const UConfigPropertySection* Parent, const FString& Key)
	{
		const TObjectPtr<UConfigProperty>* Found = Parent ? Parent->SectionProperties.Find(Key) : nullptr;
		return Found ? Cast<UConfigPropertySection>(*Found) : nullptr;
	}

	/** Reads a value into Target; returns true if it differed */
	template <typename TProperty, typename TValue>
	bool Read(const UConfigPropertySection* Section, const FString& Key, TValue& Target)
	{
		const TObjectPtr<UConfigProperty>* Found = Section ? Section->SectionProperties.Find(Key) : nullptr;
		const TProperty* Property = Found ? Cast<TProperty>(*Found) : nullptr;
		if (!Property || Target == Property->Value)
		{
			return false;
		}
		Target = Property->Value;
		return true;
	}

	struct FEffectLabel
	{
		const TCHAR* Id;
		const TCHAR* English;
		const TCHAR* Russian;
	};

	const FEffectLabel EffectLabels[] = {
		{ TEXT("hi"),      TEXT("!hi - wave"),                   TEXT("!hi - помахать") },
		{ TEXT("gift"),    TEXT("!gift - basic resources"),      TEXT("!gift - базовые ресурсы") },
		{ TEXT("biggift"), TEXT("!biggift - advanced parts"),    TEXT("!biggift - продвинутые детали") },
		{ TEXT("jump"),    TEXT("!jump - super jump"),           TEXT("!jump - супер-прыжок") },
		{ TEXT("moon"),    TEXT("!moon - moon gravity"),         TEXT("!moon - лунная гравитация") },
		{ TEXT("heavy"),   TEXT("!heavy - heavy gravity"),       TEXT("!heavy - тяжёлая гравитация") },
		{ TEXT("ambush"),  TEXT("!ambush - enemy ambush"),       TEXT("!ambush - засада врагов") },
		{ TEXT("boss"),    TEXT("!boss - alpha monster"),        TEXT("!boss - альфа-монстр") },
	};
}

UChatControlModConfig::UChatControlModConfig()
{
	ConfigId = GetId();
	DisplayName = FText::FromString(TEXT("ChatControl"));
	Description = FText::FromString(TEXT("Twitch chat integration"));
}

FConfigId UChatControlModConfig::GetId()
{
	FConfigId Id;
	Id.ModReference = TEXT("ChatControl");
	return Id;
}

void UChatControlModConfig::Register(UConfigManager* ConfigManager, const FChatControlSettings& Defaults)
{
	UChatControlModConfig* CDO = GetMutableDefault<UChatControlModConfig>();
	CDO->DisplayName = FText::FromString(TEXT("ChatControl"));
	CDO->Description = Tr(TEXT("Let your Twitch chat play with you"), TEXT("Твой чат на Twitch играет вместе с тобой"));

	UConfigPropertySection* Root = NewSection(CDO, FText::GetEmpty(), false);
	CDO->RootSection = Root;

	// --- General ---
	UConfigPropertySection* General = AddSection(Root, TEXT("General"), Tr(TEXT("General"), TEXT("Общее")));
	AddBool(General, TEXT("Enabled"), Defaults.bEnabled,
		Tr(TEXT("Enabled"), TEXT("Включено")),
		Tr(TEXT("Turn chat effects on or off"), TEXT("Включить или выключить эффекты от чата")));
	AddString(General, TEXT("TwitchChannel"), Defaults.TwitchChannel,
		Tr(TEXT("Twitch channel"), TEXT("Канал Twitch")),
		Tr(TEXT("Channel name from the URL, e.g. swittyids"), TEXT("Имя канала из ссылки, например swittyids")));
	AddInt(General, TEXT("ChaosBudgetPerMinute"), Defaults.ChaosBudgetPerMinute,
		Tr(TEXT("Chaos budget per minute"), TEXT("Бюджет хаоса в минуту")),
		Tr(TEXT("How many chaos points harmful effects may use per minute. Extra effects wait in a queue."),
		   TEXT("Сколько очков хаоса вредные эффекты могут потратить за минуту. Остальные ждут в очереди.")));
	AddInt(General, TEXT("DeathProtectionSeconds"), FMath::RoundToInt(Defaults.DeathProtectionSeconds),
		Tr(TEXT("Death protection (s)"), TEXT("Защита после смерти (с)")),
		Tr(TEXT("No harmful effects for this long after you die"), TEXT("Сколько секунд после смерти вредные эффекты не срабатывают")));

	// --- Auto-Chaos ---
	UConfigPropertySection* AutoChaos = AddSection(Root, TEXT("AutoChaos"), Tr(TEXT("Auto-Chaos"), TEXT("Автохаос")));
	AddBool(AutoChaos, TEXT("Enabled"), Defaults.bAutoChaos,
		Tr(TEXT("Enabled"), TEXT("Включено")),
		Tr(TEXT("ADA triggers a random effect when chat is quiet. Works even without viewers."),
		   TEXT("ADA запускает случайный эффект, когда чат молчит. Работает даже без зрителей.")));
	AddInt(AutoChaos, TEXT("IdleMinutes"), FMath::RoundToInt(Defaults.AutoChaosIdleSeconds / 60.f),
		Tr(TEXT("Quiet time (min)"), TEXT("Время тишины (мин)")),
		Tr(TEXT("Minutes without effects before ADA steps in"), TEXT("Сколько минут без эффектов до вмешательства ADA")));

	// --- Economy ---
	UConfigPropertySection* Economy = AddSection(Root, TEXT("Economy"), Tr(TEXT("FICSIT Chips"), TEXT("Чипы FICSIT")));
	AddInt(Economy, TEXT("ChipsPerInterval"), Defaults.ChipsPerInterval,
		Tr(TEXT("Chips for activity"), TEXT("Чипы за активность")),
		Tr(TEXT("Given to every active viewer each payout"), TEXT("Сколько получает каждый активный зритель при начислении")));
	AddInt(Economy, TEXT("IntervalMinutes"), FMath::RoundToInt(Defaults.ChipsIntervalSeconds / 60.f),
		Tr(TEXT("Payout interval (min)"), TEXT("Интервал начисления (мин)")));
	AddInt(Economy, TEXT("FirstMessageBonus"), Defaults.FirstMessageBonus,
		Tr(TEXT("First message bonus"), TEXT("Бонус за первое сообщение")));
	AddInt(Economy, TEXT("ChipsPerBit"), Defaults.ChipsPerBit,
		Tr(TEXT("Chips per bit"), TEXT("Чипов за 1 бит")));
	AddInt(Economy, TEXT("ChipsPerSub"), Defaults.ChipsPerSub,
		Tr(TEXT("Chips per sub"), TEXT("Чипов за подписку")));
	AddInt(Economy, TEXT("CostScalePercent"), FMath::RoundToInt(Defaults.CostScalePerViewer * 100.f),
		Tr(TEXT("Price growth per viewer (%)"), TEXT("Рост цен за зрителя (%)")),
		Tr(TEXT("Prices grow by this percent for each active viewer above one"), TEXT("На сколько процентов растут цены за каждого активного зрителя сверх одного")));
	AddFloat(Economy, TEXT("MaxCostMultiplier"), Defaults.MaxCostMultiplier,
		Tr(TEXT("Max price multiplier"), TEXT("Макс. множитель цен")));

	// --- Effects ---
	UConfigPropertySection* Effects = AddSection(Root, TEXT("Effects"), Tr(TEXT("Effects"), TEXT("Эффекты")));
	for (const FEffectLabel& Label : EffectLabels)
	{
		const FChatEffectSettings* Effect = Defaults.Effects.Find(Label.Id);
		if (!Effect)
		{
			continue;
		}
		UConfigPropertySection* Section = AddSection(Effects, Label.Id, Tr(Label.English, Label.Russian));
		AddBool(Section, TEXT("Enabled"), Effect->bEnabled, Tr(TEXT("Enabled"), TEXT("Включено")));
		AddInt(Section, TEXT("Cost"), Effect->Cost,
			Tr(TEXT("Price (chips)"), TEXT("Цена (чипы)")),
			Tr(TEXT("0 = free"), TEXT("0 = бесплатно")));
		AddInt(Section, TEXT("ViewerCooldown"), FMath::RoundToInt(Effect->ViewerCooldown),
			Tr(TEXT("Cooldown per viewer (s)"), TEXT("Кулдаун на зрителя (с)")));
		if (Effect->Duration > 0.f)
		{
			AddInt(Section, TEXT("Duration"), FMath::RoundToInt(Effect->Duration),
				Tr(TEXT("Duration (s)"), TEXT("Длительность (с)")));
		}
		if (Effect->Amount > 0)
		{
			AddInt(Section, TEXT("Amount"), Effect->Amount,
				Tr(TEXT("Creatures"), TEXT("Количество существ")));
		}
	}

	ConfigManager->RegisterModConfiguration(StaticClass());
}

bool UChatControlModConfig::ApplyTo(UConfigManager* ConfigManager, FChatControlSettings& Settings)
{
	const UConfigPropertySection* Root = ConfigManager->GetConfigurationRootSection(GetId());
	if (!Root)
	{
		return false;
	}
	bool bChanged = false;

	const UConfigPropertySection* General = GetSection(Root, TEXT("General"));
	bChanged |= Read<UConfigPropertyBool>(General, TEXT("Enabled"), Settings.bEnabled);
	bChanged |= Read<UConfigPropertyString>(General, TEXT("TwitchChannel"), Settings.TwitchChannel);
	bChanged |= Read<UConfigPropertyInteger>(General, TEXT("ChaosBudgetPerMinute"), Settings.ChaosBudgetPerMinute);
	int32 DeathProtection = FMath::RoundToInt(Settings.DeathProtectionSeconds);
	if (Read<UConfigPropertyInteger>(General, TEXT("DeathProtectionSeconds"), DeathProtection))
	{
		Settings.DeathProtectionSeconds = DeathProtection;
		bChanged = true;
	}

	const UConfigPropertySection* AutoChaos = GetSection(Root, TEXT("AutoChaos"));
	bChanged |= Read<UConfigPropertyBool>(AutoChaos, TEXT("Enabled"), Settings.bAutoChaos);
	int32 IdleMinutes = FMath::RoundToInt(Settings.AutoChaosIdleSeconds / 60.f);
	if (Read<UConfigPropertyInteger>(AutoChaos, TEXT("IdleMinutes"), IdleMinutes))
	{
		Settings.AutoChaosIdleSeconds = FMath::Max(1, IdleMinutes) * 60.f;
		bChanged = true;
	}

	const UConfigPropertySection* Economy = GetSection(Root, TEXT("Economy"));
	bChanged |= Read<UConfigPropertyInteger>(Economy, TEXT("ChipsPerInterval"), Settings.ChipsPerInterval);
	int32 IntervalMinutes = FMath::RoundToInt(Settings.ChipsIntervalSeconds / 60.f);
	if (Read<UConfigPropertyInteger>(Economy, TEXT("IntervalMinutes"), IntervalMinutes))
	{
		Settings.ChipsIntervalSeconds = FMath::Max(1, IntervalMinutes) * 60.f;
		bChanged = true;
	}
	bChanged |= Read<UConfigPropertyInteger>(Economy, TEXT("FirstMessageBonus"), Settings.FirstMessageBonus);
	bChanged |= Read<UConfigPropertyInteger>(Economy, TEXT("ChipsPerBit"), Settings.ChipsPerBit);
	bChanged |= Read<UConfigPropertyInteger>(Economy, TEXT("ChipsPerSub"), Settings.ChipsPerSub);
	int32 CostScalePercent = FMath::RoundToInt(Settings.CostScalePerViewer * 100.f);
	if (Read<UConfigPropertyInteger>(Economy, TEXT("CostScalePercent"), CostScalePercent))
	{
		Settings.CostScalePerViewer = CostScalePercent / 100.f;
		bChanged = true;
	}
	bChanged |= Read<UConfigPropertyFloat>(Economy, TEXT("MaxCostMultiplier"), Settings.MaxCostMultiplier);

	const UConfigPropertySection* Effects = GetSection(Root, TEXT("Effects"));
	for (auto& Pair : Settings.Effects)
	{
		const UConfigPropertySection* Section = GetSection(Effects, Pair.Key);
		if (!Section)
		{
			continue;
		}
		FChatEffectSettings& Effect = Pair.Value;
		bChanged |= Read<UConfigPropertyBool>(Section, TEXT("Enabled"), Effect.bEnabled);
		bChanged |= Read<UConfigPropertyInteger>(Section, TEXT("Cost"), Effect.Cost);
		int32 Cooldown = FMath::RoundToInt(Effect.ViewerCooldown);
		if (Read<UConfigPropertyInteger>(Section, TEXT("ViewerCooldown"), Cooldown))
		{
			Effect.ViewerCooldown = Cooldown;
			bChanged = true;
		}
		int32 Duration = FMath::RoundToInt(Effect.Duration);
		if (Read<UConfigPropertyInteger>(Section, TEXT("Duration"), Duration))
		{
			Effect.Duration = Duration;
			bChanged = true;
		}
		bChanged |= Read<UConfigPropertyInteger>(Section, TEXT("Amount"), Effect.Amount);
	}

	return bChanged;
}
