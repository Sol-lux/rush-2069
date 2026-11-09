# Mods

The running list of gameplay mods in this fork, done and planned. The game as it shipped is always one setting away:
Battle mods sit behind **Battle Mode Alt** (General tab, `src/battle_tuning.cpp`), whose Original value is Battle as
it shipped.

## Battle Mode Alt

| Mod | Status | What it does |
|---|---|---|
| Double health | Done, played | Every hit does half its damage, so a car lasts twice as long. A machine gun round does 57 rather than 114 of 800, and the health bar stays right. |
| Machine gun spin-up | Done, played | A chain gun wind-up: 5 rounds a second from rest, up to 30 over 1.5 s of fire held, full speed kept for 20 frames after you let go, then back down to 5 over 1.5 s. As shipped it fires a steady 15. |
| Weapon damage tweaks | Planned | Per-weapon damage. All damage goes through one routine (below), so this goes beside double health. Which weapon is which type is still to be matched. |
| Speed on weapon pickup | Planned, harder | A speed change when a weapon is picked up. The pickup routine is known; the car's velocity isn't yet (a short recording driving straight, boosting and braking finds it). |

## Other ideas

The owner's:

- Bespoke weapons and cars: new ones of their own, not just retuned stats. Far harder than tuning (a new weapon's
  model, effect and behaviour means rewriting game functions in C); worked up to after the tuning mods.
- Track editor. A big project: the track format (geometry, collision, AI paths, checkpoints, spawn points, arena
  weapon spawns) isn't worked out. Moving weapon pickups and spawn points in the Battle arenas is the realistic
  first step.

Suggested along the way, not yet asked for: handling and physics tweaks, Battle rules (score limits, timers,
respawn), practice HUDs (speedometer, splits), music control, randomizers.

## What the game keeps where (Battle)

Found by recording hand-played Battle sessions (`--record`, `--dump-ram`, `--park`, `--wwatch`; `docs/building.md`).

| What | Where |
|---|---|
| Player record | `0x80152818` + player × `0x3B8` (player 0 is player 1) |
| Car index / car | record +`0x35B`; the car is `0x8014A250` + index × `0x808` |
| Weapon | record +`0x384`, s8: 8 none, 1 machine gun |
| Ammo | record +`0x385`, s8: -1 none; the machine gun comes with 100 |
| Health | record +`0x386`, s16: 800 full |
| Last damage taken | record +`0x388`, s16 |
| Refire timer | record +`0x3AC`, float seconds, counted down by the frame's time (`0x8002EB94`) |

| Code | Where |
|---|---|
| Spawn health (800) | `func_800D1004`, `0x800D1114` |
| Pickup (weapon, ammo; a health pickup sets 800) | `func_8010D3C0`, called from `func_800BEAA0`; weapon and ammo stored at `0x8010D5C4` |
| Damage (all hits) | `ovl2_func_8038D3A4`; the damage in a2 comes off the health at `0x8038D438` |
| Firing | `ovl2_func_8038FCE0`; the reload by weapon is a jump table at `0x80394EBC` |

Reload by weapon type, from that table:

| Type | Reload | Rate |
|---|---|---|
| 1 (machine gun) | 0.0667 s (`0x80394EE4`) | 15 a second |
| 0, 2, 6, 8 | 0.1667 s (`0x80394EB8`) | 6 a second |
| 4, 7 | 1 s | 1 a second (7 comes with 5 ammo) |
| 3, 5 | not set by the table | |
