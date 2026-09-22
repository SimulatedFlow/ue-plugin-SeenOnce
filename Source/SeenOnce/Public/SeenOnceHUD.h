// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SeenOnceTypes.h"
#include "SeenOnceHUD.generated.h"

class UCanvas;
class UFont;
class USeenOnceSubsystem;

/**
 * The overview: every hint, its state, and when it was seen - drawn on UCanvas.
 *
 * This screen is the reason to buy the plugin rather than write the queue yourself, and it is drawn on
 * canvas rather than built in UMG for one reason: it has to exist in the packaged Shipping build. The
 * question it answers - "which of my hints has never once fired?" - cannot be answered in the editor,
 * because a condition that can never become true looks exactly like a condition that has not become true
 * yet. Only a playthrough separates them, and playthroughs happen in builds.
 *
 * The layout is fixed on purpose:
 *
 *   hints 18 | seen 11 | pending 2 | never triggered 5 | suppressed: cutscene
 *
 * then the never-triggered hints, at the top and in warning colour, then everything that has been seen
 * with the timestamp of when. Never triggered goes first because it is the only line here that is a
 * finding rather than a record.
 *
 * Set this as the HUD class, or add it to your own AHUD subclass by calling DrawOverview from DrawHUD.
 */
UCLASS(Blueprintable, meta = (DisplayName = "SeenOnce HUD"))
class SEENONCE_API ASeenOnceHUD : public AHUD
{
	GENERATED_BODY()

public:
	ASeenOnceHUD();

	//~ AHUD
	virtual void DrawHUD() override;

	/**
	 * Draw the overview at the given position, and return how tall it turned out.
	 *
	 * Public so that a project with its own HUD class can keep that class and still have this screen -
	 * one call from DrawHUD is the whole integration.
	 */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	float DrawOverview(float OriginX, float OriginY);

	/** Show or hide the overview. Same switch as the Hint.Overview console command. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	void SetOverviewVisible(bool bVisible);

	/** Flip it. What a debug key binds to. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	void ToggleOverview();

	/** Is the overview currently drawing? */
	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	bool IsOverviewVisible() const;

	//~ Look -------------------------------------------------------------------------------------------

	/** The font. Left at the engine's small font, which is present in every build. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SeenOnce|Style")
	TObjectPtr<UFont> Font;

	/** Text scale. The overview is dense on purpose; this is the dial for a 4K screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SeenOnce|Style", meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float TextScale = 1.0f;

	/** Panel background. Alpha below one keeps the game visible behind it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SeenOnce|Style")
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.03f, 0.82f);

	/** The header line. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SeenOnce|Style")
	FLinearColor HeaderColor = FLinearColor(0.92f, 0.95f, 1.0f, 1.0f);

	/** Hints that have never fired. Warning colour, because this line is a finding. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SeenOnce|Style")
	FLinearColor NeverTriggeredColor = FLinearColor(1.0f, 0.62f, 0.16f, 1.0f);

	/** Hints waiting for their turn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SeenOnce|Style")
	FLinearColor PendingColor = FLinearColor(0.42f, 0.78f, 1.0f, 1.0f);

	/** Hints that have been seen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SeenOnce|Style")
	FLinearColor SeenColor = FLinearColor(0.62f, 0.66f, 0.72f, 1.0f);

	/** Entries in the memory that no hint claims. Dim, present, never dropped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SeenOnce|Style")
	FLinearColor OrphanColor = FLinearColor(0.45f, 0.45f, 0.50f, 1.0f);

	/** Width of the panel in pixels before text scale. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SeenOnce|Style", meta = (ClampMin = "200.0"))
	float PanelWidth = 560.0f;

private:
	/** One line of the list. Returns the height it used. */
	float DrawLine(float X, float Y, const FString& Left, const FString& Right, const FLinearColor& Color, float LineHeight, float Width);

	/** The subsystem for this HUD's game instance, or null. */
	USeenOnceSubsystem* GetSubsystem() const;
};
