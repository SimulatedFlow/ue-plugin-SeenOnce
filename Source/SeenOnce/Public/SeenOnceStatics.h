// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SeenOnceTypes.h"
#include "SeenOnceStatics.generated.h"

class USeenOnceSubsystem;

/**
 * The decision logic, and the Blueprint front door.
 *
 * The upper half of this class is the part worth reading twice: ShouldShow, IsDue, PickNext,
 * SummarizeState and the three state operations are static functions over plain structs. No world, no
 * game instance, no asset, no frame. They are what the subsystem calls, and they are what the automation
 * tests exercise - so "a hint on screen is not interrupted before its minimum display time" is a claim
 * that gets checked on the build machine rather than by somebody playing carefully.
 *
 * That split is also why they are public. A project that wants its own scheduler, its own display or its
 * own save format can call these directly and keep the rules without keeping anything else.
 */
UCLASS()
class SEENONCE_API USeenOnceStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//~ The rules, with no world behind them ----------------------------------------------------------

	/**
	 * Does the repeat rule still allow this hint to be shown at all?
	 *
	 * This is memory only: it asks whether the hint has shows left, not whether its condition is true and
	 * not whether now is a good moment. Once and OncePerSave answer true until the first show; NTimes
	 * until MaxShowCount; EveryTime always.
	 */
	UFUNCTION(BlueprintPure, Category = "SeenOnce|Rules")
	static bool ShouldShow(const FSeenOnceHintDef& Hint, const FSeenOnceState& State, double NowSeconds);

	/**
	 * May this hint go into the queue right now?
	 *
	 * ShouldShow, plus the hint's own reshow interval. The two are separate because they fail for
	 * different reasons and a caller usually wants to know which: "you have already seen this" is
	 * permanent and worth logging once, "not yet" is temporary and worth logging never.
	 */
	UFUNCTION(BlueprintPure, Category = "SeenOnce|Rules")
	static bool IsDue(const FSeenOnceHintDef& Hint, const FSeenOnceState& State, double NowSeconds);

	/**
	 * Which queued hint should be on screen next - or -1 for "leave things as they are".
	 *
	 * The whole scheduling policy is here, and it is four rules:
	 *
	 *   1. While suppression is on, nothing starts. Nothing is dropped either; the queue is untouched and
	 *      the hint appears when suppression ends. That is the difference between "not now" and "never",
	 *      and getting it wrong silently loses tutorial hints in cutscene-heavy games.
	 *   2. With the screen free, nothing starts before the cooldown has passed.
	 *   3. With a hint on screen, nothing interrupts it before its minimum display time - however
	 *      important. Text that appears for four frames taught nobody anything.
	 *   4. After that time, only a strictly higher priority interrupts. Equal priority waits, which is
	 *      what keeps a set of same-importance hints from cutting each other off.
	 *
	 * Among candidates the highest priority wins, and ties break on arrival order, so the result is the
	 * same on every machine and every run.
	 */
	UFUNCTION(BlueprintPure, Category = "SeenOnce|Rules")
	static int32 PickNext(const TArray<FSeenOnceQueuedHint>& Queue, const FSeenOnceActiveHint& Current, double NowSeconds);

	/**
	 * Every hint counted against the memory: seen, never triggered, and entries nothing claims.
	 *
	 * The lines come back ordered the way the overview prints them - never triggered first, then seen by
	 * recency, then orphans - because the ordering is part of the answer. The never-triggered hints are
	 * the reason to look at this screen at all, so they cannot be somewhere in the middle.
	 */
	UFUNCTION(BlueprintPure, Category = "SeenOnce|Rules")
	static FSeenOnceSummary SummarizeState(const TArray<FSeenOnceHintDef>& Hints, const FSeenOnceState& State);

	//~ State operations, also with no world behind them ------------------------------------------------

	/** Writes down that a hint was shown: count, rule, first and last timestamp, session time. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|State")
	static void RecordShow(UPARAM(ref) FSeenOnceState& State, const FSeenOnceHintDef& Hint, double NowSeconds, const FDateTime& NowUtc);

	/** Forgets one hint completely, whatever its rule. Returns true when there was something to forget. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|State")
	static bool ResetHintInState(UPARAM(ref) FSeenOnceState& State, FName HintId);

	/**
	 * Forgets a set of hints, and returns how many it forgot.
	 *
	 * Two things about this one are deliberate and both protect somebody's save file.
	 *
	 * Entries whose recorded rule is Once are kept unless bIncludePermanent is set. "Once, forever" has
	 * to mean something even against a reset of everything else, or the rule is just a longer way of
	 * spelling OncePerSave.
	 *
	 * Entries whose id is not in KnownHintIds are never touched. Those belong to hints this build does
	 * not have - a renamed asset, a newer save, a disabled feature - and throwing them away is how a
	 * player ends up watching the tutorial twice. An empty KnownHintIds therefore resets nothing, which
	 * is the safe reading of "I do not know what any of this is".
	 */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|State")
	static int32 ResetAllInState(UPARAM(ref) FSeenOnceState& State, bool bIncludePermanent, const TArray<FName>& KnownHintIds);

	/**
	 * Makes a state that came off disk, or out of somebody else's save game, safe to use.
	 *
	 * Drops nameless entries, collapses duplicate ids onto the higher count, and forgets the session
	 * timing (which meant something in a process that has since exited).
	 *
	 * What it does not do is filter: every id survives, including ids no hint in this build claims. That
	 * is the single most important line in this plugin's save handling. A build that quietly dropped
	 * unknown ids would erase the memory of anybody who renamed a hint, loaded an older save, or ran a
	 * branch with a feature switched off - and the symptom would be a player being taught something they
	 * already knew, which nobody reports as a bug because it looks like a design decision.
	 */
	UFUNCTION(BlueprintPure, Category = "SeenOnce|State")
	static FSeenOnceState SanitizeLoadedState(const FSeenOnceState& Loaded);

	/** How many entries in this state belong to no hint in the list. The orphan count, on its own. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce|State")
	static int32 CountOrphanEntries(const TArray<FSeenOnceHintDef>& Hints, const FSeenOnceState& State);

	//~ Blueprint front door ---------------------------------------------------------------------------

	/** The subsystem for this world's game instance, or null outside a game. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce", meta = (WorldContext = "WorldContextObject"))
	static USeenOnceSubsystem* GetSeenOnce(const UObject* WorldContextObject);

	/** Trigger a hint by id. True when it was queued; false when it was already seen, or is not due. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce", meta = (WorldContext = "WorldContextObject"))
	static bool TriggerHint(const UObject* WorldContextObject, FName HintId);

	/** Trigger every hint carrying this tag. Returns how many were queued. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce", meta = (WorldContext = "WorldContextObject"))
	static int32 TriggerHintsByTag(const UObject* WorldContextObject, FGameplayTag Tag);

	/** Has this hint been shown in the current memory? */
	UFUNCTION(BlueprintPure, Category = "SeenOnce", meta = (WorldContext = "WorldContextObject"))
	static bool HasSeenHint(const UObject* WorldContextObject, FName HintId);

	/** Turn suppression on or off, with a reason the overview prints. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce", meta = (WorldContext = "WorldContextObject"))
	static void SetHintsSuppressed(const UObject* WorldContextObject, bool bSuppressed, FName Reason);

	/** Nice, short, human text for a timestamp in the overview. Empty for a never-seen hint. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	static FString FormatSeenTimestamp(const FDateTime& SeenUtc);

	/** The one-line header the overview prints, so a custom HUD can print the same one. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	static FString FormatSummaryHeader(const FSeenOnceSummary& Summary, int32 PendingCount, FName SuppressionReason);
};
