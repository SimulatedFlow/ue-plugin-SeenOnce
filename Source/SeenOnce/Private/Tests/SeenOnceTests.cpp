// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "SeenOnceStatics.h"
#include "SeenOnceTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SeenOnceTests
{
	// CommandletContext as well as EditorContext, so these run on a build machine with no editor open.
	// Every rule below is a place this plugin can be quietly wrong - "quietly" being the operative word,
	// since the symptom of every one of them is a hint that does not appear, and nobody files a bug about
	// something they did not see.
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter;

	FSeenOnceHintDef MakeHint(const TCHAR* Id, const ESeenOnceRepeat Repeat, const int32 Priority = 0)
	{
		FSeenOnceHintDef Hint;
		Hint.HintId = FName(Id);
		Hint.Body = FText::FromString(FString(TEXT("Body of ")) + Id);
		Hint.Repeat = Repeat;
		Hint.Priority = Priority;
		Hint.DisplaySeconds = 5.0f;
		Hint.MinDisplaySeconds = 2.0f;
		return Hint;
	}

	FSeenOnceQueuedHint MakeQueued(const TCHAR* Id, const int32 Priority, const int64 Sequence, const double QueuedAt = 0.0)
	{
		FSeenOnceQueuedHint Queued;
		Queued.HintId = FName(Id);
		Queued.Priority = Priority;
		Queued.Sequence = Sequence;
		Queued.QueuedAtSeconds = QueuedAt;
		return Queued;
	}

	/** An entry written by hand, for the cases where the show did not come from this build. */
	FSeenOnceEntry MakeEntry(const TCHAR* Id, const ESeenOnceRepeat Rule, const int32 ShowCount = 1)
	{
		FSeenOnceEntry Entry;
		Entry.HintId = FName(Id);
		Entry.Rule = Rule;
		Entry.ShowCount = ShowCount;
		Entry.FirstSeenUtc = FDateTime(2026, 9, 1, 12, 0, 0);
		Entry.LastSeenUtc = FDateTime(2026, 9, 1, 12, 0, 0);
		return Entry;
	}

	/** What the subsystem does when a hint goes on screen, without a subsystem. */
	void Show(FSeenOnceState& State, const FSeenOnceHintDef& Hint, const double NowSeconds)
	{
		USeenOnceStatics::RecordShow(State, Hint, NowSeconds, FDateTime::UtcNow());
	}

	FSeenOnceActiveHint MakeActive(const TCHAR* Id, const int32 Priority, const double StartedAt,
		const double MinDisplay, const double Display)
	{
		FSeenOnceActiveHint Active;
		Active.HintId = FName(Id);
		Active.Priority = Priority;
		Active.StartedSeconds = StartedAt;
		Active.MinUntilSeconds = StartedAt + MinDisplay;
		Active.HideAtSeconds = StartedAt + Display;
		return Active;
	}
}

//
// (1) Once means once.
//
// The whole product in one assertion: a hint with the default rule answers "show me" exactly one time,
// and every trigger after that is refused. If this ever passes twice, the plugin is a queue with extra
// steps and the tutorial repeats itself.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSeenOnceShouldShowOnceTest,
	"SeenOnce.Memory.OnceShowsExactlyOnce",
	SeenOnceTests::TestFlags)

