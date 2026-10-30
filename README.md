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
| `!speed`, `!slow`, `!fov`, `!teleport`, `!hub` | everyone | player chaos, paid with chips |
| `!strike` | everyone | random machines stop working for a while, paid with chips |
| `!pause` | streamer, mods | pause/resume harmful effects |
| `!panic` | streamer, mods | cancel all chaos and remove creatures from chat |
| `!test <effect>` | streamer | run an effect for free |
| `!test chips/bits/sub/gifts/raid [N]`, `!test autochaos` | streamer | simulate events for testing |

Creatures arrive 3-5 s after a warning. No creatures while you are near the HUB (safe zone). Difficulty 1-3 changes all prices and creature counts at once. Raids spawn one ambush per 5 raiders (up to 5) and a boss for raids of 30+. Spawned creatures despawn after 5 minutes. Everything above, including spawn distance, delays, effect strength, raid rules and language, is configurable in Mods -> ChatControl.

Prices grow with the number of active viewers.

## Configuration

`%LOCALAPPDATA%\FactoryGame\Saved\ChatControl\Settings.json` is created on first launch. Viewer balances are stored next to it in `Economy.json`.

---

# ChatControl (RU)

Мод для Satisfactory: твой чат на Twitch играет вместе с тобой.

- **Чипы FICSIT** зрители получают за активность в чате и тратят на эффекты. Баланс сохраняется между стримами.
- **Биты, сабы и гифт-сабы** превращаются в чипы, **рейд** запускает волну хаоса.
- **Баллы канала**: награды с вводом текста привязываются к эффектам в `Settings.json`.
- **Автохаос**: когда чат молчит, ADA сама запускает случайный эффект.
- **Защита**: бюджет хаоса, кулдауны, защита после смерти, `!pause` для стримера и модераторов.

Токен Twitch не нужен.
