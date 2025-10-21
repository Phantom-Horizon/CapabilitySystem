// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CapabilitySystem : ModuleRules {
    public CapabilitySystem(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
            }
        );
        
        PrivateIncludePaths.AddRange(
            new string[] {
            }
        );
        
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "EnhancedInput",
                "AngelscriptCode",
                "DeveloperSettings"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "NetCore"
            }
        );
    }
}