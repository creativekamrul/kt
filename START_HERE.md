# Start KT

**Windows release.** Extract the complete Windows game ZIP to a writable folder and open `KT.exe`.

**Source checkout.** Use Python 3.13 x64 and Git on Windows. Run `setup.cmd -Quick` from the project root to install project-local tools, fetch the pinned dependency sources, build and test. Run `kt.cmd play` to launch.

**Offline laptop kit.** Keep the bundled `support` folder beside `setup.cmd`. Running `setup.cmd` verifies its checksums, installs its bundled Python and wheels, builds the game and verifies the simulation baselines. Logs are under `out`. You can launch from Git Bash with `./kt.cmd play`.

## Controls and campaign

KT opens at Willow Bay in full-screen mode. Walk with **WASD** or **arrow keys**. Head west to Willow Bridge and press **E** to repair it. Cross the repaired bridge and follow the gold marker on the west bank to **Bracken Rise**. The north pass also reaches the foot of the rise. Teal flags mark the longer Fern Switchback, which grants an extra heart in the clearing. Orange flags mark the shorter Cairn Steps, where a parry stuns the copperhorn for longer.

Keep moving during the fight. Dodge with **Space / B**, parry with **Q / X** as a charge or ripple reaches you, and strike with **E / A** while the copperhorn is recovering. Clearing the pass saves immediately. At the upper pennants, press **E / A** to continue.

The upper pennants lead into **Glasswind Hollow**. Inspect a mirror pool or ring a hanging bell with **E / A**. Reflect the moonkite's gold feather with **Q / X**, dodge its marked shadows, and strike when it lands. Return downhill at either pass. Willow Bay's north pass offers a shortcut to your latest cleared guardian.

On the upper mountain, cross either footbridge and
use **E** at trail anchors, supply bundles, crates and field markers. Tie the
anchors in the displayed order, then walk up the rope to the next ledge.
**J** opens your journal, **Esc** pauses the climb, and **F11** switches display
mode. You can also start with `kt.cmd play --windowed`.

With an Xbox-layout controller, use the **left stick or D-pad** to walk and
navigate menus, **A** to interact, **B** to go back, **Y** for the journal and
**Start** to pause. At Willow Bridge, use **A** to pick up a plank and **A**
again to place it. You can also click a plank and then its gap, or drag it with the mouse.

Follow the animated gold trail from the plaza. Nia carries a flower basket,
Sol wears a straw hat, and Kit wears a teal cap with a gold jacket. Approach
and press **E** or **A** to talk. Creek bells and the trail pennant save as
you go. Kit offers an optional practice bout with Flick. Press **E**, **A**,
or click when Flick flashes gold to parry. **Esc** or **B** leaves the bout.

Each fitted bridge plank saves
immediately. Bridge Builder stays earned when you replay the crossing.

Climbs pass through forest, rocky slopes and snowline to the summit at 90
steps. At the summit, walk up and right along the ridge. Press **E / A** at the flagpole, then return to Willow Bay. The journal tracks ranks, badges and expedition unlocks. Completed
encounters save locally. Leaving a route midway restarts that encounter when
you next launch KT.

```powershell
.\kt.cmd build       # rebuild and run tests
.\kt.cmd test        # detailed test output
.\kt.cmd sim         # full 300 x 300 x 5 comparison
.\kt.cmd charts      # charts in out/figures
.\kt.cmd cli         # terminal version
.\setup.cmd -Quick   # build/tests + smaller SQLite/console checks
```

Logs: `out/setup.log`, `out/setup-result.json`, `out/setup-checks`.
Game and all play data remain local. KT includes original pixel scenery, animated hikers and an original trail theme.

## Share a release

Run `kt.cmd package` after building. The versioned Windows game, GitHub-ready source and SHA-256 file are written to `out`. Personal saves stay in the game folder. To move your own progress, close KT and copy `sdp100.db` separately.

The full offline university kit additionally includes the course proposal, Python, wheels and dependency sources. From that kit, `tools/pack_laptop.py` can create another offline transfer kit.