bool FSeenOnceShouldShowOnceTest::RunTest(const FString& Parameters)
{
	const FSeenOnceHintDef Hint = SeenOnceTests::MakeHint(TEXT("Hint_Door"), ESeenOnceRepeat::Once);
	FSeenOnceState State;

	TestTrue(TEXT("a hint nobody has seen may be shown"), USeenOnceStatics::ShouldShow(Hint, State, 0.0));

	SeenOnceTests::Show(State, Hint, 0.0);

	TestFalse(TEXT("after one show it may not be shown again"), USeenOnceStatics::ShouldShow(Hint, State, 1.0));
	TestFalse(TEXT("nor an hour later"), USeenOnceStatics::ShouldShow(Hint, State, 3600.0));
	TestFalse(TEXT("nor is it due"), USeenOnceStatics::IsDue(Hint, State, 3600.0));

	// The memory is one line, keyed on the id, and it counts.
	TestEqual(TEXT("one entry was written"), State.Entries.Num(), 1);
	TestEqual(TEXT("counted once"), State.Entries[0].ShowCount, 1);
	TestEqual(TEXT("under its own id"), State.Entries[0].HintId.ToString(), FString(TEXT("Hint_Door")));

	// A hint with no id cannot be remembered, so it must never be shown - otherwise it comes back on
	// every trigger for the rest of the game.
	FSeenOnceHintDef Nameless = Hint;
	Nameless.HintId = NAME_None;
	TestFalse(TEXT("a hint without an id is never shown"), USeenOnceStatics::ShouldShow(Nameless, State, 0.0));

	// NTimes is the same rule with a bigger number, and it stops on the number rather than after it.
	FSeenOnceHintDef Twice = SeenOnceTests::MakeHint(TEXT("Hint_Twice"), ESeenOnceRepeat::NTimes);
	Twice.MaxShowCount = 2;

	FSeenOnceState TwiceState;
	TestTrue(TEXT("N times: first"), USeenOnceStatics::ShouldShow(Twice, TwiceState, 0.0));
	SeenOnceTests::Show(TwiceState, Twice, 0.0);
	TestTrue(TEXT("N times: second"), USeenOnceStatics::ShouldShow(Twice, TwiceState, 1.0));
	SeenOnceTests::Show(TwiceState, Twice, 1.0);
	TestFalse(TEXT("N times: no third"), USeenOnceStatics::ShouldShow(Twice, TwiceState, 2.0));

	return true;
}

//
// (2) A reset forgets what it is allowed to forget, and nothing else.
//
// Three claims in one test, because they only mean anything together: OncePerSave comes back, Once does
// not, and an id belonging to no hint this build knows is not touched by either. The third is the one
// that protects somebody's save file.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSeenOnceResetAllTest,
	"SeenOnce.Memory.ResetAllForgetsPerSaveButNotForeverOrForeign",
	SeenOnceTests::TestFlags)

bool FSeenOnceResetAllTest::RunTest(const FString& Parameters)
{
	const FSeenOnceHintDef Forever = SeenOnceTests::MakeHint(TEXT("Hint_Forever"), ESeenOnceRepeat::Once);
	const FSeenOnceHintDef PerSave = SeenOnceTests::MakeHint(TEXT("Hint_PerSave"), ESeenOnceRepeat::OncePerSave);

	FSeenOnceState State;
	SeenOnceTests::Show(State, Forever, 0.0);
	SeenOnceTests::Show(State, PerSave, 1.0);

	// A memory from another build: renamed hint, newer save, feature switched off. Same shape either way.
	State.Entries.Add(SeenOnceTests::MakeEntry(TEXT("Hint_FromAnotherBranch"), ESeenOnceRepeat::OncePerSave));

	const TArray<FName> Known = { FName(TEXT("Hint_Forever")), FName(TEXT("Hint_PerSave")) };

	const int32 Removed = USeenOnceStatics::ResetAllInState(State, /*bIncludePermanent=*/false, Known);
	TestEqual(TEXT("exactly one entry was forgotten"), Removed, 1);

	TestTrue(TEXT("the per-save hint can be earned again"), USeenOnceStatics::ShouldShow(PerSave, State, 10.0));
	TestFalse(TEXT("the forever hint stays seen"), USeenOnceStatics::ShouldShow(Forever, State, 10.0));
	TestNotNull(TEXT("the foreign entry was not touched"), State.Find(FName(TEXT("Hint_FromAnotherBranch"))));

	// With the permanent memory included, the forever hint goes too - and the foreign one still does not.
	const int32 RemovedAll = USeenOnceStatics::ResetAllInState(State, /*bIncludePermanent=*/true, Known);
	TestEqual(TEXT("the forever hint was forgotten this time"), RemovedAll, 1);
	TestTrue(TEXT("and can be earned again"), USeenOnceStatics::ShouldShow(Forever, State, 20.0));
	TestNotNull(TEXT("the foreign entry survives both resets"), State.Find(FName(TEXT("Hint_FromAnotherBranch"))));

	// An empty list of known ids is "I do not know what any of this is", and the safe answer to that is
	// to leave the file alone.
	const int32 RemovedNothing = USeenOnceStatics::ResetAllInState(State, /*bIncludePermanent=*/true, TArray<FName>());
	TestEqual(TEXT("knowing nothing forgets nothing"), RemovedNothing, 0);

	return true;
}

