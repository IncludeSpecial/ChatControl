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
#include "ChatText.h"

using ChatText::Tr;

namespace
{
	const TCHAR* WidgetPath = TEXT("/SML/Interface/UI/Menu/Mods/ConfigProperties/");

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

	/**
	 * The section widget defaults to a horizontal list without a header.
	 * Groups use a vertical list; each effect is a horizontal row so all its options sit on one line.
	 */
	UConfigPropertySection* NewSection(UObject* Outer, const FText& Name, bool bHasHeader, bool bHorizontal = false, const FText& Tooltip = FText::GetEmpty())
	{
		UConfigPropertySection* Section = NewProperty<UConfigPropertySection>(Outer, TEXT("BP_ConfigPropertySection"), Name, Tooltip);
		if (UCP_Section* Widget = Cast<UCP_Section>(Section))
		{
			Widget->WidgetType = bHorizontal ? ECP_SectionWidgetType::CPS_Horizontal : ECP_SectionWidgetType::CPS_Vertical;
			Widget->HasHeader = bHasHeader;
			Widget->HeaderText = Name;
		}
		return Section;
	}

	UConfigPropertySection* AddSection(UConfigPropertySection* Parent, const FString& Key, const FText& Name, bool bHorizontal = false, const FText& Tooltip = FText::GetEmpty())
	{
		UConfigPropertySection* Section = NewSection(Parent, Name, true, bHorizontal, Tooltip);
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

	/** Follows a "A/B/C" path of section keys */
	const UConfigPropertySection* FindPath(const UConfigPropertySection* Root, const FString& Path)
	{
		TArray<FString> Keys;
		Path.ParseIntoArray(Keys, TEXT("/"));
		const UConfigPropertySection* Section = Root;
		for (const FString& Key : Keys)
		{
			Section = GetSection(Section, Key);
		}
		return Section;
	}

	/** Reads an integer field shown in Scale units (60 = minutes) into a float of seconds */
	bool ReadSeconds(const UConfigPropertySection* Section, const FString& Key, float& Target, float Scale = 1.f, float Min = 0.f)
	{
		int32 Value = FMath::RoundToInt(Target / Scale);
		if (!Read<UConfigPropertyInteger>(Section, Key, Value))
		{
			return false;
		}
		Target = FMath::Max(Min, Value * Scale);
		return true;
	}

	struct FEffectLabel
	{
		const TCHAR* Id;
		const TCHAR* Category;
		const TCHAR* English;
		const TCHAR* Russian;
	};

	const FEffectLabel EffectLabels[] = {
		{ TEXT("hi"),      TEXT("Free"),      TEXT("!hi - wave"),                TEXT("!hi - помахать") },
		{ TEXT("gift"),    TEXT("Free"),      TEXT("!gift - basic resources"),   TEXT("!gift - базовые ресурсы") },
		{ TEXT("biggift"), TEXT("Gifts"),     TEXT("!biggift - advanced parts"), TEXT("!biggift - продвинутые детали") },
		{ TEXT("jump"),    TEXT("Movement"),  TEXT("!jump - super jump"),        TEXT("!jump - супер-прыжок") },
		{ TEXT("moon"),    TEXT("Movement"),  TEXT("!moon - moon gravity"),      TEXT("!moon - лунная гравитация") },
		{ TEXT("heavy"),   TEXT("Movement"),  TEXT("!heavy - heavy gravity"),    TEXT("!heavy - тяжёлая гравитация") },
		{ TEXT("speed"),   TEXT("Movement"),  TEXT("!speed - fast-forward"),     TEXT("!speed - ускорение") },
		{ TEXT("slow"),    TEXT("Movement"),  TEXT("!slow - slow motion"),       TEXT("!slow - замедление") },
		{ TEXT("fov"),     TEXT("Movement"),  TEXT("!fov - camera warp"),        TEXT("!fov - искривление камеры") },
		{ TEXT("teleport"),TEXT("Movement"),  TEXT("!teleport - random teleport (strength = meters)"), TEXT("!teleport - случайный телепорт (сила = метры)") },
		{ TEXT("hub"),     TEXT("Movement"),  TEXT("!hub - back to the HUB"),    TEXT("!hub - обратно к ХАБу") },
		{ TEXT("strike"),  TEXT("Factory"),   TEXT("!strike - machines stop"),   TEXT("!strike - забастовка машин") },
		{ TEXT("underclock"), TEXT("Factory"), TEXT("!underclock - machines slow down (strength = speed)"), TEXT("!underclock - замедление машин (сила = скорость)") },
		{ TEXT("blackout"), TEXT("Factory"),  TEXT("!blackout - generators down"), TEXT("!blackout - генераторы стоят") },
		{ TEXT("rain"),    TEXT("Gifts"),     TEXT("!rain - supply crate"),      TEXT("!rain - ящик с припасами") },
		{ TEXT("sink"),    TEXT("Gifts"),     TEXT("!sink - AWESOME Sink coupons"), TEXT("!sink - купоны AWESOME Sink") },
		{ TEXT("night"),   TEXT("World"),     TEXT("!night - midnight"),         TEXT("!night - полночь") },
		{ TEXT("day"),     TEXT("World"),     TEXT("!day - noon"),               TEXT("!day - полдень") },
		{ TEXT("sign"),    TEXT("Viewers"),   TEXT("!sign <text> - floating sign (duration = how long it stays)"), TEXT("!sign <текст> - табличка (длительность = сколько висит)") },
		{ TEXT("pet"),     TEXT("Viewers"),   TEXT("!pet - named pet"),          TEXT("!pet - именной питомец") },
		{ TEXT("ambush"),  TEXT("Creatures"), TEXT("!ambush - enemy ambush"),    TEXT("!ambush - засада врагов") },
		{ TEXT("boss"),    TEXT("Creatures"), TEXT("!boss - alpha monster"),     TEXT("!boss - альфа-монстр") },
	};

	FText CategoryName(const FString& Category)
	{
		if (Category == TEXT("Free"))     return Tr(TEXT("Free commands"), TEXT("Бесплатные команды"));
		if (Category == TEXT("Gifts"))    return Tr(TEXT("Paid gifts"), TEXT("Подарки за чипы"));
		if (Category == TEXT("Movement")) return Tr(TEXT("Chaos: player"), TEXT("Хаос: игрок"));
		if (Category == TEXT("Factory"))  return Tr(TEXT("Chaos: factory"), TEXT("Хаос: фабрика"));
		if (Category == TEXT("World"))    return Tr(TEXT("World"), TEXT("Мир"));
		if (Category == TEXT("Viewers"))  return Tr(TEXT("Viewers in the world"), TEXT("Зрители в мире"));
		return Tr(TEXT("Chaos: creatures"), TEXT("Хаос: существа"));
	}
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

	// --- Main: what a streamer sets once ---
	UConfigPropertySection* Main = AddSection(Root, TEXT("Main"), Tr(TEXT("Main"), TEXT("Основное")));
	AddBool(Main, TEXT("Enabled"), Defaults.bEnabled,
		Tr(TEXT("Mod enabled"), TEXT("Мод включён")),
		Tr(TEXT("Turn chat effects on or off"), TEXT("Включить или выключить эффекты от чата")));
	AddString(Main, TEXT("TwitchChannel"), Defaults.TwitchChannel,
		Tr(TEXT("Twitch channel"), TEXT("Канал Twitch")),
		Tr(TEXT("Channel name from the URL, e.g. swittyids"), TEXT("Имя канала из ссылки, например swittyids")));
	AddString(Main, TEXT("Language"), Defaults.Language,
		Tr(TEXT("Language (auto / en / ru)"), TEXT("Язык (auto / en / ru)")),
		Tr(TEXT("auto = same as the game. The settings page switches after restart."),
		   TEXT("auto = как в игре. Страница настроек переключится после перезапуска.")));

	AddInt(Main, TEXT("Difficulty"), Defaults.Difficulty,
		Tr(TEXT("Difficulty (1-3)"), TEXT("Сложность (1-3)")),
		Tr(TEXT("1 = easy: prices x1.5, half the creatures. 2 = normal. 3 = hardcore: prices x0.6, double creatures."),
		   TEXT("1 = лёгкая: цены x1.5, существ вдвое меньше. 2 = обычная. 3 = хардкор: цены x0.6, существ вдвое больше.")));

	// --- YouTube ---
	UConfigPropertySection* YouTube = AddSection(Root, TEXT("YouTube"), (Tr(TEXT("YouTube chat"), TEXT("Чат YouTube"))));
	AddString(YouTube, TEXT("ApiKey"), Defaults.YouTubeApiKey,
		Tr(TEXT("API key"), TEXT("API-ключ")),
		Tr(TEXT("YouTube Data API v3 key from console.cloud.google.com. Empty = YouTube off."),
		   TEXT("Ключ YouTube Data API v3 из console.cloud.google.com. Пусто = YouTube выключен.")));
	AddString(YouTube, TEXT("Channel"), Defaults.YouTubeChannel,
		Tr(TEXT("Channel or stream"), TEXT("Канал или трансляция")),
		Tr(TEXT("Channel id (UC...) or link - the mod finds the live stream itself. Or a link to the stream."),
		   TEXT("ID канала (UC...) или ссылка на него - мод сам найдёт эфир. Или ссылка на трансляцию.")));
	AddInt(YouTube, TEXT("PollSeconds"), FMath::RoundToInt(Defaults.YouTubePollSeconds),
		Tr(TEXT("Check chat every (s)"), TEXT("Проверять чат каждые (с)")),
		Tr(TEXT("Lower = faster, but the free daily quota runs out sooner. 10 s is about 5 hours of streaming."),
		   TEXT("Меньше = быстрее, но бесплатная дневная квота кончится раньше. 10 с - примерно 5 часов эфира.")));
	AddInt(YouTube, TEXT("ChipsPerSuperChatUnit"), Defaults.ChipsPerSuperChatUnit,
		Tr(TEXT("Chips per 1 currency unit of Super Chat"), TEXT("Чипов за 1 единицу валюты Super Chat")));

	// --- Chips ---
	UConfigPropertySection* Economy = AddSection(Root, TEXT("Economy"), Tr(TEXT("FICSIT Chips"), TEXT("Чипы FICSIT")));
	AddInt(Economy, TEXT("ChipsPerInterval"), Defaults.ChipsPerInterval,
		Tr(TEXT("Chips for activity"), TEXT("Чипы за активность")),
		Tr(TEXT("Given to every viewer who chatted recently, each payout"), TEXT("Каждому, кто недавно писал в чат, при каждом начислении")));
	AddInt(Economy, TEXT("IntervalMinutes"), FMath::RoundToInt(Defaults.ChipsIntervalSeconds / 60.f),
		Tr(TEXT("Payout every (min)"), TEXT("Начислять каждые (мин)")));
	AddInt(Economy, TEXT("FirstMessageBonus"), Defaults.FirstMessageBonus,
		Tr(TEXT("First message bonus"), TEXT("Бонус за первое сообщение")));
	AddInt(Economy, TEXT("ChipsPerBit"), Defaults.ChipsPerBit,
		Tr(TEXT("Chips per bit"), TEXT("Чипов за 1 бит")));
	AddInt(Economy, TEXT("ChipsPerSub"), Defaults.ChipsPerSub,
		Tr(TEXT("Chips per sub"), TEXT("Чипов за подписку")));

	// --- Effects: one row per effect, grouped by kind ---
	UConfigPropertySection* Effects = AddSection(Root, TEXT("Effects"), Tr(TEXT("Effects"), TEXT("Эффекты")));
	TMap<FString, UConfigPropertySection*> Categories;
	for (const FEffectLabel& Label : EffectLabels)
	{
		const FChatEffectSettings* Effect = Defaults.Effects.Find(Label.Id);
		if (!Effect)
		{
			continue;
		}
		UConfigPropertySection*& Category = Categories.FindOrAdd(Label.Category);
		if (!Category)
		{
			Category = AddSection(Effects, Label.Category, CategoryName(Label.Category));
		}
		UConfigPropertySection* Row = AddSection(Category, Label.Id, Tr(Label.English, Label.Russian), true);
		AddBool(Row, TEXT("Enabled"), Effect->bEnabled, Tr(TEXT("On"), TEXT("Вкл.")));
		AddInt(Row, TEXT("Cost"), Effect->Cost,
			Tr(TEXT("Price"), TEXT("Цена")),
			Tr(TEXT("In chips, 0 = free. Grows with the number of active viewers."), TEXT("В чипах, 0 = бесплатно. Растёт с числом активных зрителей.")));
		AddInt(Row, TEXT("ViewerCooldown"), FMath::RoundToInt(Effect->ViewerCooldown),
			Tr(TEXT("Cooldown (s)"), TEXT("Кулдаун (с)")),
			Tr(TEXT("How often one viewer can use it"), TEXT("Как часто один зритель может это запускать")));
		if (Effect->Duration > 0.f)
		{
			AddInt(Row, TEXT("Duration"), FMath::RoundToInt(Effect->Duration), Tr(TEXT("Duration (s)"), TEXT("Длительность (с)")));
		}
		if (Effect->Strength > 0.f)
		{
			AddFloat(Row, TEXT("Strength"), Effect->Strength,
				Tr(TEXT("Strength"), TEXT("Сила")),
				Tr(TEXT("Jump/gravity multiplier, or share of a stack for gifts (0.5 = half)"),
				   TEXT("Множитель прыжка/гравитации или доля стака для подарков (0.5 = половина)")));
		}
		if (Effect->Amount > 0)
		{
			AddInt(Row, TEXT("Amount"), Effect->Amount, Tr(TEXT("Count"), TEXT("Количество")));
		}
		if (Effect->DelayMax > 0.f || Effect->Amount > 0)
		{
			AddFloat(Row, TEXT("DelayMin"), Effect->DelayMin,
				Tr(TEXT("Delay from (s)"), TEXT("Задержка от (с)")),
				Tr(TEXT("Time between the warning and the spawn"), TEXT("Время между предупреждением и появлением")));
			AddFloat(Row, TEXT("DelayMax"), Effect->DelayMax, Tr(TEXT("to (s)"), TEXT("до (с)")));
		}
	}

	// --- Auto-Chaos ---
	UConfigPropertySection* AutoChaos = AddSection(Root, TEXT("AutoChaos"), Tr(TEXT("Auto-Chaos"), TEXT("Автохаос")));
	AddBool(AutoChaos, TEXT("Enabled"), Defaults.bAutoChaos,
		Tr(TEXT("Enabled"), TEXT("Включён")),
		Tr(TEXT("ADA triggers a random paid effect when chat is quiet. Works even without viewers."),
		   TEXT("ADA запускает случайный платный эффект, когда чат молчит. Работает даже без зрителей.")));
	AddInt(AutoChaos, TEXT("IdleMinutes"), FMath::RoundToInt(Defaults.AutoChaosIdleSeconds / 60.f),
		Tr(TEXT("After quiet time (min)"), TEXT("После тишины (мин)")));

	// --- Chat games ---
	UConfigPropertySection* Games = AddSection(Root, TEXT("Games"), Tr(TEXT("Chat games"), TEXT("Игры с чатом")));
	AddBool(Games, TEXT("Votes"), Defaults.bVotesEnabled,
		Tr(TEXT("Votes"), TEXT("Голосования")),
		Tr(TEXT("Chat picks one of three paid effects with !1 / !2 / !3, for free"), TEXT("Чат выбирает один из трёх платных эффектов командами !1 / !2 / !3, бесплатно")));
	AddInt(Games, TEXT("VoteInterval"), Defaults.VoteIntervalMinutes, Tr(TEXT("Vote every (min)"), TEXT("Голосование каждые (мин)")));
	AddInt(Games, TEXT("VoteDuration"), FMath::RoundToInt(Defaults.VoteDurationSeconds), Tr(TEXT("Vote lasts (s)"), TEXT("Голосование длится (с)")));
	AddInt(Games, TEXT("VoteReward"), Defaults.VoteReward, Tr(TEXT("Chips for voting"), TEXT("Чипы за голос")));
	AddBool(Games, TEXT("Goal"), Defaults.bGoalEnabled,
		Tr(TEXT("Shared pot"), TEXT("Общая копилка")),
		Tr(TEXT("Viewers chip in with !pot <amount>; when it is full the effect below runs"), TEXT("Зрители скидываются командой !pot <сколько>; когда копилка полна, запускается эффект ниже")));
	AddString(Games, TEXT("GoalEffect"), Defaults.GoalEffect, Tr(TEXT("Pot effect"), TEXT("Эффект копилки")));
	AddInt(Games, TEXT("GoalChips"), Defaults.GoalChips, Tr(TEXT("Pot size (chips)"), TEXT("Размер копилки (чипы)")));

	// --- OBS ---
	UConfigPropertySection* Obs = AddSection(Root, TEXT("Obs"), Tr(TEXT("OBS overlay"), TEXT("Оверлей OBS")));
	AddBool(Obs, TEXT("Enabled"), Defaults.bOverlayEnabled,
		Tr(TEXT("Enabled"), TEXT("Включён")),
		Tr(TEXT("Widgets in OBS read data from http://localhost:<port>/state. Changes apply after restart."),
		   TEXT("Виджеты в OBS берут данные с http://localhost:<порт>/state. Применяется после перезапуска.")));
	AddInt(Obs, TEXT("Port"), Defaults.OverlayPort, Tr(TEXT("Port"), TEXT("Порт")));

	// --- Viewers in the world ---
	UConfigPropertySection* Viewers = AddSection(Root, TEXT("Viewers"), Tr(TEXT("Viewers in the world"), TEXT("Зрители в мире")));
	AddBool(Viewers, TEXT("PetOnSub"), Defaults.bPetOnSub,
		Tr(TEXT("Pet for a subscription"), TEXT("Питомец за подписку")));
	AddInt(Viewers, TEXT("PetLifeSpan"), FMath::RoundToInt(Defaults.PetLifeSpanSeconds),
		Tr(TEXT("Pets stay for (s)"), TEXT("Питомцы остаются (с)")));
	AddBool(Viewers, TEXT("CreatureNameTags"), Defaults.bCreatureNameTags,
		Tr(TEXT("Viewer names above creatures"), TEXT("Ники зрителей над существами")));

	// --- Raids ---
	UConfigPropertySection* Raids = AddSection(Root, TEXT("Raids"), Tr(TEXT("Raids"), TEXT("Рейды")));
	AddInt(Raids, TEXT("ViewersPerAmbush"), Defaults.RaidViewersPerAmbush,
		Tr(TEXT("1 ambush per N raiders"), TEXT("1 засада на N зрителей рейда")));
	AddInt(Raids, TEXT("MaxAmbushes"), Defaults.RaidMaxAmbushes,
		Tr(TEXT("Max ambushes"), TEXT("Макс. засад")));
	AddInt(Raids, TEXT("BossMinViewers"), Defaults.RaidBossMinViewers,
		Tr(TEXT("Boss from N raiders"), TEXT("Босс от N зрителей рейда")),
		Tr(TEXT("0 = never"), TEXT("0 = никогда")));

	// --- Advanced: limits and fine tuning ---
	UConfigPropertySection* Advanced = AddSection(Root, TEXT("Advanced"), Tr(TEXT("Advanced"), TEXT("Дополнительно")));
	AddInt(Advanced, TEXT("ChaosBudgetPerMinute"), Defaults.ChaosBudgetPerMinute,
		Tr(TEXT("Chaos budget per minute"), TEXT("Бюджет хаоса в минуту")),
		Tr(TEXT("Chaos points harmful effects may use per minute. Extra effects wait in a queue."),
		   TEXT("Сколько очков хаоса вредные эффекты тратят за минуту. Остальные ждут в очереди.")));
	AddInt(Advanced, TEXT("DeathProtectionSeconds"), FMath::RoundToInt(Defaults.DeathProtectionSeconds),
		Tr(TEXT("Death protection (s)"), TEXT("Защита после смерти (с)")),
		Tr(TEXT("No harmful effects for this long after you die"), TEXT("Столько секунд после смерти вредные эффекты не срабатывают")));
	AddInt(Advanced, TEXT("CostScalePercent"), FMath::RoundToInt(Defaults.CostScalePerViewer * 100.f),
		Tr(TEXT("Price growth per viewer (%)"), TEXT("Рост цен за зрителя (%)")));
	AddFloat(Advanced, TEXT("MaxCostMultiplier"), Defaults.MaxCostMultiplier,
		Tr(TEXT("Max price multiplier"), TEXT("Макс. множитель цен")));
	AddFloat(Advanced, TEXT("SafeZoneRadius"), Defaults.SafeZoneRadius,
		Tr(TEXT("Safe zone around the HUB (m)"), TEXT("Безопасная зона у ХАБа (м)")),
		Tr(TEXT("Creatures wait while you are this close to the HUB. 0 = off"), TEXT("Существа ждут, пока ты ближе к ХАБу. 0 = выключено")));
	AddFloat(Advanced, TEXT("SpawnDistanceMin"), Defaults.SpawnDistanceMin,
		Tr(TEXT("Creatures spawn from (m)"), TEXT("Существа появляются от (м)")));
	AddFloat(Advanced, TEXT("SpawnDistanceMax"), Defaults.SpawnDistanceMax,
		Tr(TEXT("to (m)"), TEXT("до (м)")));
	AddInt(Advanced, TEXT("LifeSpan"), FMath::RoundToInt(Defaults.CreatureLifeSpanSeconds),
		Tr(TEXT("Creatures despawn after (s)"), TEXT("Существа исчезают через (с)")));
	AddString(Advanced, TEXT("IgnoredUsers"), FString::Join(Defaults.IgnoredUsers, TEXT(", ")),
		Tr(TEXT("Ignored users"), TEXT("Игнорируемые пользователи")),
		Tr(TEXT("Comma-separated chat logins, e.g. bots"), TEXT("Логины через запятую, например боты")));

	UConfigPropertySection* Limits = AddSection(Advanced, TEXT("EffectLimits"), Tr(TEXT("Effect limits"), TEXT("Ограничения эффектов")));
	for (const FEffectLabel& Label : EffectLabels)
	{
		const FChatEffectSettings* Effect = Defaults.Effects.Find(Label.Id);
		if (!Effect)
		{
			continue;
		}
		UConfigPropertySection* Row = AddSection(Limits, Label.Id, Tr(Label.English, Label.Russian), true);
		AddInt(Row, TEXT("GlobalCooldown"), FMath::RoundToInt(Effect->GlobalCooldown),
			Tr(TEXT("Global cooldown (s)"), TEXT("Общий кулдаун (с)")),
			Tr(TEXT("How often anyone can use it"), TEXT("Как часто это может запустить кто угодно")));
		AddInt(Row, TEXT("ChaosWeight"), Effect->ChaosWeight,
			Tr(TEXT("Chaos points"), TEXT("Очки хаоса")),
			Tr(TEXT("Share of the chaos budget it uses"), TEXT("Сколько бюджета хаоса тратит")));
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

	const UConfigPropertySection* Main = GetSection(Root, TEXT("Main"));
	bChanged |= Read<UConfigPropertyBool>(Main, TEXT("Enabled"), Settings.bEnabled);
	bChanged |= Read<UConfigPropertyString>(Main, TEXT("TwitchChannel"), Settings.TwitchChannel);
	bChanged |= Read<UConfigPropertyString>(Main, TEXT("Language"), Settings.Language);
	bChanged |= Read<UConfigPropertyInteger>(Main, TEXT("Difficulty"), Settings.Difficulty);

	const UConfigPropertySection* YouTube = GetSection(Root, TEXT("YouTube"));
	bChanged |= Read<UConfigPropertyString>(YouTube, TEXT("ApiKey"), Settings.YouTubeApiKey);
	bChanged |= Read<UConfigPropertyString>(YouTube, TEXT("Channel"), Settings.YouTubeChannel);
	bChanged |= ReadSeconds(YouTube, TEXT("PollSeconds"), Settings.YouTubePollSeconds, 1.f, 5.f);
	bChanged |= Read<UConfigPropertyInteger>(YouTube, TEXT("ChipsPerSuperChatUnit"), Settings.ChipsPerSuperChatUnit);

	const UConfigPropertySection* Economy = GetSection(Root, TEXT("Economy"));
	bChanged |= Read<UConfigPropertyInteger>(Economy, TEXT("ChipsPerInterval"), Settings.ChipsPerInterval);
	bChanged |= ReadSeconds(Economy, TEXT("IntervalMinutes"), Settings.ChipsIntervalSeconds, 60.f, 60.f);
	bChanged |= Read<UConfigPropertyInteger>(Economy, TEXT("FirstMessageBonus"), Settings.FirstMessageBonus);
	bChanged |= Read<UConfigPropertyInteger>(Economy, TEXT("ChipsPerBit"), Settings.ChipsPerBit);
	bChanged |= Read<UConfigPropertyInteger>(Economy, TEXT("ChipsPerSub"), Settings.ChipsPerSub);

	const UConfigPropertySection* AutoChaos = GetSection(Root, TEXT("AutoChaos"));
	bChanged |= Read<UConfigPropertyBool>(AutoChaos, TEXT("Enabled"), Settings.bAutoChaos);
	bChanged |= ReadSeconds(AutoChaos, TEXT("IdleMinutes"), Settings.AutoChaosIdleSeconds, 60.f, 60.f);

	const UConfigPropertySection* Games = GetSection(Root, TEXT("Games"));
	bChanged |= Read<UConfigPropertyBool>(Games, TEXT("Votes"), Settings.bVotesEnabled);
	bChanged |= Read<UConfigPropertyInteger>(Games, TEXT("VoteInterval"), Settings.VoteIntervalMinutes);
	bChanged |= ReadSeconds(Games, TEXT("VoteDuration"), Settings.VoteDurationSeconds, 1.f, 10.f);
	bChanged |= Read<UConfigPropertyInteger>(Games, TEXT("VoteReward"), Settings.VoteReward);
	bChanged |= Read<UConfigPropertyBool>(Games, TEXT("Goal"), Settings.bGoalEnabled);
	bChanged |= Read<UConfigPropertyString>(Games, TEXT("GoalEffect"), Settings.GoalEffect);
	bChanged |= Read<UConfigPropertyInteger>(Games, TEXT("GoalChips"), Settings.GoalChips);

	const UConfigPropertySection* Obs = GetSection(Root, TEXT("Obs"));
	bChanged |= Read<UConfigPropertyBool>(Obs, TEXT("Enabled"), Settings.bOverlayEnabled);
	bChanged |= Read<UConfigPropertyInteger>(Obs, TEXT("Port"), Settings.OverlayPort);

	const UConfigPropertySection* Viewers = GetSection(Root, TEXT("Viewers"));
	bChanged |= Read<UConfigPropertyBool>(Viewers, TEXT("PetOnSub"), Settings.bPetOnSub);
	bChanged |= ReadSeconds(Viewers, TEXT("PetLifeSpan"), Settings.PetLifeSpanSeconds, 1.f, 60.f);
	bChanged |= Read<UConfigPropertyBool>(Viewers, TEXT("CreatureNameTags"), Settings.bCreatureNameTags);

	const UConfigPropertySection* Raids = GetSection(Root, TEXT("Raids"));
	bChanged |= Read<UConfigPropertyInteger>(Raids, TEXT("ViewersPerAmbush"), Settings.RaidViewersPerAmbush);
	bChanged |= Read<UConfigPropertyInteger>(Raids, TEXT("MaxAmbushes"), Settings.RaidMaxAmbushes);
	bChanged |= Read<UConfigPropertyInteger>(Raids, TEXT("BossMinViewers"), Settings.RaidBossMinViewers);

	const UConfigPropertySection* Advanced = GetSection(Root, TEXT("Advanced"));
	bChanged |= Read<UConfigPropertyInteger>(Advanced, TEXT("ChaosBudgetPerMinute"), Settings.ChaosBudgetPerMinute);
	bChanged |= ReadSeconds(Advanced, TEXT("DeathProtectionSeconds"), Settings.DeathProtectionSeconds);
	int32 CostScalePercent = FMath::RoundToInt(Settings.CostScalePerViewer * 100.f);
	if (Read<UConfigPropertyInteger>(Advanced, TEXT("CostScalePercent"), CostScalePercent))
	{
		Settings.CostScalePerViewer = CostScalePercent / 100.f;
		bChanged = true;
	}
	bChanged |= Read<UConfigPropertyFloat>(Advanced, TEXT("MaxCostMultiplier"), Settings.MaxCostMultiplier);
	bChanged |= Read<UConfigPropertyFloat>(Advanced, TEXT("SafeZoneRadius"), Settings.SafeZoneRadius);
	bChanged |= Read<UConfigPropertyFloat>(Advanced, TEXT("SpawnDistanceMin"), Settings.SpawnDistanceMin);
	bChanged |= Read<UConfigPropertyFloat>(Advanced, TEXT("SpawnDistanceMax"), Settings.SpawnDistanceMax);
	bChanged |= ReadSeconds(Advanced, TEXT("LifeSpan"), Settings.CreatureLifeSpanSeconds, 1.f, 10.f);
	FString Ignored = FString::Join(Settings.IgnoredUsers, TEXT(", "));
	if (Read<UConfigPropertyString>(Advanced, TEXT("IgnoredUsers"), Ignored))
	{
		TArray<FString> Parts;
		Ignored.ParseIntoArray(Parts, TEXT(","));
		Settings.IgnoredUsers.Reset();
		for (const FString& Part : Parts)
		{
			const FString Login = Part.TrimStartAndEnd().ToLower();
			if (!Login.IsEmpty())
			{
				Settings.IgnoredUsers.Add(Login);
			}
		}
		bChanged = true;
	}

	for (const FEffectLabel& Label : EffectLabels)
	{
		FChatEffectSettings* Effect = Settings.Effects.Find(Label.Id);
		if (!Effect)
		{
			continue;
		}
		if (const UConfigPropertySection* Row = FindPath(Root, FString::Printf(TEXT("Effects/%s/%s"), Label.Category, Label.Id)))
		{
			bChanged |= Read<UConfigPropertyBool>(Row, TEXT("Enabled"), Effect->bEnabled);
			bChanged |= Read<UConfigPropertyInteger>(Row, TEXT("Cost"), Effect->Cost);
			bChanged |= ReadSeconds(Row, TEXT("ViewerCooldown"), Effect->ViewerCooldown);
			bChanged |= ReadSeconds(Row, TEXT("Duration"), Effect->Duration);
			bChanged |= Read<UConfigPropertyFloat>(Row, TEXT("Strength"), Effect->Strength);
			bChanged |= Read<UConfigPropertyInteger>(Row, TEXT("Amount"), Effect->Amount);
			bChanged |= Read<UConfigPropertyFloat>(Row, TEXT("DelayMin"), Effect->DelayMin);
			bChanged |= Read<UConfigPropertyFloat>(Row, TEXT("DelayMax"), Effect->DelayMax);
		}
		if (const UConfigPropertySection* Row = FindPath(Root, FString::Printf(TEXT("Advanced/EffectLimits/%s"), Label.Id)))
		{
			bChanged |= ReadSeconds(Row, TEXT("GlobalCooldown"), Effect->GlobalCooldown);
			bChanged |= Read<UConfigPropertyInteger>(Row, TEXT("ChaosWeight"), Effect->ChaosWeight);
		}
	}

	return bChanged;
}
