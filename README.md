# ChatControl

Satisfactory mod that lets your Twitch chat play with you.

- **FICSIT Chips**: viewers earn chips just by being active in chat and spend them on effects. Balances persist between streams.
- **Bits, subs, gift subs** turn into chips; **raids** unleash a wave of chaos.
- **Channel points**: rewards with text input are mapped to effects in `Settings.json`.
- **Auto-Chaos**: when chat is quiet, ADA triggers a random effect, so the mod is fun on small streams and even offline.
- **Safety**: chaos budget per minute, cooldowns, death protection, `!pause` for the streamer and moderators.

No Twitch token needed: chat is read anonymously.

## Chat commands

| Command | Who | What |
|---|---|---|
| `!chips` | everyone | show your balance |
| `!effects` | everyone | list effects and current prices |
| `!hi`, `!gift` | everyone | free effects with cooldowns |
| `!biggift`, `!jump`, `!moon`, `!heavy` | everyone | paid with chips |
| `!ambush`, `!boss` | everyone | spawn enemies around the streamer, paid with chips |
| `!pause` | streamer, mods | pause/resume harmful effects |
| `!test <effect>` | streamer | run an effect for free |
| `!test chips/bits/sub/gifts/raid [N]`, `!test autochaos` | streamer | simulate events for testing |

Creatures arrive 3-5 s after a warning. Raids spawn one ambush per 5 raiders (up to 5) and a boss for raids of 30+. Spawned creatures despawn after 5 minutes. Everything above, including spawn distance, delays, effect strength, raid rules and language, is configurable in Mods -> ChatControl.

Prices grow with the number of active viewers.

## Configuration

`%LOCALAPPDATA%\FactoryGame\Saved\ChatControl\Settings.json` is created on first launch. Viewer balances are stored next to it in `Economy.json`.

---

# ChatControl (RU)

РњРѕРґ РґР»СЏ Satisfactory: С‚РІРѕР№ С‡Р°С‚ РЅР° Twitch РёРіСЂР°РµС‚ РІРјРµСЃС‚Рµ СЃ С‚РѕР±РѕР№.

- **Р§РёРїС‹ FICSIT** Р·СЂРёС‚РµР»Рё РїРѕР»СѓС‡Р°СЋС‚ Р·Р° Р°РєС‚РёРІРЅРѕСЃС‚СЊ РІ С‡Р°С‚Рµ Рё С‚СЂР°С‚СЏС‚ РЅР° СЌС„С„РµРєС‚С‹. Р‘Р°Р»Р°РЅСЃ СЃРѕС…СЂР°РЅСЏРµС‚СЃСЏ РјРµР¶РґСѓ СЃС‚СЂРёРјР°РјРё.
- **Р‘РёС‚С‹, СЃР°Р±С‹ Рё РіРёС„С‚-СЃР°Р±С‹** РїСЂРµРІСЂР°С‰Р°СЋС‚СЃСЏ РІ С‡РёРїС‹, **СЂРµР№Рґ** Р·Р°РїСѓСЃРєР°РµС‚ РІРѕР»РЅСѓ С…Р°РѕСЃР°.
- **Р‘Р°Р»Р»С‹ РєР°РЅР°Р»Р°**: РЅР°РіСЂР°РґС‹ СЃ РІРІРѕРґРѕРј С‚РµРєСЃС‚Р° РїСЂРёРІСЏР·С‹РІР°СЋС‚СЃСЏ Рє СЌС„С„РµРєС‚Р°Рј РІ `Settings.json`.
- **РђРІС‚РѕС…Р°РѕСЃ**: РєРѕРіРґР° С‡Р°С‚ РјРѕР»С‡РёС‚, ADA СЃР°РјР° Р·Р°РїСѓСЃРєР°РµС‚ СЃР»СѓС‡Р°Р№РЅС‹Р№ СЌС„С„РµРєС‚.
- **Р—Р°С‰РёС‚Р°**: Р±СЋРґР¶РµС‚ С…Р°РѕСЃР°, РєСѓР»РґР°СѓРЅС‹, Р·Р°С‰РёС‚Р° РїРѕСЃР»Рµ СЃРјРµСЂС‚Рё, `!pause` РґР»СЏ СЃС‚СЂРёРјРµСЂР° Рё РјРѕРґРµСЂР°С‚РѕСЂРѕРІ.

РўРѕРєРµРЅ Twitch РЅРµ РЅСѓР¶РµРЅ.