//
// (3) The most important hint goes first, and equal ones go in the order they arrived.
//
// The second half is what makes a tutorial reproducible. A queue ordered by priority alone comes out in
// whatever order the array happened to be built in, and then two machines teach the same lesson in two
// different orders and nobody can reproduce the report.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSeenOncePickNextTest,
	"SeenOnce.Queue.PickNextTakesHighestPriorityAndBreaksTiesStably",
	SeenOnceTests::TestFlags)

bool FSeenOncePickNextTest::RunTest(const FString& Parameters)
{
	const FSeenOnceActiveHint Idle;

	TArray<FSeenOnceQueuedHint> Queue;
	Queue.Add(SeenOnceTests::MakeQueued(TEXT("Hint_Low"), 0, 0));
	Queue.Add(SeenOnceTests::MakeQueued(TEXT("Hint_High"), 10, 1));
	Queue.Add(SeenOnceTests::MakeQueued(TEXT("Hint_Mid"), 5, 2));

	const int32 First = USeenOnceStatics::PickNext(Queue, Idle, 0.0);
	TestEqual(TEXT("the most important one goes first"), Queue[First].HintId.ToString(), FString(TEXT("Hint_High")));

	// Three of equal importance, deliberately added out of sequence order.
	TArray<FSeenOnceQueuedHint> Tied;
	Tied.Add(SeenOnceTests::MakeQueued(TEXT("Hint_Third"), 3, 30));
	Tied.Add(SeenOnceTests::MakeQueued(TEXT("Hint_First"), 3, 10));
	Tied.Add(SeenOnceTests::MakeQueued(TEXT("Hint_Second"), 3, 20));

	const int32 Tie = USeenOnceStatics::PickNext(Tied, Idle, 0.0);
	TestEqual(TEXT("a tie goes to whoever came due first"), Tied[Tie].HintId.ToString(), FString(TEXT("Hint_First")));

	// And the answer does not depend on where in the array it sits.
	Tied.Swap(0, 1);
	const int32 TieAgain = USeenOnceStatics::PickNext(Tied, Idle, 0.0);
	TestEqual(TEXT("still the same one after reordering the array"), Tied[TieAgain].HintId.ToString(), FString(TEXT("Hint_First")));

	TestEqual(TEXT("an empty queue picks nothing"),
		USeenOnceStatics::PickNext(TArray<FSeenOnceQueuedHint>(), Idle, 0.0), INDEX_NONE);

	// The cooldown after the previous hint holds an empty screen empty.
	FSeenOnceActiveHint Cooling;
	Cooling.CooldownUntilSeconds = 100.0;
	TestEqual(TEXT("nothing starts during the cooldown"), USeenOnceStatics::PickNext(Queue, Cooling, 99.0), INDEX_NONE);
	TestTrue(TEXT("and something starts the moment it is over"),
		USeenOnceStatics::PickNext(Queue, Cooling, 100.0) != INDEX_NONE);

	return true;
}

//
// (4) A hint on screen is protected, and then only outranked.
//
// Without the first half, two hints coming due together produce one frame of the first one - text that
// flashed and taught nobody anything. Without the second half, a set of same-importance hints cuts
// itself off in a chain.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSeenOnceInterruptionTest,
	"SeenOnce.Queue.MinimumDisplayTimeProtectsAndOnlyHigherPriorityInterrupts",
	SeenOnceTests::TestFlags)

