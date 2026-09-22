// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SeenOnceTypes.h"
#include "SeenOnceHint.generated.h"

class USoundBase;
class UTexture2D;

/**
 * One hint, as an asset.
 *
 * A hint is a data asset and not a Blueprint node, and that choice is the difference between a tutorial
 * that can be reviewed and one that cannot. Assets can be listed, diffed, localized, handed to somebody
 * who does not open Blueprints, and - the point of this plugin - counted, so the overview can say that
 * eighteen hints exist and five of them have never fired. Hints scattered through event graphs can be
 * none of those things.
 *
 * The id is the identity. It is what the memory is keyed on, so renaming it is the same as creating a
 * new hint: the old id stays in the save (see USeenOnceSubsystem::ImportState) and the new one has never
 * been seen. That is a documented consequence, not an accident - the alternative, keying on the asset
 * path, breaks the moment somebody moves a folder.
 */
UCLASS(BlueprintType, meta = (DisplayName = "SeenOnce Hint"))
class SEENONCE_API USeenOnceHint : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USeenOnceHint();

	/**
	 * The hint as plain values, which is what every rule in the plugin actually reads.
	 *
	 * When HintId is left empty the asset's own name is used. That is a convenience with teeth: it means
	 * a renamed asset is a renamed hint, so anybody relying on it should set the id explicitly.
	 */
	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	FSeenOnceHintDef GetDefinition() const;

	/** The id this hint is remembered under: the explicit one, or the asset name when none is set. */
	UFUNCTION(BlueprintPure, Category = "SeenOnce")
	FName GetHintId() const;

	//~ The hint itself -------------------------------------------------------------------------------

	/** Everything a rule needs: id, text, importance, timing, repeat rule and condition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SeenOnce", meta = (ShowOnlyInnerProperties))
	FSeenOnceHintDef Hint;

	//~ Presentation, which the plugin passes on and never interprets -----------------------------------

	/** Optional icon, handed to the display widget. Soft, so an unused hint costs nothing at load. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Optional sound, played once when the hint appears. Soft for the same reason. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<USoundBase> Sound;

	/**
	 * A note for whoever reads the overview later.
	 *
	 * Not shown to players. This is where you write "only reachable after the second boss", so that a
	 * hint sitting in the never-triggered list can be judged without opening the level it belongs to.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (MultiLine = true))
	FString DesignerNote;

	//~ UPrimaryDataAsset
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	//~ UObject
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
