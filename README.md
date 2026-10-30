# ChatControl

Satisfactory mod that lets your Twitch and YouTube chat play with you.

- **FICSIT Chips**: viewers earn chips by being active in chat and spend them on effects. Balances persist between streams.
- **Bits, subs, gift subs, Super Chats, memberships** turn into chips; **raids** bring ambushes and a boss.
- **Channel points**: rewards with text input are mapped to effects in `Settings.json`.
- **Votes, shared pot, leaderboard** keep a small chat involved.
- **Auto-Chaos**: when chat is quiet, ADA triggers a random effect, so the mod is fun on small streams and even offline.
- **Safety**: chaos budget, cooldowns, death protection, safe zone around the HUB, `!pause` and `!panic`.
- **OBS widgets**: the mod serves live data at `http://localhost:8787/state`.

Twitch needs no token (chat is read anonymously). YouTube needs a free YouTube Data API v3 key.

## Chat commands

| Command | Who | What |
|---|---|---|
| `!chips` | everyone | your balance |
| `!effects` | everyone | effects and current prices |
| `!hi`, `!gift` | everyone | free, with cooldowns |
| `!biggift`, `!rain`, `!sink`, `!pet`, `!sign <text>`, `!day` | everyone | gifts and fun, paid with chips |
| `!jump`, `!moon`, `!heavy`, `!speed`, `!slow`, `!fov`, `!teleport`, `!hub`, `!night` | everyone | player chaos |
| `!strike`, `!underclock`, `!blackout` | everyone | factory chaos |
| `!ambush`, `!boss` | everyone | creatures with the viewer's name above them |
| `!1` `!2` `!3` / `!vote N` | everyone | vote for the next effect |
| `!pot <chips>` | everyone | chip into the shared pot |
| `!top`, `!stats` | everyone | leaderboard, session stats |
| `!pause`, `!panic` | streamer, mods | pause chaos / cancel everything and remove creatures |
| `!test <effect>` | streamer | run an effect for free |
| `!test chips/bits/sub/gifts/raid [N]`, `!test autochaos` | streamer | simulate events |

Everything (prices, cooldowns, strength, delays, counts, raids, votes, pot, difficulty, language, YouTube, OBS port) is configurable in **Mods -> ChatControl**.

## Files

- `%LOCALAPPDATA%\FactoryGame\Saved\ChatControl\Settings.json` - settings
- `Economy.json` - viewer balances and the pot
- `Sessions\` - a summary of every session

---

# ChatControl (RU)

Мод для Satisfactory: чат на Twitch и YouTube играет вместе с тобой.

- **Чипы FICSIT** зрители получают за активность в чате и тратят на эффекты.
- **Биты, сабы, Super Chat и спонсорство** превращаются в чипы, **рейд** приносит засады и босса.
- **Голосования, общая копилка, топ** - чтобы маленький чат был вовлечён.
- **Автохаос**: когда чат молчит, ADA сама запускает случайный эффект.
- **Защита**: бюджет хаоса, кулдауны, защита после смерти, безопасная зона у ХАБа, `!pause` и `!panic`.
- **Виджеты OBS** берут данные с `http://localhost:8787/state`.

Все настройки - в игре: **Моды -> ChatControl**.
