// Copyright 2026 Silvan Teufel. All Rights Reserved.

using UnrealBuildTool;

public class SeenOnce : ModuleRules
{
	public SeenOnce(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// One runtime module, and that is deliberate.
		//
		// Everything this plugin does happens while the game is running and has to keep happening in a
		// cooked Shipping build: the memory of what a player has already been taught, the queue that keeps
		// two hints from landing on top of each other, and the overview that says which hint has never
		// fired. An editor module would only be able to look at assets, and the interesting failures here
		// are never in the asset - they are in the fourth session of a real save game.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",

			// UGameInstanceSubsystem, USaveGame, UGameplayStatics, AHUD, UCanvas, UActorComponent and the
			// overlap events the component binds to.
			"Engine",

			// UUserWidget. The shipped hint display is a widget on purpose: a hint is part of the game's
			// interface and has to be able to look like it. It is also replaceable - a project that binds
			// its own widget to OnHintShown/OnHintHidden never instantiates ours.
			"UMG",

			// FSlateApplication and the font info behind the overview's canvas text.
			"Slate",
			"SlateCore",

			// FGameplayTag conditions, and TriggerByTag.
			"GameplayTags",

			// USeenOnceSettings is a UDeveloperSettings, so the slot name, the cooldown and the suppression
			// states appear under Project Settings > Plugins > SeenOnce with no editor module involved.
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// GWhiteTexture - the one-pixel texture the overview panel's background is tiled from.
			"RenderCore",
		});

		// Deliberately NOT here:
		//   UnrealEd - a hint that has never triggered cannot be found by looking at the asset. It is found
		//              by playing, which is why the overview draws on UCanvas from an AHUD and ships in the
		//              packaged build rather than living in an editor window.
	}
}
