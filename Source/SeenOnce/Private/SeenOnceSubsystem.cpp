// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "SeenOnceSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SeenOnceHint.h"
#include "SeenOnceHintWidget.h"
#include "SeenOnceLog.h"
#include "SeenOnceSaveGame.h"
#include "SeenOnceSettings.h"
#include "SeenOnceStatics.h"
#include "Sound/SoundBase.h"

void USeenOnceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const USeenOnceSettings& Settings = USeenOnceSettings::Get();

	bOverviewVisible = Settings.bOverviewVisibleOnStart;

	RegisterSettingsHints();

	if (Settings.bUseOwnSaveSlot)
	{
		LoadFromSlot();
	}

	// A core ticker rather than a world tick. Play time, cooldowns and the display timer have to keep
	// running across a level transition: a hint queued during the last second of one level belongs on
	// screen in the first second of the next, not in the rubble of the world that queued it.
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &USeenOnceSubsystem::Tick));

	UE_LOG(LogSeenOnce, Log, TEXT("SeenOnce ready: %d hints registered, %d remembered%s."),
		Hints.Num(), State.Entries.Num(),
		Settings.bUseOwnSaveSlot ? TEXT("") : TEXT(" (own save slot off - use ExportState/ImportState)"));
}

void USeenOnceSubsystem::Deinitialize()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	// The last thing the memory is for is surviving the moment the game closes, so this write is not
	// optional housekeeping - it is the promise.
	if (bStateDirty)
	{
		SaveToSlot();
	}

	if (HintWidget != nullptr)
	{
		HintWidget->RemoveFromParent();
		HintWidget = nullptr;
	}

	ActiveHint = nullptr;
	Queue.Reset();
	Hints.Reset();

	Super::Deinitialize();
}

void USeenOnceSubsystem::RegisterSettingsHints()
{
	const USeenOnceSettings& Settings = USeenOnceSettings::Get();

	for (const TSoftObjectPtr<USeenOnceHint>& SoftHint : Settings.Hints)
	{
		if (SoftHint.IsNull())
		{
			continue;
		}

		// Synchronous, and deliberately so. These are small assets, this runs once when the game instance
		// starts, and the alternative - hints that appear in the registry some seconds later - would make
		// "never triggered" mean "not loaded yet" for the first part of every session.
		if (USeenOnceHint* Hint = SoftHint.LoadSynchronous())
		{
			RegisterHint(Hint);
		}
		else
		{
			UE_LOG(LogSeenOnce, Warning, TEXT("SeenOnce: a hint listed in Project Settings could not be loaded: %s"),
				*SoftHint.ToString());
		}
	}
}

bool USeenOnceSubsystem::Tick(const float DeltaSeconds)
{
	SessionSeconds += DeltaSeconds;

	const UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	const bool bPaused = (World != nullptr) && World->IsPaused();

	// Play time is what the player experienced, so a paused game does not accumulate it. Somebody who
	// leaves the game open over lunch has not played for an hour.
	if (World != nullptr && !bPaused)
	{
		PlayTimeSeconds += DeltaSeconds;
	}

	Active.bSuppressed = bExplicitlySuppressed || IsImplicitlySuppressed();

	EvaluateTimedConditions();

	if (Active.IsShowing() && SessionSeconds >= Active.HideAtSeconds)
	{
		HideActive(ESeenOnceHideReason::Elapsed, /*bApplyCooldown=*/true);
	}

	ExpireQueue();
	AdvanceQueue();

	return true;
}

bool USeenOnceSubsystem::IsImplicitlySuppressed() const
{
	const USeenOnceSettings& Settings = USeenOnceSettings::Get();
	if (!Settings.bSuppressWhilePaused)
	{
		return false;
	}

	const UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	return World != nullptr && World->IsPaused();
}

//~ Triggering -----------------------------------------------------------------------------------------

