# Authoring content

Content lives in text files, not in code. Nothing on this page requires a compiler,
which is the point: a curriculum officer, a translator or a teacher can change what the
game teaches and in what order.

## Files

| File | What it controls |
|---|---|
| content/curriculum.json | sub-skills, their difficulty tier tables, zones and their ordering, review/landmark/session cadence |
| content/quiz/*.csv | quiz question banks, one file per topic |
| content/strings.json | every user-visible string, by key |

## curriculum.json

The shipped file is generated from the compiled-in defaults, so it is always in sync:

~~~
build/apps/sdp_content_export.exe > content/curriculum.json
~~~

Edit it freely afterwards - the loader reads it at start-up and reports any problem it
finds (missing file, bad JSON, unknown skill in a zone) in a LoadReport, falling back to
the built-in defaults rather than refusing to start. Run the tests after editing:

~~~
python tools/build.py --test
~~~

The content tests check that the shipped file loads, validates and still has the expected
shape, so a typo is caught before a child sees it.

Structure:

~~~json
{
  "landmark_interval": 10,
  "session_length": 10,
  "review_interval": 5,
  "skills": [
    {
      "id": "math.addition",
      "domain": "math",
      "display_key": "skill.math.addition",
      "template": "math.addition",
      "quiz_bank": "",
      "tiers": [
        {"tier": 0, "difficulty": 0.0, "min_value": 1, "max_value": 5,
         "operand_count": 2, "distractor_count": 3, "time_limit_ms": 0,
         "allow_carry": false, "allow_borrow": false}
      ]
    }
  ],
  "zones": [
    {"id": 0, "key": "forest", "display_key": "zone.forest",
     "altitude_start": 0, "altitude_end": 30, "palette": "forest",
     "unlocks": ["math.counting", "math.addition"]}
  ]
}
~~~

Rules the loader enforces (Curriculum::validate):

* every skill needs a non-empty id, a template and at least one tier;
* every sub-skill named in a zone's unlocks must exist;
* zones must start at strictly increasing altitudes.

Field notes:

* **domain** is one of math, problem_solving, quiz.
* **template** selects the generator template: math.counting, math.addition,
  math.subtraction, logic.patterns, logic.odd_one_out, logic.ordering, quiz.bank.
  An unknown template falls back to a simple arithmetic item rather than crashing.
* **tiers** are the bandit's arms, in order of increasing difficulty; the tier index is
  what the engine learns over. Add tiers for finer control, remove them for faster
  convergence. The difficulty value is stored in telemetry, so keep it monotone.
* **min_value / max_value / operand_count / distractor_count** feed the templates.
  distractor_count = 3 means four options, which is also the guessing floor the engine
  assumes - change it and you should revisit EngineConfig.latent_guess.
* **allow_carry / allow_borrow** make the arithmetic templates actually construct
  carrying or borrowing problems rather than hoping for them.
* **quiz_bank** names the CSV under content/quiz (without the extension) for quiz skills.
* **zones** double as curriculum order: a zone's unlocks become available once the child's
  altitude reaches altitude_start. A new zone also creates a checkpoint and a landmark.

## Quiz banks (CSV)

Header, then one row per question:

~~~
prompt,answer,distractor1,distractor2,distractor3,hint,tier_min,tier_max
"Which animal says moo?",cow,dog,cat,duck,"Cows give us milk.",0,3
~~~

* Quote any field containing a comma; double a quote to escape it.
* Lines starting with # are comments, blank lines are ignored.
* tier_min / tier_max are inclusive tier windows, so one bank can serve the whole ladder:
  an item with 0,3 is offered while the child's tier for that sub-skill is 0 to 3.
* If a bank is too thin to supply distinct wrong answers, the generator pads with neutral
  options so the option count always matches distractor_count + 1.
* Aim for at least three sub-skill topics per domain and at least three questions per
  tier window; the design document's rule of thumb is that three types done well beat ten
  done badly.

## Strings and translation

Every user-visible string is looked up by key with a built-in fallback, so a partial
translation degrades gracefully instead of breaking. Keys in use:

| Key pattern | Used for |
|---|---|
| zone.forest, zone.rocky, zone.snowline, zone.summit | zone names on the summary screen |
| skill.math.addition etc. | sub-skill names |
| feedback.correct.0 .. 3 | encouragement variants |
| feedback.wrong / feedback.wrong_again | gentle wrong-answer feedback |
| summary.reached | session summary opening words |

To add a language, copy strings.json to (for example) strings.de.json, translate the
values, and point the shell at it. No recompilation, no code change - which is the
translation argument in the funding pitch.

## Adding a whole new sub-skill

1. Add an entry to skills[] with a unique id (use the pattern domain.topic).
2. Give it a tier table (copy an existing one and adjust).
3. Either reuse an existing template or add one in core/src/generator.cpp and register it
   in LevelGenerator::registerTemplates.
4. Add its id to a zone's unlocks.
5. Add any quiz bank CSV and its display strings.
6. Run the tests - the content tests will tell you immediately if the file is malformed.

## Data lives elsewhere

Never put state in the content files. Progress, attempts, beliefs and milestones live in
the SQLite database; the content files are read-only inputs. That separation is what
makes a content file safe to ship, email or translate.
