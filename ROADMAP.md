# ChatControl roadmap

## Done (needs one full in-game test)
- Twitch chat (anonymous IRC), YouTube live chat (Data API v3)
- FICSIT Chips economy, price scaling, difficulty 1-3, chaos budget, Auto-Chaos
- Bits, subs, gift subs, raids, channel points, Super Chats, memberships
- Effects: hi, gift, biggift, rain, sink, pet, sign, day, night, jump, moon, heavy, speed, slow, fov, teleport, hub,
  strike, underclock, blackout, ambush, boss
- Viewer names above creatures (role colors, ranks), named pets for subs that stay in the save
- Greeting in the world on a viewer's first message, `!daily`, ranks by chips spent, `!name` for machines
- Effects: fly, shrink, giant, hungry, inventory, fog, dance, heal
- Votes, shared pot, `!top`, `!stats`, session summary file
- `!pause`, `!panic`, safe zone around the HUB
- OBS data endpoint + widgets (feed, vote, pot and top)
- In-game settings page, Russian/English, icon and logo

## Not possible or postponed
- `!overclock` above 100%: the game requires power shards, so `!underclock` is used instead
- `!drop`, inverted controls: no reliable game API found yet
- `!conveyor` (slow belts): belt speed is baked into the game's conveyor chains and cannot be changed at runtime
- Rain: rain is visual-only blueprint weather without a C++ switch; `!fog` covers bad weather
- Twitch follows and all channel point rewards: need a WebSocket client (the game does not ship one)
- Twitch polls/predictions: need an OAuth token with write scopes

## Dormant
- Twitch login (follows, auto channel point rewards, native polls) is implemented but hidden: Twitch requires 2FA
  to register an app and it did not work with Switty's phone number. Putting `TwitchClientId` into Settings.json
  brings the menu section back.

## Dropped
- Kick, Discord, `!order`, bets, multiplayer (decided 2026-10-31)
