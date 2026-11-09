# Weapon stats (2P Battle)

Damage is per hit out of 800 health, before Double Health halves it. Rate is the refire, in rounds a minute.
Names from your sessions: 111406 (light missile, heavy missile, bouncy ball, mines, plasma in turn) and 134653
(plasma, mines, bouncy ball, Sonic Boom). Measured damage matches the code's numbers in both.

Lock-on: the game runs its aim routine (`ovl2_func_8038F568`) only for weapons 0, 1, 6 and 8. It picks the nearest
enemy ahead within 2000 and swings the aim toward it 0.01 rad a frame (about 34°/s at 60 fps), the same for all of
them. The others fire straight.

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
{Suggest changes in {}, updated when it's coded}

### Snapshot 1

| #   | Weapon              | Ammo | Damage                                                  | Rate (rpm)                                          | Lock-on                                          |
| --- | ------------------- | ---- | ------------------------------------------------------- | --------------------------------------------------- | ------------------------------------------------ |
| 8   | Pea shooter         | ∞    | ==107 direct (+33%)==                                   | 360, ==auto fire while held==                       | yes                                              |
| 1   | Machine gun         | 100  | 114 {80}                                                | ==300 from rest, spins up to 1800 over 1.5 s held== | yes                                              |
| 6   | Light missile       | 20   | 520 / splash 520                                        | 360                                                 | yes                                              |
| 4   | Heavy 3-shot rocket | 3    | 960 / splash 960                                        | 60                                                  | no                                               |
| 2   | Bouncy ball         | 20   | 800 / splash 800 {600 / 500}                            | 360                                                 | no                                               |
| 0   | Plasma blaster      | 20   | 400 {push out onefold (rammer is fourfold)}             | 360 {420}                                           | yes in the code                                  |
| 3   | Mines               | 3    | 800 on touch                                            | one drop a press                                    | n/a                                              |
| 7   | Sonic Boom          | 5    | 800 / 400 / 200; ==push 1.5× out, 0.4× up, flip 0.45×== | 60                                                  | no                                               |
| 5   | Rammer              | 5    | ==200 + 5 × speed push fourfold==                       | ==60 max (1 s)==                                    | n/a; ==pushes the target away along the ground== |

# Snapshots

Stats known to work, to roll back to: `git checkout snapshot-N -- src/battle_tuning.cpp include/rush2049_game.h us.toml`
(then `tools/recompile.sh` and a build).

- **Snapshot 1** (2026-11-09, tag `snapshot-1`): the Battle Mode Alt table above as it stands, plus the cooldown bar.

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
- ~~Powerups give a small cooldown bar at the bottom~~ coded, to play: 64 x 3 px amber bar, bottom centre of each view,
  for reloads of 0.25 s and up (size, gap and threshold are constants in `include/rush2049_game.h`)

# Mod: Armor Rehaul
will get merged into Battle+
Heavy > H. Alloy > Standard > S. Alloy > Light > L. Alloy