bool FSeenOnceInterruptionTest::RunTest(const FString& Parameters)
{
	// On screen since t=10, protected until t=12, down by itself at t=15.
	const FSeenOnceActiveHint Active = SeenOnceTests::MakeActive(TEXT("Hint_OnScreen"), 5, 10.0, 2.0, 5.0);

	TArray<FSeenOnceQueuedHint> MoreImportant;
	MoreImportant.Add(SeenOnceTests::MakeQueued(TEXT("Hint_Urgent"), 9, 1));

	TestEqual(TEXT("not interrupted immediately"), USeenOnceStatics::PickNext(MoreImportant, Active, 10.5), INDEX_NONE);
	TestEqual(TEXT("not interrupted a frame before the minimum"), USeenOnceStatics::PickNext(MoreImportant, Active, 11.99), INDEX_NONE);
	TestEqual(TEXT("interrupted once the minimum has passed"), USeenOnceStatics::PickNext(MoreImportant, Active, 12.0), 0);

	// Equal importance waits, however long it has been waiting. Otherwise a run of hints at one priority
	// takes turns cutting each other off, and every one of them is on screen for its minimum only.
	TArray<FSeenOnceQueuedHint> EquallyImportant;
	EquallyImportant.Add(SeenOnceTests::MakeQueued(TEXT("Hint_Equal"), 5, 1));
	TestEqual(TEXT("equal importance does not interrupt"), USeenOnceStatics::PickNext(EquallyImportant, Active, 14.0), INDEX_NONE);

	TArray<FSeenOnceQueuedHint> LessImportant;
	LessImportant.Add(SeenOnceTests::MakeQueued(TEXT("Hint_Minor"), 1, 1));
	TestEqual(TEXT("less importance does not interrupt"), USeenOnceStatics::PickNext(LessImportant, Active, 14.0), INDEX_NONE);

	// And with the screen free, the one that was waiting gets it.
	FSeenOnceActiveHint Free;
	Free.CooldownUntilSeconds = 15.0;
	TestEqual(TEXT("the waiting hint goes up once the screen is free"),
		USeenOnceStatics::PickNext(EquallyImportant, Free, 15.0), 0);

	return true;
}

//
// (5) Suppression delays, it does not delete.
//
// The difference between "not now" and "never". A bool that skips the trigger loses the hint; this
// queues it and hands it over the moment the cutscene ends, in the order the hints came due.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSeenOnceSuppressionTest,
	"SeenOnce.Queue.SuppressedHintsWaitInsteadOfBeingLost",
	SeenOnceTests::TestFlags)

bool FSeenOnceSuppressionTest::RunTest(const FString& Parameters)
{
	TArray<FSeenOnceQueuedHint> Queue;
	Queue.Add(SeenOnceTests::MakeQueued(TEXT("Hint_DuringCutscene"), 4, 1, /*QueuedAt=*/20.0));
	Queue.Add(SeenOnceTests::MakeQueued(TEXT("Hint_AlsoDuringCutscene"), 4, 2, /*QueuedAt=*/21.0));

	FSeenOnceActiveHint Suppressed;
	Suppressed.bSuppressed = true;

	TestEqual(TEXT("nothing is shown while suppressed"), USeenOnceStatics::PickNext(Queue, Suppressed, 25.0), INDEX_NONE);
	TestEqual(TEXT("and nothing was taken out of the queue"), Queue.Num(), 2);

	// Suppression lifts. Both are still there, and the one that came due first goes first.
	FSeenOnceActiveHint Allowed;
	const int32 Index = USeenOnceStatics::PickNext(Queue, Allowed, 30.0);
	TestTrue(TEXT("a hint is shown once suppression ends"), Index != INDEX_NONE);
	TestEqual(TEXT("the one that came due first"), Queue[Index].HintId.ToString(), FString(TEXT("Hint_DuringCutscene")));

	// A more important hint arriving during suppression still does not jump the gate.
	Queue.Add(SeenOnceTests::MakeQueued(TEXT("Hint_Urgent"), 99, 3, 22.0));
	TestEqual(TEXT("importance does not defeat suppression"), USeenOnceStatics::PickNext(Queue, Suppressed, 26.0), INDEX_NONE);

	return true;
}

//
// (6) An import keeps what it does not recognise.
//
// The most expensive silent failure this plugin could have. A build that dropped unknown ids would erase
// the memory of anybody who renamed a hint, loaded a save from a newer branch, or ran with a feature
// switched off - and the symptom is a player being taught something they already know, which nobody
// reports because it looks like a design decision.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSeenOnceImportKeepsUnknownIdsTest,
	"SeenOnce.Save.ImportKeepsUnknownIds",
	SeenOnceTests::TestFlags)