bool USeenOnceSubsystem::Trigger(const FName HintId)
{
	USeenOnceHint* Hint = FindHint(HintId);
	if (Hint == nullptr)
	{
		// Loud on purpose. A trigger for an id nothing claims is the mistake that produces a hint which
		// never appears and never explains itself, and it is usually a typo.
		UE_LOG(LogSeenOnce, Warning, TEXT("SeenOnce: Trigger('%s') - no hint with that id is registered."),
			*HintId.ToString());
		return false;
	}

	return Enqueue(Hint, /*bBypassMemory=*/false);
}

int32 USeenOnceSubsystem::TriggerByTag(const FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return 0;
	}

	// The hint's tag is the parent: triggering Tutorial.Movement.Jump fires a hint listening for
	// Tutorial.Movement. The other direction would fire a specific hint on a general event, which is
	// almost never what anybody means.
	const TArray<USeenOnceHint*> Matching = GatherHints([Tag](const USeenOnceHint& Hint)
	{
		return Hint.Hint.ConditionTag.IsValid() && Tag.MatchesTag(Hint.Hint.ConditionTag);
	});

	int32 Queued = 0;
	for (USeenOnceHint* Hint : Matching)
	{
		Queued += Enqueue(Hint, /*bBypassMemory=*/false) ? 1 : 0;
	}

	return Queued;
}

TArray<USeenOnceHint*> USeenOnceSubsystem::GatherHints(TFunctionRef<bool(const USeenOnceHint&)> Predicate) const
{
	// Collected first, acted on second, and never both at once. Enqueue broadcasts OnHintQueued and can
	// end in OnHintShown, and a listener is entitled to register a hint of its own from there - which
	// would rehash the map somebody was still iterating.
	TArray<USeenOnceHint*> Matching;
	for (const TPair<FName, TObjectPtr<USeenOnceHint>>& Pair : Hints)
	{
		if (Pair.Value != nullptr && Predicate(*Pair.Value))
		{
			Matching.Add(Pair.Value);
		}
	}

	return Matching;
}

bool USeenOnceSubsystem::ForceShow(const FName HintId)
{
	USeenOnceHint* Hint = FindHint(HintId);
	if (Hint == nullptr)
	{
		UE_LOG(LogSeenOnce, Warning, TEXT("SeenOnce: ForceShow('%s') - no hint with that id is registered."),
			*HintId.ToString());
		return false;
	}

	// Force means force: past the memory, past the cooldown, past suppression. This is the preview path,
	// and a preview that silently did nothing because the hint had been seen would be useless. It still
	// records the show, which is why Hint.Reset lives next to Hint.Show.
	Queue.RemoveAll([HintId](const FSeenOnceQueuedHint& Queued) { return Queued.HintId == HintId; });

	if (Active.IsShowing())
	{
		HideActive(ESeenOnceHideReason::Interrupted, /*bApplyCooldown=*/false);
	}

	Active.CooldownUntilSeconds = SessionSeconds;
	ShowHint(Hint);
	return true;
}

//~ Conditions -----------------------------------------------------------------------------------------

int32 USeenOnceSubsystem::NotifyAreaEntered(const FName AreaId)
{
	if (AreaId.IsNone())
	{
		return 0;
	}

	const TArray<USeenOnceHint*> Matching = GatherHints([AreaId](const USeenOnceHint& Hint)
	{
		return Hint.Hint.Condition == ESeenOnceCondition::AreaEntered && Hint.Hint.ConditionKey == AreaId;
	});

	int32 Queued = 0;
	for (USeenOnceHint* Hint : Matching)
	{
		Queued += Enqueue(Hint, /*bBypassMemory=*/false) ? 1 : 0;
	}

	return Queued;
}

int32 USeenOnceSubsystem::NotifyAction(const FName ActionId, const int32 Count)
{
	if (ActionId.IsNone() || Count <= 0)
	{
		return 0;
	}

	int32& Total = ActionCounts.FindOrAdd(ActionId);
	Total += Count;

	const int32 Reached = Total;
	const TArray<USeenOnceHint*> Matching = GatherHints([ActionId, Reached](const USeenOnceHint& Hint)
	{
		return Hint.Hint.Condition == ESeenOnceCondition::ActionCount
			&& Hint.Hint.ConditionKey == ActionId
			&& Reached >= FMath::Max(1, Hint.Hint.RequiredCount);
	});

	int32 Queued = 0;
	for (USeenOnceHint* Hint : Matching)
	{
		Queued += Enqueue(Hint, /*bBypassMemory=*/false) ? 1 : 0;
	}

	return Queued;
}

