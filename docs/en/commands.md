# Commands and effects

[Setup](setup.md) · [Settings](settings.md) · [FAQ](faq.md) · [На русском](../ru/commands.md)

Commands are typed in Twitch or YouTube chat. Answers appear in the game chat and in the OBS event feed.

## Everyone

| Command | What it does |
|---|---|
| `!chips` / `!чипы` | Your chip balance and rank |
| `!daily` / `!бонус` | Daily chip bonus, once per calendar day |
| `!give <nick> <amount>` / `!передать` | Give chips to another viewer. 10-500 per transfer, 10% goes to the pot, 1000 sent and 1000 received per day, one transfer a minute; only to viewers who have chatted before |
| `!effects`, `!help` | All enabled effects with current prices |
| `!1` `!2` `!3`, `!vote N` / `!голос N` | Vote during a chat vote (free, gives a few chips) |
| `!pot N` / `!копилка N` | Put N chips into the first shared pot; when full, it summons a boss (on new installs) |
| `!pot titan N` | Put N chips into the second pot; when full, it summons a titan. `!pot` alone shows how both stand |
| `!top` / `!топ` | Top viewers by chips spent |
| `!stats` / `!статистика` | Stats of the current session |

## Effects

Prices are the defaults at difficulty 2 with one active viewer. They grow by 5% per extra active viewer (up to x3)
and change with difficulty. `!effects` shows the current prices. Every number below can be changed in the settings.

**Harmful** effects go through the chaos queue: they respect the chaos budget, `!pause`, death protection
and (for creatures) the safe zone around the HUB, so they may run a little later.

### Free
| Command | Effect |
|---|---|
| `!hi` | Waves at the streamer |
| `!gift` | Half a stack of a basic resource (iron, wire, concrete...) |

### Gifts and help
| Command | Price | Effect |
|---|---|---|
| `!biggift` | 150 | Half a stack of an advanced part (rotors, motors, computers...) |
| `!rain` | 100 | Supply crate with 5 stacks in front of the streamer |
| `!sink` | 300 | AWESOME Sink coupon |
| `!heal` | 80 | Full health |
| `!dance` | 40 | Freezes all creatures within 50 m for 6 s |
| `!day` | 60 | Sets the time to noon |

### Viewers in the world
| Command | Price | Effect |
|---|---|---|
| `!sign <text>` | 20 | A floating sign with the viewer's name and text, stays for 10 min |
| `!pet` | 100 | The viewer's own lizard doggo with their name; 50% chance it is already tame |
| `!name` | 200 | Signs the machine or vehicle the streamer is looking at (or the nearest one). Stays in the save; one per viewer |

### Chaos: player (harmful)
| Command | Price | Effect |
|---|---|---|
| `!jump` | 30 | Super jump for 20 s |
| `!moon` | 50 | Moon gravity for 30 s |
| `!heavy` | 60 | Heavy gravity for 20 s |
| `!speed` | 40 | Everything the player does is faster for 20 s |
| `!slow` | 50 | Slow motion for 20 s |
| `!fov` | 40 | Camera warp for 30 s |
| `!fly` | 60 | Lifts the player 8 m into the air for 15 s, then a soft landing |
| `!shrink` | 50 | Tiny player for 30 s |
| `!giant` | 50 | Giant player for 30 s |
| `!teleport` | 100 | Teleport to a random spot about 80 m away |
| `!hub` | 150 | Teleport back to the HUB |
| `!night` | 60 | Sets the time to midnight |
| `!fog` | 60 | Thick fog for 60 s |

### Chaos: survival (harmful)
| Command | Price | Effect |
|---|---|---|
| `!hungry` | 80 | Health drains for 60 s until the streamer eats something; never goes below 10% |
| `!inventory` | 100 | Shuffles the inventory slots |
| `!drop` | 120 | 3 random stacks fall out into a crate about 20 m away |

### Chaos: factory (harmful)
| Command | Price | Effect |
|---|---|---|
| `!strike` | 120 | 5 random machines stop for 60 s |
| `!underclock` | 120 | 10 random machines run at half speed for 60 s |
| `!blackout` | 200 | All generators stop for 15 s |

### Chaos: creatures (harmful)
| Command | Price | Effect |
|---|---|---|
| `!ambush` | 80 | 3 enemies appear around the streamer after a 3-5 s warning |
| `!boss` | 300 | An alpha monster after a 3-5 s warning |

Creatures carry the viewer's name and rank, leave after 5 minutes and never spawn near the HUB.

