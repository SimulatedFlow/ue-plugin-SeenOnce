// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "SeenOnceComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "SeenOnceHint.h"
#include "SeenOnceLog.h"
#include "SeenOnceStatics.h"
#include "SeenOnceSubsystem.h"
#include "TimerManager.h"

USeenOnceComponent::USeenOnceComponent()
{
	// Nothing here ticks. Overlaps are events, the delay is a timer, and the queue belongs to the
	// subsystem - a component that ticked would be a per-actor cost for something that happens a handful
	// of times in a whole playthrough.
	PrimaryComponentTick.bCanEverTick = false;
}

void USeenOnceComponent::BeginPlay()
{
	Super::BeginPlay();

	// Registration first, and unconditionally. This is what makes the hints on this actor part of the set
	// the overview counts, so a hint sitting on a trigger volume nobody ever walks into can be reported
	// as never triggered instead of being invisible in every sense.
	if (USeenOnceSubsystem* Subsystem = GetSubsystem())
	{
		for (USeenOnceHint* Hint : Hints)
		{
			Subsystem->RegisterHint(Hint);
		}
	}

	switch (TriggerMode)
	{
	case ESeenOnceTriggerMode::Overlap:
		BindOverlaps();
		break;

	case ESeenOnceTriggerMode::BeginPlay:
		Fire();
		break;

	default:
		break;
	}
}

void USeenOnceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DelayTimer);
	}

	// The hints stay registered on purpose. The subsystem outlives the level, and a hint that stopped
	// being counted the moment its level unloaded would drop out of the overview exactly when somebody is
	// looking for it.

	Super::EndPlay(EndPlayReason);
}

void USeenOnceComponent::BindOverlaps()
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	int32 Bound = 0;
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Owner);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive != nullptr && Primitive->GetGenerateOverlapEvents())
		{
			Primitive->OnComponentBeginOverlap.AddDynamic(this, &USeenOnceComponent::HandleOverlap);
			++Bound;
		}
	}

	if (Bound == 0)
	{
		// The single most common way to get a hint that never appears: a trigger actor whose collision
		// generates no overlap events. Said once, at BeginPlay, with the actor's name in it.
		UE_LOG(LogSeenOnce, Warning,
			TEXT("SeenOnce: '%s' is set to trigger on overlap but has no component that generates overlap events. Its hints can never fire."),
			*Owner->GetName());
	}
}

void USeenOnceComponent::HandleOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (OtherActor == nullptr || OtherActor == GetOwner())
	{
		return;
	}

	if (bPlayerOnly && !IsPlayerActor(OtherActor))
	{
		return;
	}

	if (bFireOnce && bHasFired)
	{
		return;
	}

	// The area is announced even when this component's own hints are all used up, because other hints
	// elsewhere in the project may be listening for it through the AreaEntered condition.
	if (!AreaId.IsNone())
	{
		if (USeenOnceSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->NotifyAreaEntered(AreaId);
		}
	}

	if (TriggerDelaySeconds > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			// A weak lambda rather than CreateUObject: Fire returns a count, and the timer wants nothing
			// back. The weak binding is what stops a pending delay from firing into a destroyed actor.
			World->GetTimerManager().SetTimer(DelayTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { Fire(); }),
				TriggerDelaySeconds, /*bLoop=*/false);
			return;
		}
	}

	Fire();
}

int32 USeenOnceComponent::Fire()
{
	bHasFired = true;

	USeenOnceSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr)
	{
		return 0;
	}

	int32 Queued = 0;
	for (USeenOnceHint* Hint : Hints)
	{
		if (Hint == nullptr)
		{
			continue;
		}

		// Registered again here rather than assumed: a hint assigned at runtime, or a component spawned
		// after BeginPlay, has to end up in the set too.
		Subsystem->RegisterHint(Hint);
		Queued += Subsystem->Trigger(Hint->GetHintId()) ? 1 : 0;
	}

	return Queued;
}

void USeenOnceComponent::Rearm()
{
	bHasFired = false;
}

bool USeenOnceComponent::AreAllHintsSeen() const
{
	const USeenOnceSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr)
	{
		return false;
	}

	for (const USeenOnceHint* Hint : Hints)
	{
		if (Hint != nullptr && !Subsystem->HasSeen(Hint->GetHintId()))
		{
			return false;
		}
	}

	return true;
}

bool USeenOnceComponent::IsPlayerActor(const AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn != nullptr && Pawn->IsPlayerControlled();
}

USeenOnceSubsystem* USeenOnceComponent::GetSubsystem() const
{
	return USeenOnceStatics::GetSeenOnce(this);
}
