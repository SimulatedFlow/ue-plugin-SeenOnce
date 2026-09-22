// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "SeenOnceHint.h"
#include "SeenOnceLog.h"
#include "SeenOnceStatics.h"
#include "SeenOnceSubsystem.h"

/**
 * The console surface.
 *
 * These six exist because checking a hint system otherwise means starting a new game, and a tutorial you
 * can only test from the beginning is a tutorial nobody tests twice. Hint.Reset and Hint.ResetAll turn a
 * one-shot into something repeatable; Hint.Show previews without playing to the trigger; Hint.List and
 * Hint.Overview answer the question that has no other answer, which is which hints have never fired.
 *
 * All six need a running game, because the memory belongs to a game instance and there is no memory
 * without one.
 */
namespace SeenOnceCommands
{
	/** The live subsystem, from any game or PIE world that has one. */
	static USeenOnceSubsystem* FindSubsystem()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType != EWorldType::Game && Context.WorldType != EWorldType::PIE)
			{
				continue;
			}

			if (UGameInstance* Instance = Context.OwningGameInstance)
			{
				if (USeenOnceSubsystem* Subsystem = Instance->GetSubsystem<USeenOnceSubsystem>())
				{
					return Subsystem;
				}
			}
		}

		return nullptr;
	}

	static USeenOnceSubsystem* RequireSubsystem(const TCHAR* CommandName)
	{
		USeenOnceSubsystem* Subsystem = FindSubsystem();
		if (Subsystem == nullptr)
		{
			UE_LOG(LogSeenOnce, Warning,
				TEXT("%s needs a running game. The hint memory belongs to the game instance, and there is not one."),
				CommandName);
		}

		return Subsystem;
	}

	static bool ParseBool(const TArray<FString>& Args, const bool bDefault)
	{
		if (Args.Num() == 0)
		{
			return bDefault;
		}

		return Args[0].ToBool() || Args[0] == TEXT("1");
	}

	static FAutoConsoleCommand GShow(
		TEXT("Hint.Show"),
		TEXT("Hint.Show <Id> - show a hint now, seen or not. It counts as seen; use Hint.Reset to undo that."),
		FConsoleCommandWithArgsDelegate::CreateStatic([](const TArray<FString>& Args)
		{
			USeenOnceSubsystem* Subsystem = RequireSubsystem(TEXT("Hint.Show"));
			if (Subsystem == nullptr)
			{
				return;
			}

			if (Args.Num() == 0)
			{
				UE_LOG(LogSeenOnce, Display, TEXT("Hint.Show <Id>. Hint.List prints the ids."));
				return;
			}

			if (!Subsystem->ForceShow(FName(*Args[0])))
			{
				UE_LOG(LogSeenOnce, Warning, TEXT("Hint.Show: no hint with the id '%s' is registered."), *Args[0]);
			}
		}));

	static FAutoConsoleCommand GList(
		TEXT("Hint.List"),
		TEXT("Hint.List - every hint with its state: seen and when, pending, or never triggered."),
		FConsoleCommandDelegate::CreateStatic([]()
		{
			if (const USeenOnceSubsystem* Subsystem = RequireSubsystem(TEXT("Hint.List")))
			{
				Subsystem->LogOverview();
			}
		}));

	static FAutoConsoleCommand GReset(
		TEXT("Hint.Reset"),
		TEXT("Hint.Reset <Id> - forget one hint, whatever its rule, so it can be earned again."),
		FConsoleCommandWithArgsDelegate::CreateStatic([](const TArray<FString>& Args)
		{
			USeenOnceSubsystem* Subsystem = RequireSubsystem(TEXT("Hint.Reset"));
			if (Subsystem == nullptr)
			{
				return;
			}

			if (Args.Num() == 0)
			{
				UE_LOG(LogSeenOnce, Display, TEXT("Hint.Reset <Id>. Hint.ResetAll forgets the whole set."));
				return;
			}

			const FName HintId(*Args[0]);
			if (!Subsystem->Reset(HintId))
			{
				UE_LOG(LogSeenOnce, Display, TEXT("Hint.Reset: '%s' had not been seen anyway."), *Args[0]);
			}
		}));

	static FAutoConsoleCommand GResetAll(
		TEXT("Hint.ResetAll"),
		TEXT("Hint.ResetAll [all] - forget the hints. Without 'all', hints marked 'Once (forever)' are kept, as are ids this build does not know."),
		FConsoleCommandWithArgsDelegate::CreateStatic([](const TArray<FString>& Args)
		{
			USeenOnceSubsystem* Subsystem = RequireSubsystem(TEXT("Hint.ResetAll"));
			if (Subsystem == nullptr)
			{
				return;
			}

			// "all" has to be typed. Forgetting the permanent memory is the one operation here that a
			// player could notice, and it should not be one keystroke away from the ordinary reset.
			const bool bIncludePermanent = Args.Num() > 0 && Args[0].Equals(TEXT("all"), ESearchCase::IgnoreCase);

			const int32 Removed = Subsystem->ResetAll(bIncludePermanent);
			UE_LOG(LogSeenOnce, Display, TEXT("Hint.ResetAll: forgot %d hint(s)%s."),
				Removed, bIncludePermanent ? TEXT(", including the permanent ones") : TEXT(" (add 'all' to include the permanent ones)"));
		}));

	static FAutoConsoleCommand GSuppress(
		TEXT("Hint.Suppress"),
		TEXT("Hint.Suppress 0|1 [reason] - stop or allow hints. Nothing is lost while it is on; the queue waits."),
		FConsoleCommandWithArgsDelegate::CreateStatic([](const TArray<FString>& Args)
		{
			USeenOnceSubsystem* Subsystem = RequireSubsystem(TEXT("Hint.Suppress"));
			if (Subsystem == nullptr)
			{
				return;
			}

			const bool bSuppressed = ParseBool(Args, !Subsystem->IsSuppressed());
			const FName Reason = (Args.Num() > 1) ? FName(*Args[1]) : FName(TEXT("console"));

			Subsystem->SetSuppressed(bSuppressed, Reason);
		}));

	static FAutoConsoleCommand GOverview(
		TEXT("Hint.Overview"),
		TEXT("Hint.Overview [0|1] - the on-screen overview. Drawn on canvas, so it works in a packaged build."),
		FConsoleCommandWithArgsDelegate::CreateStatic([](const TArray<FString>& Args)
		{
			USeenOnceSubsystem* Subsystem = RequireSubsystem(TEXT("Hint.Overview"));
			if (Subsystem == nullptr)
			{
				return;
			}

			const bool bVisible = ParseBool(Args, !Subsystem->IsOverviewVisible());
			Subsystem->SetOverviewVisible(bVisible);

			UE_LOG(LogSeenOnce, Display, TEXT("Hint.Overview: %s.%s"),
				bVisible ? TEXT("on") : TEXT("off"),
				bVisible ? TEXT(" It needs a HUD - ASeenOnceHUD, or your own calling DrawOverview.") : TEXT(""));
		}));
}
