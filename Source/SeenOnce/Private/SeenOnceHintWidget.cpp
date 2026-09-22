// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "SeenOnceHintWidget.h"

#include "Engine/Texture2D.h"
#include "SeenOnceHint.h"

void USeenOnceHintWidget::ShowHint(USeenOnceHint* Hint, const FText& Body)
{
	CurrentHint = Hint;
	CurrentBody = Body;
	bHintVisible = true;

	// Resolved here rather than in the subsystem so that the widget can be driven by hand from Blueprint
	// with the same one call. LoadSynchronous on a soft pointer that is usually already resident is a
	// hash lookup; on the first hint of a session it is one small texture, at a moment when the game has
	// just decided to interrupt the player anyway.
	CurrentIcon = (Hint != nullptr) ? Hint->Icon.LoadSynchronous() : nullptr;

	ReceiveHintShown(Hint, Body);
}

void USeenOnceHintWidget::HideHint(const ESeenOnceHideReason Reason)
{
	USeenOnceHint* Hint = CurrentHint;

	bHintVisible = false;

	// The hint reference is kept until after the event so an implementation can fade the same text out.
	ReceiveHintHidden(Hint, Reason);

	CurrentHint = nullptr;
	CurrentIcon = nullptr;
	CurrentBody = FText::GetEmpty();
}
