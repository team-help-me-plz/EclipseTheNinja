using UnrealBuildTool;

public class EclipseLocalTools : ModuleRules
{
	public EclipseLocalTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"ToolsetRegistry",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"EditorSubsystem",
				"UnrealEd",
			}
		);

		// Live Coding은 인터페이스만 쓰고 모듈은 실행 중에 찾는다. 링크 의존을 만들지 않는다
		if (Target.bWithLiveCoding)
		{
			PrivateIncludePathModuleNames.Add("LiveCoding");
		}
	}
}