int32 USeenOnceSubsystem::NotifyItemReceived(const FName ItemId, const int32 Count)
{
	if (ItemId.IsNone() || Count <= 0)
	{
		return 0;
	}

	int32& Total = ItemCounts.FindOrAdd(ItemId);
	Total += Count;

	const int32 Reached = Total;
	const TArray<USeenOnceHint*> Matching = GatherHints([ItemId, Reached](const USeenOnceHint& Hint)
	{
		return Hint.Hint.Condition == ESeenOnceCondition::ItemReceived
			&& Hint.Hint.ConditionKey == ItemId
			&& Reached >= FMath::Max(1, Hint.Hint.RequiredCount);
	});

	int32 Queued = 0;
	for (USeenOnceHint* Hint : Matching)
	{
		Queued += Enqueue(Hint, /*bBypassMemory=*/false) ? 1 : 0;
	}

	return Queued;
}

int32 USeenOnceSubsystem::AddConditionTag(const FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return 0;
	}

	ConditionTags.AddTag(Tag);

	const TArray<USeenOnceHint*> Matching = GatherHints([this](const USeenOnceHint& Hint)
	{
		return Hint.Hint.Condition == ESeenOnceCondition::GameplayTag
			&& Hint.Hint.ConditionTag.IsValid()
			&& ConditionTags.HasTag(Hint.Hint.ConditionTag);
	});

	int32 Queued = 0;
	for (USeenOnceHint* Hint : Matching)
	{
		Queued += Enqueue(Hint, /*bBypassMemory=*/false) ? 1 : 0;
	}

	return Queued;
}

void USeenOnceSubsystem::RemoveConditionTag(const FGameplayTag Tag)
{
	ConditionTags.RemoveTag(Tag);
}

bool USeenOnceSubsystem::HasConditionTag(const FGameplayTag Tag) const
{
	return Tag.IsValid() && ConditionTags.HasTag(Tag);
}

int32 USeenOnceSubsystem::GetActionCount(const FName ActionId) const
{
	const int32* Count = ActionCounts.Find(ActionId);
	return Count != nullptr ? *Count : 0;
}

void USeenOnceSubsystem::EvaluateTimedConditions()
{
	// Play time is the only condition that becomes true on its own, with nobody calling anything. The
	// others are notified, which is cheaper and easier to reason about.
	const double Played = PlayTimeSeconds;
	const TArray<USeenOnceHint*> Due = GatherHints([Played](const USeenOnceHint& Hint)
	{
		return Hint.Hint.Condition == ESeenOnceCondition::PlayTime
			&& Played >= static_cast<double>(Hint.Hint.RequiredPlayTimeSeconds);
	});

	for (USeenOnceHint* Hint : Due)
	{
		Enqueue(Hint, /*bBypassMemory=*/false);
	}
}

//~ The queue ------------------------------------------------------------------------------------------

