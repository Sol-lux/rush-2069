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

Measured with the sync probe (`src/sync_probe.cpp`; `--sync-record`, `--sync-replay`, `--sync-hash`, `--sync-dump`,
`--sync-random`, `--sync-hw`, `--sync-seed` in `docs/building.md`): a 2-player Battle round recorded once and replayed twice, memory compared at the same
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
- [x] The game's 60ths timer (`0x8002EB64`, the scheduler's retrace count) under `--frame-lock`: every read of it
      in the timer module (the frame's time, the clock, the countdown) gets thread 5's steps since the boot instead.
      The about twenty other places that read the count (the scheduler's pacing among them) still get the real one.
- [x] `--frame-lock` (`src/frame_lock.cpp`): one frame of the frame loop per step of thread 5, each 1/60 s. It
      boots and holds 60 steps a second. Two locked boots into the attract race still differ from its first tick, and
      the race starts on a different read (552 and 933), as unlocked boots do (513 and 791): the loads take machine
      time, and the attract race follows the random state. So the test of the lock is a recording replayed, not two
      boots.
      A race recorded with the lock and replayed twice with it: the tick count is the same at every read, and the
      race's memory hashes the same from the race's first read (511) to read 1097, about 10 s, then differs to the
      end. The timer's count of 60ths was 1 apart between the two runs until then and became 3 apart at read 1098.
      With the timer locked too, the same recording replayed twice: the ticks agree at every read, and the hashes
      agree from the race's first read to read 1724, about 20 s. From there one read in 8 differs and the next
      seven agree again, so the race's state stays the same and something that changes every 8 frames is caught at
      a different point; from read 4866 they differ to the end. Counting thread 7's frames instead of thread 5's
      steps puts the two runs 18 frames apart before the first step (the boot's frames), and they never agree.
- [ ] The read in 8 that differs, and the lasting difference. Neither is at a fixed read: over three pairs of
      replays the reads in 8 started at 1725, 1727 and 3217, and the lasting difference at 4866, 4898 and about
      4705. Where the runs differ for good, the random state (`0x8011735C`) differs, and so does a table of 0x44-byte
      entries at `0x80132D7C`-`0x80133014`. No other word of the race's memory differs before those.
      `--sync-random` logs every entry of the 28 functions that step the random state. In a race, 96% of the calls
      are thread 7's, nearly all of them `func_8008B69C` and `func_8008D93C`; thread 5 calls `func_800E0050`,
      `func_800DEF68` and `func_800D348C`. In one pair of locked replays the two logs were the same, call for call,
      with the same state at the same reads, to read 8104 (the end of the shorter run), yet the hashes differed
      from read 518, the race's first tick. So the random generator is not the only cause. In another pair, dumped at
      reads 517, 518 and 581, the frame loop's state (`0x801174B4`) and the random state already differed at read
      517: one run was a frame further into the race's start than the other. From read 518 the words that differ
      include floats at `0x801192BC`-`0x80119370` and `0x8011A374`-`0x8011A39C` (0.183 against 0.2, i.e. 11 against
      12 60ths) and counters beside them, which look like times counted in frames.
      `--sync-hw` logs where the game meets the machine's timing: each step of thread 5, each of thread 7's waits for
      a frame (and which wait), the retraces each frame really counted, the graphics task counter and the spin
      yields. Every read of the real retrace count (`0x8002EB64`) is in the timer module, which the lock already
      covers. In two more pairs the hardware logs were the same line for line (steps, waits, the frame loop's state,
      the ticks, one retrace a frame), and in one of them the random logs were too, yet the hashes still differed.
      In that pair, dumped before the race, the race's memory was the same at reads 1 to 400. By 450 two words
      differed, and at the race's start they still did: sound IDs in the cars' looping-sound table (`0x80140420`, 0x54
      bytes a car, `func_800E0050`, e.g. `0x801404B4`). The sound manager hands out those IDs on the audio thread's
      own clock, so sound feeds back into the race's memory. In the other pair the random state and words at
      `0x80152B8C` and `0x80160358`-`0x801603BC` also differed by read 516. So there is more than one cause, and which
      ones show up changes from run to run.
      A sound's ID is the manager's count of sounds started, shifted, with the low bits of the node it was given
      (`func_80092278`), and which node is free depends on when the audio thread freed the last ones. Only the audio
      thread reads the manager's lists (`0x80144028`, `0x80144C58`); no race code compares an ID with anything but -1,
      and the one answer the race gets back, whether a slot's sound still plays (`func_800BF148`, at `0x800E0480` in
      `func_800E0050`), only decides whether a new sound is started into that slot. IDs are stored in the cars' tables
      (`0x80140420`-`0x80140B14`), each car's +0x3FC, the positional sounds' objects (`0x8014E0F0`-`0x8014E300`, 0x70
      bytes each, the ID at +0x60), a list from `0x80149868` (+0x38) and the Battle overlay's `0x803BA1D0`. A word of
      the manager's own (`0x80143CF8`) is inside the race's memory too.
      `--sync-seed` holds the random state at one value at every read until the race runs. In two pairs of seeded
      replays the random logs were the same through the whole round, and the race's memory at reads 700 to 1500
      differed only in sound IDs and `0x80143CF8`: the cars, the weapons and the random state were the same. In a
      third run the menus fell one step behind (a load started at read 307, not 306: one more frame of the screen
      before it, state `0x00080000`), the race started at read 519, and from there everything differed. That run also
      stopped stepping at read 3211 in the race, with the game still open: a hang under the lock, not yet looked at.
      What held that screen: a load's files are read by a loader thread (thread 8, through the PI's DMA), and the frame
      loop polls each file's ready byte once a frame, so whether a read was done by the poll depended on when thread 8
      ran. Under the lock, `func_80097798` now reads every file inline on the thread that asks (`src/frame_lock.cpp`).
      With that and the seed, every replay starts the race on the same read (514), and `--sync-hash`'s second column
      (the race's memory without the words that hold a sound ID) was the same in three replays at all 8608 of the
      round's reads: the same game from the same input, on one machine. No run hung with inline loads (seven runs to
      the round's end); the one hang, with the loader thread, is not explained.
- [ ] The sound IDs: kept out of the desync check for good. Only the audio thread reads them back, and the race
      only compares them with -1. But sound did feed back into the race: when `func_800BF148` says a car's slot has
      stopped (the audio thread's clock), `func_800E0050` steps the random state (`0x8011735C`) inline, once or twice
      (`0x800E0488`-`0x800E0538`), to pick whether to start another. One replay pair on one machine never showed it,
      as both had the same audio timing; two machines would. Under the lock those steps now use a generator of
      their own (`src/frame_lock.cpp`), so the race's random state no longer follows sound. In two seeded, locked
      replays with it the random logs were the same call for call through the round. Their race hashes still parted
      at read 1727, three reads in eight at first: the words that differ there are a display list (`0x80125F50` and
      `0x8012AA50`, a buffer each frame), a copy of the real retrace count (`0x80111958`) and a counter
      (`0x80124F70`), i.e. drawing inside what the hash counts as the race. Whether the cars and weapons part later
      is not yet checked. The other two callers of `func_800BF148` (`func_800BF1C8`, `func_800BF45C`) are called only from
      `func_800C0AC0`, which nothing calls directly (reached by pointer, not yet traced).
      The thorough fix, if one is ever needed (another feedback found, or the sounds themselves wanted the same on
      every machine, e.g. for replays): make "still playing" a function of the race alone, a sound counted done a
      set number of steps after it starts (its length from the sample table), so every machine gets the same
      answer, and hand out IDs from the race's own count rather than the manager's free nodes.
- [ ] Battle and Stunt and Race each, and two different machines.
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
