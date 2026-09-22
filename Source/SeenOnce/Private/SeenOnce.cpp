// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "SeenOnce.h"
#include "SeenOnceLog.h"

DEFINE_LOG_CATEGORY(LogSeenOnce);

#define LOCTEXT_NAMESPACE "FSeenOnceModule"

void FSeenOnceModule::StartupModule()
{
	UE_LOG(LogSeenOnce, Log, TEXT("SeenOnce started."));
}

void FSeenOnceModule::ShutdownModule()
{
	UE_LOG(LogSeenOnce, Log, TEXT("SeenOnce shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSeenOnceModule, SeenOnce)
