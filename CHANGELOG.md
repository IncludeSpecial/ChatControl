# Changelog

## 0.1.4

### New
- **Two shared pots**: one for a boss and one for a titan, each with its own size. `!pot 100` fills the first, `!pot titan 100` the second, `!pot` shows both. Both are on the `pot.html` widget (now 640 x 450) and can be changed in **Chat games**.
- **New OBS widget `balance.html`**: the chips of the viewers in chat, with the last change next to each number, so nobody has to type `!chips`.
- `!test titan <boss> <traits>` picks the traits too, e.g. `!test titan queen radioactive`.
- `!test as <nick> <message>` writes as a plain viewer, to try `!give` and the like without a second account.

### Changed
- Titans with a Jetpack really fly, peaceful ones too: they wear the game's jetpack, shake on the ground as the engines fire up, climb 18 m, hang over the player, then dive at the spot where the player stood. The landing makes a shockwave and stuns the titan for 2 seconds. Moving away during the dive dodges the hit, and a player who runs far away gets chased sooner.
- You can't wait out a titan in the air: after 3 seconds of powered flight your jetpack loses half its fuel and you take damage every 2 seconds, and the hover pack shuts down. Falling, parachutes and jump pads don't count. Titans with a Jetpack ram a flying player and throw them down. Can be turned off under **Raids and titans**.
- Titans with a Jetpack stay on foot while you are under a roof, in a cave or in the safe zone.
- You can't shoot a titan from safety: if you stay at least 5 m above or below it for 6 seconds, it gets stuck and can't reach you, or it hasn't hurt you for 12 seconds, it flies over to you, jetpack or not. Can be turned off under **Raids and titans**.
- Titans push you out from under them, so the camera doesn't end up inside the model, and the shove hurts. Can be turned off under **Raids and titans**.

### Fixed
- The health bar of a defeated titan no longer stays in the air, and a titan killed in flight drops its loot on the ground.
- A titan's minions appear around the titan when there is no room around the player, instead of not at all.
- Shockwaves no longer reach a player far above or below them, and don't throw a player out of a vehicle, off a zipline or out of a hypertube.
- Loot titans don't stomp from the air.
- Viewers could choose the boss by typing `!titan giraffe`, even with peaceful titans turned off. Now only `!test` can.
- Russian titan messages no longer use "он" for female bosses or forms like "победил(а)".

## 0.1.3

### New
- **Titans (`!titan`)**: a real boss fight instead of a single monster.
  - 13 bosses with their own fights: Hog King, Cliff Tyrant, Nuclear Hog, Stinger Queen, Stinger Lord, four Spitter Matriarchs
    (green, fire, sand, deep), Nest Mother (lays crab nests), and loot titans that stomp (Sky Giraffe, Colossal Beetle, Giant Strider).
  - 12 random traits that change the fight and the name: Armored, Swift, Colossal, Undying, Summoning, Nocturnal, Misty,
    Sparking, Bloodthirsty, Rabid, Radioactive, with a Jetpack.
  - Bigger size, many times more health, they go straight for the streamer, three stages (at 66% and 33%) with rage, minions, swarms, night and fog.
  - Health bar, stage and the summoner's name above the boss, and a new OBS widget `http://localhost:8787/obs/boss.html`.
  - Victory drops a loot crate and pays every active viewer; if the titan wins, the summoner gets chips.
  - Default goal of the shared pot for new installs; raids with 60+ viewers bring a titan. `!test titan <boss>` picks a boss.
- **Chip transfers**: `!give <nick> <amount>` / `!передать`, with limits per transfer and per day, a cooldown and a fee to the shared pot. All configurable.
- **Mod developer highlight**: the author of ChatControl gets a "Mod developer" title, a rainbow glitch name tag and highlighted feed events on any stream (recognized by Twitch account id; can be turned off).
- **OBS port** applies at once, without restarting the game.

### Fixed
- An invalid OBS port (below 1024, e.g. a typo like `1`) is reset to 8787, in the menu and on load.
- If the chosen OBS port is taken by another program, the widgets fall back to 8787.


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
