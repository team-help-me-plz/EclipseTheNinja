// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EclipseTheNinja : ModuleRules
{
	public EclipseTheNinja(ReadOnlyTargetRules Target) : base(Target)
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
            "SlateCore",
            "Niagara",
            "GameplayAbilities", "GameplayTags", "GameplayTasks"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"EclipseTheNinja",
			"EclipseTheNinja/Variant_Platforming",
			"EclipseTheNinja/Variant_Platforming/Animation",
			"EclipseTheNinja/Variant_Combat",
			"EclipseTheNinja/Variant_Combat/AI",
			"EclipseTheNinja/Variant_Combat/Animation",
			"EclipseTheNinja/Variant_Combat/Gameplay",
			"EclipseTheNinja/Variant_Combat/Interfaces",
			"EclipseTheNinja/Variant_Combat/UI",
			"EclipseTheNinja/Variant_SideScrolling",
			"EclipseTheNinja/Variant_SideScrolling/AI",
			"EclipseTheNinja/Variant_SideScrolling/Gameplay",
			"EclipseTheNinja/Variant_SideScrolling/Interfaces",
			"EclipseTheNinja/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