bool FSeenOnceImportKeepsUnknownIdsTest::RunTest(const FString& Parameters)
{
	FSeenOnceState Incoming;
	Incoming.Version = 1;
	Incoming.Entries.Add(SeenOnceTests::MakeEntry(TEXT("Hint_Known"), ESeenOnceRepeat::Once));
	Incoming.Entries.Add(SeenOnceTests::MakeEntry(TEXT("Hint_RenamedLastPatch"), ESeenOnceRepeat::Once));
	Incoming.Entries.Add(SeenOnceTests::MakeEntry(TEXT("Hint_FromTheDlc"), ESeenOnceRepeat::OncePerSave));

	// Session timing from a process that has since exited. It has to be forgotten, or a repeatable hint
	// looks as if it had just been shown - immediately after a load, of all moments.
	Incoming.Entries[0].LastShownSessionSeconds = 999.0;

	const FSeenOnceState Loaded = USeenOnceStatics::SanitizeLoadedState(Incoming);

	TestEqual(TEXT("every entry survived the import"), Loaded.Entries.Num(), 3);
	TestNotNull(TEXT("the renamed hint's memory is still there"), Loaded.Find(FName(TEXT("Hint_RenamedLastPatch"))));
	TestNotNull(TEXT("so is the one from content this build does not have"), Loaded.Find(FName(TEXT("Hint_FromTheDlc"))));
	TestTrue(TEXT("session timing was forgotten"), Loaded.Find(FName(TEXT("Hint_Known")))->LastShownSessionSeconds < 0.0);

	// Only this build's hint is known, so the other two are orphans - counted, named, never dropped.
	const TArray<FSeenOnceHintDef> Hints = { SeenOnceTests::MakeHint(TEXT("Hint_Known"), ESeenOnceRepeat::Once) };
	TestEqual(TEXT("two entries belong to no hint here"), USeenOnceStatics::CountOrphanEntries(Hints, Loaded), 2);

	// And they come back out of a summary as orphans rather than as hints that were never triggered,
	// which would be a lie in the one column that has to be trusted.
	const FSeenOnceSummary Summary = USeenOnceStatics::SummarizeState(Hints, Loaded);
	TestEqual(TEXT("one hint is known"), Summary.TotalHints, 1);
	TestEqual(TEXT("two entries are foreign"), Summary.OrphanCount, 2);
	TestEqual(TEXT("nothing is falsely reported as never triggered"), Summary.NeverTriggeredCount, 0);

	// Nameless junk is the one thing a load does drop: an entry with no id is not memory, it is noise.
	FSeenOnceState WithJunk;
	WithJunk.Entries.AddDefaulted();
	TestEqual(TEXT("a nameless entry is dropped"), USeenOnceStatics::SanitizeLoadedState(WithJunk).Entries.Num(), 0);

	// A duplicate id from a hand-merged file keeps the stronger memory rather than the last one written.
	FSeenOnceState Duplicated;
	Duplicated.Entries.Add(SeenOnceTests::MakeEntry(TEXT("Hint_Dup"), ESeenOnceRepeat::NTimes, /*ShowCount=*/3));
	Duplicated.Entries.Add(SeenOnceTests::MakeEntry(TEXT("Hint_Dup"), ESeenOnceRepeat::NTimes, /*ShowCount=*/1));

	const FSeenOnceState Merged = USeenOnceStatics::SanitizeLoadedState(Duplicated);
	TestEqual(TEXT("duplicates collapse to one"), Merged.Entries.Num(), 1);
	TestEqual(TEXT("keeping the higher count"), Merged.Entries[0].ShowCount, 3);

	return true;
}

//
// (7) The overview counts what it says it counts.
//
// "Never triggered" is the line somebody makes a decision on - it is how a hint whose condition can
// never become true gets found, and there is no other way to find that one. If this count is ever
// approximate, the screen is worse than not having it.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSeenOnceSummaryTest,
	"SeenOnce.Overview.SummaryCountsNeverTriggeredCorrectly",
	SeenOnceTests::TestFlags)

