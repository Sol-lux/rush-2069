# Netplay

Everything that has to be done to play Race, Stunt and Battle online, and what is known so far.

## What it is to be

- Two to four players, each on their own machine with their own ROM, each seeing their own view full screen.
- Search for a match by mode: Race, Stunt and Battle each a toggle, and a match is made in a mode both sides have on.
- Delay-based play: every machine runs the same game from the same inputs, and each input is held a few frames so
  that it reaches the others in time. No rollback: the game never restores an earlier state. Rollback is only looked
  at again if delay-based play does not feel good enough.
- Players connect to each other directly. A small server only pairs them. There is no relay: two players whose
  routers cannot reach each other cannot play, until a relay is added.
- Rewind stays single-player only, hard-coded, offline and online.
- Offline play is unchanged: everything here is off unless an online session is running.

## What is known

Measured with the sync probe (`src/sync_probe.cpp`; `--sync-record`, `--sync-replay`, `--sync-hash`, `--sync-dump`
in `docs/building.md`): a 2-player Battle round recorded once and replayed twice, memory compared at the same
controller reads.

- **Input.** Thread 5 (`func_800E7808`) waits for a retrace, reads the four controllers (`func_800E73D8`,
  `osContGetReadData` into four 6-byte pads on its stack) and, in a race, runs the tick (`func_800E6AF8`). The hook
  just after the read (`0x800E7500`) takes or replaces all four pads. That is where network input goes in.
- **The tick is a fixed step.** The race clock is the count of ticks (`0x80143FF4`) times a constant, and each car's
  step is the ticks since its last. No wall clock.
- **What already plays the same.** Boot, menus and loading (the race starts on the same read each run), the tick
  count, the timer's count of 60ths (`0x8002EB64`), and the player's car and health through the round.
- **What does not.** The random number state (`0x8011735C`, the generator is `func_8008B2E4`) differs between runs
  already in the menus. Of about 5,100 words a round writes, about 1,400 differ: lists and pools in the Battle
  overlay (`0x80399B60`-`0x8039AFC0`) and at `0x8013C378`, a float in each player's record (+`0x374`), a few floats
  at `0x8014D294` and `0x80160328`.
- **Why, as far as it is understood.** Weapons, effects and the random generator run in the frame loop (thread 7,
  `func_800FD464`), which draws a frame when the last one is done, so how many frames fall between two ticks depends
  on the machine. Not yet traced call by call.

## To do

In order. Each stage is tested before the next is started.

### 1. The same game from the same input

The rest stands on this. Done when two replays of one recording write the same `--sync-hash` file for a whole
round, in each mode, and when two different machines do.

- [ ] A session mode that locks the frame loop to the tick: one frame of game logic for each tick, always in the
      same order, with the frame's time (`0x8002EB94`) fixed at 1/60 s. A machine that cannot keep 60 slows the
      game down; it never skips.
- [x] Find how the frame loop waits for a frame and where it takes its time from (`src/frame_lock.cpp`'s header).
- [ ] Which of the frame loop's work is logic (changes the race) and which is only drawing.
- [ ] The game's 60ths timer (`0x8002EB64`, the scheduler's retrace count) and the clock and countdown from it
      follow the retraces even under `--frame-lock`; about twenty places read it.
- [x] `--frame-lock` (`src/frame_lock.cpp`): one frame of the frame loop per step of thread 5, each 1/60 s. It
      boots and holds 60 steps a second. Two locked boots into the attract race still differ from its first tick, and
      the race starts on a different read (552 and 933), as unlocked boots do (513 and 791): the loads take machine
      time, and the attract race follows the random state. So the test of the lock is a recording replayed, not two
      boots.
- [ ] Set the random state at the start of a match from a seed both sides share.
- [ ] Find every caller of the random generator and sort them: those in drawing code (effects, sounds) must either
      run the same number of times on every machine or use a generator of their own.
- [ ] Anything the game does only for what is in view (culling, level of detail, effects) must not change the race:
      check Draw Distance, Scenery Culling and Level of Detail, which differ by setting and by view.
- [ ] Explain each remaining cluster of differing words, and either fix it or show it is only drawing and leave it
      out of the hash.
- [ ] What starts a match must be the same on both sides: the state the menus leave behind (the attract race's
      leftovers in the unused cars already differ), profiles, unlocked cars and tracks, the Controller Pak.
