# ChatControl

**Let your Twitch and YouTube chat play Satisfactory with you.** [На русском](README.ru.md)

Viewers earn **FICSIT Chips** by chatting and spend them on effects: gifts and supply crates, moon gravity and flight,
machine strikes and blackouts, ambushes and alpha bosses. Their names show up in your world - above the monsters they send,
on signs, on machines they signed and on their pets. Made for small streamers: when chat is quiet, ADA keeps things going.

## Quick start

1. Install **ChatControl** with [Satisfactory Mod Manager](https://smm.ficsit.app/).
2. In the game: **Mods -> ChatControl -> Main -> Twitch channel**, enter your channel name.
3. Load a save and type `!test boss` in your chat. That's it.

Twitch needs no token or login. YouTube is optional and needs a free API key.

## Documentation

| | English | Русский |
|---|---|---|
| Install, Twitch, YouTube, OBS, telling viewers | [Setup](docs/en/setup.md) | [Установка](docs/ru/setup.md) |
| All commands and effects with prices | [Commands](docs/en/commands.md) | [Команды](docs/ru/commands.md) |
| Every setting and the files on disk | [Settings](docs/en/settings.md) | [Настройки](docs/ru/settings.md) |
| Troubleshooting and questions | [FAQ](docs/en/faq.md) | [Вопросы](docs/ru/faq.md) |
| What changed | [Changelog](CHANGELOG.md) | |

## Features

- **Chat**: Twitch (anonymous, no token) and YouTube live chat (free API key), both at once.
- **FICSIT Chips**: for chatting, the first message, a daily bonus, bits, Super Chats, subs and memberships.
  Prices grow with the number of active viewers. Chips are kept between streams.
- **33 effects**: free ones, gifts and help, player chaos, survival, factory sabotage and creatures. See [the full list](docs/en/commands.md).
- **Viewers in the world**: name tags with ranks and role colors, greetings with fireworks, signs, named machines and vehicles,
  pets for subscribers that stay in your save.
- **Chat games**: votes, a shared chip pot for a big effect, top viewers, ranks, giving chips to each other (`!give`, with limits).
- **Titans**: boss fights with 13 bosses, random traits, three stages, a health bar and loot.
- **Raids** bring ambushes, big raids a boss or a titan.
- **Safety**: chaos budget, cooldowns, death protection, a safe zone around the HUB, `!pause`, `!panic`, `!clear`.
- **OBS widgets** built in: event feed, vote, pot and top viewers, titan fight.
- **Everything is configurable** in the game, in English and Russian.

## Good to know

- Singleplayer only for now.
- Effects can really hurt you and your factory. Start with difficulty 1 or turn off what you do not want.
- Auto-Chaos works even without viewers. Turn it off in **Mods -> ChatControl -> Auto-Chaos** if you only want chat effects.
- The mod reads chat, it never writes to it. The OBS server is reachable from your PC only. No telemetry.

## Building from source

The mod is built with the [Satisfactory Modding](https://docs.ficsit.app/) toolchain (Unreal Engine 5.6 CSS, SML 3.12):
put this repository into `Mods/ChatControl` of the SML starter project and use Alpakit.

## License

[MIT](LICENSE) © Switty
