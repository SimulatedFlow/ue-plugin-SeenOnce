// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "SeenOnceHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GlobalRenderResources.h"
#include "SeenOnceHint.h"
#include "SeenOnceSettings.h"
#include "SeenOnceStatics.h"
#include "SeenOnceSubsystem.h"

namespace SeenOncePanel
{
	constexpr float BasePadding = 10.0f;
	constexpr float BaseLineHeight = 15.0f;

	/** The width the right-hand column reserves for a state and a timestamp. */
	constexpr float BaseStatusColumn = 210.0f;

	void DrawFilledRect(UCanvas* Canvas, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color)
	{
		FCanvasTileItem Tile(Position, GWhiteTexture, Size, Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}
}

ASeenOnceHUD::ASeenOnceHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASeenOnceHUD::DrawHUD()
{
	Super::DrawHUD();

	if (Canvas == nullptr)
	{
		return;
	}

	const USeenOnceSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr || !Subsystem->IsOverviewVisible())
	{
		return;
	}

	const FVector2D Origin = USeenOnceSettings::Get().OverviewOrigin;
	DrawOverview(static_cast<float>(Origin.X), static_cast<float>(Origin.Y));
}

float ASeenOnceHUD::DrawOverview(const float OriginX, const float OriginY)
{
	if (Canvas == nullptr)
	{
		return 0.0f;
	}

	USeenOnceSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr)
	{
		return 0.0f;
	}

	UFont* DrawFont = (Font != nullptr) ? Font.Get() : (GEngine != nullptr ? GEngine->GetSmallFont() : nullptr);
	if (DrawFont == nullptr)
	{
		return 0.0f;
	}

	const USeenOnceSettings& Settings = USeenOnceSettings::Get();

	// Everything is read on the frame it is drawn. Nothing here is cached, so the panel cannot show one
	// set of counts while the memory holds another - which matters, because this screen exists to be
	// believed and the numbers in it end up in bug reports.
	const FSeenOnceSummary Summary = Subsystem->Summarize();
	const TArray<FSeenOnceQueuedHint>& Queue = Subsystem->GetQueue();

	const float Scale = FMath::Max(0.5f, TextScale);
	const float LineHeight = SeenOncePanel::BaseLineHeight * Scale;
	const float Padding = SeenOncePanel::BasePadding * Scale;
	const float Width = FMath::Max(PanelWidth * Scale, SeenOncePanel::BaseStatusColumn * Scale * 2.0f);

	const int32 MaxLines = FMath::Max(1, Settings.OverviewMaxLines);
	const int32 DrawnLines = FMath::Min(Summary.Lines.Num(), MaxLines);
	const int32 HiddenLines = Summary.Lines.Num() - DrawnLines;

	// Header, separator, the lines, and the "+N more" line when there is one.
	const int32 TotalRows = 2 + DrawnLines + (HiddenLines > 0 ? 1 : 0);
	const float Height = (TotalRows * LineHeight) + (2.0f * Padding);

	SeenOncePanel::DrawFilledRect(Canvas, FVector2D(OriginX, OriginY), FVector2D(Width, Height), BackgroundColor);

	float Y = OriginY + Padding;
	const float X = OriginX + Padding;
	const float InnerWidth = Width - (2.0f * Padding);

	// The header, in the shape the documentation and every screenshot use:
	//   hints 18 | seen 11 | pending 2 | never triggered 5 | suppressed: cutscene
	const FString Header = USeenOnceStatics::FormatSummaryHeader(Summary, Queue.Num(), Subsystem->GetSuppressionReason());
	Y += DrawLine(X, Y, Header, FString(), HeaderColor, LineHeight, InnerWidth);

	// A rule under the header rather than a blank line: it keeps the panel readable over a bright scene,
	// where a gap just looks like the panel ended.
	SeenOncePanel::DrawFilledRect(Canvas, FVector2D(X, Y + (LineHeight * 0.4f)), FVector2D(InnerWidth, 1.0f * Scale),
		FLinearColor(HeaderColor.R, HeaderColor.G, HeaderColor.B, 0.25f));
	Y += LineHeight;

	for (int32 Index = 0; Index < DrawnLines; ++Index)
	{
		const FSeenOnceHintStatus& Line = Summary.Lines[Index];

		FString Status;
		FLinearColor Color;

		if (Line.bOrphan)
		{
			// Kept, and shown as kept. This line is the proof that nothing was thrown away when a save
			// from another build was loaded - the claim in the documentation, on screen.
			Status = TEXT("unknown id, kept");
			Color = OrphanColor;
		}
		else if (Line.bNeverTriggered)
		{
			const bool bPending = Queue.ContainsByPredicate(
				[&Line](const FSeenOnceQueuedHint& Queued) { return Queued.HintId == Line.HintId; });

			Status = bPending ? TEXT("pending") : TEXT("NEVER TRIGGERED");
			Color = bPending ? PendingColor : NeverTriggeredColor;
		}
		else
		{
			const FString Timestamp = USeenOnceStatics::FormatSeenTimestamp(Line.LastSeenUtc);
			Status = (Line.ShowCount > 1)
				? FString::Printf(TEXT("seen x%d  %s"), Line.ShowCount, *Timestamp)
				: FString::Printf(TEXT("seen  %s"), *Timestamp);
			Color = SeenColor;
		}

		Y += DrawLine(X, Y, Line.HintId.ToString(), Status, Color, LineHeight, InnerWidth);
	}

	if (HiddenLines > 0)
	{
		// Never silently truncated. A panel that shows twenty of twenty-six lines and says nothing is a
		// panel that can be read as "these are all of them".
		Y += DrawLine(X, Y, FString::Printf(TEXT("+ %d more (raise Overview Max Lines, or use Hint.List)"), HiddenLines),
			FString(), OrphanColor, LineHeight, InnerWidth);
	}

	return Height;
}