bool USeenOnceSubsystem::Enqueue(USeenOnceHint* Hint, const bool bBypassMemory)
{
	if (Hint == nullptr)
	{
		return false;
	}

	const FSeenOnceHintDef Def = Hint->GetDefinition();
	if (!Def.IsValid())
	{
		UE_LOG(LogSeenOnce, Warning, TEXT("SeenOnce: '%s' has no hint id and cannot be remembered, so it will not be shown."),
			*Hint->GetName());
		return false;
	}

	if (Active.IsShowing() && Active.HintId == Def.HintId)
	{
		return false;
	}

	if (Queue.ContainsByPredicate([&Def](const FSeenOnceQueuedHint& Queued) { return Queued.HintId == Def.HintId; }))
	{
		return false;
	}

	if (!bBypassMemory && !USeenOnceStatics::IsDue(Def, State, SessionSeconds))
	{
		// Verbose, not Warning. In a shipped game this is the common case by a wide margin: the player
		// has opened this door forty times and been taught about it once.
		UE_LOG(LogSeenOnce, Verbose, TEXT("SeenOnce: '%s' not shown - %s."),
			*Def.HintId.ToString(),
			USeenOnceStatics::ShouldShow(Def, State, SessionSeconds) ? TEXT("shown too recently") : TEXT("already seen"));
		return false;
	}

	FSeenOnceQueuedHint& Queued = Queue.AddDefaulted_GetRef();
	Queued.HintId = Def.HintId;
	Queued.Priority = Def.Priority;
	Queued.Sequence = NextSequence++;
	Queued.QueuedAtSeconds = SessionSeconds;

	OnHintQueued.Broadcast(Hint, Queue.Num());

	UE_LOG(LogSeenOnce, Verbose, TEXT("SeenOnce: '%s' queued (priority %d, %d waiting)."),
		*Def.HintId.ToString(), Def.Priority, Queue.Num());

	// Run the queue immediately rather than on the next tick, so a hint triggered on a free screen is on
	// screen in the same frame the player did the thing it is about.
	AdvanceQueue();

	return true;
}

void USeenOnceSubsystem::AdvanceQueue()
{
	Active.bSuppressed = bExplicitlySuppressed || IsImplicitlySuppressed();

	// The loop only ever runs a second time when the winner turned out to be a hint that has since been
	// unregistered - in which case it is dropped and the next one gets its turn.
	while (Queue.Num() > 0)
	{
		const int32 Index = USeenOnceStatics::PickNext(Queue, Active, SessionSeconds);
		if (Index == INDEX_NONE)
		{
			return;
		}

		const FName WinnerId = Queue[Index].HintId;
		USeenOnceHint* Winner = FindHint(WinnerId);

		Queue.RemoveAt(Index);

		if (Winner == nullptr)
		{
			UE_LOG(LogSeenOnce, Verbose, TEXT("SeenOnce: '%s' left the queue - its hint is no longer registered."),
				*WinnerId.ToString());
			continue;
		}

		if (Active.IsShowing())
		{
			// No cooldown here. The interruption rule already required a strictly higher priority and a
			// finished minimum display time; making the more important hint then wait out a quiet period
			// would be the third obstacle on something the rules just decided was urgent.
			HideActive(ESeenOnceHideReason::Interrupted, /*bApplyCooldown=*/false);
		}

		ShowHint(Winner);
		return;
	}
}

void USeenOnceSubsystem::ExpireQueue()
{
	const float ExpirySeconds = USeenOnceSettings::Get().QueueExpirySeconds;
	if (ExpirySeconds <= 0.0f)
	{
		return;
	}

	const double Cutoff = SessionSeconds - static_cast<double>(ExpirySeconds);

	// Logged one by one. A hint disappearing without a trace is exactly the failure this plugin is here
	// to prevent, so when a project switches expiry on, the log says which hint it cost.
	for (int32 Index = Queue.Num() - 1; Index >= 0; --Index)
	{
		if (Queue[Index].QueuedAtSeconds < Cutoff)
		{
			UE_LOG(LogSeenOnce, Log, TEXT("SeenOnce: '%s' dropped from the queue after waiting %.0f s (queue expiry)."),
				*Queue[Index].HintId.ToString(), SessionSeconds - Queue[Index].QueuedAtSeconds);
			Queue.RemoveAt(Index);
		}
	}
}

