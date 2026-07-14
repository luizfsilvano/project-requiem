// Copyright Project Requiem. All Rights Reserved.

using UnrealBuildTool;

public class ProjectRequiem : ModuleRules
{
	public ProjectRequiem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"AnimGraphRuntime",
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput"
		});
	}
}
