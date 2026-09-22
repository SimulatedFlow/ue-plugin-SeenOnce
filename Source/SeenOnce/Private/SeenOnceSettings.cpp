// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "SeenOnceSettings.h"

USeenOnceSettings::USeenOnceSettings()
{
	// Names, not behaviour. A project renames these to whatever its own states are called; what matters
	// is that everybody spells the reason the same way, because the reason ends up in the overview and
	// from there in bug reports.
	SuppressionStates = { TEXT("Cutscene"), TEXT("Menu"), TEXT("Combat"), TEXT("Loading") };
}

FName USeenOnceSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName USeenOnceSettings::GetSectionName() const
{
	return TEXT("SeenOnce");
}

const USeenOnceSettings& USeenOnceSettings::Get()
{
	const USeenOnceSettings* Settings = GetDefault<USeenOnceSettings>();
	check(Settings != nullptr);
	return *Settings;
}
