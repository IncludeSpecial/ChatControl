# ChatControl roadmap

## Done (needs one full in-game test)
- Twitch chat (anonymous IRC), YouTube live chat (Data API v3)
- FICSIT Chips economy, price scaling, difficulty 1-3, chaos budget, Auto-Chaos
- Bits, subs, gift subs, raids, channel points, Super Chats, memberships
- Effects: hi, gift, biggift, rain, sink, pet, sign, day, night, jump, moon, heavy, speed, slow, fov, teleport, hub,
  strike, underclock, blackout, ambush, boss
- Viewer names above creatures, named pets for subs
- Votes, shared pot, `!top`, `!stats`, session summary file
- `!pause`, `!panic`, safe zone around the HUB
- OBS data endpoint + widgets (feed, vote, pot and top)
- In-game settings page, Russian/English, icon and logo

## Not possible or postponed
- `!overclock` above 100%: the game requires power shards, so `!underclock` is used instead
- `!fog`, `!drop`, inverted controls: no reliable game API found yet
- Twitch follows and all channel point rewards: need a WebSocket client (the game does not ship one)
- Twitch polls/predictions: need an OAuth token with write scopes
- Publishing on GitHub and ficsit.app (needs Switty's GitHub account)

## Dropped
- Kick, Discord, `!order`, bets, multiplayer (decided 2026-10-31)