void USeenOnceSubsystem::ShowHint(USeenOnceHint* Hint)
{
	if (Hint == nullptr)
	{
		return;
	}

	const USeenOnceSettings& Settings = USeenOnceSettings::Get();
	const FSeenOnceHintDef Def = Hint->GetDefinition();

	// The memory is written when the hint goes up, not when it comes down. A hint that was interrupted
	// after its minimum display time was read; a hint the player skipped by quitting was on screen. The
	// alternative - only counting hints that ran their full time - is how a crash-happy build teaches the
	// same lesson every session.
	USeenOnceStatics::RecordShow(State, Def, SessionSeconds, FDateTime::UtcNow());
	MarkStateDirty();

	const float MinDisplay = (Def.MinDisplaySeconds > 0.0f) ? Def.MinDisplaySeconds : Settings.DefaultMinDisplaySeconds;
	const float Display = FMath::Max(Def.DisplaySeconds, MinDisplay);

	ActiveHint = Hint;
	Active.HintId = Def.HintId;
	Active.Priority = Def.Priority;
	Active.StartedSeconds = SessionSeconds;
	Active.MinUntilSeconds = SessionSeconds + static_cast<double>(MinDisplay);
	Active.HideAtSeconds = SessionSeconds + static_cast<double>(Display);

	if (Settings.bUseBuiltInDisplay)
	{
		if (USeenOnceHintWidget* Widget = GetOrCreateHintWidget())
		{
			Widget->ShowHint(Hint, Def.Body);
		}
	}

	if (USoundBase* Sound = Hint->Sound.LoadSynchronous())
	{
		if (const UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr)
		{
			UGameplayStatics::PlaySound2D(World, Sound);
		}
	}

	OnHintShown.Broadcast(Hint, Def.Body);

	UE_LOG(LogSeenOnce, Log, TEXT("SeenOnce: showing '%s' for %.1f s (priority %d, %d waiting)."),
		*Def.HintId.ToString(), Display, Def.Priority, Queue.Num());
}

void USeenOnceSubsystem::HideActive(const ESeenOnceHideReason Reason, const bool bApplyCooldown)
{
	if (!Active.IsShowing())
	{
		return;
	}

	USeenOnceHint* Hint = ActiveHint;
	const FName HintId = Active.HintId;

	float Cooldown = 0.0f;
	if (bApplyCooldown)
	{
		const USeenOnceSettings& Settings = USeenOnceSettings::Get();
		Cooldown = (Hint != nullptr)
			? Hint->Hint.GetCooldownSeconds(Settings.DefaultCooldownSeconds)
			: Settings.DefaultCooldownSeconds;
	}

	Active.HintId = NAME_None;
	Active.Priority = 0;
	Active.MinUntilSeconds = SessionSeconds;
	Active.HideAtSeconds = SessionSeconds;
	Active.CooldownUntilSeconds = SessionSeconds + static_cast<double>(Cooldown);
	ActiveHint = nullptr;

	if (HintWidget != nullptr)
	{
		HintWidget->HideHint(Reason);
	}

	OnHintHidden.Broadcast(Hint, Reason);

	UE_LOG(LogSeenOnce, Verbose, TEXT("SeenOnce: '%s' hidden (%s), next hint no earlier than +%.1f s."),
		*HintId.ToString(),
		Reason == ESeenOnceHideReason::Elapsed ? TEXT("time up") : (Reason == ESeenOnceHideReason::Interrupted ? TEXT("interrupted") : TEXT("cleared")),
		Cooldown);
}

void USeenOnceSubsystem::ClearActiveHint()
{
	HideActive(ESeenOnceHideReason::Cleared, /*bApplyCooldown=*/true);
}

USeenOnceHintWidget* USeenOnceSubsystem::GetOrCreateHintWidget()
{
	const USeenOnceSettings& Settings = USeenOnceSettings::Get();

	UGameInstance* GameInstance = GetGameInstance();
	APlayerController* Controller = GameInstance != nullptr ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	if (Controller == nullptr)
	{
		return nullptr;
	}

	// A widget added to the viewport does not survive a level change, and the subsystem does. Rebuilding
	// it when the world underneath it has moved on is cheaper than the alternative, which is a hint that
	// silently stops appearing after the first level transition.
	if (HintWidget != nullptr && HintWidget->IsInViewport() && HintWidget->GetWorld() == Controller->GetWorld())
	{
		return HintWidget;
	}

	if (HintWidget != nullptr)
	{
		HintWidget->RemoveFromParent();
		HintWidget = nullptr;
	}

	UClass* WidgetClass = Settings.HintWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		// Not an error. A project that binds its own display and never sets this class is in a perfectly
		// valid configuration; it just should not also leave the built-in display switched on.
		UE_LOG(LogSeenOnce, Verbose,
			TEXT("SeenOnce: the built-in display is on but no widget class is set. Hints are still decided and broadcast; nothing is drawn."));
		return nullptr;
	}

	HintWidget = CreateWidget<USeenOnceHintWidget>(Controller, WidgetClass);
	if (HintWidget != nullptr)
	{
		HintWidget->AddToViewport(Settings.HintWidgetZOrder);
	}

	return HintWidget;
}

