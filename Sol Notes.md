# Weapon stats (2P Battle)

Damage is per hit out of 800 health, before Double Health halves it. Rate is the refire, in rounds a minute.
Names from your sessions: 111406 (light missile, heavy missile, bouncy ball, mines, plasma in turn) and 134653
(plasma, mines, bouncy ball, Sonic Boom). Measured damage matches the code's numbers in both.

Lock-on: the game runs its aim routine (`ovl2_func_8038F568`) only for weapons 0, 1, 6 and 8. It picks the nearest
enemy ahead within 2000 and swings the aim toward it 0.01 rad a frame (about 34°/s at 60 fps), the same for all of
them. The others fire straight.

Accuracy: only the machine gun has spread. Each round gets a random yaw and pitch offset of up to ±0.0125 rad
(±0.72°, uniform, constants at `0x80394EE8`/`0x80394EEC` and `0x80394EF0`/`0x80394EF4` in the MG case of
`ovl2_func_8038FCE0`, from the game's RNG `func_8008B2E4`). That's about ±25 units of drift at the lock-on range
of 2000. Every other weapon, the pea shooter and light missile included, fires exactly along its aim. The other
random calls in the fire code only vary sounds and effects.

## Original

| # | Weapon | Ammo | Damage | Rate (rpm) | Lock-on |
|---|---|---|---|---|---|
| 8 | Pea shooter (no pickup) | ∞ | 80 direct | 360, one shot a press | yes |
| 1 | Machine gun | 100 | 114 | 900, held | yes |
| 6 | Light missile (small missile launcher) | 20 | 520 direct, splash up to 520 (radius 30) | 360, one shot a press | yes |
| 4 | Heavy 3-shot missile | 3 | 960 direct, splash up to 960 | 60 | no |
| 2 | Bouncy ball | 20 | 800 direct, splash up to 800 | 360 | no |
| 0 | Plasma blaster | 20 | 400 direct | 360 | yes in the code (you saw none) |
| 3 | Mines | 3 | 800 to the car that touches one; 280 to a Rammer that meets it head-on | one drop a press | n/a |
| 7 | Sonic Boom | 5 | 800 / 400 / 200 by distance; push 330000 out + 66000 up, plus flip | 60 | no |
| 5 | Rammer | 5 | 160 + 7 × speed | 180 max (0.333 s) | n/a |

Cars touching do no damage on their own: only the Rammer hurts on contact. A shield cuts any hit to 0.2×.

## Battle Mode Alt (now)

Changes are ==highlighted==. On top of these, every hit is halved (Double Health), so e.g. a mine does 400.
{Suggest changes in {}, remove and update entry when it's coded}

### Snapshot 2wip

| #   | Weapon              | Ammo | Damage                                                        | Rate (rpm)                                                                                                               | Lock-on                                          |                                                  |
| --- | ------------------- | ---- | ------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------ | ------------------------------------------------ |
| 8   | Pea shooter         | ∞    | ==107 direct (+33%)==                                         | 360, ==auto fire while held==                                                                                            | yes                                              |                                                  |
| 1   | Machine gun         | 100  | ==80==                                                        | ==300 from rest, spins up to 1800 over 3 s held, down over 1.5 s== {may need a whir up and down sound to help indicate.} | yes                                              |                                                  |
| 6   | Light missile       | 20   | 520 / splash 520                                              | 360                                                                                                                      | yes                                              |                                                  |
| 0   | Plasma blaster      | 20   | 400; ==push away from the shooter (slider, 620000 played)==   | ==420== {Auto fire}                                                                                                      | yes in the code                                  |                                                  |
| 3   | Mines               | 3    | ==1600 on touch== (280 to a head-on Rammer)                   | one drop a press                                                                                                         | n/a                                              |                                                  |
| 5   | Rammer              | 5    | ==200 + 5 × speed; push 800000 at speed 80 (200000-1000000)== | ==60 max (1 s)==<br>{What if circle used a charge and gave a boost? }                                                    | n/a; ==pushes the target away along the ground== |                                                  |
| 2   | Bouncy ball         | 20   | ==600 / splash 500== {680, 600}                               | 360                                                                                                                      | no                                               | {The trail is offset by ball's radius sometimes} |
| 7   | Sonic Boom          | 5    | 800 / 400 / 200; ==push 1.5× out, 0.4× up, flip 0.45×==       | 60                                                                                                                       | no                                               |                                                  |
| 4   | Heavy 3-shot rocket | 3    | ==1320 / splash 1000==                                        | 60                                                                                                                       | no                                               |                                                  |
{Snapshot 1 was supposed to get a 'Vulnerability timer' for a flipped car, instead of instantly exploding, you get a cooldown before actually exploding, allowing an enemy to finish you off. This carries to Snapshot 2}

| Other changes |                      |                           |                            |     |     |
| ------------- | -------------------- | ------------------------- | -------------------------- | --- | --- |
| Powerups      | - added cooldown bar | {increase spawn rate 15%} | {make new spawn locations} |     |     |
# Snapshots

Stats known to work, to roll back to: `git checkout snapshot-N -- src/battle_tuning.cpp include/rush2049_game.h us.toml`
(then `tools/recompile.sh` and a build).

- **Snapshot 1** (2026-11-09, tag `snapshot-1`): the Battle Mode Alt table above as it STOOD, plus the cooldown bar.

# Still to do

- Self-destruct countdown for upside-down cars (could have a vignette as a 'death' signal for the loss of control,
  with maybe a 1-1.5 s period where the car is vulnerable)
- Sonic Boom: slower fire rate; lowers max speed by 15%.
	==needs graphical fix/overhaul==
- Rammer: speed +5%, steering -5%, front armour mult from .35 to .25
- Light missile: faster lock-on pan (lock-on is shared at 0.01 rad/frame, so this means giving type 6 its own);
  lock-on range too (2000 for all)
- Bouncy ball: damage too high for 20 fast shots (about 400 a ball after halving); wants a number, ideally tuned
  in-game
- Mines: the first two in 111406 did no damage with the enemy right there
- An in-game way to dial stats: a mods-menu panel, or F1-F10 keys with an overlay
- ~~Powerups give a small cooldown bar~~ coded, to play: 64 x 3 px bar, top centre of each view, one per timed
  powerup (invisibility white, shield blue; both 30 s), emptying as it runs out (colours and size are constants in
  `include/rush2049_game.h`)
- ~~Invisibility hides the arrow~~ coded, to play. Next: the car's own alpha (the game keeps it at 0x20 or more,
  `func_8008C884`; the fade is record +0x3A1, 255 down to 16 by 8 a frame)
MAPPING:
Large missile rocket flying speed (+10%)
 
# Mod: Armor Rehaul
will get merged into Battle+
Heavy +10% armor > H. Alloy > Standard > S. Alloy > Light > L. Alloy +5% speed
