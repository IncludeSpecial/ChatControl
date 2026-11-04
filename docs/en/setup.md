# Setup

[Commands](commands.md) · [Settings](settings.md) · [FAQ](faq.md) · [На русском](../ru/setup.md)

## 1. Install

1. Install [Satisfactory Mod Manager](https://smm.ficsit.app/).
2. Search for **ChatControl** and press install. SMM also installs the Satisfactory Mod Loader it needs.
3. Start the game from SMM.

Singleplayer only. Multiplayer is not supported yet.

## 2. Connect Twitch

1. In the main menu open **Mods -> ChatControl -> Main**.
2. Enter your channel name in **Twitch channel**: the part after `twitch.tv/`, for example `swittyids`.
3. Load a save. Type `!chips` in your Twitch chat: the answer appears in the game chat (press Enter in game to see it).

No token, password or login is needed: the mod reads the chat as an anonymous guest and never writes to it.

## 3. Connect YouTube (optional)

The mod reads YouTube live chat through the free YouTube Data API. You need an API key once.

**Get a key (about 5 minutes):**
1. Open [console.cloud.google.com](https://console.cloud.google.com) with any Google account.
2. **Select a project -> New project**, name it e.g. `ChatControl`, **Create**.
3. **APIs & Services -> Library**, find **YouTube Data API v3**, **Enable**.
4. **APIs & Services -> Credentials -> Create credentials -> API key**. Copy the key (`AIza...`).
5. Recommended: open the key, **API restrictions -> Restrict key -> YouTube Data API v3**, save.

**Enter it in the game:** **Mods -> ChatControl -> YouTube chat**
- **API key**: the key.
- **Channel or stream**: your channel id (`UC...`, 24 characters; YouTube -> your avatar -> Settings -> Advanced settings -> Channel ID),
  or a link to the stream (`youtube.com/live/...`, `youtube.com/watch?v=...`). `@handles` are not supported.

**How it works:**
- With a channel id the mod looks for a running stream every 3 minutes. With a stream link it connects right away.
- While live, chat is read every 10 seconds (**Check chat every**).
- Google gives 10 000 quota units per day. At 10 seconds that is about 5 hours of streaming. Searching for the stream costs more,
  so a stream link saves quota on long streams. The quota resets at midnight Pacific time.
- Super Chats and Super Stickers give chips; new and gifted memberships work like subs.

## 4. OBS widgets (optional)

While the game is running, the mod serves these widgets from this PC only (`127.0.0.1`).

In OBS: **Sources -> + -> Browser**, uncheck **Local file**, paste the URL and set the size:

| Widget | URL | Size |
|---|---|---|
| Event feed: who triggered what, raids, subs | `http://localhost:8787/obs/feed.html` | 560 x 600 |
| Vote (visible only while a vote runs) | `http://localhost:8787/obs/vote.html` | 760 x 330 |
| Shared pots for a boss and a titan, top viewers | `http://localhost:8787/obs/pot.html` | 640 x 450 |
| Chips of the viewers in chat, so nobody has to type `!chips` (hidden while nobody is active; `?n=12` shows more rows) | `http://localhost:8787/obs/balance.html` | 460 x 420 |
| Titan fight: boss, health bar, stage (visible only during a fight) | `http://localhost:8787/obs/boss.html` | 760 x 190 |

- Without the game the widgets are empty; they come alive by themselves once the game starts.
- `?k=1.4` at the end of a URL makes a widget bigger (any number works).
- Do not use **Local file**: OBS blocks local pages from reading the game data.
- The widgets use the mod language (English or Russian).

**Own design:** copy the files from [`Resources/Overlay`](../../Resources/Overlay) into any folder, change colors in
`assets/ficsit.css` (or anything else) and set the folder in **Mods -> ChatControl -> OBS overlay -> Widget folder**.
Your files open as `http://localhost:8787/obs/<file name>`, the built-in ones keep working.
Live data is at `http://localhost:8787/state` (JSON: events, vote, pot, top, balances, chaos, boss, language).

## 5. Tell your viewers

Viewers do not see the game chat, so tell them how to play. A chat bot command works well, for example in Nightbot
(`!commands add` in your chat):

```
!commands add !mod This stream runs the ChatControl mod: chat plays Satisfactory with me. Chat to earn FICSIT Chips, take a daily bonus with !daily and spend chips on effects, from gifts to monsters: !gift, !rain, !jump, !fog, !ambush, !boss and more. Full list on the panels below the stream.
```

Answers to `!chips`, `!effects` and the like appear in the **game** chat, which viewers see only if the game is on stream.
The bot command and stream panels are the way to explain things in the Twitch chat itself.

Bots are ignored by default (`nightbot`, `streamelements`, `streamlabs`, `moobot`, `fossabot`), so their messages earn nothing
and trigger nothing. Add your own bots in **Advanced -> Ignored users**.

## 6. Try everything yourself

As the channel owner you can run anything for free:

- `!test boss`, `!test fog`, `!test drop` - any effect by name
- `!test chips 1000` - give yourself chips
- `!test bits 100`, `!test sub`, `!test gifts 5`, `!test raid 30` - simulate events
- `!test welcome` - the greeting new viewers get
- `!test autochaos` - make ADA act now

Stop everything with `!panic`, pause chaos with `!pause`.