bool FSeenOnceSummaryTest::RunTest(const FString& Parameters)
{
	TArray<FSeenOnceHintDef> Hints;
	Hints.Add(SeenOnceTests::MakeHint(TEXT("Hint_Door"), ESeenOnceRepeat::Once));
	Hints.Add(SeenOnceTests::MakeHint(TEXT("Hint_Jump"), ESeenOnceRepeat::Once));
	Hints.Add(SeenOnceTests::MakeHint(TEXT("Hint_Inventory"), ESeenOnceRepeat::Once));
	Hints.Add(SeenOnceTests::MakeHint(TEXT("Hint_Unreachable"), ESeenOnceRepeat::Once));

	FSeenOnceState State;
	SeenOnceTests::Show(State, Hints[0], 1.0);
	SeenOnceTests::Show(State, Hints[1], 2.0);

	const FSeenOnceSummary Summary = USeenOnceStatics::SummarizeState(Hints, State);

	TestEqual(TEXT("four hints exist"), Summary.TotalHints, 4);
	TestEqual(TEXT("two have been seen"), Summary.SeenCount, 2);
	TestEqual(TEXT("two have never fired"), Summary.NeverTriggeredCount, 2);
	TestEqual(TEXT("no foreign entries"), Summary.OrphanCount, 0);
	TestEqual(TEXT("every hint has a line"), Summary.Lines.Num(), 4);

	// The never-triggered hints come first, because they are the reason to open this screen at all.
	TestTrue(TEXT("the first line has never fired"), Summary.Lines[0].bNeverTriggered);
	TestTrue(TEXT("the second line has never fired"), Summary.Lines[1].bNeverTriggered);
	TestFalse(TEXT("the third line has been seen"), Summary.Lines[2].bNeverTriggered);

	// Seen hints carry the timestamp the overview prints, and a hint that was never shown has none.
	const FSeenOnceHintStatus* Door = Summary.Lines.FindByPredicate(
		[](const FSeenOnceHintStatus& Line) { return Line.HintId == FName(TEXT("Hint_Door")); });
	TestNotNull(TEXT("the door hint is listed"), Door);
	TestTrue(TEXT("with a timestamp"), Door != nullptr && Door->LastSeenUtc != FDateTime(0));
	TestFalse(TEXT("and something to print"), USeenOnceStatics::FormatSeenTimestamp(Door->LastSeenUtc).IsEmpty());
	TestTrue(TEXT("a never-seen hint prints no timestamp"), USeenOnceStatics::FormatSeenTimestamp(FDateTime(0)).IsEmpty());

	// An empty memory is four findings, not zero. A screen that showed nothing until something had been
	// seen would hide exactly the case it exists for.
	const FSeenOnceSummary Fresh = USeenOnceStatics::SummarizeState(Hints, FSeenOnceState());
	TestEqual(TEXT("a fresh save has never triggered anything"), Fresh.NeverTriggeredCount, 4);
	TestEqual(TEXT("and has seen nothing"), Fresh.SeenCount, 0);

	// The header is a published shape - screenshots and documentation both use it.
	const FString Header = USeenOnceStatics::FormatSummaryHeader(Summary, /*PendingCount=*/2, FName(TEXT("Cutscene")));
	TestTrue(TEXT("the header counts hints"), Header.Contains(TEXT("hints 4")));
	TestTrue(TEXT("the header counts seen"), Header.Contains(TEXT("seen 2")));
	TestTrue(TEXT("the header counts pending"), Header.Contains(TEXT("pending 2")));
	TestTrue(TEXT("the header counts never triggered"), Header.Contains(TEXT("never triggered 2")));
	TestTrue(TEXT("the header names the suppression"), Header.Contains(TEXT("suppressed: cutscene")));

	const FString Quiet = USeenOnceStatics::FormatSummaryHeader(Summary, 0, NAME_None);
	TestTrue(TEXT("and says so when nothing is suppressed"), Quiet.Contains(TEXT("suppressed: no")));

	return true;
}

//
// (8) The repeatable rules repeat, and wait the interval they were given.
//
// EveryTime is the rule people reach for when they want a reminder rather than a lesson, and the reshow
// interval is the only thing between that and a hint that reappears every frame the condition is true.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSeenOnceReshowIntervalTest,
	"SeenOnce.Memory.EveryTimeRespectsTheReshowInterval",
	SeenOnceTests::TestFlags)

