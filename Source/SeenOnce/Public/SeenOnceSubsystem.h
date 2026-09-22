// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameplayTagContainer.h"
#include "SeenOnceTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/Function.h"
#include "SeenOnceSubsystem.generated.h"

class USeenOnceHint;
class USeenOnceHintWidget;

/** A hint went on screen. Everything a custom display needs is here; the built-in one uses the same event. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSeenOnceHintShown, USeenOnceHint*, Hint, const FText&, Body);

/** A hint came off screen, and why. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSeenOnceHintHidden, USeenOnceHint*, Hint, ESeenOnceHideReason, Reason);

/** A hint came due but had to wait - for the hint in front of it, for the cooldown, or for suppression. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSeenOnceHintQueued, USeenOnceHint*, Hint, int32, QueueLength);

/**
 * The memory, the queue and the conditions.
 *
 * Lives on the game instance, so it survives a level change and dies with the game - which is the same
 * lifetime the memory it manages should have. It does four things and it is worth being clear about
 * which is which:
 *
 *   1. It remembers. Every show is written into a state struct, and that struct goes into a save game
 *      (its own, or yours through ExportState/ImportState). A bool on the game instance forgets when the
 *      process exits; this does not, and that is the entire product.
 *   2. It queues. Hints that come due while another is on screen wait rather than overwrite, ordered by
 *      importance and then by arrival. Nothing that comes due is ever silently dropped except by the
 *      queue expiry a project explicitly switches on.
 *   3. It watches the built-in conditions - tag present, area entered, action count, play time, item
 *      received - and triggers the hints attached to them.
 *   4. It knows the whole set, which is what makes "never triggered" answerable. A hint the subsystem
 *      has never been told about cannot be reported as missing, which is why hints are registered in
 *      Project Settings rather than discovered from whatever level happens to be loaded.
 *
 * It ticks through FTSTicker rather than a world tick, on purpose: play time, cooldowns and display
 * timers have to keep running across a level transition, and a hint queued in the last second of one
 * level should appear in the first second of the next rather than vanish with the world that queued it.
 */
