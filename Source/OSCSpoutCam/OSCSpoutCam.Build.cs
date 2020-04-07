// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OSCSpoutCam : ModuleRules
{
	private string ModulePath
	{
		get { return ModuleDirectory; }
	}

	private string ThirdPartyPath
	{
		get { return Path.GetFullPath(Path.Combine(ModulePath, "../../ThirdParty/")); }
	}

	public string GetUProjectPath()
	{
		return Path.Combine(ModuleDirectory, "../../../..");
	}

	private string CopyToProjectBinaries(string Filepath, ReadOnlyTargetRules Target)
	{
		string BinariesDir = Path.Combine(GetUProjectPath(), "Binaries", Target.Platform.ToString());
		string Filename = Path.GetFileName(Filepath);

		//convert relative path 
		string FullBinariesDir = Path.GetFullPath(BinariesDir);

		if (!Directory.Exists(FullBinariesDir))
		{
			Directory.CreateDirectory(FullBinariesDir);
		}

		string FullExistingPath = Path.Combine(FullBinariesDir, Filename);
		bool ValidFile = false;

		//File exists, check if they're the same
		if (File.Exists(FullExistingPath))
		{
			ValidFile = true;
		}

		//No valid existing file found, copy new dll
		if (!ValidFile)
		{
			File.Copy(Filepath, Path.Combine(FullBinariesDir, Filename), true);
		}
		return FullExistingPath;
	}

	public OSCSpoutCam(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(ThirdPartyPath, "Spout/include"),
				Path.Combine(ThirdPartyPath, "oscpp/include"),
				// ... add public include paths required here ...
			}
			);

		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"RenderCore",
				"RHI",
				"D3D11RHI",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Slate",
				"SlateCore",
				"Networking",
				"Sockets",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
		{
			string PlatformString = (Target.Platform == UnrealTargetPlatform.Win64) ? "amd64" : "x86";
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "Spout/lib", PlatformString, "Spout.lib"));

			string pluginDLLPath = Path.Combine(ThirdPartyPath, "Spout", "lib", PlatformString, "Spout.dll");
			string binariesPath = CopyToProjectBinaries(pluginDLLPath, Target);
			System.Console.WriteLine("Using Spout DLL: " + binariesPath);
			RuntimeDependencies.Add(binariesPath);

			// Delay-load the DLL, so we can load it from the right place first
			PublicDelayLoadDLLs.Add(Path.Combine(ThirdPartyPath, "Spout/lib", PlatformString, "Spout.dll"));
		}
	}
}
