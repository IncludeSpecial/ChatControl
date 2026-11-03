# FAQ and troubleshooting

[Setup](setup.md) · [Commands](commands.md) · [Settings](settings.md) · [На русском](../ru/faq.md)

### Nothing happens when chat types commands
1. **Mods -> ChatControl -> Main**: is **Mod enabled** on and the **Twitch channel** filled in (name only, no link)?
2. You need to be in a loaded save; in the main menu effects do not run.
3. Answers appear in the **game chat**: press Enter in game to open it.
4. Paid effects need chips. Give yourself some with `!test chips 1000`, or try a free one: `!hi`.
5. Harmful effects can wait in a queue: chaos budget, `!pause`, 60 s after death, creatures near the HUB.
6. Still nothing? Look for `LogChatControl` lines in `%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log`.

### Effects happen even though nobody is in chat
That is Auto-Chaos: after 10 quiet minutes ADA triggers a random effect. Turn it off in **Auto-Chaos -> Enabled**
or make the wait longer.

### It is too much, how do I calm it down?
- `!pause` - harmful effects wait until you type it again. `!panic` - cancels everything at once.
- **Difficulty 1**, higher prices, longer cooldowns, a smaller **Chaos budget per minute**.
- Turn off single effects in **Effects** or take them out of the random pool (**In random**).
- A bigger **Safe zone around the HUB** keeps creatures away from your base.

### Does it work in multiplayer?
Not yet. The mod is made for singleplayer.

### Are commands English only?
Effects are: `!boss`, `!fog`, `!gift` and so on. The basic commands also have Russian versions:
`!чипы`, `!бонус`, `!голос`, `!копилка`, `!топ`, `!статистика`, `!убрать`. Game chat messages and widgets use the mod language.

### YouTube does not connect
- The key must be for **YouTube Data API v3** and the API must be enabled in the same Google project.
- Use the channel id `UC...` or a stream link, not an `@handle`.
- The stream must be live (public or unlisted). Without a link the mod checks every 3 minutes.
- The daily quota (10 000 units) may be used up; it resets at midnight Pacific time.
- The log shows `YouTube: HTTP 403 ...` with the reason.

### OBS widgets are empty or show "widget not found"
- The game must be running with a loaded save; the widgets come alive by themselves.
- In the Browser source **Local file** must be unchecked and the URL must start with `http://localhost:8787/obs/`.
- "Widget not found" lists all widgets that exist; click one to see the right URL.
- Port 8787 taken by another program? Change **OBS overlay -> Port** (applies at once) and update the URLs.

### Can my viewers abuse `!test`, `!panic` or `!clear`?
No. `!test` works only for the channel owner, checked by the Twitch/YouTube owner badge, which chat cannot fake.
`!pause`, `!panic` and `!clear` also work for moderators.

### A viewer wrote something bad on a sign
`!clear nick` removes that viewer's signs, `!clear` removes all. Shorter **Duration** for `!sign` makes signs go away sooner.

### Are chips kept between streams?
Yes, in `Economy.json`. They are tied to the viewer's account, not their display name.

### What stays in my save if I remove the mod?
Pets stay as ordinary lizard doggos without name tags. Named machines lose their names. Nothing else is written into saves.

### Does the mod send any data anywhere?
It reads Twitch chat anonymously (`irc.chat.twitch.tv`) and, if you set a key, YouTube chat through Google's API.
The OBS server is reachable only from your PC. No telemetry. Settings and stats stay on your disk.
