// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SeenOnceTypes.generated.h"

/**
 * How often a hint may be shown.
 *
 * The difference between the first two is the one that matters and the one everybody gets wrong the
 * first time, so it is spelled out here rather than in the documentation only.
 *
 *   Once         - once, and it stays shown. A reset of the whole set does not bring it back; only an
 *                  explicit reset of this hint, or a different save, does. This is the default because
 *                  it is what a tutorial hint means: the player has been taught this.
 *   OncePerSave  - once per run through the memory. ResetAll() forgets it and it can be earned again.
 *                  Use it for hints that belong to a chapter, a difficulty, a new game plus.
 *   EveryTime    - every time the condition becomes true, subject to the reshow interval. Use it for
 *                  reminders, not for teaching.
 *   NTimes       - up to MaxShowCount times, then never again. For the hint people need twice.
 *
 * All four are per save game. A new save has an empty memory and shows the hints again, and that is
 * correct: it is a different player getting to the same place for the first time.
 */
UENUM(BlueprintType)
enum class ESeenOnceRepeat : uint8
{
	/** Once, and it stays shown. Survives ResetAll(). */
	Once UMETA(DisplayName = "Once (forever)"),

	/** Once until the memory is reset. */
	OncePerSave UMETA(DisplayName = "Once per save"),

	/** Every time the condition becomes true, subject to the reshow interval. */
	EveryTime UMETA(DisplayName = "Every time"),

	/** Up to MaxShowCount times. */
	NTimes UMETA(DisplayName = "N times"),
};

/**
 * What makes a hint come due.
 *
 * The built-in five cover the majority of tutorial hints in the games that need tutorial hints, and the
 * sixth is the door out: Manual is triggered from Blueprint or C++ with one call, so nothing is locked
 * away behind a condition type this plugin failed to imagine.
 */
UENUM(BlueprintType)
enum class ESeenOnceCondition : uint8
{
	/** Nothing automatic. Trigger(Id) from Blueprint, C++ or a USeenOnceComponent. */
	Manual UMETA(DisplayName = "Manual (triggered)"),

	/** A gameplay tag is present in the subsystem's condition tags. */
	GameplayTag UMETA(DisplayName = "Gameplay tag present"),

	/** An actor entered an area whose id matches ConditionKey. */
	AreaEntered UMETA(DisplayName = "Area entered"),

	/** The action named by ConditionKey has been performed RequiredCount times. */
	ActionCount UMETA(DisplayName = "Action performed N times"),

	/** RequiredPlayTimeSeconds of play time have passed. */
	PlayTime UMETA(DisplayName = "Time played"),

	/** The item named by ConditionKey has been received RequiredCount times. */
	ItemReceived UMETA(DisplayName = "Item received"),
};

/** Why a hint stopped being shown. Carried by OnHintHidden so a custom widget can animate differently. */
UENUM(BlueprintType)
enum class ESeenOnceHideReason : uint8
{
	/** Its display time ran out. The normal case. */
	Elapsed,

	/** A more important hint took the screen after the minimum display time had passed. */
	Interrupted,

	/** Suppression came on, the hint was reset, or the game tore down. */
	Cleared,
};

/**
 * A hint reduced to plain values.
 *
 * USeenOnceHint is the data asset an author edits; this is what the decision logic sees. The split is
 * the whole reason the rules are testable: ShouldShow, IsDue, PickNext and SummarizeState take structs
 * like this one and a state, and answer without a world, a game instance, an asset or a frame.
 */
USTRUCT(BlueprintType)
struct SEENONCE_API FSeenOnceHintDef
{
	GENERATED_BODY()

