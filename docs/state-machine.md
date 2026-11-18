# Game loop and scene state machine

## Session (one day of climbing)

~~~mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Preparing: startSession(child_id)
    Preparing --> Preparing: load progression and history
    Preparing --> Preparing: engine.replay(history) - rebuild posteriors from SQLite
    Preparing --> Serving: nextPuzzle()
    Serving --> AwaitingAnswer: render prompt, options, mountain
    AwaitingAnswer --> Evaluating: submitAnswer(index, response_ms)
    Evaluating --> Telemetry: logAttempt + engine.observe (one transaction)
    Telemetry --> Correct: answer was right
    Telemetry --> Wrong: answer was wrong
    Correct --> Rewarding: altitude +1 (never -1), landmark, milestone
    Rewarding --> Serving: session not complete
    Wrong --> Gentle: no altitude change, offer one retry
    Gentle --> Serving: retry pending (same sub-skill, one tier easier)
    Gentle --> Serving: no retry (already retried, or session ending)
    Serving --> Summary: complete() == true
    Summary --> [*]
~~~

Two invariants are enforced here rather than merely intended:

1. **Altitude is monotone.** The only transition that touches it is Correct, and it
   adds exactly one step ("1 solved problem = 1 step up"). ProgressionService::onWrong
   returns an event whose before and after altitudes are identical, and a test asserts
   that fifty consecutive wrong answers change nothing.
2. **A retry is bounded.** The retry is anchored to the practice tier, so a struggling
   child cannot cascade downwards and silently override the difficulty policy. (This
   was a real bug found by the simulation; see docs/simulation.md.)

## Session kinds

~~~mermaid
flowchart LR
    A[altitude is a zone boundary?] -->|yes| B[Checkpoint: boss item mixing climbed sub-skills]
    A -->|no| C[attempt index hits review interval?]
    C -->|yes| D[Review: the sub-skill practised longest ago]
    C -->|no| E[Practice: weakest unlocked sub-skill]
~~~

Checkpoints are spaced retrieval practice at a natural dramatic beat (the zone gate);
review items interleave the material that has waited longest. Both are decided purely
from the persisted log, so they are reproducible.

## Shell scenes (optional graphical shell)

~~~mermaid
stateDiagram-v2
    [*] --> ProfileSelect
    ProfileSelect --> Mountain: choose or create a child
    Mountain --> Puzzle: tap to start climbing
    Puzzle --> Puzzle: correct - climb animation, landmark, badge
    Puzzle --> RetryPrompt: wrong - gentle feedback
    RetryPrompt --> Puzzle: easier variant of the same sub-skill
    RetryPrompt --> Puzzle: move on
    Puzzle --> Checkpoint: zone gate
    Checkpoint --> Puzzle: boss item cleared
    Puzzle --> CampSummary: session length reached
    CampSummary --> Mountain: tuck in / continue next time
    Mountain --> ProfileSelect: switch child
~~~

The shell owns presentation only. Mounting, animation and audio are its business; when a
puzzle is answered it forwards the same call the console shell makes.
