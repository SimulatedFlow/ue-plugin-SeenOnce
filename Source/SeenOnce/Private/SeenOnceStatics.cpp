// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "SeenOnceStatics.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "SeenOnceSubsystem.h"

namespace SeenOnceRules
{
	/** Long enough ago that no reshow interval can consider it recent, and finite so arithmetic is safe. */
	constexpr double LongAgoSeconds = -1.0e9;

	/** The number of shows a rule allows in total, or MAX_int32 for "no limit". */
	static int32 AllowedShows(const FSeenOnceHintDef& Hint)
	{
		switch (Hint.Repeat)
		{
		case ESeenOnceRepeat::Once:
		case ESeenOnceRepeat::OncePerSave:
			return 1;

		case ESeenOnceRepeat::NTimes:
			// A hint that says "show me at most zero times" is a configuration mistake, not an instruction
			// to never show it - the rule for that is to delete the hint. One is the floor.
			return FMath::Max(1, Hint.MaxShowCount);

		case ESeenOnceRepeat::EveryTime:
		default:
			return MAX_int32;
		}
	}
}

bool USeenOnceStatics::ShouldShow(const FSeenOnceHintDef& Hint, const FSeenOnceState& State, const double NowSeconds)
{
	if (!Hint.IsValid())
	{
		// No id means no memory, and a hint that cannot be remembered must not be shown - it would come
		// back on the next trigger and the next, forever, which looks exactly like the bug this plugin
		// claims to fix.
		return false;
	}

	const FSeenOnceEntry* Entry = State.Find(Hint.HintId);
	if (Entry == nullptr)
	{
		return true;
	}

	return Entry->ShowCount < SeenOnceRules::AllowedShows(Hint);
}

bool USeenOnceStatics::IsDue(const FSeenOnceHintDef& Hint, const FSeenOnceState& State, const double NowSeconds)
{
	if (!ShouldShow(Hint, State, NowSeconds))
	{
		return false;
	}

	if (Hint.ReshowIntervalSeconds <= 0.0f)
	{
		return true;
	}

	const FSeenOnceEntry* Entry = State.Find(Hint.HintId);
	if (Entry == nullptr || Entry->ShowCount <= 0)
	{
		return true;
	}

	return (NowSeconds - Entry->LastShownSessionSeconds) >= static_cast<double>(Hint.ReshowIntervalSeconds);
}

int32 USeenOnceStatics::PickNext(const TArray<FSeenOnceQueuedHint>& Queue, const FSeenOnceActiveHint& Current, const double NowSeconds)
{
	if (Queue.Num() == 0)
	{
		return INDEX_NONE;
	}

	// Rule 1. Suppression stops the screen, not the queue. Everything waiting stays waiting.
	if (Current.bSuppressed)
	{
		return INDEX_NONE;
	}

	// Rule 3. A hint that is on screen is protected for its minimum display time, whatever is waiting.
	if (Current.IsShowing() && NowSeconds < Current.MinUntilSeconds)
	{
		return INDEX_NONE;
	}

	// Rule 2. With the screen free, the quiet time after the last hint has to have passed first.
	if (!Current.IsShowing() && NowSeconds < Current.CooldownUntilSeconds)
	{
		return INDEX_NONE;
	}

	// Highest priority wins; equal priority goes in arrival order. The second half is what makes the
	// result reproducible - a queue ordered only by priority comes out differently depending on how the
	// array happened to be built, and then two machines show the tutorial in two different orders.
	int32 BestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Queue.Num(); ++Index)
	{
		const FSeenOnceQueuedHint& Candidate = Queue[Index];
		if (BestIndex == INDEX_NONE)
		{
			BestIndex = Index;
			continue;
		}

		const FSeenOnceQueuedHint& Best = Queue[BestIndex];
		if (Candidate.Priority > Best.Priority ||
			(Candidate.Priority == Best.Priority && Candidate.Sequence < Best.Sequence))
		{
			BestIndex = Index;
		}
	}

	// Rule 4. Interrupting costs a strictly higher priority. Equal waits its turn.
	if (Current.IsShowing() && Queue[BestIndex].Priority <= Current.Priority)
	{
		return INDEX_NONE;
	}

	return BestIndex;
}

