# Remaining work

Milestones 1-5 of the design document are complete and verified. This is what is left,
in the order it should be done.

## 1. Polish the graphical shell (milestone 6, partial)

The shell exists as code (shell/) behind the optional CMake target SDP_BUILD_GAMESHELL
and needs the SFML SDK to build. It renders the mountain, the character, the puzzle and
the feedback, and it drives the same SessionRunner as the console build.

- [ ] Install SFML and build with python tools/build.py --shell
- [ ] Wire the scene state machine (docs/state-machine.md) into actual scenes
- [ ] Character sprite with climb, idle and celebrate animations
- [ ] Parallax background per zone (four palettes, the cheap-asset trick)
- [ ] Audio: correct and gentle-wrong cues, no negative feedback sound
- [ ] Landmark and checkpoint animations
- [ ] Camp/summit summary screen with the day's badges

## 2. Assets (about 15-20 files, the design document's budget)

- [ ] One character sprite sheet, four zone backgrounds, step and landmark markers,
      UI icons, two or three sound cues
- [ ] Confirm every asset licence in THIRD_PARTY.md

## 3. Packaging and publication (milestone 7)

- [ ] Portable build per OS plus an installer
- [ ] README screenshots and a two to three minute demo video
- [ ] Crash-free pass on weird input (no input, huge input, rapid input, killed mid-write)
- [ ] Verify the build on an old, low-end machine with no developer tools installed
- [ ] Confirm the offline claim by inspection: no sockets, no HTTP, no telemetry

## 4. Evidence and report

- [ ] Write the report around docs/simulation.md: method, results, three defects found,
      caveats
- [ ] Include docs/adaptive-engine.md's maths section verbatim
- [ ] Add the adaptive-versus-static chart as the money slide in both decks
- [ ] Record the environment and exact commands for reproducibility

## 5. Pilot (only after ethics approval)

- [ ] See docs/ethics-checklist.md; start the approval process early because it dominates
      the timeline
- [ ] 5-10 children, 2-3 sessions each, parental consent
- [ ] Compare measured success rate against the simulation's prediction
- [ ] Check that the engine's learning is visible in the attempts log

## 6. Model improvements (research upside, not required)

- [ ] Add a drift term to the posterior update so the controller stops trailing a growing
      child; needs a real learning-rate estimate from the pilot first
- [ ] Calibrate the response model's slope from pilot data instead of assuming 0.8
- [ ] Add finer tiers at the bottom of the ladder so the weakest children are not pinned
      at tier 0
- [ ] Explore a time-limit dimension (DifficultyParams already carries it)
- [ ] Consider multi-skill checkpoints that mix domains, not just sub-skills within one

## Explicit non-goals

3D, explorable worlds, walking physics, multiplayer, accounts, servers, cloud sync in
v1, and deep reinforcement learning. These are recorded so that saying no stays easy.
