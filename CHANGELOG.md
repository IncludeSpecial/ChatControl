# Changelog

## 0.1.2

### New
- **`!drop`**: a few random stacks fall out of the streamer's inventory into a crate that lands about 20 m away.
  If the crate cannot be placed, the items go back into the inventory.
- **OBS widgets come with the mod.** Event feed, vote and pot with top viewers work right after installing:
  `http://localhost:8787/obs/feed.html`, `vote.html`, `pot.html`. They have a neutral FICSIT look and follow the mod language.
- **Readable "widget not found" page** with links to all available widgets, instead of an empty error.
- **`!test` runs ordinary commands too**: `!test clear`, `!test daily`, `!test pot 100`. `!test sign hello` passes the text.
- **Full documentation**: setup, all commands, every setting, FAQ - in English and Russian (`docs/`).

### Fixed
- **`!hungry`** ended on its own after a few seconds, because natural health regeneration looked like eating.
  Now it ends only when food or medicine actually leaves the inventory, and regeneration is held back while it lasts.
- **The OBS web server** was reachable from the whole local network. It now listens on this PC only (`127.0.0.1`).
- **A custom widget folder** hid the built-in widgets. Both now work side by side.

## 0.1.1
First public release.

- Twitch chat (no token needed) and YouTube live chat (free API key)
- FICSIT Chips economy: activity, first message bonus, `!daily`, bits, Super Chats, subs and memberships; prices scale with active viewers
- 30+ effects: gifts, supply crates, Sink coupons, gravity, super jump, flight, shrink/giant, speed, teleport, fog, day/night,
  machine strikes, underclock, blackouts, hunger, inventory shuffle, ambushes and alpha bosses, freeze creatures, heal
- Viewers in the world: names and ranks above creatures, role colors, greetings with fireworks, signs, named machines and vehicles,
  pets for subscribers that stay in the save
- Votes, shared chip pot, leaderboard, ranks, session stats
- Raids bring ambushes and a boss
- Safety: chaos budget, cooldowns, death protection, safe zone around the HUB, `!pause`, `!panic`, `!clear`
- OBS data endpoint at `http://localhost:8787/state`
- In-game settings for everything, English and Russian
- Singleplayer only
