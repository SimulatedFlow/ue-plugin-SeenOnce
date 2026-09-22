# SeenOnce — Tutorial Hints That Show Once And Stay Shown

A hint fires when its condition becomes true, shows once, and is never shown again.

Not this session. Not next week. Because it is written into the save.

Drawing the box is half an hour of work. The two days after that are the reason this plugin exists:
does the memory survive a restart, what happens when two conditions become true in the same frame,
what happens on the second save game, and how does anybody check the whole set without playing from
the beginning again.

SeenOnce is those two days.

## 1. The memory survives

Shown hints are written into a save game, not into a bool on the game instance. Quitting the game
does not forget them, and neither does crashing — the memory is written when the hint goes up, not
when the session ends.

A *different* save has a different memory. A new game shows the hints again, and that is correct: it
is a different player getting to the same place for the first time. The documentation says which
setting gives you which behaviour, in plain words, because that question arrives as a bug report
otherwise.

## 2. They do not overwrite each other

Two conditions true in the same frame become one queue ordered by importance, never two boxes on top
of each other.

A hint that is on screen is interrupted only by a **more important** one, and only after its minimum
display time has passed — otherwise text flashes for four frames and taught nobody anything. Equal
importance waits its turn, so a set of hints that share one priority can never cut each other off in
a chain. Ties break on arrival order, so the tutorial comes out in the same order on every machine,
which is what makes a report about it reproducible.

## 3. They do not arrive at the wrong moment

A cooldown between hints, and a suppression switch for the states where nothing may appear —
cutscene, menu, boss fight, loading.

A hint that comes due **while suppression is on is not lost**. It waits, and it appears afterwards,
in the order it came due. That is the difference between *not now* and *never*, and it is the
difference a bool on the trigger cannot express.

## 4. Conditions are data where they can be

Built in: gameplay tag present, actor entered an area, action performed N times, time played, item
received. Everything else is one Blueprint node with one line in it.

A hint is a **data asset**, not a node buried in an event graph: an id, translatable `FText`, an
importance, a minimum display time, a cooldown, a repeat rule (once forever, once per save, every
time, N times), a condition and its parameters, an optional icon and sound. Assets can be listed,
diffed, localized and — the point of this plugin — **counted**.

There is a component you drop on a trigger volume that needs no wiring at all: it registers its
hints, filters overlaps down to the player, and announces its area so that other hints elsewhere can
listen for the same place.

## 5. You can check it without starting over

This is the part no competitor has.

This is the overview line from the demo map that ships with the plugin, read straight off the screen
after playing through it once:

```
hints 6 | seen 5 | pending 0 | never triggered 1 | suppressed: no
```

Below the header, every hint with its state: seen **and the date and time it was seen**, pending, or
**never triggered** — the never-triggered ones first and in warning colour. In the demo that one is
`SealedVault`, and it is there on purpose.

**Never triggered is the important line.** It finds the hint whose condition can never become true —
the typo in the tag, the trigger volume with overlap events switched off, the door that got sealed
two patches ago. Nothing is broken when that happens. No error, no warning, no crash. The hint simply
never appears, and nobody notices what they did not see.

The overview is drawn on `UCanvas` from an `AHUD`, so it exists in a packaged **Shipping** build.
That is not a stylistic choice: a condition that can *never* become true looks exactly like one that
has *not yet*, and only a playthrough separates them. Playthroughs happen in builds.

Console commands reset a single hint or all of them, so the same run can be checked twice instead of
being played twice.

## The save game — the part that can go wrong

SeenOnce brings **no save system** and replaces none. Two routes, and the documentation says which is
which:

* **Its own slot** (default) — a small `USaveGame`, slot name configurable. Works with no
  integration at all.
* **Yours** — `ExportState()` gives you a compact struct of ids and timestamps, `ImportState()` takes
  it back. Put it inside your own save game and switch the own slot off. Nothing is then written to
  disk behind your back, and there is no mode in which SeenOnce writes into your save file.

**Unknown ids on load are kept, never dropped.** Rename a hint, load a save from a newer branch, run
a configuration with a feature switched off — an import that filtered unknown ids would silently
erase the memory, and the symptom is a player being taught something they already know. Nobody
reports that as a bug, because it looks like a design decision. The overview counts those kept
entries in its header so you can see they survived. For projects that take save-schema versioning
seriously, **SaveStateMigrator** is the companion for that half.

## Your display, not ours

The hint display is a UMG widget that ships with the plugin and is meant to be replaced. Bind your
own widget to `OnHintShown` / `OnHintHidden`, switch the built-in display off, and SeenOnce decides
only **whether** and **when** — never what it looks like. Every rule behaves identically either way.

The hint text is `FText`, so it is translatable, and **LocaleGuard** can check it like any other line
in the game.

## The rules are testable, and tested

Should this show, which one is next, what does the state add up to — all of it is static functions
over plain values, with no world, no game instance and no frame behind them. Nine automation tests
cover them, including the ones that are otherwise invisible: that a hint is not interrupted before
its minimum display time, that a suppressed hint is not lost, that a reset forgets *once per save*
but not *once forever*, and that an import keeps ids it does not recognise.

They run under both `EditorContext` and `CommandletContext`, so they work on a build machine with no
editor open. You can call the same functions from your own tooling.

## Companion note

**CaptionCue** repeats what is being said — subtitles, speaker colour, minimum reading time. SeenOnce
never repeats anything: it teaches something once and remembers that it did. If you have CaptionCue
you already have a text layer, and SeenOnce can render its hints through your own display and supply
only the decision.

**EverloreEngine** narrates. **SaveStateMigrator** versions save schemas. SeenOnce writes into the
save and is a user of that, never a replacement.

## What is in the box

* One runtime module, Win64, full C++ source, no third-party code
* `USeenOnceSubsystem` — the memory, the queue, the conditions
* `USeenOnceHint` — the hint as a data asset
* `USeenOnceComponent` — hints on an actor with no wiring
* `ASeenOnceHUD` — the overview on canvas, in cooked builds too
* `USeenOnceStatics` — Blueprint nodes, and the rules as static functions
* `USeenOnceSettings` — slot, timing, hints, display, overview, suppression
* Six console commands: `Hint.Show`, `Hint.List`, `Hint.Reset`, `Hint.ResetAll`, `Hint.Suppress`,
  `Hint.Overview`
* A demo map that fires a hint, queues two at once, holds one through a suppression, and reloads the
  game to prove the memory came back off disk
* Nine automation tests, a README and full documentation

Unreal Engine 5.8 · Win64 · Blueprint and C++ · documentation:
<https://wiki.teufel-engineering.com/en/SeenOnce/documentation>