FSeenOnceSummary USeenOnceStatics::SummarizeState(const TArray<FSeenOnceHintDef>& Hints, const FSeenOnceState& State)
{
	FSeenOnceSummary Summary;
	Summary.Lines.Reserve(Hints.Num() + State.Entries.Num());

	TSet<FName> KnownIds;
	KnownIds.Reserve(Hints.Num());

	for (const FSeenOnceHintDef& Hint : Hints)
	{
		if (!Hint.IsValid())
		{
			continue;
		}

		KnownIds.Add(Hint.HintId);
		++Summary.TotalHints;

		FSeenOnceHintStatus Status;
		Status.HintId = Hint.HintId;
		Status.Repeat = Hint.Repeat;

		if (const FSeenOnceEntry* Entry = State.Find(Hint.HintId))
		{
			Status.ShowCount = Entry->ShowCount;
			Status.LastSeenUtc = Entry->LastSeenUtc;
			Status.bSeen = Entry->ShowCount > 0;
		}

		// "Never triggered" is exactly "no show has ever been recorded". Not "not on screen", not "not
		// due" - the hint has never once fired in this memory, which is the only reading that finds the
		// hint whose condition can never become true.
		Status.bNeverTriggered = !Status.bSeen;

		Summary.SeenCount += Status.bSeen ? 1 : 0;
		Summary.NeverTriggeredCount += Status.bNeverTriggered ? 1 : 0;

		Summary.Lines.Add(MoveTemp(Status));
	}

	for (const FSeenOnceEntry& Entry : State.Entries)
	{
		if (Entry.HintId.IsNone() || KnownIds.Contains(Entry.HintId))
		{
			continue;
		}

		++Summary.OrphanCount;

		FSeenOnceHintStatus Status;
		Status.HintId = Entry.HintId;
		Status.Repeat = Entry.Rule;
		Status.ShowCount = Entry.ShowCount;
		Status.LastSeenUtc = Entry.LastSeenUtc;
		Status.bSeen = Entry.ShowCount > 0;
		Status.bNeverTriggered = false;
		Status.bOrphan = true;

		Summary.Lines.Add(MoveTemp(Status));
	}

	// Never triggered at the top, because that is what this screen is for. Then the seen hints newest
	// first, because after a session the interesting question is what just happened. Orphans last: they
	// are memory, not hints, and they are here to prove nothing was thrown away.
	Summary.Lines.Sort([](const FSeenOnceHintStatus& A, const FSeenOnceHintStatus& B)
	{
		auto GroupOf = [](const FSeenOnceHintStatus& Status)
		{
			return Status.bOrphan ? 2 : (Status.bNeverTriggered ? 0 : 1);
		};

		const int32 GroupA = GroupOf(A);
		const int32 GroupB = GroupOf(B);
		if (GroupA != GroupB)
		{
			return GroupA < GroupB;
		}

		if (GroupA == 0)
		{
			// Nothing to sort them by but their names, and names are at least stable.
			return A.HintId.LexicalLess(B.HintId);
		}

		if (A.LastSeenUtc != B.LastSeenUtc)
		{
			return A.LastSeenUtc > B.LastSeenUtc;
		}

		return A.HintId.LexicalLess(B.HintId);
	});

	return Summary;
}

void USeenOnceStatics::RecordShow(FSeenOnceState& State, const FSeenOnceHintDef& Hint, const double NowSeconds, const FDateTime& NowUtc)
{
	if (!Hint.IsValid())
	{
		return;
	}

	FSeenOnceEntry& Entry = State.FindOrAdd(Hint.HintId);

	// The rule is written down with the entry rather than looked up later, so a reset can tell a
	// "forever" memory from a "this run" memory even for a hint whose asset is no longer in the build.
	Entry.Rule = Hint.Repeat;
	Entry.ShowCount = FMath::Max(0, Entry.ShowCount) + 1;
	Entry.LastSeenUtc = NowUtc;
	Entry.LastShownSessionSeconds = NowSeconds;

	if (Entry.FirstSeenUtc == FDateTime(0))
	{
		Entry.FirstSeenUtc = NowUtc;
	}
}

bool USeenOnceStatics::ResetHintInState(FSeenOnceState& State, const FName HintId)
{
	if (HintId.IsNone())
	{
		return false;
	}

	return State.Entries.RemoveAll([HintId](const FSeenOnceEntry& Entry) { return Entry.HintId == HintId; }) > 0;
}

int32 USeenOnceStatics::ResetAllInState(FSeenOnceState& State, const bool bIncludePermanent, const TArray<FName>& KnownHintIds)
{
	if (KnownHintIds.Num() == 0)
	{
		return 0;
	}

	const TSet<FName> Known(KnownHintIds);

	const int32 Removed = State.Entries.RemoveAll([&Known, bIncludePermanent](const FSeenOnceEntry& Entry)
	{
		// Not ours to forget.
		if (!Known.Contains(Entry.HintId))
		{
			return false;
		}

		// "Once, forever" survives a reset of everything else. Otherwise the rule would mean nothing.
		if (Entry.Rule == ESeenOnceRepeat::Once && !bIncludePermanent)
		{
			return false;
		}

		return true;
	});

	if (Removed > 0)
	{
		++State.Generation;
	}

	return Removed;
}

