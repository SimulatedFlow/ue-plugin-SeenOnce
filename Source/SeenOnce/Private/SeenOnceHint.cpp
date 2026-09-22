// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "SeenOnceHint.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "SeenOnceHint"

USeenOnceHint::USeenOnceHint()
{
	// A hint with no text is the one mistake worth defaulting against, so the body starts with something
	// that is obviously placeholder rather than something that is invisibly empty.
	Hint.Body = LOCTEXT("DefaultBody", "Hint text");
}

FName USeenOnceHint::GetHintId() const
{
	return Hint.HintId.IsNone() ? GetFName() : Hint.HintId;
}

FSeenOnceHintDef USeenOnceHint::GetDefinition() const
{
	FSeenOnceHintDef Definition = Hint;
	Definition.HintId = GetHintId();
	return Definition;
}

FPrimaryAssetId USeenOnceHint::GetPrimaryAssetId() const
{
	// One type for every hint, so a project that uses the asset manager can load them as a group.
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("SeenOnceHint")), GetFName());
}

#if WITH_EDITOR
EDataValidationResult USeenOnceHint::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Everything below is a condition that can never become true, which is precisely the failure this
	// plugin exists to surface. Catching the ones that are visible in the asset costs nothing; the rest
	// show up in the overview as "never triggered" after a playthrough, which is the only place they can.
	if (Hint.Body.IsEmpty())
	{
		Context.AddError(LOCTEXT("EmptyBody", "This hint has no text. It would show an empty box and still count as seen."));
		Result = EDataValidationResult::Invalid;
	}

	if (Hint.MinDisplaySeconds > Hint.DisplaySeconds)
	{
		Context.AddWarning(LOCTEXT("MinLongerThanDisplay",
			"Minimum display time is longer than the display time. The hint will stay up for the minimum, which is probably not what was meant."));
	}

	switch (Hint.Condition)
	{
	case ESeenOnceCondition::GameplayTag:
		if (!Hint.ConditionTag.IsValid())
		{
			Context.AddError(LOCTEXT("NoTag", "Condition is 'gameplay tag present' but no tag is set. This hint can never come due."));
			Result = EDataValidationResult::Invalid;
		}
		break;

	case ESeenOnceCondition::AreaEntered:
	case ESeenOnceCondition::ActionCount:
	case ESeenOnceCondition::ItemReceived:
		if (Hint.ConditionKey.IsNone())
		{
			Context.AddError(LOCTEXT("NoKey", "This condition needs a key (the area, action or item name). Without one the hint can never come due."));
			Result = EDataValidationResult::Invalid;
		}
		break;

	case ESeenOnceCondition::PlayTime:
		if (Hint.RequiredPlayTimeSeconds <= 0.0f)
		{
			Context.AddWarning(LOCTEXT("NoPlayTime", "Play time condition with no time set. The hint comes due immediately."));
		}
		break;

	default:
		break;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
