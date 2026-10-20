// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CastleCTF : ModuleRules
{
	public CastleCTF(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate"
			,"SlateCore", "NavigationSystem", "Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
		if (Target.bBuildEditor) PrivateDependencyModuleNames.Add("UnrealEd");

		PublicIncludePaths.AddRange(new string[] {
			"CastleCTF",
			"CastleCTF/Variant_Platforming",
			"CastleCTF/Variant_Platforming/Animation",
			"CastleCTF/Variant_Combat",
			"CastleCTF/Variant_Combat/AI",
			"CastleCTF/Variant_Combat/Animation",
			"CastleCTF/Variant_Combat/Gameplay",
			"CastleCTF/Variant_Combat/Interfaces",
			"CastleCTF/Variant_Combat/UI",
			"CastleCTF/Variant_SideScrolling",
			"CastleCTF/Variant_SideScrolling/AI",
			"CastleCTF/Variant_SideScrolling/Gameplay",
			"CastleCTF/Variant_SideScrolling/Interfaces",
			"CastleCTF/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
