// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SeenOnceTypes.h"
#include "SeenOnceHintWidget.generated.h"

class USeenOnceHint;
class USoundBase;
class UTexture2D;

/**
 * The base class of the display that ships with the plugin - and the one class here that is meant to be
 * thrown away.
 *
 * A hint is part of the game's interface and has to look like it, so the display is UMG and the plugin
 * makes no attempt to be clever about it: the subsystem hands this widget the text, the icon and the
 * hint, and the widget Blueprint does the rest. What SeenOnce is actually selling is the decision - has
 * this player already been taught this, is now a good moment, which of these two goes first - and that
 * decision is identical whether it drives this widget or nothing at all.
 *
 * Using your own display is one setting and two bindings: switch bUseBuiltInDisplay off, bind your own
 * widget to USeenOnceSubsystem::OnHintShown and OnHintHidden. Nothing else changes.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "SeenOnce Hint Widget"))
class SEENONCE_API USeenOnceHintWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Show this hint. Called by the subsystem; safe to call yourself.
	 *
	 * The text is passed alongside the hint because that is the value the widget should print: a hint
	 * whose asset unloads mid-fade still has a sentence to finish showing.
	 */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	void ShowHint(USeenOnceHint* Hint, const FText& Body);

	/** Take the hint down, with the reason it came down. */
	UFUNCTION(BlueprintCallable, Category = "SeenOnce")
	void HideHint(ESeenOnceHideReason Reason);

	/** Build the look: text, icon, animation in. The widget Blueprint implements this. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SeenOnce", meta = (DisplayName = "On Hint Shown"))
	void ReceiveHintShown(USeenOnceHint* Hint, const FText& Body);

	/** Take the look down. Fade out here; the subsystem has already moved on. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SeenOnce", meta = (DisplayName = "On Hint Hidden"))
	void ReceiveHintHidden(USeenOnceHint* Hint, ESeenOnceHideReason Reason);

	/** The hint currently on screen, or null. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	TObjectPtr<USeenOnceHint> CurrentHint;

	/** The text currently on screen. Kept separately so the widget survives the asset going away. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	FText CurrentBody;

	/** The icon of the current hint, already resolved, or null when it has none. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	TObjectPtr<UTexture2D> CurrentIcon;

	/** True between ShowHint and HideHint. */
	UPROPERTY(BlueprintReadOnly, Category = "SeenOnce")
	bool bHintVisible = false;
};
