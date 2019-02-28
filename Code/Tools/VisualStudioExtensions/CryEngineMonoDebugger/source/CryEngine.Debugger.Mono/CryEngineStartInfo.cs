// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using EnvDTE;
using Mono.Debugging.Soft;
using Mono.Debugging.VisualStudio;

namespace CryEngine.Debugger.Mono
{
	internal class CryEngineStartInfo : StartInfo
	{
		public CryEngineProjectData ProjectData { get; private set; }

		public LauncherType LauncherType { get; private set; }

		public CryEngineStartInfo(SoftDebuggerStartArgs args, DebuggingOptions options, Project startupProject, CryEngineProjectData projectData, LauncherType launcherType) 
			: base(args, options, startupProject)
		{
			ProjectData = projectData;
			LauncherType = launcherType;
		}
	}
}