//~ The memory -----------------------------------------------------------------------------------------

bool USeenOnceSubsystem::HasSeen(const FName HintId) const
{
	const FSeenOnceEntry* Entry = State.Find(HintId);
	return Entry != nullptr && Entry->ShowCount > 0;
}

bool USeenOnceSubsystem::Reset(const FName HintId)
{
	const bool bChanged = USeenOnceStatics::ResetHintInState(State, HintId);
	if (bChanged)
	{
		MarkStateDirty();
		UE_LOG(LogSeenOnce, Log, TEXT("SeenOnce: '%s' forgotten. It can be earned again."), *HintId.ToString());
	}

	return bChanged;
}

int32 USeenOnceSubsystem::ResetAll(const bool bIncludePermanent)
{
	const int32 Removed = USeenOnceStatics::ResetAllInState(State, bIncludePermanent, CollectRegisteredIds());
	if (Removed > 0)
	{
		MarkStateDirty();
	}

	const int32 Kept = State.Entries.Num();
	UE_LOG(LogSeenOnce, Log, TEXT("SeenOnce: forgot %d hint(s), kept %d entr(ies)%s."),
		Removed, Kept,
		bIncludePermanent ? TEXT("") : TEXT(" (hints marked 'Once (forever)' and any unknown ids were kept)"));

	return Removed;
}

FSeenOnceState USeenOnceSubsystem::ExportState() const
{
	return State;
}

void USeenOnceSubsystem::ImportState(const FSeenOnceState& NewState)
{
	State = USeenOnceStatics::SanitizeLoadedState(NewState);

	const int32 Orphans = USeenOnceStatics::CountOrphanEntries(CollectDefinitions(), State);

	MarkStateDirty();

	UE_LOG(LogSeenOnce, Log, TEXT("SeenOnce: memory imported - %d entr(ies), %d of them for hints this build does not know (kept)."),
		State.Entries.Num(), Orphans);
}

bool USeenOnceSubsystem::SaveToSlot()
{
	const USeenOnceSettings& Settings = USeenOnceSettings::Get();
	if (!Settings.bUseOwnSaveSlot)
	{
		return false;
	}

	USeenOnceSaveGame* Save = Cast<USeenOnceSaveGame>(UGameplayStatics::CreateSaveGameObject(USeenOnceSaveGame::StaticClass()));
	if (Save == nullptr)
	{
		return false;
	}

	Save->State = State;

	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, Settings.SaveSlotName, Settings.SaveUserIndex);
	if (bSaved)
	{
		bStateDirty = false;
	}
	else
	{
		UE_LOG(LogSeenOnce, Error, TEXT("SeenOnce: could not write the memory to slot '%s'. Hints will repeat after a restart."),
			*Settings.SaveSlotName);
	}

	return bSaved;
}

