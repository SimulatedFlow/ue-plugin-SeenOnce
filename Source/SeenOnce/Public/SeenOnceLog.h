// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/**
 * Everything SeenOnce says goes through this one category, so a project can silence it with a single
 * line and, more usefully, can turn it up to Verbose and get a running account of why a hint did or did
 * not show - which is the question people actually have.
 */
SEENONCE_API DECLARE_LOG_CATEGORY_EXTERN(LogSeenOnce, Log, All);