float ASeenOnceHUD::DrawLine(const float X, const float Y, const FString& Left, const FString& Right,
	const FLinearColor& Color, const float LineHeight, const float Width)
{
	UFont* DrawFont = (Font != nullptr) ? Font.Get() : (GEngine != nullptr ? GEngine->GetSmallFont() : nullptr);
	if (DrawFont == nullptr || Canvas == nullptr)
	{
		return LineHeight;
	}

	const float Scale = FMath::Max(0.5f, TextScale);

	FCanvasTextItem LeftItem(FVector2D(X, Y), FText::FromString(Left), DrawFont, Color);
	LeftItem.Scale = FVector2D(Scale, Scale);
	Canvas->DrawItem(LeftItem);

	if (!Right.IsEmpty())
	{
		// Measured and right-aligned rather than tabbed to a fixed column: hint ids are as long as
		// somebody made them, and a timestamp that slides off the panel is worse than a cramped one.
		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		Canvas->StrLen(DrawFont, Right, TextWidth, TextHeight);
		TextWidth *= Scale;

		const float RightX = FMath::Max(X + (Width * 0.45f), X + Width - TextWidth);

		FCanvasTextItem RightItem(FVector2D(RightX, Y), FText::FromString(Right), DrawFont, Color);
		RightItem.Scale = FVector2D(Scale, Scale);
		Canvas->DrawItem(RightItem);
	}

	return LineHeight;
}

void ASeenOnceHUD::SetOverviewVisible(const bool bVisible)
{
	if (USeenOnceSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->SetOverviewVisible(bVisible);
	}
}

void ASeenOnceHUD::ToggleOverview()
{
	if (USeenOnceSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->SetOverviewVisible(!Subsystem->IsOverviewVisible());
	}
}

bool ASeenOnceHUD::IsOverviewVisible() const
{
	const USeenOnceSubsystem* Subsystem = GetSubsystem();
	return Subsystem != nullptr && Subsystem->IsOverviewVisible();
}

USeenOnceSubsystem* ASeenOnceHUD::GetSubsystem() const
{
	return USeenOnceStatics::GetSeenOnce(this);
}
