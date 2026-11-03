# Settings

[Setup](setup.md) · [Commands](commands.md) · [FAQ](faq.md) · [На русском](../ru/settings.md)

Everything is set in the game: **Mods -> ChatControl**. Changes apply within a second, no restart needed
(except the language of the settings page). Default values are in brackets.

## Main
- **Mod enabled** (on) - turns all chat effects on or off.
- **Twitch channel** (empty) - your channel name from `twitch.tv/<name>`. Empty = Twitch off.
- **Language** (`auto`) - `auto` follows the game, or `en` / `ru`. Game chat messages and widgets switch at once,
  the settings page after a restart.
- **Difficulty** (2) - 1 = easy: prices x1.5, half the creatures. 2 = normal. 3 = hardcore: prices x0.6, double creatures.

## YouTube chat
- **API key** (empty) - YouTube Data API v3 key. Empty = YouTube off. See [Setup](setup.md#3-connect-youtube-optional).
- **Channel or stream** (empty) - channel id `UC...` or a stream link.
- **Check chat every (s)** (10) - lower is faster but uses the daily quota sooner.
- **Chips per 1 currency unit of Super Chat** (5).

## FICSIT Chips
- **Chips for activity** (10) - given to every active viewer at each payout.
- **Payout every (min)** (5).
- **Viewer counts as active for (min)** (15) - only viewers who wrote within this time get payouts and raise prices.
- **Announce payouts in game chat** (on).
- **First message bonus** (20) - once per session.
- **Chips per bit** (2), **Chips per sub** (500) - subs, resubs, memberships and each gifted sub.

## Chip transfers (!give)
- **Viewers can give chips** (on).
- **Smallest transfer** (10), **Biggest transfer** (500).
- **Fee to the pot (%)** (10) - the receiver gets the amount minus the fee, the fee goes to the shared pot.
- **Send limit per day** (1000), **Receive limit per day** (1000) - per viewer and calendar day, 0 = no limit.
- **Cooldown between transfers (s)** (60).

Limits keep viewers from pooling everything on one account or farming chips with second accounts.

## Effects
Grouped by kind. Every effect has its own block:
- **On** - the command works at all.
- **Price** - in chips, 0 = free. Grows with active viewers and depends on difficulty.
- **Cooldown (s)** - how often one viewer can use it.
- **Duration (s)** - for timed effects; for `!sign` how long the sign stays.
- **Strength** - depends on the effect: multiplier for jump and gravity, share of a stack for gifts, meters for `!teleport`,
  `!fly` and `!drop`, size for `!shrink`/`!giant`, HP per 2 s for `!hungry`, fog density for `!fog`, radius for `!dance`,
  machine speed for `!underclock`.
- **Count** - creatures for `!ambush`/`!boss`, machines for `!strike`/`!underclock`, stacks for `!rain`/`!drop`, coupons for `!sink`.
- **In random** - can be picked by votes, Auto-Chaos and the pot.
- **Delay from / to (s)** - warning time before creatures appear.

## Auto-Chaos
- **Enabled** (on) - ADA triggers a random effect from the "In random" pool when chat is quiet. Works even without viewers.
- **After quiet time (min)** (10).

## Chat games
- **Votes** (on), **Vote every (min)** (10), **Vote lasts (s)** (60), **Chips for voting** (5).
- **Shared pot** (on), **Pot effect** (`titan` on new installs, `boss` before 0.1.3), **Pot size** (1000) - viewers fill it with `!pot N`; when full, the effect runs.

## OBS overlay
- **Enabled** (on), **Port** (8787, 1024-65535, applies at once; an invalid port is reset to 8787, a busy one falls back to 8787).
- **Widget folder** (empty) - a folder with your own widgets. The built-in `feed.html`, `vote.html`, `pot.html` work either way.

## Viewers in the world
- **Greet new chatters in the world** (on), **Greeting stays for (s)** (8), **Fireworks with the greeting** (on).
- **!daily bonus** (50) - 0 = off.
- **Viewer ranks** (on), **Custom ranks** (empty) - format `0=Rookie;500=Pro;2000=Legend`. Empty = Pioneer, Engineer (200),
  Senior Engineer (1000), Project Lead (3000), FICSIT Director (10000).
- **Pet for a subscription** (on), **Pets stay forever** (on) - kept in the save; off = they leave after **Pets stay for (s)** (1800).
- **Chance a pet comes tamed** (0.5) - 0 = never, 1 = always.
- **Viewer names above creatures** (on).
- **Name color by chat role** (on) and the colors (hex): streamer `#FF2A4A`, moderators `#19E3FF`, VIP `#FF6AD5`,
  subscribers and members `#B57BFF`. Viewers without a role keep the usual color.

## Raids and titans
- **1 ambush per N raiders** (5), **Max ambushes** (5), **Boss from N raiders** (30, 0 = never), **Titan from N raiders** (60, 0 = never).
- **Titan defeated: chips per active viewer** (50), **Titan won: chips for the summoner** (200).
- **Peaceful loot titans** (on) - giant giraffes, beetles and striders that drop loot.
- The titan's own block in **Effects** sets its price, **Strength** (health multiplier, 1 = normal), **Count** (max traits),
  **Duration** (time limit of the fight) and the warning delay.

## Advanced
- **Chaos budget per minute** (4) - chaos points harmful effects may use per minute; the rest waits in the queue.
- **Death protection (s)** (60) - no harmful effects for this long after you die.
- **Price growth per viewer (%)** (5), **Max price multiplier** (3).
- **Safe zone around the HUB (m)** (40) - creatures wait while you are this close to the HUB. 0 = off.
- **Creatures spawn from / to (m)** (6 / 10), **Creatures despawn after (s)** (300).
- **Ignored users** - comma-separated chat logins (bots).
- **Manual channel point rewards** - `rewardId=effect`, comma-separated, for Twitch rewards with text input you made yourself.
  The mod prints unknown reward ids to the game chat.
- **Save session summaries** (on).
- **Effect limits** - per effect: **Global cooldown (s)** (how often anyone can use it) and **Chaos points**.

## Files

All in `%LOCALAPPDATA%\FactoryGame\Saved\ChatControl\`:

| File | Contents |
|---|---|
| `Settings.json` | All settings (the menu edits the same file) |
| `Economy.json` | Viewer chips, chips spent, daily bonus dates, the pot |
| `Worlds\<session>.json` | Named machines and pets of each save |
| `Sessions\` | A summary of every session: length, effects, deaths, most active viewers |

Delete `Economy.json` to reset all chips, delete `Settings.json` to reset all settings.
