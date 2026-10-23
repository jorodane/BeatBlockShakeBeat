// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BBSB : ModuleRules
{
	public BBSB(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"LevelSequence",
			"MovieScene",
			"MovieSceneTracks"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore", "RenderCore", "RHI", "Landscape" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AnimGraph", "BlueprintGraph" }); // PIE tests and gaze authoring helper only.
		}

		PublicIncludePaths.AddRange(new string[] {
			"BBSB",
			"BBSB/Variant_Platforming",
			"BBSB/Variant_Platforming/Animation",
			"BBSB/Variant_Combat",
			"BBSB/Variant_Combat/AI",
			"BBSB/Variant_Combat/Animation",
			"BBSB/Variant_Combat/Gameplay",
			"BBSB/Variant_Combat/Interfaces",
			"BBSB/Variant_Combat/UI",
			"BBSB/Variant_SideScrolling",
			"BBSB/Variant_SideScrolling/AI",
			"BBSB/Variant_SideScrolling/Gameplay",
			"BBSB/Variant_SideScrolling/Interfaces",
			"BBSB/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
