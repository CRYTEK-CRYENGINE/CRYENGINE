// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Collections.Generic;
using System.IO;
using MonoDevelop.Core;
using MonoDevelop.Core.Assemblies;
using MonoDevelop.Core.Execution;
using MonoDevelop.Projects;

namespace MonoDevelop.CryEngine.Projects
{
	public sealed class CryEngineExecutionTarget : ExecutionTarget
	{
		public override string Id { get; }
		public override string Name { get; }
		public string LauncherPath { get; }
		public string CommandArguments { get; }
		//public bool IsVR { get; }

		public CryEngineExecutionTarget(string id, string name, string launcherPath)//, bool isVR = false)
		{
			Id = id;
			Name = name;
			LauncherPath = launcherPath;

			/* VR is disabled for now because it would also require it to load the VR plugin.
			IsVR = isVR;

			if (IsVR)
			{
				Id += "_VR";
				Name += " (VR)";
			}

			var vrEnabled = IsVR ? 1 : 0;
			CommandArguments = $"+sys_vr_support={vrEnabled}";*/
		}
	}

	public class CryEngineGameProjectExtension : DotNetProjectExtension
	{
		#region Properties

		public CryEngineParameters CryEngineParameters { get; private set; }
		public TargetRuntime CryEngineRuntime { get; private set; }

		#endregion

		protected override System.Threading.Tasks.Task<TargetEvaluationResult> OnRunTarget(ProgressMonitor monitor, string target, ConfigurationSelector configuration, TargetEvaluationContext context)
		{
			return base.OnRunTarget(monitor, target, configuration, context);
		}

		private ExecutionCommand CreateExecutionCommand()
		{
			return new CryEngineGameExecutionCommand
			{
				CryProjectFile = Project.FileName,
				CryEngineParameters = CryEngineParameters
			};
		}

		protected override void OnEndLoad()
		{
			base.OnEndLoad();

			ApplyParameters();
		}

		protected override ProjectFeatures OnGetSupportedFeatures()
		{
			return base.OnGetSupportedFeatures() | ProjectFeatures.Execute;
		}

		private string GetEngineDirectoryName(CryEngineParameters projectParameters)
		{
			if(projectParameters == null || string.IsNullOrWhiteSpace(projectParameters.LauncherPath))
			{
				return null;
			}

			var binDirectory = Directory.GetParent(projectParameters.LauncherPath);
			if(binDirectory == null || binDirectory.Parent == null || binDirectory.Parent.Parent == null)
			{
				return null;
			}

			var engineDirectory = binDirectory.Parent.Parent;

			return engineDirectory.FullName;
		}

		public void ApplyParameters()
		{
			CryEngineParameters = new CryEngineParameters(Project);

			var engineDirectory = GetEngineDirectoryName(CryEngineParameters);
			if(engineDirectory == null)
			{
				LoggingService.LogError("Unable to set the correct mono-runtime! Make sure the launcher path is set in the project's settings.");
				return;
			}

			var engineRoot = (FilePath)engineDirectory;
			var monoRoot = engineRoot.Combine("bin", "common", "Mono").FullPath;

			if(!Directory.Exists(monoRoot))
			{
				LoggingService.LogError("Unable to locate the mono-runtime at {0}! Make sure the launcher path is set to the correct launcher in the project's settings.", monoRoot);
				return;
			}

			var currentRuntime = Runtime.SystemAssemblyService.CurrentRuntime as MonoTargetRuntime;
			if(currentRuntime != null && ((FilePath)currentRuntime.Prefix).CanonicalPath.CompareTo(monoRoot.CanonicalPath) == 0)
			{
				// The right runtime is already set, so our job is done here.
				return;
			}

			// Apply proper mono runtime for CryEngine
			foreach (TargetRuntime runtime in Runtime.SystemAssemblyService.GetTargetRuntimes())
			{
				var monoRuntime = runtime as MonoTargetRuntime;
				if (monoRuntime == null)
				{
					continue;
				}

				var monoRuntimePath = (FilePath)monoRuntime.Prefix;
				if (monoRuntimePath.CanonicalPath.CompareTo(monoRoot.CanonicalPath) == 0)
				{
					CryEngineRuntime = monoRuntime;
				}
			}

			if (CryEngineRuntime == null)
			{
				var mri = new MonoRuntimeInfo(monoRoot.FullPath);
				CryEngineRuntime = MonoTargetRuntime.RegisterRuntime(mri);
			}

			Runtime.SystemAssemblyService.DefaultRuntime = CryEngineRuntime;

			Project.NotifyModified("RunTime");
		}

		protected override void OnInitializeFromTemplate(ProjectCreateInformation projectCreateInfo, System.Xml.XmlElement template)
		{
			base.OnInitializeFromTemplate(projectCreateInfo, template);

			var engineParameters = new CryEngineParameters(Project);
			engineParameters.LauncherPath = projectCreateInfo.Parameters["BuildLocation"];
			engineParameters.ProjectPath = projectCreateInfo.Parameters["ProjectLocation"];
			           
			engineParameters.Save(Project);

			ApplyParameters();
		}

		protected override bool OnGetCanExecute(ExecutionContext context, ConfigurationSelector configuration, SolutionItemRunConfiguration runConfiguration)
		{
			var cmd = CreateExecutionCommand();

			return context.ExecutionHandler.CanExecute(cmd);
		}

		protected override IEnumerable<ExecutionTarget> OnGetExecutionTargets(ConfigurationSelector configuration)
		{
			var targets = new List<ExecutionTarget>();

			if (CryEngineParameters != null)
			{
				var gameLauncherName = Path.GetFileNameWithoutExtension(CryEngineParameters.LauncherPath);

				targets.Add(new CryEngineExecutionTarget(gameLauncherName, gameLauncherName, CryEngineParameters.LauncherPath));
				//VR is disabled for now
				//targets.Add(new CryEngineExecutionTarget(gameLauncherName, gameLauncherName, CryEngineParameters.LauncherPath, true));

				var directory = Path.GetDirectoryName(CryEngineParameters.LauncherPath);
				var sandboxPath = Path.Combine(directory, "Sandbox.exe");
				if (File.Exists(sandboxPath))
				{
					targets.Add(new CryEngineExecutionTarget("Sandbox", "Sandbox", sandboxPath));
				}

				var serverPath = Path.Combine(directory, "Game_Server.exe");
				if(File.Exists(serverPath))
				{
					targets.Add(new CryEngineExecutionTarget("Server", "Server", serverPath));
				}
			}

			return targets;
		}

		protected async override System.Threading.Tasks.Task OnExecute(ProgressMonitor monitor, ExecutionContext context, ConfigurationSelector configuration, SolutionItemRunConfiguration runConfiguration)
		{
			var cmd = CreateExecutionCommand();
			cmd.Target = context.ExecutionTarget;

			var console = context.ConsoleFactory.CreateConsole(monitor.CancellationToken);
			var operation = context.ExecutionHandler.Execute(cmd, console);

			using (monitor.CancellationToken.Register(operation.Cancel))
			{
				await operation.Task;
			}
		}
	}
}