// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "Engine/TimerHandle.h"
#include "SeenOnceTypes.h"
#include "SeenOnceComponent.generated.h"

class AActor;
class APawn;
class UPrimitiveComponent;
class USeenOnceHint;
class USeenOnceSubsystem;

/** When this component fires the hints it carries. */
UENUM(BlueprintType)
enum class ESeenOnceTriggerMode : uint8
{
	/** When something overlaps the owner. The usual case: a trigger volume around a door. */
	Overlap,

	/** Once, when the owner starts play. For hints tied to a level rather than to a place in it. */
	BeginPlay,

	/** Only when Fire() is called. Everything else is still done for you: registration and the area id. */
	Manual,
};

/**
 * Put hints on an actor without wiring anything.
 *
 * This is the path for the ninety percent: drop the component on the trigger volume by the door, pick
 * the hint asset, done. It does three things that are easy to forget by hand - it registers the hints it
 * carries with the subsystem so they can be counted (a hint nobody registered can never be reported as
 * "never triggered"), it filters overlaps down to the player by default, and it announces the area id so
 * that hints elsewhere in the project can use the AreaEntered condition on this same volume.
 *
 * The component deliberately does no remembering of its own. Whether the hint has been seen is the
 * subsystem's question and only the subsystem's, because a component's answer would die with the level
 * and the whole point is that the answer does not.
 */
UCLASS(ClassGroup = (SeenOnce), meta = (BlueprintSpawnableComponent, DisplayName = "SeenOnce Hints"))
class SEENONCE_API USeenOnceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USeenOnceComponent();

	//~ UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Fire the hints this component carries, now.
	 *
	 * Returns how many were actually queued, which is usually zero after the first time - that is the
	 * plugin working, not failing.
	 */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	int32 Fire();

	/** True once this component has fired, whether or not any hint was actually shown. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	bool HasFired() const { return bHasFired; }

	/** Let this component fire again. Does not forget anything - the hint memory is the subsystem's. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	void Rearm();

	/** True when every hint on this component has already been seen. What a "used up" trigger looks like. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	bool AreAllHintsSeen() const;

	//~ Configuration ----------------------------------------------------------------------------------

	/** The hints this actor is responsible for. Registered with the subsystem when play starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	TArray<TObjectPtr<USeenOnceHint>> Hints;

	/** What makes this component fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	ESeenOnceTriggerMode TriggerMode = ESeenOnceTriggerMode::Overlap;

	/**
	 * The name this place is known by.
	 *
	 * Announced to the subsystem on overlap, so hints anywhere in the project can use the AreaEntered
	 * condition against it. It is how a hint that belongs to a level rather than to an actor gets a
	 * trigger without anybody editing this actor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	FName AreaId;

	/** Only the player's pawn counts as an overlap. On by default; a patrolling guard is not a reader. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce", meta = (EditCondition = "TriggerMode == ESeenOnceTriggerMode::Overlap"))
	bool bPlayerOnly = true;

	/** Stop listening after the first fire. Off means every overlap asks again - the memory still decides. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce")
	bool bFireOnce = false;

	/** Seconds to wait after the trigger before the hints are fired. For a hint that should follow a door opening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeenOnce", meta = (ClampMin = "0.0", Units = "s"))
	float TriggerDelaySeconds = 0.0f;

private:
	/** Overlap handler. Named for what it is so nothing here shadows a UActorComponent member. */
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Binds to every primitive on the owner that generates overlap events. */
	void BindOverlaps();

	/** True when this actor is the one the player is controlling. */
	static bool IsPlayerActor(const AActor* Actor);

	/** The subsystem, or null outside a game. */
	USeenOnceSubsystem* GetSubsystem() const;

	/** Set once the component has fired, and cleared by Rearm. */
	bool bHasFired = false;

	/** The delayed fire, if TriggerDelaySeconds is set. */
	FTimerHandle DelayTimer;
};
