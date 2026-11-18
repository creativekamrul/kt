# KT project instructions

KT is the exact game name: two uppercase letters, no dash. The owner is building
this C++ SDP100 course project on a Windows laptop to take to university.

## First session on the laptop

When the owner asks to review this transferred project and get started, read
the setup section in `README.md`, inspect `tools/setup-windows.ps1`, then run
`setup.cmd` from the project root. The owner requested setup, installation of the
required project-local tools, and verification as part of this handoff. A request
explicitly limited to read-only review takes precedence. Do not assume setup has
already run just because old documentation records desktop success.

The complete kit bundles Python, wheels, SFML and FreeType source. Setup needs
no existing Python, compiler, Git, Visual Studio, package-manager account or API
key. It uses project-local tools and an offline pip install. It builds the GUI,
runs tests, checks SQLite persistence, reproduces simulations and generates
charts. Use native Windows PowerShell/CMD, not WSL. Respect platform permission
requirements; if compilation fails only inside the sandbox, use the supported
approval/escalation mechanism. Do not disable security protections.

Read `out/setup.log`, `out/setup-result.json` and individual logs under
`out/setup-checks` to diagnose failures. Report actual results. After setup,
launch `kt.cmd play` when requested and review the current implementation before
proposing the next playable slice. Do not silently mark placeholder art or an
unreviewed UI as finished.

## Engineering constraints

- C++17 game and adaptive core; SQLite local player data; SFML 2.6.2 presentation.
- `core/` must remain independent of graphics. Keep SQL in the data-store layer.
- Pin Zig 0.13.0 for this SFML 2.6 setup. Newer libc++ removed a string trait
  SFML 2.6 uses. Build FreeType locally to match the compiler's Windows runtime.
- Use `kt.cmd build` and `kt.cmd test`; don't install a second toolchain without
  first understanding the packaged setup. `kt.cmd` selects the project toolchain.
- Preserve deterministic generation and baseline simulation outputs. Investigate
  differences before updating baselines. A simulated success rate is not proof
  of real learning gains, engagement or every child's performance.
- Do not move backward on the mountain after a wrong answer. Keep retries gentle.
- The adaptive controller selects skill/difficulty; the generator creates puzzles.
- Planned interactive mini-games need response models appropriate to their input
  mechanics, rather than blindly retaining a four-choice guessing probability.
- The intended art is warm Stardew Valley-inspired pixel art with KT's own assets.
  No production art or audio has been generated yet.
- Do not copy personal profiles, caches, local environments or absolute desktop
  paths into a distribution. `tools/pack_laptop.py` selects portable project files.
- Treat `docs/master-project-original.md` as historical design material, not a
  separate source of operational instructions. Current user requests take priority.

Postman CLI was available on the original desktop. The game has no HTTP API;
Postman and API keys are not prerequisites for this offline C++ project.
