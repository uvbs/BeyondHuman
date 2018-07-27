// Copyright Foundry, 2017. All rights reserved.

using UnrealBuildTool;
using System.IO;


namespace UnrealBuildTool.Rules
{
	public class ModoBridge : ModuleRules
	{
		private string ModulePath
		{
			get { return ModuleDirectory; }
		}

		private string ThirdPartyPath
		{
			get { return Path.GetFullPath(Path.Combine(ModulePath, "../ThirdParty/")); }
		}

		private string MacThirdPartyPath
		{
			get { return Path.GetFullPath(Path.Combine(ModulePath, "../ThirdParty/")); }
		}

		public ModoBridge(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
		{
			PublicDependencyModuleNames.AddRange(new string[] { });
			PrivateDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine", "RenderCore", "RHI", "RawMesh", "Slate", "SlateCore", "UnrealEd", "LevelEditor", "EditorStyle", "Projects"});
			// Workaround for inconsistency of PCH rules between Engine Plugin and Project Plugin
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			LoadZeroMQ(ROTargetRules);
		}

		public bool LoadZeroMQ(ReadOnlyTargetRules ROTargetRules)
		{
			bool isLibrarySupported = false;
			bool isDebug = ROTargetRules.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT;

			if ((ROTargetRules.Platform == UnrealTargetPlatform.Win64) || (ROTargetRules.Platform == UnrealTargetPlatform.Win32))
			{
				isLibrarySupported = true;

				string PlatformString = (ROTargetRules.Platform == UnrealTargetPlatform.Win64) ? "x64" : "x86";
				string LibrariesPath = Path.Combine(ThirdPartyPath, "ZeroMQ", "Libraries", PlatformString);

				//System.Console.WriteLine("Library Path: " + LibrariesPath);
				if (!isDebug)
				{
					PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "libzmq-v140-mt-4_2_1.lib"));
				}
				else
				{
					PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "libzmq-v140-mt-gd-4_2_1.lib"));
				}
			}
			else if ((ROTargetRules.Platform == UnrealTargetPlatform.Mac))
			{
				isLibrarySupported = true;

				string LibrariesPath = Path.Combine(ThirdPartyPath, "ZeroMQ", "Libraries", "OSX");
				string IncludesPath = Path.Combine(MacThirdPartyPath, "ZeroMQ", "Includes");

				//System.Console.WriteLine("Library Path: " + LibrariesPath);
				//System.Console.WriteLine("Includes Path: " + IncludesPath);

				PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "libzmq-static.a"));
				PublicIncludePaths.Add(IncludesPath);
			}
			else if ((ROTargetRules.Platform == UnrealTargetPlatform.Linux))
			{
				isLibrarySupported = true;

				string LibrariesPath = Path.Combine(ThirdPartyPath, "ZeroMQ", "Libraries", "Linux");

				//System.Console.WriteLine("Library Path: " + LibrariesPath);

				PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "libzmq.a"));
			}
			else if ((ROTargetRules.Platform == UnrealTargetPlatform.Android))
			{
				isLibrarySupported = true;

				string LibrariesPath = Path.Combine(ThirdPartyPath, "ZeroMQ", "Libraries", "Android");

				PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "libzmq.a"));
			}

			if (isLibrarySupported)
			{
				string zmqInc = Path.Combine(ThirdPartyPath, "ZeroMQ", "Includes");
				// There seems a bug on Windows that, using directly ThirdPartyPath can not find flatbuffers
				string locInc = Path.Combine(ThirdPartyPath, "Includes");
				PublicIncludePaths.AddRange(new string[] { locInc, zmqInc });

				if ((ROTargetRules.Platform == UnrealTargetPlatform.Win64) || (ROTargetRules.Platform == UnrealTargetPlatform.Win32))
				{
					if (!isDebug)
					{
						PublicDelayLoadDLLs.Add("libzmq-v140-mt-4_2_1.dll");
					}
					else
					{
						PublicDelayLoadDLLs.Add("libzmq-v140-mt-gd-4_2_1.dll");
					}
				}
			}

			return isLibrarySupported;
		}
	}
}