UCLASS(DisplayName = "SeenOnce Subsystem")
class SEENONCE_API USeenOnceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//~ Triggering -------------------------------------------------------------------------------------

	/**
	 * Bring a hint on screen, subject to every rule.
	 *
	 * Returns true when the hint entered the queue. False means it was refused, and the log at Verbose
	 * says which of the reasons it was: unknown id, already seen, not yet due, or already waiting.
	 * Refused is the normal case in a shipped game - the whole point is that the ninth time the player
	 * opens a door, nothing happens.
	 */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	bool Trigger(FName HintId);

	/** Trigger every registered hint carrying this tag. Returns how many were queued. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	int32 TriggerByTag(FGameplayTag Tag);

	/**
	 * Show a hint now, whether it has been seen or not.
	 *
	 * The console command Hint.Show uses this, and so should any "preview this hint" button. It still
	 * records the show, so previewing a hint does count as having seen it - which is honest, and which is
	 * why Hint.Reset exists next to it.
	 */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	bool ForceShow(FName HintId);

	//~ The built-in conditions -------------------------------------------------------------------------

	/** An actor entered the area with this id. Triggers every AreaEntered hint watching it. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|Conditions")
	int32 NotifyAreaEntered(FName AreaId);

	/** The named action happened. Triggers ActionCount hints once their count is reached. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|Conditions")
	int32 NotifyAction(FName ActionId, int32 Count = 1);

	/** The named item was received. Triggers ItemReceived hints once their count is reached. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|Conditions")
	int32 NotifyItemReceived(FName ItemId, int32 Count = 1);

	/** Add a condition tag. GameplayTag hints watching it come due immediately. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|Conditions")
	int32 AddConditionTag(FGameplayTag Tag);

	/** Remove a condition tag. Nothing is untriggered by this; it only stops future matches. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|Conditions")
	void RemoveConditionTag(FGameplayTag Tag);

	/** Is this condition tag currently present? */
	UFUNCTION(BlueprintPure, Category = "SeenOnce|Conditions")
	bool HasConditionTag(FGameplayTag Tag) const;

	/** How often the named action has been counted so far. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce|Conditions")
	int32 GetActionCount(FName ActionId) const;

	//~ The memory -------------------------------------------------------------------------------------

	/** Has this hint been shown at least once in the current memory? */
	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	bool HasSeen(FName HintId) const;

	/** Forget one hint, whatever its rule. It can be earned again. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	bool Reset(FName HintId);

	/**
	 * Forget the registered hints and return how many were forgotten.
	 *
	 * Hints whose rule is "Once (forever)" are kept unless bIncludePermanent is set - otherwise the rule
	 * would be indistinguishable from "once per save". Entries belonging to hints this build does not
	 * know are never touched.
	 */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	int32 ResetAll(bool bIncludePermanent = false);

	/**
	 * The memory as a compact struct: ids, counts, timestamps.
	 *
	 * This is the integration point for a project with its own save system. Put this in your save game,
	 * hand it back with ImportState on load, and switch the own-slot option off in Project Settings.
	 */
	UFUNCTION(BlueprintPure, Category = "SeenOnce|Save")
	FSeenOnceState ExportState() const;

	/**
	 * Take a memory back, from your own save game or from anywhere else.
	 *
	 * Ids this build does not recognise are kept, not dropped, and they come back out of the next
	 * ExportState untouched. That is what makes it safe to rename a hint, to load a save from a newer
	 * branch, or to run with a feature switched off - all three are cases where a filtering import would
	 * quietly erase the memory and show the player a tutorial they have already read.
	 */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|Save")
	void ImportState(const FSeenOnceState& NewState);

	/** Write the memory to SeenOnce's own slot now. False when the own slot is switched off. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|Save")
	bool SaveToSlot();

	/** Read the memory back from SeenOnce's own slot. False when there is nothing there. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|Save")
	bool LoadFromSlot();

	/** Delete SeenOnce's own slot from disk and clear the in-memory state with it. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|Save")
	bool DeleteSlot();

	//~ Suppression ------------------------------------------------------------------------------------

	/**
	 * Stop hints appearing, and say why.
	 *
	 * Nothing is lost while suppression is on. Hints that come due are queued and appear when it is
	 * lifted, in the order they came due - which is the behaviour a cutscene-heavy game needs, and the
	 * behaviour a bool that just skips the trigger does not have.
	 */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	void SetSuppressed(bool bSuppressed, FName Reason);

	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	bool IsSuppressed() const;

	/** The reason suppression was switched on, or None. Printed by the overview. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	FName GetSuppressionReason() const;

	//~ The set ----------------------------------------------------------------------------------------

	/** Make a hint known to the subsystem, so it can be triggered and counted. Idempotent. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|Registry")
	bool RegisterHint(USeenOnceHint* Hint);

	/** Forget that a hint exists. Its memory is untouched - it simply stops being listed and triggerable. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|Registry")
	bool UnregisterHint(USeenOnceHint* Hint);

	/** Every registered hint. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce|Registry")
	TArray<USeenOnceHint*> GetRegisteredHints() const;

	/** One registered hint by id, or null. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce|Registry")
	USeenOnceHint* FindHint(FName HintId) const;

	/** The whole set counted against the memory - what the overview draws. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	FSeenOnceSummary Summarize() const;

	//~ What is happening right now ---------------------------------------------------------------------

	/** The hint on screen, or null. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	USeenOnceHint* GetActiveHint() const;

	/** The scheduling state: what is showing, until when, and whether suppression is on. */
	const FSeenOnceActiveHint& GetActiveState() const { return Active; }

	/** The hints waiting for their turn, in the order they arrived. */
	const TArray<FSeenOnceQueuedHint>& GetQueue() const { return Queue; }

	/** How many hints are waiting. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	int32 GetPendingCount() const { return Queue.Num(); }

	/** Play time counted by this subsystem, in seconds. What the PlayTime condition measures against. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	double GetPlayTimeSeconds() const { return PlayTimeSeconds; }

	/** Monotonic session seconds. The clock every rule in the plugin is expressed against. */
	double GetSessionSeconds() const { return SessionSeconds; }

	/** Take the current hint down early. Used by the console and by anything that owns the screen. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	void ClearActiveHint();

	//~ The overview -----------------------------------------------------------------------------------

	/** Show or hide the canvas overview drawn by ASeenOnceHUD. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|Overview")
	void SetOverviewVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "SeenOnce|Overview")
	bool IsOverviewVisible() const { return bOverviewVisible; }

	/** Write the whole overview to the log, for a build machine or a bug report. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce|Overview")
	void LogOverview() const;

	//~ Events -----------------------------------------------------------------------------------------

	/** A hint went on screen. Bind here for your own display. */
	UPROPERTY(BlueprintAssignable, Category = "SeenOnce|Events")
	FSeenOnceHintShown OnHintShown;

	/** A hint came off screen. Bind here for your own display. */
	UPROPERTY(BlueprintAssignable, Category = "SeenOnce|Events")
	FSeenOnceHintHidden OnHintHidden;

	/** A hint came due and had to wait. Useful for a "1 more tip" indicator. */
	UPROPERTY(BlueprintAssignable, Category = "SeenOnce|Events")
	FSeenOnceHintQueued OnHintQueued;