### Titans (harmful)
| Command | Price | Effect |
|---|---|---|
| `!titan` | 1000 | A boss fight after a 5-8 s warning: a random boss with 1-2 random traits, three stages and a health bar |

**Bosses:** Hog King, Cliff Tyrant, Nuclear Hog (its minions explode), Stinger Queen and Stinger Lord (swarms),
Green, Fire, Sand and Deep Matriarch (spitters of other elements, night and fog), Nest Mother (lays crab nests that burst into flying crabs),
and loot titans - Sky Giraffe, Colossal Beetle, Giant Strider - that follow you, stomp when you get close, charge at you, leap and slam the ground from stage 2, and drop loot at every stage.

**Traits:** Armored (more health), Swift (faster), Colossal (bigger), Undying (heals when left alone), Summoning (minions every 30 s),
Nocturnal (night), Misty (fog), Sparking (blackout at stage 2), Bloodthirsty (heals when it hits you), Rabid (faster as it gets hurt), Radioactive (hurts you within 10 m),
with a Jetpack (climbs, hangs over you and dives at the spot where you stood: move away to dodge, and it is stunned for 2 s after landing; if you are in the air, it rams you and throws you down; under a roof, in a cave or in the safe zone it stays on foot).

**Stages:** at 66% the boss gets angry and calls help, at 33% it goes all in. One titan at a time; another one waits in the queue.
**Flying:** you can't wait out a fight in the air. After 3 s in flight your jetpack loses half its fuel and you take damage every 2 s, and the hover pack shuts down.
**Out of reach:** if you stay at least 5 m above or below a titan for 6 s, it gets stuck and can't reach you, or it hasn't hurt you for 12 s, it flies over to you, jetpack or not. Under a roof it stays on foot.
**Victory:** a loot crate and 50 chips for every active viewer. **Defeat:** the summoner gets 200 chips. After 5 minutes the titan leaves.
`!boss` stays a quick alpha monster without a fight.

## Streamer and moderators

| Command | What it does |
|---|---|
| `!pause` | Pauses harmful effects (they wait in the queue). Again to resume |
| `!panic` | Cancels everything: queue, timed effects, creatures, signs |
| `!clear [nick]` / `!убрать [nick]` | Removes all signs, or only the signs of that viewer |

## Streamer only

| Command | What it does |
|---|---|
| `!test <effect> [text]` | Runs any effect for free, e.g. `!test boss`, `!test sign hello` |
| `!test titan <boss> <traits>` | A titan fight with a chosen boss: `hogking`, `cliff`, `nuclear`, `queen`, `stinger`, `green`, `fire`, `sand`, `deep`, `crabs`, `giraffe`, `beetle`, `strider`; and traits: `armored`, `swift`, `colossal`, `undying`, `summoning`, `nocturnal`, `misty`, `sparking`, `bloodthirsty`, `rabid`, `radioactive`, `jetpack`. Both are optional |
| `!test as <nick> <message>` | A plain viewer says it, e.g. `!test as bob !chips`. The viewer stays in the chip list |
| `!test chips N` | Gives you N chips |
| `!test bits N`, `!test sub`, `!test gifts N`, `!test raid N` | Simulates bits, a sub, gifted subs, a raid |
| `!test welcome` | Shows the greeting new viewers get |
| `!test autochaos` | ADA triggers a random effect now |
| `!test <command>` | Runs an ordinary command, e.g. `!test clear`, `!test daily` |

## What happens on its own

| Event | Result |
|---|---|
| Chatting | 10 chips every 5 minutes for everyone who wrote in the last 15 minutes |
| First message in a session | +20 chips and a greeting with the viewer's name and fireworks next to the streamer |
| Bits | 2 chips per bit; a command in the cheer message runs right away |
| Super Chat / Super Sticker | 5 chips per currency unit; a command in the message runs right away |
| Sub, resub, new membership | 500 chips and the viewer's pet, which stays on the base |
| Gifted subs / memberships | 500 chips per gift to the gifter |
| Raid | One ambush per 5 raiders (up to 5), plus a boss for 30+ raiders or a titan for 60+ |
| Every 10 minutes | A chat vote between three effects for 60 s |
| Quiet chat for 10 minutes | Auto-Chaos: ADA triggers a random effect |
| Spending chips | Ranks: Pioneer, Engineer (200), Senior Engineer (1000), Project Lead (3000), FICSIT Director (10000) |

Name tags are colored by chat role: streamer red, moderators cyan, VIP pink, subscribers purple.