bool USeenOnceSubsystem::LoadFromSlot()
{
	const USeenOnceSettings& Settings = USeenOnceSettings::Get();
	if (!Settings.bUseOwnSaveSlot)
	{
		return false;
	}

	if (!UGameplayStatics::DoesSaveGameExist(Settings.SaveSlotName, Settings.SaveUserIndex))
	{
		// The first run of a fresh install, and the correct behaviour is an empty memory: every hint is
		// new to this player, which is the whole reason a new save shows them again.
		return false;
	}

	const USeenOnceSaveGame* Save = Cast<USeenOnceSaveGame>(
		UGameplayStatics::LoadGameFromSlot(Settings.SaveSlotName, Settings.SaveUserIndex));

	if (Save == nullptr)
	{
		UE_LOG(LogSeenOnce, Warning, TEXT("SeenOnce: slot '%s' exists but could not be read. Starting with an empty memory."),
			*Settings.SaveSlotName);
		return false;
	}

	ImportState(Save->State);

	// Loading is not a change worth writing back, and writing here would turn a read-only start into a
	// disk write on every launch.
	bStateDirty = false;

	return true;
}

bool USeenOnceSubsystem::DeleteSlot()
{
	const USeenOnceSettings& Settings = USeenOnceSettings::Get();

	const bool bDeleted = UGameplayStatics::DeleteGameInSlot(Settings.SaveSlotName, Settings.SaveUserIndex);

	State = FSeenOnceState();
	bStateDirty = false;

	UE_LOG(LogSeenOnce, Log, TEXT("SeenOnce: slot '%s' %s, memory cleared."),
		*Settings.SaveSlotName, bDeleted ? TEXT("deleted") : TEXT("was not there"));

	return bDeleted;
}

void USeenOnceSubsystem::MarkStateDirty()
{
	bStateDirty = true;

	const USeenOnceSettings& Settings = USeenOnceSettings::Get();
	if (Settings.bUseOwnSaveSlot && Settings.bSaveAfterEveryShow)
	{
		SaveToSlot();
	}
}

//~ Suppression ----------------------------------------------------------------------------------------

void USeenOnceSubsystem::SetSuppressed(const bool bSuppressed, const FName Reason)
{
	bExplicitlySuppressed = bSuppressed;
	SuppressionReason = bSuppressed ? Reason : NAME_None;
	Active.bSuppressed = bExplicitlySuppressed || IsImplicitlySuppressed();

	if (bSuppressed && Active.IsShowing())
	{
		// Whatever the suppression is for - a cutscene starting, a menu opening - it owns the screen now.
		// The hint has already been recorded as seen, which is a deliberate trade: showing it again later
		// would need a rule for "seen but perhaps not read", and that rule has no honest answer.
		HideActive(ESeenOnceHideReason::Cleared, /*bApplyCooldown=*/false);
	}

	UE_LOG(LogSeenOnce, Log, TEXT("SeenOnce: hints %s%s. %d waiting."),
		bSuppressed ? TEXT("suppressed") : TEXT("allowed again"),
		(bSuppressed && !Reason.IsNone()) ? *FString::Printf(TEXT(" (%s)"), *Reason.ToString()) : TEXT(""),
		Queue.Num());

	if (!bSuppressed)
	{
		// Nothing was lost while it was on, so this is where the waiting hints get their turn.
		AdvanceQueue();
	}
}

bool USeenOnceSubsystem::IsSuppressed() const
{
	return bExplicitlySuppressed || IsImplicitlySuppressed();
}

FName USeenOnceSubsystem::GetSuppressionReason() const
{
	if (bExplicitlySuppressed)
	{
		return SuppressionReason.IsNone() ? FName(TEXT("suppressed")) : SuppressionReason;
	}

	return IsImplicitlySuppressed() ? FName(TEXT("paused")) : NAME_None;
}

//~ The set --------------------------------------------------------------------------------------------

bool USeenOnceSubsystem::RegisterHint(USeenOnceHint* Hint)
{
	if (Hint == nullptr)
	{
		return false;
	}

	const FName HintId = Hint->GetHintId();
	if (HintId.IsNone())
	{
		UE_LOG(LogSeenOnce, Warning, TEXT("SeenOnce: '%s' has no id and was not registered."), *Hint->GetName());
		return false;
	}

	if (const TObjectPtr<USeenOnceHint>* Existing = Hints.Find(HintId))
	{
		if (*Existing != Hint)
		{
			// Two assets claiming one id share one memory: showing either marks both as seen. Worth
			// saying out loud, because the symptom - a hint that never appears - looks like anything else.
			UE_LOG(LogSeenOnce, Warning,
				TEXT("SeenOnce: two hints claim the id '%s' ('%s' and '%s'). They will share one memory; the first one registered wins."),
				*HintId.ToString(), *(*Existing)->GetName(), *Hint->GetName());
		}

		return false;
	}

	Hints.Add(HintId, Hint);
	return true;
}

