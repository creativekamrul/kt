# Ethics checklist for working with children

Status: **open**. Nothing in this project has been tested with a real child yet. This is
the working checklist for the pilot described in the design document, and the first item
is the one with a long lead time.

## Before anything else

- [ ] **Contact the university ethics committee now.** Approval for research with minors
      typically takes weeks to months, and a pilot cannot start without it. Confirm the
      required form, the named supervisor and the expected turnaround.
- [ ] Confirm whether the project needs ethics approval at all if the pilot is framed as
      usability testing rather than research, and get that in writing.
- [ ] Check whether the school or club partner requires its own approval, safeguarding
      policy or background checks (DBS or equivalent) for the researcher.
- [ ] Decide the publication path in advance (report, demo, paper) so consent covers it.

## Consent

- [ ] Written parental or guardian consent, in plain language, covering: what is
      collected, where it is stored, who can see it, how long it is kept, how to withdraw.
- [ ] Child assent: a short, age-appropriate explanation and an easy way to stop. The
      child's "no" overrides the parent's "yes".
- [ ] Consent to withdraw at any time, with data deletion on request.

## Data minimisation and protection

- [ ] Collect only what the study needs: first name or a code, age band, and answer
      telemetry. No surnames, no addresses, no photographs, no audio, no video without a
      separate explicit consent.
- [ ] All data stays on the device and in the study file, consistent with the product's
      design (local SQLite, no network code, no accounts, no telemetry).
- [ ] Pseudonymise at collection: store a participant code, keep the mapping in a single
      locked file, and delete the mapping at the end of the study.
- [ ] Encrypt or password-protect the device and the study folder. Never commit data to
      the repository: the .gitignore already excludes *.db, but check before pushing.
- [ ] Define a retention period and a deletion procedure, and follow both.

## Conduct of the session

- [ ] Sessions are short (15-20 minutes), supervised, in a familiar setting, with a break
      available at any time.
- [ ] No time pressure, no scores compared between children, no leaderboards.
- [ ] A wrong answer must never produce a negative sound or a sad animation - check the
      build against the design rule that visible progress never regresses.
- [ ] Adults do not look over the child's shoulder during the session unless asked.
- [ ] Have a safeguarding plan: two adults present or a visible location, and a school
      contact.

## Reporting

- [ ] Report aggregate results only; never quote an identifiable child.
- [ ] State limitations plainly: small n, convenience sample, no control group unless one
      exists.
- [ ] Report what the simulation predicted versus what children actually did - a mismatch
      is a result, not a failure.
- [ ] Keep the anonymised CSV export as the only artefact that leaves the device, and only
      with consent.

## Product-level protections to verify before any pilot

- [ ] The database file can be deleted by a parent without breaking the game
      (deleteChild/resetProgress paths exist; verify the shell exposes them).
- [ ] No outbound network calls: confirm by inspection that no socket or HTTP code exists
      in the tree.
- [ ] The child cannot reach any settings that would erase progress accidentally.