bool FSeenOnceReshowIntervalTest::RunTest(const FString& Parameters)
{
	FSeenOnceHintDef Reminder = SeenOnceTests::MakeHint(TEXT("Hint_Reminder"), ESeenOnceRepeat::EveryTime);
	Reminder.ReshowIntervalSeconds = 30.0f;

	FSeenOnceState State;
	TestTrue(TEXT("the first time is due"), USeenOnceStatics::IsDue(Reminder, State, 0.0));

	SeenOnceTests::Show(State, Reminder, 100.0);

	// The rule still allows it - the wait is a separate answer, and the caller usually wants to know
	// which of the two it got.
	TestTrue(TEXT("the rule still allows it"), USeenOnceStatics::ShouldShow(Reminder, State, 105.0));
	TestFalse(TEXT("but it is not due yet"), USeenOnceStatics::IsDue(Reminder, State, 105.0));
	TestFalse(TEXT("nor one second early"), USeenOnceStatics::IsDue(Reminder, State, 129.0));
	TestTrue(TEXT("due again after the interval"), USeenOnceStatics::IsDue(Reminder, State, 130.0));

	// With no interval set it is due again immediately, which is what "every time" says on the box.
	FSeenOnceHintDef Always = SeenOnceTests::MakeHint(TEXT("Hint_Always"), ESeenOnceRepeat::EveryTime);
	FSeenOnceState AlwaysState;
	SeenOnceTests::Show(AlwaysState, Always, 10.0);
	TestTrue(TEXT("no interval means due again at once"), USeenOnceStatics::IsDue(Always, AlwaysState, 10.0));

	// And a fresh session cannot inherit a wait: the loaded state has no session timing left.
	FSeenOnceState AfterLoad = USeenOnceStatics::SanitizeLoadedState(State);
	TestTrue(TEXT("after a load the reminder is due again"), USeenOnceStatics::IsDue(Reminder, AfterLoad, 0.0));

	return true;
}

//
// (9) A show is written down once, with both timestamps.
//
// First seen is what a support case is built on ("they saw this in the tutorial, not last night"), so it
// must never be overwritten by a later show, and it must never be empty for a hint that has been seen.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSeenOnceRecordShowTest,
	"SeenOnce.Memory.RecordShowKeepsFirstSeenAndAdvancesLastSeen",
	SeenOnceTests::TestFlags)

bool FSeenOnceRecordShowTest::RunTest(const FString& Parameters)
{
	FSeenOnceHintDef Hint = SeenOnceTests::MakeHint(TEXT("Hint_Repeatable"), ESeenOnceRepeat::NTimes);
	Hint.MaxShowCount = 5;

	FSeenOnceState State;

	const FDateTime FirstMoment(2026, 9, 8, 10, 0, 0);
	const FDateTime SecondMoment(2026, 9, 8, 11, 30, 0);

	USeenOnceStatics::RecordShow(State, Hint, 5.0, FirstMoment);
	USeenOnceStatics::RecordShow(State, Hint, 500.0, SecondMoment);

	const FSeenOnceEntry* Entry = State.Find(FName(TEXT("Hint_Repeatable")));
	TestNotNull(TEXT("the entry exists"), Entry);
	if (Entry == nullptr)
	{
		return false;
	}

	TestEqual(TEXT("one entry, not two"), State.Entries.Num(), 1);
	TestEqual(TEXT("counted twice"), Entry->ShowCount, 2);
	TestTrue(TEXT("first seen is the first moment"), Entry->FirstSeenUtc == FirstMoment);
	TestTrue(TEXT("last seen is the later one"), Entry->LastSeenUtc == SecondMoment);
	TestTrue(TEXT("session timing follows the last show"), FMath::IsNearlyEqual(Entry->LastShownSessionSeconds, 500.0));
	TestTrue(TEXT("the rule is written down with the entry"), Entry->Rule == ESeenOnceRepeat::NTimes);

	// Forgetting one hint takes the whole entry with it, so it is genuinely new again.
	TestTrue(TEXT("reset removes it"), USeenOnceStatics::ResetHintInState(State, FName(TEXT("Hint_Repeatable"))));
	TestEqual(TEXT("nothing left"), State.Entries.Num(), 0);
	TestFalse(TEXT("resetting it twice changes nothing"), USeenOnceStatics::ResetHintInState(State, FName(TEXT("Hint_Repeatable"))));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
