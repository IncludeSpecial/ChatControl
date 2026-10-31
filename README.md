# ChatControl

Satisfactory mod that lets your Twitch and YouTube chat play with you.

- **FICSIT Chips**: viewers earn chips by being active in chat and spend them on effects. Balances persist between streams.
- **Bits, subs, gift subs, Super Chats, memberships** turn into chips; **raids** bring ambushes and a boss.
- **Channel points**: rewards with text input are mapped to effects in `Settings.json`.
- **Votes, shared pot, leaderboard** keep a small chat involved.
- **Auto-Chaos**: when chat is quiet, ADA triggers a random effect, so the mod is fun on small streams and even offline.
- **Safety**: chaos budget, cooldowns, death protection, safe zone around the HUB, `!pause` and `!panic`.
- **OBS widgets** come with the mod: event feed, votes, pot and top viewers (see below).

Twitch needs no token (chat is read anonymously). YouTube needs a free YouTube Data API v3 key.

## Chat commands

| Command | Who | What |
|---|---|---|
| `!chips` | everyone | your balance and rank |
| `!daily` | everyone | daily chip bonus |
| `!effects` | everyone | effects and current prices |
| `!hi`, `!gift` | everyone | free, with cooldowns |
| `!biggift`, `!rain`, `!sink`, `!pet`, `!sign <text>`, `!day` | everyone | gifts and fun, paid with chips |
| `!jump`, `!moon`, `!heavy`, `!speed`, `!slow`, `!fov`, `!teleport`, `!hub`, `!night` | everyone | player chaos |
| `!strike`, `!underclock`, `!blackout` | everyone | factory chaos |
| `!ambush`, `!boss` | everyone | creatures with the viewer's name above them |
| `!fly`, `!shrink`, `!giant` | everyone | hover, tiny or giant player |
| `!hungry`, `!inventory`, `!drop`, `!fog` | everyone | health drains until you eat, shuffled inventory, items fall out into a crate far away, thick fog |
| `!dance`, `!heal` | everyone | freeze nearby creatures, full health |
| `!name` | everyone | sign the machine the streamer looks at; stays in the save |
| `!1` `!2` `!3` / `!vote N` | everyone | vote for the next effect |
| `!pot <chips>` | everyone | chip into the shared pot |
| `!top`, `!stats` | everyone | leaderboard, session stats |
| `!pause`, `!panic` | streamer, mods | pause chaos / cancel everything and remove creatures |
| `!clear [nick]` | streamer, mods | remove all signs, or only the ones left by that viewer |
| `!test <effect>` | streamer | run an effect for free |
| `!test chips/bits/sub/gifts/raid [N]`, `!test welcome`, `!test autochaos` | streamer | simulate events |

Everything (prices, cooldowns, strength, delays, counts, raids, votes, pot, difficulty, language, YouTube, OBS port) is configurable in **Mods -> ChatControl**.

## OBS widgets

The mod runs a small web server on this PC only (`127.0.0.1`) while the game is running, and serves three ready-made widgets.
Add each one in OBS as **Sources -> + -> Browser**, uncheck **Local file**, paste the URL and set the size:

| Widget | URL | Size |
|---|---|---|
| Event feed: who triggered what, raids, subs | `http://localhost:8787/obs/feed.html` | 560 x 600 |
| Vote (shows up only while a vote runs) | `http://localhost:8787/obs/vote.html` | 760 x 330 |
| Pot and top viewers | `http://localhost:8787/obs/pot.html` | 640 x 330 |

- The widgets are empty while the game is not running; they reconnect by themselves once it starts.
- Add `?k=1.4` to a URL to make a widget bigger (any number works).
- Do not use **Local file** in OBS: OBS blocks local pages from reading the game data.
- The widgets have a neutral FICSIT look and speak the mod's language (English or Russian).
- Own design: copy the files from [`Resources/Overlay`](Resources/Overlay) into any folder, change colors in `assets/ficsit.css` or anything else, and set the folder in **Mods -> ChatControl -> OBS overlay -> Widget folder**. Files from your folder open as `http://localhost:8787/obs/<file>`; the built-in ones keep working. Live data is at `/state` (JSON).
- Port in use? Change it in the same settings section and in the URLs.

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
- **Виджеты OBS** идут в комплекте с модом: лента событий, голосование, копилка и топ.

Все настройки - в игре: **Моды -> ChatControl**.

## Виджеты для OBS

Пока игра запущена, мод держит маленький веб-сервер, доступный только с этого компьютера, и отдаёт три готовых виджета.
В OBS: **Источники -> + -> Браузер**, сними галочку **Локальный файл**, вставь адрес и задай размер:

| Виджет | Адрес | Размер |
|---|---|---|
| Лента событий: кто что запустил, рейды, сабы | `http://localhost:8787/obs/feed.html` | 560 x 600 |
| Голосование (видно только во время голосования) | `http://localhost:8787/obs/vote.html` | 760 x 330 |
| Копилка и топ зрителей | `http://localhost:8787/obs/pot.html` | 640 x 330 |

- Без запущенной игры виджеты пустые и сами оживают, когда игра запустится.
- `?k=1.4` в конце адреса делает виджет крупнее (подойдёт любое число).
- Не включай **Локальный файл**: OBS не даёт локальным страницам читать данные игры.
- Виджеты в нейтральном стиле FICSIT и подписаны на языке мода (русский или английский).
- Свой дизайн: скопируй файлы из [`Resources/Overlay`](Resources/Overlay) в любую папку, поменяй цвета в `assets/ficsit.css` или что угодно ещё и укажи папку в **Моды -> ChatControl -> Оверлей OBS -> Папка виджетов**. Файлы из твоей папки открываются как `http://localhost:8787/obs/<файл>`, встроенные продолжают работать. Данные лежат по адресу `/state` (JSON).
