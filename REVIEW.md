# The reviews before a merge

How the reviews of a pull request run, before the user is asked to merge it
(`CLAUDE.md`, "Rules that do not change"). A pull request of documentation alone keeps
its lighter review, written there. Set by the user on 2026-11-17, after #88: its second
and third reviews found style only, and a fix of style brought the next finding.

## What a reviewer is given

A full review is given the issue, the whole diff, the pull request's text, `CLAUDE.md`
and this file. Not the author's view of the change, nor what an earlier review found:
a reviewer told that a change is sound tends to find it so.

A review of fixes alone is given the diff of those fixes and the findings they fix. It
checks each fix and what the fix changed around it, not the whole change again.

## What failure looks like here

- A comment, or an option off by default, changes an image it should leave the same.
- A build drops what it is asked for: an option, a kernel, a scheduler (#84, #86).
- A store the timer interrupt or the other core relies on, moved or merged with another
  (`CLAUDE.md`, "Pitfalls met here").
- A fix made to one kernel or one port, and not to those that share its code.
- A text that lies: a figure not checked against the code, a measurement without its
  date, a claim the code does not keep.
- A tool or a job of the CI that now refuses, or skips without a word, what it ran
  before.

## A finding

The report starts with the number of findings. Each one gives a file and a line, a
concrete scenario (what is given, what comes out wrong) and a tag:

- **Important**: a wrong behaviour, or a false statement.
- **Nit**: style, wording, a pronoun whose referent is unclear.
- **Pre-existing**: not brought by the diff. It goes to an issue, not to this pull
  request.

A finding without a scenario is not one.

## How deep, by risk

- **A kernel, a port or a driver**: three reviewers at once, each on one side: the
  logic, the facts of the texts, the tools and the CI that run the code. The mutants,
  as `CLAUDE.md` says.
- **Anything else**, a tool, the CI, an example, a Makefile, a host test, the
  emulation: one reviewer.
- A pull request that touches both takes the deeper review.

Before it is fixed, each Important finding is checked by a fresh agent, given the
finding and the code. A finding the agent shows wrong is dropped, its reason written
in the pull request. It does not count as found.

## When the reviews stop

- An Important finding is fixed with its whole class: each other place of the same
  fault in the diff. Then a full review reads the whole diff again.
- The nits are fixed together, in one commit. One review of those fixes alone reads
  them.
- If the review of the fixes finds something Important, it is fixed as above, and a
  full review follows. If it finds nothing Important, its nits are fixed, and the
  reviews stop there.
- The reviews stop at the first full review with nothing Important, once its nits are
  fixed and read.
- After three full reviews in a row that each found something Important, the author
  stops and asks the user whether the approach is the right one.
