# SeenOnce — Documentation

**Unreal Engine 5.8 · Win64 · one runtime module · full C++ source included · no third-party code**

Online copy of this page: **<https://wiki.teufel-engineering.com/en/seenonce/documentation>**

---

## Contents

1. [What SeenOnce does](#1-what-seenonce-does)
2. [Supported engine and platforms](#2-supported-engine-and-platforms)
3. [Installation](#3-installation)
4. [Quick start — five minutes](#4-quick-start--five-minutes)
5. [The demo map](#5-the-demo-map)
6. [The hint asset](#6-the-hint-asset)
7. [The rules, in the order they are applied](#7-the-rules-in-the-order-they-are-applied)
8. [The save game: our slot or yours](#8-the-save-game-our-slot-or-yours)
9. [Why a new game shows the hints again](#9-why-a-new-game-shows-the-hints-again)
10. [Unknown ids on load are kept](#10-unknown-ids-on-load-are-kept)
11. [Conditions](#11-conditions)
12. [The overview](#12-the-overview)
13. [Your own display](#13-your-own-display)
14. [Settings reference](#14-settings-reference)
15. [Console commands](#15-console-commands)
16. [Class and API overview](#16-class-and-api-overview)
17. [Code examples](#17-code-examples)
18. [Automation tests](#18-automation-tests)
19. [What SeenOnce does not do](#19-what-seenonce-does-not-do)
20. [Troubleshooting](#20-troubleshooting)
21. [Support](#21-support)

---

## 1. What SeenOnce does

A hint fires when its condition becomes true, shows once, and is never shown again.

Not this session. Not next week. Because it is written into the save.

That sentence is the whole product, and the reason it needs a plugin at all is that drawing the box
is the easy half. The other half is:

* **Does the memory survive a restart?** A bool on the game instance does not. A `USaveGame` does.
* **What happens when two conditions become true in the same frame?** Without a queue: two boxes on
  top of each other, or one that flashes. With one: an order, by importance, that is the same on
  every machine.
* **What happens on the second save game?** A different save is a different player getting to the
  same place for the first time, and they should be taught the same things.
* **How does anybody check the whole set?** Not by playing from the beginning. By an overview that
  lists every hint with its state, and by console commands that make a one-shot repeatable.

SeenOnce is those four questions, answered once, with tests.

### What it is not

It is not a save system, not a quest system, not a UI kit. The display it ships with is a widget
Blueprint you are meant to replace. What it sells is the **decision** — has this player already been
taught this, is now a good moment, and which of these two goes first.

---

## 2. Supported engine and platforms

* Unreal Engine **5.8**
* **Win64**, shipped as a code plugin with full source
* One runtime module, `LoadingPhase` `PreDefault`
* Works in the editor, in PIE, in Standalone and in a cooked **Shipping** build — including the
  overview, which is drawn on `UCanvas` precisely so that it survives cooking
* Blueprint and C++, both complete. Nothing in this plugin is C++ only.

Module dependencies: `Core`, `CoreUObject`, `Engine`, `UMG`, `Slate`, `SlateCore`, `GameplayTags`,
`DeveloperSettings`, `RenderCore`. No `UnrealEd`, no third-party code.

---

## 3. Installation

1. Copy the `SeenOnce` folder into your project's `Plugins` directory.
2. Open the project. Unreal offers to build the module; accept.
3. **Edit → Plugins → Gameplay Features → SeenOnce** — make sure it is enabled.
4. **Project Settings → Plugins → SeenOnce** is where everything below lives.

---

## 4. Quick start — five minutes

**1. Make a hint.** Content Browser → **Miscellaneous → Data Asset → SeenOnce Hint**. Name it
`DA_Hint_Door`. Set:

* **Hint Id**: `Hint_Door` (leave it empty and the asset name is used — see §6)
* **Body**: `Press E to open.`
* **Repeat**: `Once (forever)`
* **Priority**: `0`

**2. Register it.** **Project Settings → Plugins → SeenOnce → Hints** → add `DA_Hint_Door`.

This is what lets the overview say *"never triggered"*. A hint the subsystem has never heard of
cannot be reported as missing, and a hint referenced by nothing else would not even be in the cooked
build.

**3. Give it a display.** Create a Widget Blueprint whose parent class is **SeenOnce Hint Widget**,
put a Text Block in it, and implement **On Hint Shown** to set the text from the `Body` pin. Set it
under **Project Settings → Plugins → SeenOnce → Hint Widget Class**.

**4. Show the overview.** Set your game mode's **HUD Class** to `SeenOnceHUD`. In game, type
`Hint.Overview`.

**5. Trigger it.** Either:

* drop a **SeenOnce Hints** component on the trigger volume by your door, add `DA_Hint_Door` to its
  *Hints* array, and leave *Trigger Mode* on **Overlap**; or
* call **Trigger Hint** (`SeenOnce` category) from any Blueprint with the id `Hint_Door`.

**6. Play.** The hint appears. Walk through the door again — nothing. Quit the game entirely, start
it again, walk through the door — still nothing, because the memory is on disk. `Hint.ResetAll` puts
it back.

---

## 5. The demo map

`/SeenOnce/SeenOnce/Maps/L_SeenOnceDemo`

A short parcours, and every station of it demonstrates one rule rather than the look of a box:

| Station | What it shows |
|---|---|
| **The door** | The ordinary case: a hint fires on overlap, once. Walk through it again and nothing happens. |
| **The gap** | A second hint from a different trigger, with a different priority. |
| **The item** | A hint on the *item received* condition rather than on a place. |
| **The double trigger** | One volume that makes **two** hints due in the same frame. One appears; the other is listed as **pending** in the overview and appears after it. This is the queue, visible. |
| **The suppressed trigger** | A hint that comes due while suppression is on. It does not appear, it does not vanish — the overview lists it as pending, and it appears the moment suppression is lifted. |

`/SeenOnce/SeenOnce/Hints/DA_Hint_*` — six hints: `Door`, `Gap`, `Lantern`, `QueueHigh`, `QueueLow`
and `SealedVault`. **`SealedVault` has a condition that can never become true**, on purpose: it waits
for a fourth lever pull and the level has three levers. It is there so that the *never triggered*
line of the overview has something real to show, which is the only way to demonstrate the one thing
this plugin has that nothing else does.

`/SeenOnce/SeenOnce/Blueprints/` — `BP_SeenOnceDemoGameMode` (HUD class `BP_SeenOnceDemoHUD`, which
derives from `SeenOnceHUD` and switches the overview on at BeginPlay) and `BP_SeenOnceTrigger`, the
trigger volume that carries a **SeenOnce Hints** component. Three of them are placed in the map, at
the door, the gap and the vault, with their `Area Id` set — that is the documented workflow of §5,
and it is what a *SeenOnce Hints* component looks like in the Details panel.

`/SeenOnce/SeenOnce/UI/WBP_SeenOnceHint` is the display, registered as **Hint Widget Class** in
Project Settings; the subsystem creates and drives it, and it does nothing but set a text block from
the `Body` pin of *On Hint Shown*. `/SeenOnce/SeenOnce/UI/WBP_SeenOncePanel` is the demo panel:

| Button | What it calls | What to watch |
|---|---|---|
| **Walk through the door** | `NotifyAreaEntered("Door")` | The hint appears. Press it a second time and nothing happens. |
| **Cross the gap** | `NotifyAreaEntered("Gap")` | A hint of higher priority than the door's. |
| **Pick up the lantern** | `NotifyItemReceived("Lantern")` | A hint on a thing received rather than a place entered. |
| **Enter the vault (2 at once)** | `NotifyAreaEntered("Vault")` | Two hints due in one frame. One shows; the overview lists the other as **pending** until its turn. |
| **Pull a lever** | `NotifyAction("PullLever")` | Nothing, three times over. `SealedVault` needs four, and stays in the never-triggered list. |
| **Cutscene starts / ends** | `SetSuppressed(true/false, "Cutscene")` | With suppression on, hints queue instead of appearing. The header prints `suppressed: cutscene`, and everything queued arrives when it is lifted. |
| **Forget all hints** | `ResetAll(false)` | The two *Once per save* hints go back to never triggered. The four *Once (forever)* hints keep their memory — that is the difference between the two rules, on screen. |
| **Reload the level** | `OpenLevel` | The map restarts and the memory is still there. |

The panel drives the subsystem directly: there is no demo-side bookkeeping anywhere in that graph, so
every number the overview prints is the plugin's own answer.

The point of the demo is what you cannot see in a menu: **quit the game entirely and start it again.**
Every hint is still marked seen, with the timestamp from the previous run, because the memory came
back off disk — and that is the whole product.

---

## 6. The hint asset

`USeenOnceHint` is a `UPrimaryDataAsset`. A hint is an asset and not a Blueprint node because assets
can be listed, diffed, localized, handed to somebody who does not open Blueprints, and — the point —
**counted**. Hints scattered through event graphs can be none of those things, and a hint that cannot
be counted cannot be reported as never having fired.

| Property | Meaning |
|---|---|
| **Hint Id** | The identity, and the key the memory is stored under. Empty means "use the asset name". |
| **Body** | What the player reads. `FText`, so it is translatable — and so **LocaleGuard** can check it. |
| **Priority** | Higher wins. Decides both the queue order and whether this hint may interrupt one on screen. |
| **Display Seconds** | How long it stays up. |
| **Min Display Seconds** | How long it is protected from interruption. Zero uses the project default. |
| **Cooldown Seconds** | Quiet time after this hint before the next one. Zero uses the project default. |
| **Repeat** | `Once (forever)`, `Once per save`, `Every time`, `N times`. |
| **Max Show Count** | For `N times`. |
| **Reshow Interval Seconds** | Shortest gap between two shows of *this* hint, for the repeatable rules. |
| **Condition** + parameters | See §11. |
| **Icon**, **Sound** | Optional, soft references, handed to your display. |
| **Designer Note** | Not shown to players. Write *"only reachable after the second boss"* here, so a hint sitting in the never-triggered list can be judged without opening the level. |

### The id is the identity

Renaming the id is the same as creating a new hint: the old id stays in the save (§10) and the new
one has never been seen. That is a documented consequence and not an accident — the alternative,
keying the memory on the asset path, breaks the moment somebody moves a folder.

The editor's data validation flags the hint-asset mistakes that are visible without playing: no text,
a tag condition with no tag, an area/action/item condition with no key, a minimum display time longer
than the display time. The mistakes that are *not* visible in the asset show up in the overview as
*never triggered*, which is the only place they can.

---

## 7. The rules, in the order they are applied

Everything below is `USeenOnceStatics::PickNext`, `ShouldShow` and `IsDue` — static functions over
plain structs, with no world behind them, covered by the automation tests in §18. You can call them
yourself.

**1. The memory rule.** `ShouldShow` asks whether the repeat rule has shows left:

| Repeat | Answers "yes" |
|---|---|
| `Once (forever)` | Until the first show. Survives `ResetAll` — only `Hint.Reset <Id>`, `Hint.ResetAll all` or a different save brings it back. |
| `Once per save` | Until the first show. `ResetAll` forgets it. |
| `Every time` | Always. |
| `N times` | Until `Max Show Count` is reached. |

**2. The reshow interval.** `IsDue` is `ShouldShow` plus the hint's own interval. The two are
separate because they fail for different reasons and a caller usually wants to know which: *already
seen* is permanent, *not yet* is temporary.

**3. Suppression.** While it is on, nothing is shown — and **nothing is dropped**. Hints that come
due are queued and appear when it is lifted, in the order they came due. That is the difference
between *not now* and *never*, and getting it wrong silently loses tutorial hints in cutscene-heavy
games. A hint that was already on screen when suppression came on is taken down; it counts as seen.

**4. The cooldown.** With the screen free, nothing starts before the quiet time after the last hint
has passed. Hints arriving back to back read as a wall of text even when each one is fine on its own.

**5. The minimum display time.** A hint on screen is protected for this long, whatever is waiting
behind it. Without this rule, two hints coming due together produce one frame of the first one.

**6. Interruption needs a strictly higher priority.** Equal priority waits. That is what keeps a set
of same-importance hints from cutting each other off in a chain. When a hint *is* interrupted, the
more important one goes up immediately — no cooldown, because the rules just decided it was urgent.

**7. Ties break on arrival order.** Two hints of equal importance come out in the order they came
due, on every machine and every run. A queue ordered by priority alone comes out in whatever order
the array happened to be built in, and then two machines teach the same lesson in two different
orders and nobody can reproduce the bug report.

### When the memory is written

A show is recorded when the hint **goes up**, not when it comes down. A hint that was interrupted
after its minimum display time was read; a hint the player quit through was on screen. Only counting
hints that ran their full time is how a crash-happy build teaches the same lesson every session.

---

## 8. The save game: our slot or yours

**SeenOnce brings no save system and replaces none.** There are exactly two routes, and there is
deliberately no third route where SeenOnce writes into somebody else's save file.

### Route 1 — its own slot (default)

`Project Settings → Plugins → SeenOnce`:

* **Use Own Save Slot**: on
* **Save Slot Name**: `SeenOnce`
* **Save User Index**: `0`
* **Save After Every Show**: on

The memory is a small `USeenOnceSaveGame` holding one struct. It is loaded when the game instance
starts and written after every show, because the promise includes surviving the player killing the
process from the task manager thirty seconds after reading a hint. The write is a few hundred bytes
and happens at most once per hint.

This works with no integration at all, and it has one consequence to be aware of: the slot is not
tied to your save slots, so it behaves like **one memory per player profile** rather than one per
save game. If you want per-save memory, use route 2.

### Route 2 — your save game

```cpp
// Saving
FSeenOnceState HintMemory = Subsystem->ExportState();   // put this in your USaveGame

// Loading
Subsystem->ImportState(HintMemory);
```

Switch **Use Own Save Slot** off. Nothing is then written to disk behind your back, and the memory
travels with your save game — so a second save game genuinely has a second memory.

`FSeenOnceState` is deliberately dull: a version, a generation counter, and one entry per hint with
an id, a count, a rule and two `FDateTime` timestamps. It has to be dull, because it is going to sit
inside your save format, and anything clever in it would become your problem on the next engine
upgrade.

Blueprint has the same two nodes: **Export State** and **Import State**.

### Save-schema versioning

If your project takes save-schema versioning seriously — migrating old saves rather than rejecting
them — **SaveStateMigrator** is the companion for that, and `FSeenOnceState` carries a `Version`
field for exactly this reason. SeenOnce is a user of a save system, never a replacement for one.

---

## 9. Why a new game shows the hints again

Because a new game is a different player getting to the same place for the first time, and they
should be taught the same things.

This is worth stating plainly because it will otherwise arrive as a bug report. The memory is per
save, not per install:

* Route 2 (your save game): a new save has an empty memory. The hints come back. Correct.
* Route 1 (own slot): the slot is per profile, so a new game **keeps** the memory — every hint stays
  seen. If that is not what you want, use route 2, or call `DeleteSlot()` when your project starts a
  new game.

Neither behaviour is a defect and both are one setting apart. What would be a defect is a plugin that
did not tell you which one you had.

---

## 10. Unknown ids on load are kept

This is the most important paragraph in this document.

When `ImportState` is handed a memory containing ids that this build has no hint for, those entries
are **kept**. They stay in the state, they come back out of the next `ExportState` untouched, and the
overview lists them at the bottom as `unknown id, kept` and counts them in the header.

The reason is that every path to an unknown id is a path somebody will actually take:

* somebody renamed a hint asset's id in the last patch
* a player loaded a save from a newer branch, or from the DLC build, into the base build
* a feature is switched off in this configuration and its hints are not registered

A build that quietly dropped those entries would erase that memory, and the symptom — a player being
shown a tutorial they already read — is one **nobody reports as a bug**, because it looks like a
design decision.

What a load *does* drop: entries with no id at all (noise, not memory), and duplicate ids from a
hand-merged file, which collapse onto the stronger memory — the higher show count and the earlier
first-seen — because forgetting is the expensive mistake here.

A reset never touches unknown entries either. `ResetAll` forgets only ids that are registered right
now; `Hint.ResetAll` reports how many entries it kept.

---

## 11. Conditions

Five built-in conditions cover the majority, and the sixth is the door out.

| Condition | Parameters | Made true by |
|---|---|---|
| **Manual (triggered)** | — | `Trigger(Id)` from Blueprint, C++, or a `USeenOnceComponent`. |
| **Gameplay tag present** | `Condition Tag` | `AddConditionTag(Tag)`, or `TriggerByTag(Tag)`. |
| **Area entered** | `Condition Key` | `NotifyAreaEntered(AreaId)`, which the component calls for you. |
| **Action performed N times** | `Condition Key`, `Required Count` | `NotifyAction(ActionId)`. |
| **Time played** | `Required Play Time Seconds` | On its own. Play time is counted by the subsystem and does not accumulate while the game is paused. |
| **Item received** | `Condition Key`, `Required Count` | `NotifyItemReceived(ItemId)`. |

`TriggerByTag` matches **downwards**: triggering `Tutorial.Movement.Jump` fires a hint listening for
`Tutorial.Movement`. The other direction would fire a specific hint on a general event, which is
almost never what anybody means.

Every condition still goes through every rule in §7. A condition becoming true is a *request*, not a
show.

### The component

`USeenOnceComponent` ("SeenOnce Hints") is the no-wiring path: drop it on the trigger volume, pick
the hints, done. It does three things that are easy to forget by hand:

* it **registers** the hints it carries, so they are part of the set the overview counts
* it filters overlaps down to the **player pawn** by default (a patrolling guard is not a reader)
* it announces its **Area Id** on overlap, so hints elsewhere in the project can use the *area
  entered* condition against this same volume without anybody editing this actor

It has no memory of its own, on purpose — a component's memory would die with the level, and the
whole point is that it does not. `Trigger Mode` is `Overlap`, `BeginPlay` or `Manual`;
`Trigger Delay Seconds` waits before firing, for a hint that should follow a door opening.

If the actor has no component that generates overlap events, the component says so once, by name, at
`BeginPlay`. That is the single most common cause of a hint that never appears.

---

## 12. The overview

`Hint.Overview` in game, or `Hint.List` for the same thing in the log.

```
hints 18 | seen 11 | pending 2 | never triggered 5 | suppressed: cutscene
```

Then the list: **never triggered first and in warning colour**, then everything that has been seen
with the timestamp of when, then the entries no hint claims.

The ordering is part of the answer. Never-triggered hints go first because they are the only line
here that is a **finding** rather than a record — the hint with a typo in its tag, on a volume with
overlap events off, behind a door that got sealed two patches ago. Nothing is broken when that
happens: no error, no warning, no crash. The hint simply never appears.

Every number in the header is a count of something that was looked at. `never triggered` is exactly
"this hint is registered and has no entry in the memory" — not "we could not find it".

The overview is drawn on `UCanvas` from `ASeenOnceHUD`, so it exists in a packaged Shipping build.
That is not a stylistic choice: a condition that can *never* become true looks exactly like one that
has *not yet* become true, and only a playthrough separates them. Playthroughs happen in builds.

Keeping your own HUD class costs one subclass. Derive it from `ASeenOnceHUD` instead of `AHUD` and
your `DrawHUD` runs alongside the overview:

```cpp
UCLASS()
class AMyHUD : public ASeenOnceHUD
{
    GENERATED_BODY()

    virtual void DrawHUD() override
    {
        Super::DrawHUD();   // draws the overview when it is visible
        // ... your HUD ...
    }
};
```

If your HUD has to derive from something else, call `DrawOverview` yourself. It takes the panel's
top-left corner and returns the height it drew, so you can stack something under it:

```cpp
const float UsedHeight = OverviewHud->DrawOverview(40.0f, 40.0f);
```

Both are available in Blueprint as **Draw Overview**, **Set Overview Visible** and **Toggle
Overview**. The panel never truncates silently: when there are more hints than *Overview Max Lines*,
the last line says how many it left out.

---

## 13. Your own display

The shipped display is a `USeenOnceHintWidget` — a `UUserWidget` with two Blueprint events,
**On Hint Shown** and **On Hint Hidden**. It is meant to be replaced, and replacing it changes
nothing about the rules.

Two ways:

**Subclass it.** Make your widget Blueprint's parent `SeenOnce Hint Widget`, implement the two
events, and set it as *Hint Widget Class*. SeenOnce creates it, adds it to the viewport at the
Z-order you choose, and drives it.

**Ignore it.** Switch *Use Built In Display* off and bind your own widget to the subsystem's
`OnHintShown` / `OnHintHidden` delegates. SeenOnce then draws nothing at all and only decides. The
hide event carries a reason — `Elapsed`, `Interrupted` or `Cleared` — so an animation can differ
between "its time was up" and "something more important arrived".

`OnHintQueued` carries the hint and the queue length, which is what a *"1 more tip"* indicator binds
to.

---

## 14. Settings reference

**Project Settings → Plugins → SeenOnce**

### Memory

| Setting | Default | Meaning |
|---|---|---|
| Use Own Save Slot | on | Keep the memory in SeenOnce's own save game. Off = `ExportState`/`ImportState` only. |
| Save Slot Name | `SeenOnce` | The slot. |
| Save User Index | `0` | The user index. |
| Save After Every Show | on | Write as soon as a hint has been shown, rather than only at shutdown. |

### Timing

| Setting | Default | Meaning |
|---|---|---|
| Default Cooldown Seconds | `2.0` | Quiet time between hints, for hints that set none. |
| Default Min Display Seconds | `1.5` | Interruption protection, for hints that set none. Not zero, on purpose. |
| Queue Expiry Seconds | `0.0` (off) | Drop a hint that has waited longer than this. Off by default: a hint that vanished for reasons nobody can see is worse than a late one. Every drop is logged by name. |

### Hints

| Setting | Meaning |
|---|---|
| Hints | Every hint in the project. This is what makes *never triggered* answerable, and what keeps the assets in the cooked build. |

### Display

| Setting | Default | Meaning |
|---|---|---|
| Use Built In Display | on | Let SeenOnce create and drive the widget below. |
| Hint Widget Class | none | Your `SeenOnce Hint Widget` subclass. Soft — nothing loads until the first hint. |
| Hint Widget Z Order | `100` | Viewport Z-order. |

### Overview

| Setting | Default | Meaning |
|---|---|---|
| Overview Visible On Start | off | Whether the overview draws from the start. |
| Overview Origin | `40, 40` | Top-left of the panel, in pixels. |
| Overview Max Lines | `24` | Lines before the panel says how many it left out. |

### Suppression

| Setting | Default | Meaning |
|---|---|---|
| Suppression States | Cutscene, Menu, Combat, Loading | Names only — the switch accepts any name. Listing them is how a project agrees on spelling, which matters when a bug report contains `suppressed: cutscene`. |
| Suppress While Paused | on | A hint that appears behind an open menu was marked as seen and never read. |

---

## 15. Console commands

| Command | What it does |
|---|---|
| `Hint.Show <Id>` | Show a hint now, seen or not, past the cooldown and past suppression. It **counts as seen** — which is honest, and why `Hint.Reset` is next to it. |
| `Hint.List` | The whole overview to the log: every hint, its state, its timestamp. |
| `Hint.Reset <Id>` | Forget one hint, whatever its rule. |
| `Hint.ResetAll [all]` | Forget the registered hints. Without `all`, hints marked *Once (forever)* are kept. Unknown ids are never touched either way. |
| `Hint.Suppress 0\|1 [reason]` | Stop or allow hints, with a reason the overview prints. Nothing is lost while it is on. |
| `Hint.Overview [0\|1]` | The on-screen overview. Needs a HUD — `ASeenOnceHUD`, or your own calling `DrawOverview`. |

All six need a running game: the memory belongs to a game instance, and there is no memory without
one. Each says so rather than doing nothing quietly.

---

## 16. Class and API overview

### `USeenOnceSubsystem : UGameInstanceSubsystem`

The memory, the queue, the conditions. Lives on the game instance, so it survives a level change and
dies with the game. It ticks through `FTSTicker` rather than a world tick, so a hint queued in the
last second of one level appears in the first second of the next instead of vanishing with the world
that queued it.

| Function | |
|---|---|
| `Trigger(FName)` | Request a hint. True when it was queued. |
| `TriggerByTag(FGameplayTag)` | Request every hint under a tag. Returns how many were queued. |
| `ForceShow(FName)` | Show now, past every rule. Still counts as seen. |
| `HasSeen(FName)` | |
| `Reset(FName)` / `ResetAll(bool bIncludePermanent)` | |
| `ExportState()` / `ImportState(FSeenOnceState)` | §8 and §10. |
| `SaveToSlot()` / `LoadFromSlot()` / `DeleteSlot()` | Route 1 only. |
| `SetSuppressed(bool, FName Reason)` / `IsSuppressed()` / `GetSuppressionReason()` | |
| `RegisterHint(...)` / `UnregisterHint(...)` / `GetRegisteredHints()` / `FindHint(FName)` | |
| `NotifyAreaEntered` / `NotifyAction` / `NotifyItemReceived` / `AddConditionTag` / `RemoveConditionTag` | §11. |
| `Summarize()` | The whole set counted against the memory. |
| `GetActiveHint()` / `GetPendingCount()` / `GetPlayTimeSeconds()` / `ClearActiveHint()` | |
| `SetOverviewVisible(bool)` / `LogOverview()` | |

Delegates: `OnHintShown(Hint, Body)`, `OnHintHidden(Hint, Reason)`, `OnHintQueued(Hint, QueueLength)`
— all `BlueprintAssignable`.

### `USeenOnceHint : UPrimaryDataAsset`

The hint. `GetDefinition()` returns the plain `FSeenOnceHintDef` every rule reads.

### `USeenOnceComponent : UActorComponent`

Hints on an actor with no wiring. `Fire()`, `HasFired()`, `Rearm()`, `AreAllHintsSeen()`.

### `ASeenOnceHUD : AHUD`

The overview on `UCanvas`. `DrawOverview(X, Y)`, `SetOverviewVisible(bool)`, `ToggleOverview()`, plus
the colours and the scale as properties.

### `USeenOnceStatics : UBlueprintFunctionLibrary`

The Blueprint front door — and, above it, the decision logic as static functions with no world:

```cpp
static bool  ShouldShow(const FSeenOnceHintDef&, const FSeenOnceState&, double NowSeconds);
static bool  IsDue     (const FSeenOnceHintDef&, const FSeenOnceState&, double NowSeconds);
static int32 PickNext  (const TArray<FSeenOnceQueuedHint>&, const FSeenOnceActiveHint&, double NowSeconds);
static FSeenOnceSummary SummarizeState(const TArray<FSeenOnceHintDef>&, const FSeenOnceState&);

static void  RecordShow(FSeenOnceState&, const FSeenOnceHintDef&, double NowSeconds, const FDateTime& NowUtc);
static bool  ResetHintInState(FSeenOnceState&, FName);
static int32 ResetAllInState(FSeenOnceState&, bool bIncludePermanent, const TArray<FName>& KnownHintIds);
static FSeenOnceState SanitizeLoadedState(const FSeenOnceState&);
```

A project that wants its own scheduler, its own display or its own save format can call these
directly and keep the rules without keeping anything else.

### `USeenOnceSettings : UDeveloperSettings`

§14.

---

## 17. Code examples

**Trigger a hint from C++:**

```cpp
if (USeenOnceSubsystem* Hints = USeenOnceStatics::GetSeenOnce(this))
{
    Hints->Trigger(FName("Hint_Door"));
}
```

**Suppress hints for a cutscene:**

```cpp
void AMyCutscenePlayer::BeginCutscene()
{
    if (USeenOnceSubsystem* Hints = USeenOnceStatics::GetSeenOnce(this))
    {
        Hints->SetSuppressed(true, FName("Cutscene"));
    }
}

void AMyCutscenePlayer::EndCutscene()
{
    if (USeenOnceSubsystem* Hints = USeenOnceStatics::GetSeenOnce(this))
    {
        // Anything that came due during the cutscene is still waiting, and appears now.
        Hints->SetSuppressed(false, NAME_None);
    }
}
```

**Put the memory into your own save game:**

```cpp
void UMySaveSystem::WriteSave(UMySaveGame& Save)
{
    if (const USeenOnceSubsystem* Hints = USeenOnceStatics::GetSeenOnce(this))
    {
        Save.HintMemory = Hints->ExportState();
    }
}

void UMySaveSystem::ReadSave(const UMySaveGame& Save)
{
    if (USeenOnceSubsystem* Hints = USeenOnceStatics::GetSeenOnce(this))
    {
        // Ids this build does not know are kept, not dropped.
        Hints->ImportState(Save.HintMemory);
    }
}
```

**Your own display:**

```cpp
void UMyHintPanel::NativeConstruct()
{
    Super::NativeConstruct();

    if (USeenOnceSubsystem* Hints = USeenOnceStatics::GetSeenOnce(this))
    {
        Hints->OnHintShown.AddDynamic(this, &UMyHintPanel::HandleHintShown);
        Hints->OnHintHidden.AddDynamic(this, &UMyHintPanel::HandleHintHidden);
    }
}
```

**Count the never-triggered hints on your own, e.g. for a build report:**

```cpp
const FSeenOnceSummary Summary = Hints->Summarize();
UE_LOG(LogTemp, Display, TEXT("%d of %d hints have never fired."),
    Summary.NeverTriggeredCount, Summary.TotalHints);
```

---

## 18. Automation tests

`Session Frontend → Automation → SeenOnce`, or on a build machine:

```
UnrealEditor-Cmd.exe <YourProject>.uproject -ExecCmds="Automation RunTests SeenOnce" -unattended -nopause -testexit="Automation Test Queue Empty"
```

Nine tests, covering the rules that can be quietly wrong:

| Test | Claim |
|---|---|
| `SeenOnce.Memory.OnceShowsExactlyOnce` | `Once` answers "show me" exactly one time. `N times` stops on the number. A hint with no id is never shown. |
| `SeenOnce.Memory.ResetAllForgetsPerSaveButNotForeverOrForeign` | `ResetAll` forgets `Once per save`, keeps `Once (forever)` and never touches an unknown id. |
| `SeenOnce.Queue.PickNextTakesHighestPriorityAndBreaksTiesStably` | Highest priority wins; ties go in arrival order and do not depend on array order; the cooldown holds. |
| `SeenOnce.Queue.MinimumDisplayTimeProtectsAndOnlyHigherPriorityInterrupts` | Not interrupted before the minimum, then only by a strictly higher priority. |
| `SeenOnce.Queue.SuppressedHintsWaitInsteadOfBeingLost` | Nothing shows and nothing is dropped while suppressed; order is preserved afterwards. |
| `SeenOnce.Save.ImportKeepsUnknownIds` | Unknown ids survive an import and are counted as orphans, not as never-triggered hints. Nameless junk is dropped; duplicates keep the stronger memory. |
| `SeenOnce.Overview.SummaryCountsNeverTriggeredCorrectly` | The counts, the ordering, the timestamps and the header shape. |
| `SeenOnce.Memory.EveryTimeRespectsTheReshowInterval` | `Every time` waits its interval, and a load does not inherit a wait. |
| `SeenOnce.Memory.RecordShowKeepsFirstSeenAndAdvancesLastSeen` | First-seen is written once and never overwritten. |

They run under `EditorContext` **and** `CommandletContext`, so they work on a build machine with no
editor open.

---

## 19. What SeenOnce does not do

* **It is not a save system.** It writes one small file if you let it, and hands you a struct if you
  do not. See §8.
* **It does not write into your save file.** There is no mode where it does.
* **It is not a UI kit.** One replaceable widget base class, and a canvas overview for the tool half.
* **It is not a quest system.** It teaches, it does not narrate. (`EverloreEngine` is the one that
  narrates; `CaptionCue` is the one that repeats what is being said. SeenOnce says something once and
  remembers that it did.)
* **It does not know whether the player read the hint.** It knows the hint was on screen. There is no
  honest rule for "seen but perhaps not read", so there is no rule.
* **It does not discover hints by scanning the content browser.** Hints are registered — in Project
  Settings or at runtime — because a hint that is only found when a level happens to be loaded
  cannot be reported as missing before that level has been loaded.
* **No networking.** The memory is local to a game instance. In a listen-server game, hints are a
  client-side matter and belong on the client.

---

## 20. Troubleshooting

**A hint never appears.**
Open the overview. If it is listed as *never triggered*, its condition never became true — check the
trigger, the tag spelling, and whether the trigger actor generates overlap events (the component logs
a warning at `BeginPlay` when it does not). If it is not listed at all, it is not registered: add it
to *Hints* in Project Settings.

**A hint appeared once and never again — but I want it back for testing.**
`Hint.Reset <Id>`, or `Hint.ResetAll all`.

**Hints repeat after every restart.**
The memory is not being saved. Either *Use Own Save Slot* is off and nothing is calling
`ImportState`/`ExportState`, or the slot cannot be written — the log says so at `Error` level.

**Two hints flash and I only read one.**
Raise *Min Display Seconds* on the first, or make the second's priority equal rather than higher. A
strictly higher priority is what buys the right to interrupt.

**Nothing appears during a cutscene.**
That is suppression working. The hints are queued, not lost — check *pending* in the overview header,
and they appear when you call `SetSuppressed(false, …)`.

**The overview does not draw.**
It needs a HUD. Set the game mode's HUD class to `SeenOnceHUD`, or call `DrawOverview` from your own
HUD's `DrawHUD`. `Hint.Overview` also reminds you of this in the log.

**Everything is `never triggered` after a save from another branch.**
Look at the header: `kept unknown N` means the memory arrived under ids this build does not have.
Nothing was lost — see §10 — but the ids do not match, which usually means a hint was renamed.

**Two hints share one memory.**
Two assets claim the same *Hint Id*. The log says so at registration, by asset name.

---

## 21. Support

Documentation: **<https://wiki.teufel-engineering.com/en/seenonce/documentation>**

Email: teufelsilvan@gmail.com

Full C++ source ships with the plugin. Every rule described above is in `SeenOnceStatics.cpp`, and
every claim in §18 is a test you can run yourself.
