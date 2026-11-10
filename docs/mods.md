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
| Cheat Menu | Done, played | Option Cheat Menu (General tab, `src/cheat_menu.cpp`). At Always Shown each of the game's eleven cheat flags (halfwords from `0x8011648C`) is held at `0x0101` (available, not switched on) on every retrace, so CHEATS is on the main menu from the start and needs no combos. |

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

## Lighting

Graphics enhancements in RT64 (each a Graphics-tab option whose Original value is the picture as shipped; changes to
RT64 go in `runtime-patches/`). VibinToast's RT64 fork for Jet Force Gemini (`VibinToast/rt64`, branch `jfg`, MIT) has
bloom, ambient occlusion, fog, a vignette, shadow maps, ray-traced shadows and water reflections, and is the starting
point for each.

| Enhancement | Status | Approach |
|---|---|---|
| Bloom | Done, run in a one-player race and 2-player Battle | Bright parts of the scene blurred and added back, once per frame before the HUD. The fork finds the HUD as the first draw without a depth test after at least 12 that write depth; if that splits a frame wrongly here, a marker at the scene's end (below) does it instead. Glow: textures picked by eye (RT64 texture hash, `src/config.cpp`: the sunset strip of the sky, the arrow signs' light strips, the green arrow lights, a light panel, the exhaust fire, light streaks) are drawn again into a mask after the scene, tested against its depth so only what is still in view counts (the sky, drawn with no depth test, only where the scene still shows it), and the mask's bright parts are added to the bloom at a gain, in the 16-bit float bloom buffer (`rt64-0015`). Run in the attract race on Track 1. Other tracks' textures and Battle's explosions are still to pick (`--dump-dl` + `--dump-ram` for a sheet, `--dump-textures` for the hashes). |
| Brightness, contrast, gamma | Done, run | In the VI pass over the whole picture after the VI's own gamma, so at no measurable cost: brightness ±0.04 a step, contrast ±10% a step around mid grey, a gamma exponent of 1.25 to 0.8 (`rt64-0013`). |
| HDR output | Done, run (scRGB swap chain made on D3D12; the look not yet judged on an HDR display) | An scRGB swap chain (R16G16B16A16_FLOAT, linear, 1.0 = 80 nits; `plume-0006`), read at startup. The VI pass turns the game's colours to linear light (gamma 2.2) with white at paper white, and lifts what is brighter than most of the picture (luminance above 0.5, cubed) towards the peak (`rt64-0014`); the 16-bit target is turned on with it against banding. The menus draw in 8-bit sRGB as before and are turned to linear at the same white where they reach the swap chain (`RecompFrontend-0009`). The scene target is still 0 to 1 (RT64's high precision is 16-bit unorm), so Bloom's glow, Glow's too, stops at white where it is added back: the bloom buffer could be added in the VI pass instead, past white, but that is after the HUD. |
| Shadows | Done, run in the attract race on Track 1 (one player; split screen not yet run); +0.14 ms of GPU a frame on an RX 6600 | Shadow mapping from one sun (`rt64-0016`). RT64's world position of a vertex is its position through the modelview alone, and this game keeps the camera's rotation in the projection, so it is the world's axes about the camera: each vertex is put on the CPU into the sun's orthographic view of a box 5000 units around a point ahead of its own view's camera. Before each 3D scene's HUD (found as Bloom finds it), per view, the opaque draws (indexed, perspective, depth test and write) are drawn from the sun into a 4096 shadow map, then again over the scene, where a pixel's depth is the draw's own (read in the shader: a depth test of equal missed most of the road), multiplying it by a blue-grey tint where the map has something nearer the sun; 4x4 taps soften the edge, and it fades out at the box's sides. The option's Light and Dark are the strength. `--shadow-sun x,y,z` tries another sun and `--shadow-debug` colours shadow, lit and outside the map (`docs/building.md`). Still to do, in order: (1) a sun per track, set by eye with `--shadow-tuner` (one sun for every track now: 0.3,-1,0.2; the tuner saves each track's to `shadow_suns.txt`, still to be built in); (2) what the game doesn't send casts nothing (Scenery Culling Off and Draw Distance Farthest send the most; with shadows on, `rt64-0017` stops `rt64-0008` dropping display lists off the view, not yet checked by eye for a building behind the camera); (3) the cars' blob shadows (texture `0x9807`) still drawn under the new ones; (4) the map's texels move with the camera (the box is about the camera, which RT64's world space doesn't place), so edges can shimmer; cascades for sharper near shadows. Shadows run only while a track is loaded (`src/config.cpp`), so the menus' 3D models cast nothing. The ground takes shadows too (`rt64-0019`): every perspective draw receives, so trees and cars shadow the road and grass; casters still draw whole quads into the map (no alpha test), so a tree's picture casts its quad's shape where its leaves don't fill it. Shadow acne on curved roads: `rt64-0018`'s receiver plane bias, not yet checked on the curves where it was seen. Still to check: Battle's HUD parts drawn as 3D models, which are drawn during a match. Contact shadows and ray tracing stay fallbacks. Paused as a side project; open problems, in order: (a) a large dark rectangle pops in under the player's car at some view angles (Track 1, sun az ~24 el 18): not the game's blob shadow, cause unknown; (b) on Track 2 the map mesh and some buildings cast nothing or look randomly shaded, because draws that write no depth don't cast (letting them cast, tried and dropped, made the rectangle and ground self-shadowing worse); (c) trees cast whole quads, so the Palace of Fine Arts' columns get tree-card blotches; (d) the car casts no shadow. The water's reflection takes and casts none (`rt64-0022`: a draw whose modelview times projection is mirrored). Idea to try first: a shadow dump key in `--shadow-tuner` that writes one frame's casters, receivers and sun box to a file when pressed, in place of a per-frame log (which made the HUD flicker), so the rectangle can be caught as it pops in. |
| Reflections | Planned | Screen-space reflections on chosen surfaces (the bay, glass, wet road). Chrome is already the game's own environment map. |

### Picking textures that glow

Glow's list is in `src/config.cpp` (the Bloom option), by RT64's texture hash. To add to it, for a track or a Battle
arena:

1. `--dump-dl <dl.txt> <t>` and `--dump-ram <prefix> <t>` together at the same second, where the textures are in view.
2. `tools/texsheet.py sheet <dl.txt> <prefix>0.bin <sheet.png>`: a numbered sheet of every texture that frame draws.
   The owner picks numbers by eye.
3. `--dump-textures <dir>` for a run long enough to draw them, then `tools/texsheet.py match <sheet.png> <dir> <n>...`
   for each number's hash: one difference far below the rest is the match (0 when exact).
4. A line in the list with a gain: 1.0 for the sky's strip, 1.5 for lights and fire. Only the texture's bright parts
   glow (above 0.35), so a strip whose lit cells change from texture to texture glows where it is lit.

Left out so far: the coins (shiny, not lights: the owner's call) and a plain 4x4 white texture on Track 1, which
would glow wherever it is used (to look at).

## What the game keeps where (lighting)

| What | Where |
|---|---|
| The frame's two lists | 3D at `0x801497C8`, HUD at `0x80149438` (`src/frame_list.cpp`); both built by `func_800A04C4`, the HUD list sealed by `func_800A48C8` and called and sent by `func_800EDACC` |
| A view | `func_8009F058`: viewport, pipe sync and scissor at `0x8009F128`-`0x8009F268` (in that order, which `src/widescreen.cpp` relies on), the perspective at `0x8009F4F0`, the node tree at `0x8009F9BC` |
| Views | count at `0x80146204`; table `0x8017A510`, `0x48` a view; cameras `0x8015B70` + view × `0x98` |
| Scene to HUD | `func_800A04C4` at `0x800A0754`: after the last view, before the HUD's full-screen scissor. A view starts at `0x800A0724` and ends at `0x800A0730` (view index in a0). |
| A model's lights | node +`0x1A` (s16, negative for unlit) and +`0x1C` (pointer to its light block); `func_8009DD88` sends 1 or 2 directional lights and an ambient from the block's +8, +0 and +`0x18`. There's no single sun: each model carries its own, so a sun for a view is its brightest directional light, as RT64 picks. The block's byte layout is still to confirm from a RAM dump. |
| Default lights | pointer `0x801406B8` to `0x8012EA18`, set once and never written |
