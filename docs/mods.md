# Mods

The running list of gameplay mods in this fork, done and planned. The game as it shipped is always one setting away:
Battle mods sit behind **Battle Mode Alt** (General tab, `src/battle_tuning.cpp`), whose Original value is Battle as
it shipped.

## Battle Mode Alt

| Mod | Status | What it does |
|---|---|---|
| Double health | Done, played | Every hit does half its damage, so a car lasts twice as long. A machine gun round does 57 rather than 114 of 800, and the health bar stays right. |
| Machine gun spin-up | Done, played | A chain gun wind-up: 5 rounds a second from rest, up to 30 over 3 s of fire held, full speed kept for 20 frames after you let go, then back down to 5 over 1.5 s. As shipped it fires a steady 15. |
| Weapon damage tweaks | Planned | Per-weapon damage. All damage goes through one routine (below), so this goes beside double health. Which weapon is which type is still to be matched. |
| Speed on weapon pickup | Planned, harder | A speed change when a weapon is picked up. The pickup routine is known; the car's velocity isn't yet (a short recording driving straight, boosting and braking finds it). |
| Rammer cooldown, damage and push | Done, played; push still to tune | A 1 s refire rather than 0.333 s, so one long shove costs one ammo; damage 200 + 5 x speed rather than 160 + 7 x speed, then halved by double health like every hit; and a push of the target away from the rammer, flat along the ground: a force of 800000 (an impact's is 330000 plus 66000 up, with spin, which is what flips cars) times the rammer's speed over 80, from 0.25 to 1.25 times. |
| Rammer handling and reach | Planned | +5% speed and -5% steering while held, and a longer reach (the distance the target must be ahead to count) or a 25% stronger front block. |
| Sonic Boom push | Done, to play | Thrown out more, lifted and flipped less: the push away from the blast 1.5x (330000 as shipped, `0x80394DD0`), the lift 0.4x (66000, `0x80394DCC`), and the share of the push that goes into the car's spin 0.3x, so the flip is 0.45x. In its impact, `ovl2_func_8038D498`, which only the Sonic Boom (projectile type 7) calls. |
| Sonic Boom rate and speed | Planned | A slower fire rate, and 15% less top speed while held. Its effect needs a graphical fix. |
| Pea shooter | Done, to play | The weapon you have with no pickup (type 8): fires while fire is held, 6 a second (its reload as shipped), rather than once a press; a direct hit does 107 rather than 80 (then halved by double health). |
| Bounce balls | Sliders, to tune | Launch 75 along the aim plus the car's velocity, life 3.17 s (`0x80394EE0`), gravity 2.5x the world's (`0x80142764`), a bounce keeps 0.9 of the speed into the ground (`0x80394E54`) and all of the speed along it. Each, and a tilt of the launch up from the aim, is a slider of the Battle Tuning mod (`battle_tuning_mod/`, read by `src/battle_tuning_mod.cpp`), at its shipped value by default. Fire case in `ovl2_func_8038FCE0` (type 2 of the table at `0x80394EBC`), flight and bounce in `ovl2_func_8038E114`. |
| Powerup bars | Done, to play | A 64 x 3 pixel bar centred 6 pixels below the top of each player's view for each timed powerup held, emptying over its 30 s: invisibility in white (record +`0x38C` bit `0x1`, time left +`0x390`, also ended by firing), the shield in blue (bit `0x8` while its bubble grows, shown full, then `0x4` with time left +`0x394`; timers in `ovl2_func_8038F938`). Fill rectangles at the end of the frame's list (`func_800EDACC`); each view is found by its viewport + pipe sync + scissor in that list; Battle counts as running while the fire routine's player loop runs (count at `0x801543CA`). |
| Invisible arrow | Done, to play | An invisible player's arrow is hidden outright. The player marker's draw (`func_8008C884`) takes the record's fade alpha (+`0x3A1`) and floors it at `0x20`; the hook at `0x8008CB28` sets it to 0 while the record has invisibility (+`0x38C` bit `0x1`), except while it fades back in (+`0x3A2` state 2). |
| Plasma push | Done, to play | A plasma (type 0) direct hit pushes the car hit flat away from the shooter (the shot's velocity is zeroed by the hit), 400000 by default (a missile blast is 330000 plus lift and flip; the Rammer 800000 at speed 80, 0.25x to 1.25x by speed). A Battle Tuning slider, as is the Rammer's. Hooked with the pea shooter's damage in `ovl2_func_8038E114`: s2 the record hit, s6 the shooter's. |
| Damage and refire | Done, to play | Machine gun 114 to 80, bouncy ball 800 to 600 direct and 500 splash, heavy rocket 960 to 1320 direct and 1000 splash, mines 800 to 1600 (280 to a head-on Rammer kept), plasma refire 0.1667 s to 60/420 s. `ovl2_func_8038E114` sets the damage (s0) by projectile type and uses it for the direct hit (`0x8038F154`) and as the splash base (`ovl2_func_8038D798`, called at `0x8038ECD4`, `0x8038F0D4`, `0x8038F0F0`, `0x8038F1B4`, `0x8038F1D0`); the mine's 800 is passed at `0x8010C8F8`/`0x8010C920` in `func_8010C7F4`; the plasma case of the fire routine sets its refire at `0x803902E4`. |
| Upside-down self-destruct | Planned | A short countdown that destroys a car left upside down, perhaps with a vignette as control is lost and a 1-1.5 s window where it is vulnerable. |

## Other mods

| Mod | Status | What it does |
|---|---|---|
| Cheat Menu | Done, to play | Option Cheat Menu (General tab, `src/cheat_menu.cpp`). At Always Shown each of the game's eleven cheat flags (halfwords from `0x8011648C`) is held at `0x0101` (available, not switched on) on every retrace, so CHEATS is on the main menu from the start and needs no combos. |

## Other ideas

The owner's:

- Bespoke weapons and cars: new ones of their own, not just retuned stats. Far harder than tuning (a new weapon's
  model, effect and behaviour means rewriting game functions in C); worked up to after the tuning mods.
- Track editor. A big project: the track format (geometry, collision, AI paths, checkpoints, spawn points, arena
  weapon spawns) isn't worked out. Moving weapon pickups and spawn points in the Battle arenas is the realistic
  first step.

- An AI player: a small network trained by reinforcement learning to play Battle, with the game's memory as what it
  sees, damage dealt and taken as its reward, and controller input as what it does. The pieces that exist are input
  injection (`--input`, `--record`) and the memory map below. Missing: a fast bridge between the game and the
  trainer, running the game faster than real time without drawing (the `headless/` build may be the start), and a
  quick reset to the start of a round. First step: measure how fast the game can run, then train one to ram a parked
  car.

Suggested along the way, not yet asked for: handling and physics tweaks, Battle rules (score limits, timers,
respawn), practice HUDs (speedometer, splits), music control, randomizers.

## What the game keeps where (Battle)

Found by recording hand-played Battle sessions (`--record`, `--dump-ram`, `--park`, `--wwatch`; `docs/building.md`).

| What | Where |
|---|---|
| Player record | `0x80152818` + player × `0x3B8` (player 0 is player 1) |
| Car index / car | record +`0x35B`; the car is `0x8014A250` + index × `0x808` |
| Weapon | record +`0x384`, s8: 8 none, 1 machine gun, 4 missile launcher (3 ammo), 5 Rammer (5), 6 small missile launcher (20, one shot a press), 7 Sonic Boom (5), 3 mines (3; the mine's hit is `func_8010C7F4`: 800, or 280 to a Rammer that meets it head-on), 2 and 0 not named yet (20 each). Invisibility is held elsewhere. |
| Ammo | record +`0x385`, s8: -1 none; the machine gun comes with 100 |
| Health | record +`0x386`, s16: 800 full |
| Last damage taken | record +`0x388`, s16 |
| Flags | record +`0x38C`; bit 4 is the shield, which cuts a hit to 0.2 of its damage (`0x80394DC4`) |
| Speed | car +`0x3F0`, float |
| Refire timer | record +`0x3AC`, float seconds, counted down by the frame's time (`0x8002EB94`) |

| Code | Where |
|---|---|
| Spawn health (800) | `func_800D1004`, `0x800D1114` |
| Pickup (weapon, ammo; a health pickup sets 800) | `func_8010D3C0`, called from `func_800BEAA0`; weapon and ammo stored at `0x8010D5C4` |
| Damage (all hits) | `ovl2_func_8038D3A4` (attacker, victim, damage); nothing between team mates, the shield's 0.2, the damage kept at +`0x388`, then off the health at `0x8038D438` |
| Rammer | `func_800CE358` from `0x800CE544`, Battle only: on contact, holding type 5 with the refire timer run out, and the target ahead by more than 2.25, it spends one ammo, sets the timer to 0.333 s (`0x80124108`, `0x8012410C` for the other car's side) and does 160 + 7 × the rammer's speed (car +`0x3F0`) |
| Firing | `ovl2_func_8038FCE0`; the reload by weapon is a jump table at `0x80394EBC` |

Reload by weapon type, from that table:

| Type | Reload | Rate |
|---|---|---|
| 1 (machine gun) | 0.0667 s (`0x80394EE4`) | 15 a second |
| 0, 2, 6, 8 | 0.1667 s (`0x80394EB8`) | 6 a second |
| 4, 7 | 1 s | 1 a second (7 comes with 5 ammo) |
| 3, 5 | not set by the table | |