	/** The identity of the hint, and the key it is remembered under. Renaming it forgets the hint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	FName HintId;

	/** What the player reads. FText, so it is translatable - and so LocaleGuard can check it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	FText Body;

	/**
	 * How important this hint is. Higher wins.
	 *
	 * Priority decides two things: which of several due hints goes first, and whether a hint is allowed
	 * to interrupt one that is already on screen. Only a strictly higher priority interrupts, so a set of
	 * hints that all share one number can never cut each other off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	int32 Priority = 0;

	/** How long the hint stays on screen once nothing interrupts it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce", meta = (ClampMin = "0.1", Units = "s"))
	float DisplaySeconds = 5.0f;

	/**
	 * How long the hint is protected from being interrupted.
	 *
	 * This is the number that stops text from flashing. Without it, two hints coming due together produce
	 * one frame of the first and then the second, and the player saw a flicker rather than a sentence.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce", meta = (ClampMin = "0.0", Units = "s"))
	float MinDisplaySeconds = 1.5f;

	/**
	 * Quiet time after this hint before the next one may appear. Zero uses the project default.
	 *
	 * Hints that arrive back to back read as a wall of text even when each one is individually fine.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce", meta = (ClampMin = "0.0", Units = "s"))
	float CooldownSeconds = 0.0f;

	/** How often this hint may be shown at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	ESeenOnceRepeat Repeat = ESeenOnceRepeat::Once;

	/** For NTimes: the number of shows after which the hint is done. Ignored by the other rules. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce", meta = (ClampMin = "1"))
	int32 MaxShowCount = 2;

	/**
	 * The shortest gap between two shows of this same hint, for the rules that allow more than one.
	 *
	 * Zero means no gap. It is measured in session seconds, not save-game time, because "do not repeat
	 * this within ten seconds" is about the player's attention, not about their progress.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce", meta = (ClampMin = "0.0", Units = "s"))
	float ReshowIntervalSeconds = 0.0f;

	/** What makes this hint come due. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	ESeenOnceCondition Condition = ESeenOnceCondition::Manual;

	/** GameplayTag condition: the tag that has to be present. Also what TriggerByTag matches against. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FGameplayTag ConditionTag;

	/** AreaEntered / ActionCount / ItemReceived: the area, action or item this hint watches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FName ConditionKey;

	/** ActionCount / ItemReceived: how many times before the hint comes due. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	/** PlayTime: how much play time has to have passed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition", meta = (ClampMin = "0.0", Units = "s"))
	float RequiredPlayTimeSeconds = 0.0f;

	/** True when this definition is usable at all. A hint without an id cannot be remembered. */
	bool IsValid() const
	{
		return !HintId.IsNone();
	}

	/** The cooldown this hint imposes, falling back to the project default when it sets none. */
	float GetCooldownSeconds(const float ProjectDefault) const
	{
		return (CooldownSeconds > 0.0f) ? CooldownSeconds : ProjectDefault;
	}
};

/**
 * One line of the memory: a hint that has been shown at least once.
 *
 * The rule the hint had when it was written down is stored with the entry on purpose. It is what lets
 * ResetAll() tell a "once per save" memory from a "once, forever" memory without having to look up an
 * asset - which matters exactly when the asset is gone, because that is the case where guessing would
 * silently throw away a memory.
 */
USTRUCT(BlueprintType)
struct SEENONCE_API FSeenOnceEntry
{
	GENERATED_BODY()

	/** Which hint this is the memory of. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	FName HintId;

	/** How often it has been shown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	int32 ShowCount = 0;

	/** The repeat rule in force when the entry was last written. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	ESeenOnceRepeat Rule = ESeenOnceRepeat::Once;

	/** When the player first saw it, UTC. Written once and never overwritten. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	FDateTime FirstSeenUtc = FDateTime(0);

	/** When the player last saw it, UTC. This is the timestamp the overview prints. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	FDateTime LastSeenUtc = FDateTime(0);

	/**
	 * When it was last shown in session seconds.
	 *
	 * Session time, not save-game time: it is only ever used for the reshow interval, which is about how
	 * long ago the player read the line and means nothing across a restart. Loading resets it, so a
	 * repeatable hint is never mistakenly considered "just shown" after a load.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	double LastShownSessionSeconds = -1.0e9;

	bool IsValid() const
	{
		return !HintId.IsNone();
	}
};

/**
 * The whole memory: what this save game has already been taught.
 *
 * This is the struct ExportState() hands out and ImportState() takes back, and it is deliberately small
 * and dull - ids, counts, timestamps. It has to be, because a project is expected to put it inside its
 * own save game, and anything clever in here would become that project's problem on the next engine
 * upgrade.
 *
 * Entries for hints this build has never heard of are kept, not dropped. See ImportState.
 */
USTRUCT(BlueprintType)
struct SEENONCE_API FSeenOnceState
{
	GENERATED_BODY()