private:
	/** The ticker body. Advances the clocks, evaluates the timed conditions, runs the queue. */
	bool Tick(float DeltaSeconds);

	/** Registers everything listed in Project Settings. Called once, at Initialize. */
	void RegisterSettingsHints();

	/** Puts a hint in the queue if the rules allow it. The single path into the queue. */
	bool Enqueue(USeenOnceHint* Hint, bool bBypassMemory);

	/** Takes a hint out of the queue and puts it on screen. */
	void ShowHint(USeenOnceHint* Hint);

	/** Takes the hint on screen off it, applies the cooldown, and tells everybody why. */
	void HideActive(ESeenOnceHideReason Reason, bool bApplyCooldown);

	/** PickNext plus the two things that are not policy: expiry and actually showing the winner. */
	void AdvanceQueue();

	/** Checks the conditions that become true on their own rather than on a call. Play time, for now. */
	void EvaluateTimedConditions();

	/** Drops queued hints that have waited longer than the project allows. Off by default. */
	void ExpireQueue();

	/** The built-in display, created on the first hint that needs it. Null when it is switched off. */
	USeenOnceHintWidget* GetOrCreateHintWidget();

	/** Whether the game is in a state that suppresses hints on its own - paused, mainly. */
	bool IsImplicitlySuppressed() const;

	/** Marks the memory as needing a write, and writes it when the settings say to write eagerly. */
	void MarkStateDirty();

	/**
	 * Every registered hint matching a test, collected before anything is done with them.
	 *
	 * Queuing a hint broadcasts, and a listener is entitled to register a hint of its own from there -
	 * which would rehash the map somebody was still iterating. So the condition handlers gather first
	 * and act second, always.
	 */
	TArray<USeenOnceHint*> GatherHints(TFunctionRef<bool(const USeenOnceHint&)> Predicate) const;

	/** Every registered hint as plain definitions, which is what the rules take. */
	TArray<FSeenOnceHintDef> CollectDefinitions() const;

	/** The ids of every registered hint - what ResetAll is allowed to forget. */
	TArray<FName> CollectRegisteredIds() const;

	/** Everything this player has been taught. */
	UPROPERTY()
	FSeenOnceState State;

	/** Every hint the subsystem knows about, by id. */
	UPROPERTY()
	TMap<FName, TObjectPtr<USeenOnceHint>> Hints;

	/** The built-in display, if one was ever needed. */
	UPROPERTY()
	TObjectPtr<USeenOnceHintWidget> HintWidget;

	/** The hint on screen, kept as a hard reference so it cannot be collected mid-display. */
	UPROPERTY()
	TObjectPtr<USeenOnceHint> ActiveHint;

	/** What is showing and until when. Passed to PickNext by value. */
	FSeenOnceActiveHint Active;

	/** Who is waiting. */
	TArray<FSeenOnceQueuedHint> Queue;

	/** Condition tags currently present. */
	FGameplayTagContainer ConditionTags;

	/** How often each named action has been counted. */
	TMap<FName, int32> ActionCounts;

	/** How often each named item has been received. */
	TMap<FName, int32> ItemCounts;

	/** Monotonic seconds since the subsystem started. Every rule is expressed against this. */
	double SessionSeconds = 0.0;

	/** Seconds of unpaused play. What the PlayTime condition measures. */
	double PlayTimeSeconds = 0.0;

	/** Arrival order for the queue, and the reason equal priorities are stable. */
	int64 NextSequence = 0;

	/** Why hints are suppressed, or None. */
	FName SuppressionReason;

	/** Set by SetSuppressed. Kept apart from the implicit pause suppression so both can be reported. */
	bool bExplicitlySuppressed = false;

	/** True when the memory has changed since the last write. */
	bool bStateDirty = false;

	/** Whether the canvas overview draws. */
	bool bOverviewVisible = false;

	/** The ticker handle, removed at Deinitialize. */
	FTSTicker::FDelegateHandle TickHandle;
};
