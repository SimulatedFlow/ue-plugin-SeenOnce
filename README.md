# SeenOnce — Tutorial Hints That Show Once And Stay Shown

**Unreal Engine 5.8 · Win64 · one runtime module · source included · no third-party code**

Drawing the box is half an hour of work.

The two days after that are: does the memory survive a restart, what happens when two conditions
become true in the same frame, what happens on the second save game, and how does anybody check the
whole set without playing from the beginning again.

SeenOnce is those two days.

## What it actually does

| | |
|---|---|
| **The memory survives** | Shown hints are written into a save game, not into a bool on the game instance. Quitting the game does not forget them. A *different* save has a different memory — a new game shows the hints again, and that is correct, not a bug. |
| **They do not overwrite each other** | Two conditions true at once become one queue ordered by importance. A hint on screen is interrupted only by a **more important** one, and only after its minimum display time — otherwise text flashes for four frames and nobody read it. |
| **They do not arrive at the wrong moment** | A cooldown between hints, and a suppression switch for cutscenes, menus and boss fights. A hint that comes due **while suppression is on is not lost** — it waits, and appears afterwards. |
| **Conditions are data where they can be** | Built in: gameplay tag present, actor entered an area, action performed N times, time played, item received. Everything else is one Blueprint node. |
| **You can check it without starting over** | An overview lists **every** hint with its state — seen and when, pending, or **never triggered** — plus console commands to forget one hint or all of them. |

## The line no competitor has

```
hints 18 | seen 11 | pending 2 | never triggered 5 | suppressed: cutscene
```

**Never triggered** is the important one. It finds the hint whose condition can never become true —
the one with a typo in the tag, on a trigger volume with overlap events switched off, behind a door
that got sealed two patches ago. Nothing is broken when that happens. There is no error, no warning,
no crash. The hint simply never appears, and nobody notices what they did not see.

The overview is drawn on `UCanvas` from an `AHUD`, so it exists in a packaged Shipping build — which
matters, because a condition that can *never* become true looks exactly like one that has *not yet*,
and only a playthrough separates them.

## Quick start

1. Enable the plugin.
2. Create a hint: **Content Browser → Miscellaneous → Data Asset → SeenOnce Hint**. Give it an id and
   some text.
3. List it under **Project Settings → Plugins → SeenOnce → Hints** so it can be counted.
4. Set your game mode's **HUD Class** to `SeenOnceHUD` (or call `DrawOverview` from your own HUD).
5. Drop a **SeenOnce Hints** component on a trigger volume and pick the hint. Or call
   `Trigger Hint` from anywhere in Blueprint.
6. Play. Console: `Hint.Overview` for the screen, `Hint.List` for the log, `Hint.ResetAll` to do it
   again.

## The save game — read this before shipping

SeenOnce brings **no save system** and replaces none. Two routes, and the documentation says which is
which:

* **Its own slot** (default). A small `USaveGame`, slot name in Project Settings. Works with no
  integration at all.
* **Yours.** `ExportState()` gives you a compact struct of ids and timestamps; `ImportState()` takes
  it back. Put it in your own save game and switch the own-slot option off.

**Unknown ids on load are kept, never dropped.** Rename a hint, load a save from a newer branch, run
with a feature switched off — a filtering import would silently erase the memory, and the symptom is
a player being taught something they already know. Nobody reports that as a bug, because it looks
like a design decision. There is a section about it in the documentation, and a companion note for
projects that take schema versioning seriously (see **SaveStateMigrator**).

## Your display, not ours

The hint display is a UMG widget that ships with the plugin and is meant to be replaced. Bind your
own widget to `OnHintShown` / `OnHintHidden`, switch *Use Built In Display* off, and SeenOnce decides
only **whether** and **when** — never what it looks like.

The hint text is `FText`, so it is translatable, and **LocaleGuard** can check it like any other line
in the game.

## Console commands

| Command | What it does |
|---|---|
| `Hint.Show <Id>` | Show a hint now, seen or not. Counts as seen. |
| `Hint.List` | Every hint with its state, to the log. |
| `Hint.Reset <Id>` | Forget one hint. |
| `Hint.ResetAll [all]` | Forget the set. `all` also forgets the permanent ones. |
| `Hint.Suppress 0\|1 [reason]` | Stop or allow hints. Nothing is lost while it is on. |
| `Hint.Overview [0\|1]` | The on-screen overview. |

## Documentation

Full documentation: **<https://wiki.teufel-engineering.com/en/seenonce/documentation>**

Support: teufelsilvan@gmail.com

<!-- SF-STORE-BLOCK:BEGIN -->
## 🛒 Source-available — see before you buy

This repository contains the **full source** of a commercial Unreal Engine plugin. It is **source-available, not open source**: read it, evaluate it, then buy a license to use it. See **the Fab Content License Agreement / Unreal Engine EULA (purchase required)**.

**Get it / Buy:**
- **Buy on Fab** (this plugin): https://www.fab.com/listings/2777f116-dea6-40a9-b2c4-2c0af640c0c7
- Fab store — all our UE5 plugins: https://www.fab.com/sellers/Silvan%20Teufel

### 📬 **Free UE5 Snippet-Pack**

10 ready-to-use C++/Blueprint building blocks (subsystems, versioned saves, async nodes, editor tooling) — MIT licensed. Get it by joining the newsletter — plus a heads-up when something new ships. Double opt-in, unsubscribe in one click, no address sharing.

👉 **[Get the free pack](https://silvan.teufel-engineering.com/newsletter/plugins/?q=gh)**

_© 2026 Silvan Teufel. All rights reserved._
<!-- SF-STORE-BLOCK:END -->