bool USeenOnceSubsystem::UnregisterHint(USeenOnceHint* Hint)
{
	if (Hint == nullptr)
	{
		return false;
	}

	const FName HintId = Hint->GetHintId();
	if (const TObjectPtr<USeenOnceHint>* Existing = Hints.Find(HintId); Existing != nullptr && *Existing == Hint)
	{
		Hints.Remove(HintId);
		return true;
	}

	return false;
}

TArray<USeenOnceHint*> USeenOnceSubsystem::GetRegisteredHints() const
{
	TArray<USeenOnceHint*> Result;
	Result.Reserve(Hints.Num());

	for (const TPair<FName, TObjectPtr<USeenOnceHint>>& Pair : Hints)
	{
		if (Pair.Value != nullptr)
		{
			Result.Add(Pair.Value);
		}
	}

	return Result;
}

USeenOnceHint* USeenOnceSubsystem::FindHint(const FName HintId) const
{
	const TObjectPtr<USeenOnceHint>* Found = Hints.Find(HintId);
	return Found != nullptr ? Found->Get() : nullptr;
}

TArray<FSeenOnceHintDef> USeenOnceSubsystem::CollectDefinitions() const
{
	TArray<FSeenOnceHintDef> Definitions;
	Definitions.Reserve(Hints.Num());

	for (const TPair<FName, TObjectPtr<USeenOnceHint>>& Pair : Hints)
	{
		if (Pair.Value != nullptr)
		{
			Definitions.Add(Pair.Value->GetDefinition());
		}
	}

	return Definitions;
}

TArray<FName> USeenOnceSubsystem::CollectRegisteredIds() const
{
	TArray<FName> Ids;
	Hints.GetKeys(Ids);
	return Ids;
}

FSeenOnceSummary USeenOnceSubsystem::Summarize() const
{
	return USeenOnceStatics::SummarizeState(CollectDefinitions(), State);
}

USeenOnceHint* USeenOnceSubsystem::GetActiveHint() const
{
	return ActiveHint;
}

//~ The overview ---------------------------------------------------------------------------------------

void USeenOnceSubsystem::SetOverviewVisible(const bool bVisible)
{
	bOverviewVisible = bVisible;
}

void USeenOnceSubsystem::LogOverview() const
{
	const FSeenOnceSummary Summary = Summarize();

	UE_LOG(LogSeenOnce, Display, TEXT("%s"),
		*USeenOnceStatics::FormatSummaryHeader(Summary, Queue.Num(), GetSuppressionReason()));

	for (const FSeenOnceHintStatus& Line : Summary.Lines)
	{
		if (Line.bOrphan)
		{
			UE_LOG(LogSeenOnce, Display, TEXT("  %-32s unknown id, kept   %s"),
				*Line.HintId.ToString(), *USeenOnceStatics::FormatSeenTimestamp(Line.LastSeenUtc));
		}
		else if (Line.bNeverTriggered)
		{
			const bool bPending = Queue.ContainsByPredicate(
				[&Line](const FSeenOnceQueuedHint& Queued) { return Queued.HintId == Line.HintId; });

			UE_LOG(LogSeenOnce, Display, TEXT("  %-32s %s"),
				*Line.HintId.ToString(), bPending ? TEXT("pending") : TEXT("NEVER TRIGGERED"));
		}
		else
		{
			UE_LOG(LogSeenOnce, Display, TEXT("  %-32s seen x%d           %s"),
				*Line.HintId.ToString(), Line.ShowCount, *USeenOnceStatics::FormatSeenTimestamp(Line.LastSeenUtc));
		}
	}
}
