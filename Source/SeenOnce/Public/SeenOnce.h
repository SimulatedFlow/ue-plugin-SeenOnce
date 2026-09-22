// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * SeenOnce's one runtime module.
 *
 * It owns nothing and starts nothing. The memory, the queue and the conditions live in
 * USeenOnceSubsystem, which the engine creates with the game instance; the decision logic lives in
 * USeenOnceStatics and needs no world at all.
 */
class FSeenOnceModule : public IModuleInterface
{
public:
	//~ IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