	/** Format version of this struct, for a project that stores it and later has to migrate it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	int32 Version = 1;

	/**
	 * How often the memory has been reset.
	 *
	 * Purely informational - the reset itself removes entries rather than hiding them behind a counter,
	 * because a memory that still contains what it claims to have forgotten is a memory that will show
	 * up in somebody's save file inspector and be reported as a bug.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	int32 Generation = 0;

	/** One line per hint that has been shown at least once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	TArray<FSeenOnceEntry> Entries;

	const FSeenOnceEntry* Find(const FName HintId) const
	{
		return Entries.FindByPredicate([HintId](const FSeenOnceEntry& Entry) { return Entry.HintId == HintId; });
	}

	FSeenOnceEntry* Find(const FName HintId)
	{
		return Entries.FindByPredicate([HintId](const FSeenOnceEntry& Entry) { return Entry.HintId == HintId; });
	}

	FSeenOnceEntry& FindOrAdd(const FName HintId)
	{
		if (FSeenOnceEntry* Existing = Find(HintId))
		{
			return *Existing;
		}

		FSeenOnceEntry& Added = Entries.AddDefaulted_GetRef();
		Added.HintId = HintId;
		return Added;
	}
};

/** A hint waiting for its turn. */
USTRUCT(BlueprintType)
struct SEENONCE_API FSeenOnceQueuedHint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	FName HintId;

	/** Copied from the definition when the hint was queued, so the queue can be ordered on its own. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	int32 Priority = 0;

	/**
	 * Arrival order, and the reason ties are stable.
	 *
	 * Two hints with the same importance have to come out in the order they came due, every run, or the
	 * tutorial reads differently on two machines and nobody can reproduce a report about it.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	int64 Sequence = 0;

	/** Session seconds at which it was queued. What the overview prints as "waiting since". */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	double QueuedAtSeconds = 0.0;
};

/**
 * What is on screen right now, plus the two clocks that decide what may happen next.
 *
 * Passed to PickNext by value, which is what makes the interruption rule testable without a game: the
 * rule is a function of this struct, the queue and a timestamp, and of nothing else.
 */
USTRUCT(BlueprintType)
struct SEENONCE_API FSeenOnceActiveHint
{
	GENERATED_BODY()

	/** The hint on screen, or None when the screen is free. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	FName HintId;

	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	int32 Priority = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	double StartedSeconds = 0.0;

	/** Before this moment nothing may interrupt it, however important. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	double MinUntilSeconds = 0.0;

	/** When it comes down by itself. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	double HideAtSeconds = 0.0;

	/** Quiet time after the last hint. Nothing new starts before this moment. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	double CooldownUntilSeconds = 0.0;

	/** While true, nothing is shown and nothing is lost - the queue simply waits. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	bool bSuppressed = false;

	bool IsShowing() const
	{
		return !HintId.IsNone();
	}
};

/** One line of the overview. */
USTRUCT(BlueprintType)
struct SEENONCE_API FSeenOnceHintStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	FName HintId;

	/** True when this hint has been shown at least once in this memory. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	bool bSeen = false;

	/**
	 * True when this hint has never been shown - the line the overview exists for.
	 *
	 * A hint that is listed as never triggered after a full playthrough is a hint whose condition cannot
	 * become true. There is no other way to find that one, because nothing is broken: no error, no
	 * warning, no crash. It simply never appears, and nobody notices what they did not see.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	bool bNeverTriggered = true;

	/** True when the memory holds this id but no known hint claims it. Kept, never discarded. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	bool bOrphan = false;

	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	int32 ShowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	FDateTime LastSeenUtc = FDateTime(0);

	/** The repeat rule, so the overview can say why a hint that was seen may or may not come back. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	ESeenOnceRepeat Repeat = ESeenOnceRepeat::Once;
};

/**
 * The whole set, counted.
 *
 * Every number the overview prints comes from here, and every one of them is a count of something that
 * was actually looked at. NeverTriggered is not "hints we could not find" - it is hints that exist, are
 * registered, and have no entry in the memory.
 */
USTRUCT(BlueprintType)
struct SEENONCE_API FSeenOnceSummary
{
	GENERATED_BODY()

	/** Registered hints. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	int32 TotalHints = 0;

	/** Registered hints with at least one show in this memory. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	int32 SeenCount = 0;

	/** Registered hints with no entry at all. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	int32 NeverTriggeredCount = 0;

	/** Entries in the memory that no registered hint claims. Kept on load, reported here. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	int32 OrphanCount = 0;

	/** Never triggered first and in warning colour, then seen by recency, then the orphans. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	TArray<FSeenOnceHintStatus> Lines;
};
