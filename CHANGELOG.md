# Changelog

## Unreleased
- New effect `!drop`: a few stacks fall out of the inventory into a crate about 20 m away
- OBS widgets now come with the mod: `http://localhost:8787/obs/feed.html`, `vote.html`, `pot.html` (setup in the README)
- A custom widget folder no longer hides the built-in widgets; a missing widget shows a page with the available ones
- `!hungry` ends only when you actually eat; natural regeneration no longer cancels it
- `!test` also runs ordinary commands (`!test clear`, `!test daily`) and passes text to effects (`!test sign hi`)
- The OBS web server is reachable from this PC only

## 0.1.1
First public release.

- Twitch chat (no token needed) and YouTube live chat (free API key)
- FICSIT Chips economy: activity, first message bonus, `!daily`, bits, Super Chats, subs and memberships; prices scale with active viewers
- 35+ effects: gifts, supply crates, Sink coupons, gravity, super jump, flight, shrink/giant, speed, teleport, fog, day/night,
  machine strikes, underclock, blackouts, hunger, inventory shuffle, ambushes and alpha bosses, freeze creatures, heal
- Viewers in the world: names and ranks above creatures, role colors, greetings with fireworks, signs, named machines and vehicles,
  pets for subscribers that stay in the save
- Votes, shared chip pot, leaderboard, ranks, session stats
- Raids bring ambushes and a boss
- Safety: chaos budget, cooldowns, death protection, safe zone around the HUB, `!pause`, `!panic`, `!clear`
- OBS widgets served by the mod at `http://localhost:8787/obs/`
- In-game settings for everything, English and Russian
- Singleplayer only