- [ ] Extend what counts as the race's memory to Battle and Stunt. Rewind's list was made for single-player races;
      the Battle overlay's data and each player's record need the same sorting.
- [ ] Floating point: check that the build gives the same results on different processors and compilers (no fast
      math or fused multiply-add in the recompiled code, the same maths library calls), and decide whether Windows
      and Linux builds may play each other.
- [ ] The frame rate setting, the Input Lag setting and frame interpolation must not change the race: test each.
- [ ] A test that runs the record-and-replay check without a person, for each mode.

### 2. Two machines, by address

- [ ] A transport: UDP between the players, with each packet carrying the last several frames of input so that a
      lost packet costs nothing.
- [ ] Input delay: a pad read on frame N is used on frame N + delay on every machine. The game waits when a
      player's input for the frame has not arrived. Delay chosen from the measured round trip, shown, and adjustable.
- [ ] Keeping time together: when one machine runs ahead it slows a little, so neither waits on the other all the
      time.
- [ ] Which port each player is: every machine feeds its own controller into its port and the others' from the
      network. The menus too, so that picking the mode, track and cars is played in step like the race.
- [ ] A handshake before a match: the same ROM and the same build, or the match is refused with the reason. Then the
      settings wherever a setting changes the race (Battle Mode Alt, the Battle Tuning mod's sliders, other mods):
      each player sends theirs, and every setting they differ in is settled the same way on every machine. With two
      players, a random pick between the two values, from the match's shared seed; with three or four, the value most
      players have, and a random pick from the seed among the values tied for most. The match runs with the settled
      values and every player sees which were changed; their own saved settings are left as they were.
- [ ] Desync detection: the race hash exchanged every second or so; on a mismatch the match stops and both sides
      keep a dump to compare.
- [ ] Connect by address or by code, from the menu. First target: 2-player Battle.
- [ ] Disconnect, timeout and quit: what each side sees, and that the game returns to the menu cleanly.
- [ ] Pause: off, or agreed by both sides.
- [ ] Rewind off whenever there is more than one player or a session is running, by the number of players and not
      by how the frame was drawn (today's check is that the frame was split screen).
- [ ] Sound and music: these stay each machine's own and must not feed back into the race. Check.

### 3. Your own view, full screen

- [ ] The game only knows split screen for more than one player. Draw the local player's view at full size and skip
      the others', without changing anything the race reads.
- [ ] The HUD at full size for that one player: the widescreen HUD and the Battle bars find views by their
      viewports and need to follow.
- [ ] Draw distance and culling are cut down in split screen; a full-screen online view should use the
      single-player values, once stage 1 has shown they do not change the race.
- [ ] Three and four players: Race and Stunt as well as Battle.

### 4. Finding a match

- [ ] A matchmaking server: a WebSocket service with a queue for each mode. A player sends the modes they have on,
      the build and the settings' hash; the server pairs two who match and hands each the other's address.
- [ ] Direct connection through home routers: both sides send to each other at once (hole punching), with the
      server as the meeting point. Say plainly when it fails. A relay is left for later.
- [ ] The search screen: the three mode toggles, searching, found, cancel. A name to show.
- [ ] Who picks the track, the cars and the options once matched: a vote, the host, or random.
- [ ] Rematch, and back to searching.
- [ ] Abuse: a rate limit on the server, and nothing a player sends is trusted beyond pairing.

### 5. After that

- [ ] Replays: a match is its inputs and its seed, so saving them is saving the match.
- [ ] Spectating.
- [ ] A relay for players who cannot connect directly.
- [ ] Rollback, only if delay-based play does not feel good enough. It would need the race saved and restored
      without Rewind's gaps, and the tick run several times in one frame without drawing or sound.

## Open questions

- How many frames of delay are playable in a race at 60 Hz, and at what round trip does it stop being fun. Only
  playing answers it.
- Whether the frame loop's logic can be run once per tick without breaking how the game counts frames at 30 fps.
- Whether the menus can run in step, or whether a match should start from a state built directly (mode, track,
  cars written in), skipping them.
- Linux and Windows together, or one build per match.
- Who runs the matchmaking server, and what it costs at a few players.

## Rules

- Every player needs their own ROM. Nothing ROM-derived is sent between players or to the server: inputs, hashes
  and settings only.
- Online is an option like every other change: off, the game is as it shipped.