FSeenOnceState USeenOnceStatics::SanitizeLoadedState(const FSeenOnceState& Loaded)
{
	FSeenOnceState Result;
	Result.Version = Loaded.Version;
	Result.Generation = Loaded.Generation;
	Result.Entries.Reserve(Loaded.Entries.Num());

	for (const FSeenOnceEntry& Entry : Loaded.Entries)
	{
		if (Entry.HintId.IsNone())
		{
			continue;
		}

		FSeenOnceEntry* Existing = Result.Find(Entry.HintId);
		if (Existing == nullptr)
		{
			FSeenOnceEntry& Added = Result.Entries.Add_GetRef(Entry);

			// Session seconds belonged to a process that has since ended. Keeping them would make a
			// repeatable hint look as if it had just been shown, and it would stay silent for as long as
			// its reshow interval - after a load, of all moments.
			Added.LastShownSessionSeconds = SeenOnceRules::LongAgoSeconds;
			continue;
		}

		// A duplicate id can only come from a file somebody merged by hand. Keep the stronger memory:
		// the higher count and the earlier first-seen, because forgetting is the expensive mistake here.
		Existing->ShowCount = FMath::Max(Existing->ShowCount, Entry.ShowCount);
		Existing->LastSeenUtc = FMath::Max(Existing->LastSeenUtc, Entry.LastSeenUtc);

		if (Entry.FirstSeenUtc != FDateTime(0) &&
			(Existing->FirstSeenUtc == FDateTime(0) || Entry.FirstSeenUtc < Existing->FirstSeenUtc))
		{
			Existing->FirstSeenUtc = Entry.FirstSeenUtc;
		}
	}

	return Result;
}

int32 USeenOnceStatics::CountOrphanEntries(const TArray<FSeenOnceHintDef>& Hints, const FSeenOnceState& State)
{
	TSet<FName> KnownIds;
	KnownIds.Reserve(Hints.Num());
	for (const FSeenOnceHintDef& Hint : Hints)
	{
		if (Hint.IsValid())
		{
			KnownIds.Add(Hint.HintId);
		}
	}

	int32 Count = 0;
	for (const FSeenOnceEntry& Entry : State.Entries)
	{
		Count += (!Entry.HintId.IsNone() && !KnownIds.Contains(Entry.HintId)) ? 1 : 0;
	}

	return Count;
}

USeenOnceSubsystem* USeenOnceStatics::GetSeenOnce(const UObject* WorldContextObject)
{
	if (GEngine == nullptr || WorldContextObject == nullptr)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (World == nullptr)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetSubsystem<USeenOnceSubsystem>() : nullptr;
}

bool USeenOnceStatics::TriggerHint(const UObject* WorldContextObject, const FName HintId)
{
	USeenOnceSubsystem* Subsystem = GetSeenOnce(WorldContextObject);
	return Subsystem != nullptr ? Subsystem->Trigger(HintId) : false;
}

int32 USeenOnceStatics::TriggerHintsByTag(const UObject* WorldContextObject, const FGameplayTag Tag)
{
	USeenOnceSubsystem* Subsystem = GetSeenOnce(WorldContextObject);
	return Subsystem != nullptr ? Subsystem->TriggerByTag(Tag) : 0;
}

bool USeenOnceStatics::HasSeenHint(const UObject* WorldContextObject, const FName HintId)
{
	const USeenOnceSubsystem* Subsystem = GetSeenOnce(WorldContextObject);
	return Subsystem != nullptr ? Subsystem->HasSeen(HintId) : false;
}

void USeenOnceStatics::SetHintsSuppressed(const UObject* WorldContextObject, const bool bSuppressed, const FName Reason)
{
	if (USeenOnceSubsystem* Subsystem = GetSeenOnce(WorldContextObject))
	{
		Subsystem->SetSuppressed(bSuppressed, Reason);
	}
}

FString USeenOnceStatics::FormatSeenTimestamp(const FDateTime& SeenUtc)
{
	if (SeenUtc == FDateTime(0))
	{
		return FString();
	}

	// Printed in local time, because the person reading the overview is sitting in front of the game and
	// is comparing it against their own clock, not against UTC. The memory itself stores UTC so that a
	// save taken across a time zone does not travel backwards.
	const FTimespan Offset = FDateTime::Now() - FDateTime::UtcNow();
	const FDateTime Local = SeenUtc + Offset;

	return Local.ToString(TEXT("%Y-%m-%d %H:%M"));
}

FString USeenOnceStatics::FormatSummaryHeader(const FSeenOnceSummary& Summary, const int32 PendingCount, const FName SuppressionReason)
{
	const FString Suppression = SuppressionReason.IsNone() ? TEXT("no") : SuppressionReason.ToString().ToLower();

	FString Header = FString::Printf(TEXT("hints %d | seen %d | pending %d | never triggered %d | suppressed: %s"),
		Summary.TotalHints, Summary.SeenCount, PendingCount, Summary.NeverTriggeredCount, *Suppression);

	// Only shown when it is not zero. A count of foreign memory is important when it exists and noise
	// when it does not.
	if (Summary.OrphanCount > 0)
	{
		Header += FString::Printf(TEXT(" | kept unknown %d"), Summary.OrphanCount);
	}

	return Header;
}
