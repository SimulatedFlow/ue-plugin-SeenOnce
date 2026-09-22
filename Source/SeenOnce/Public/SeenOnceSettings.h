// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SeenOnceTypes.h"
#include "SeenOnceSettings.generated.h"

class USeenOnceHint;
class USeenOnceHintWidget;

/**
 * Project-wide settings for SeenOnce, under Project Settings -> Plugins -> SeenOnce.
 *
 * Three of these decide how the plugin behaves in a real project.
 *
 * bUseOwnSaveSlot is the first and the one to read before shipping. On, SeenOnce keeps the memory in its
 * own small save game and works out of the box. Off, SeenOnce keeps the memory in RAM only and expects
 * the project to call ExportState when it saves and ImportState when it loads. There is no third mode,
 * and there is deliberately no mode where SeenOnce writes into somebody else's save file.
 *
 * Hints is the second. Registering hints here is what makes the overview able to say "never triggered":
 * a hint the subsystem has never heard of cannot be reported as missing, so the list of hints has to
 * exist somewhere that does not depend on the level being loaded. It is also what keeps them cooked -
 * a hint referenced by nothing else would not be in the packaged build at all.
 *
 * SuppressionStates is the third and it is a convenience over SetSuppressed: names for the states in
 * which no hint may appear, so gameplay code can say SetSuppressed(true, "Cutscene") and the overview
 * can print the reason rather than a bare boolean.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "SeenOnce"))
class SEENONCE_API USeenOnceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USeenOnceSettings();

	//~ UDeveloperSettings
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	/** The settings object, never null. */
	static const USeenOnceSettings& Get();

	//~ The memory ------------------------------------------------------------------------------------

	/**
	 * Keep the memory in SeenOnce's own save slot.
	 *
	 * On by default so the plugin works with no integration at all. Switch it off if the project has its
	 * own save system - then ExportState/ImportState are the whole integration and nothing is written to
	 * disk behind the project's back.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Memory")
	bool bUseOwnSaveSlot = true;

	/** The slot name used when bUseOwnSaveSlot is on. */
	UPROPERTY(config, EditAnywhere, Category = "Memory", meta = (EditCondition = "bUseOwnSaveSlot"))
	FString SaveSlotName = TEXT("SeenOnce");

	/** The user index used when bUseOwnSaveSlot is on. */
	UPROPERTY(config, EditAnywhere, Category = "Memory", meta = (EditCondition = "bUseOwnSaveSlot", ClampMin = "0"))
	int32 SaveUserIndex = 0;

	/**
	 * Write the memory as soon as a hint has been shown, rather than only when asked.
	 *
	 * On by default, because the promise is that the memory survives - including surviving the player
	 * killing the process from the task manager thirty seconds after reading the hint. The write is a few
	 * hundred bytes and happens at most once per hint.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Memory", meta = (EditCondition = "bUseOwnSaveSlot"))
	bool bSaveAfterEveryShow = true;

	//~ Timing ----------------------------------------------------------------------------------------

	/** Quiet time between hints for hints that set no cooldown of their own. */
	UPROPERTY(config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0.0", Units = "s"))
	float DefaultCooldownSeconds = 2.0f;

	/**
	 * Minimum display time applied to hints that set none.
	 *
	 * A hint is protected from interruption for this long. Zero would let two hints coming due in the
	 * same frame produce a single frame of the first one, which is the flicker this plugin is here to
	 * prevent, so the default is not zero.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0.0", Units = "s"))
	float DefaultMinDisplaySeconds = 1.5f;

	/**
	 * Drop a queued hint that has been waiting longer than this, in seconds. Zero keeps them forever.
	 *
	 * Off by default. A hint that waited through a twenty minute cutscene and then appears is confusing,
	 * but so is a hint that vanished for reasons nobody can see, and of the two the first one at least
	 * leaves evidence. Projects that want the other trade-off set a number here.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0.0", Units = "s"))
	float QueueExpirySeconds = 0.0f;

	//~ Hints -----------------------------------------------------------------------------------------

	/**
	 * Every hint in the project.
	 *
	 * These are registered when the game instance starts, which is what lets the overview count hints
	 * that have never fired, and what keeps them in the cooked build. Hints can also be registered at
	 * runtime (a USeenOnceComponent registers the ones it carries), but a hint that is only ever
	 * registered by a level cannot be reported as missing before that level has been loaded.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Hints", meta = (AllowedClasses = "/Script/SeenOnce.SeenOnceHint"))
	TArray<TSoftObjectPtr<USeenOnceHint>> Hints;

	//~ Display ---------------------------------------------------------------------------------------

	/**
	 * Let SeenOnce create and drive the display widget.
	 *
	 * On, the class below is created on the first hint and told when to show and hide. Off, SeenOnce
	 * shows nothing at all and only broadcasts OnHintShown/OnHintHidden - which is the mode to use with
	 * your own HUD. Every rule in the plugin behaves identically either way; the display is the only
	 * thing that changes.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Display")
	bool bUseBuiltInDisplay = true;

	/** The widget class created when the built-in display is on. Soft, so nothing loads until a hint shows. */
	UPROPERTY(config, EditAnywhere, Category = "Display", meta = (EditCondition = "bUseBuiltInDisplay"))
	TSoftClassPtr<USeenOnceHintWidget> HintWidgetClass;

	/** Z-order the display widget is added to the viewport with. */
	UPROPERTY(config, EditAnywhere, Category = "Display", meta = (EditCondition = "bUseBuiltInDisplay"))
	int32 HintWidgetZOrder = 100;

	//~ Overview --------------------------------------------------------------------------------------

	/** Whether the overview is on when the game starts. Hint.Overview toggles it at any time. */
	UPROPERTY(config, EditAnywhere, Category = "Overview")
	bool bOverviewVisibleOnStart = false;

	/** Where the overview panel starts, in pixels from the top left of the viewport. */
	UPROPERTY(config, EditAnywhere, Category = "Overview")
	FVector2D OverviewOrigin = FVector2D(40.0f, 40.0f);

	/** How many hint lines the overview prints before it says how many it left out. */
	UPROPERTY(config, EditAnywhere, Category = "Overview", meta = (ClampMin = "1"))
	int32 OverviewMaxLines = 24;

	//~ Suppression -----------------------------------------------------------------------------------

	/**
	 * The named states in which no hint may appear.
	 *
	 * Only documentation for the humans plus the list the console command completes from - the switch is
	 * SetSuppressed(bool, Reason) and it accepts any name. Listing them here is how a project agrees on
	 * spelling, which matters when the overview prints "suppressed: cutscene" in a bug report.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Suppression")
	TArray<FName> SuppressionStates;

	/**
	 * Suppress hints while the game is paused.
	 *
	 * On by default. A pause menu is the clearest case of "not now" there is, and a hint that appears
	 * behind an open menu is a hint that was marked as seen and never read.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Suppression")
	bool bSuppressWhilePaused = true;
};
