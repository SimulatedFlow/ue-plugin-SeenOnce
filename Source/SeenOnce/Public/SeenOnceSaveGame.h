// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SeenOnceTypes.h"
#include "SeenOnceSaveGame.generated.h"

/**
 * The small save game SeenOnce writes when it is using its own slot.
 *
 * It holds one thing. That is not laziness, it is the contract: SeenOnce brings no save system and
 * replaces none, so this file has to stay something a project can ignore entirely. A project with its
 * own save system calls ExportState/ImportState, switches the own-slot option off in Project Settings,
 * and this class is never instantiated.
 *
 * Keeping the memory in a separate slot has one visible consequence worth knowing: it is not tied to
 * the project's save slots, so it behaves like one memory per player profile rather than one per save
 * game. Projects that want per-save memory - a new game showing the hints again - want the export/import
 * route. The documentation says so in the same words.
 */
UCLASS(BlueprintType)
class SEENONCE_API USeenOnceSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** What this player has already been taught. */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "SeenOnce")
	FSeenOnceState State;

	/** Written for the benefit of whoever opens this file in five years wondering what wrote it. */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "SeenOnce")
	FString PluginVersion = TEXT("1.0.0");
};
