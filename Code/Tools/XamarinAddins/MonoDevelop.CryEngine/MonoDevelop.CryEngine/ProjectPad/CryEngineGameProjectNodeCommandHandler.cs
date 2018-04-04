using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using MonoDevelop.Components.Commands;
using MonoDevelop.CryEngine.Projects;
using MonoDevelop.Ide.Gui.Components;
using MonoDevelop.Projects;
using MonoDevelop.Core;

namespace MonoDevelop.CryEngine.ProjectPad
{
	public class CryEngineGameProjectNodeCommandHandler : NodeCommandHandler
	{
		/// <summary>
		/// Class that copies the layout of the cryproject files for easier deserializing.
		/// </summary>
		public class CryProjectFile
		{
			public class Content
			{
				public List<string> assets;
				public List<string> code;
			}
			public Content content;
		}

		private CryEngineGameProjectExtension GetProject()
		{
			var project = CurrentNode.DataItem as DotNetProject;
			if(project == null)
			{
				return null;
			}

			return project.GetFlavor<CryEngineGameProjectExtension>();
		}

		private void OpenExplorer(string directoryName)
		{
			Process.Start("explorer.exe", directoryName);
		}

		private string GetEngineDirectoryName()
		{
			var project = GetProject();
			if(project == null || project.CryEngineParameters == null)
			{
				return null;
			}

			var binDirectory = Directory.GetParent(project.CryEngineParameters.LauncherPath);
			if(binDirectory == null || binDirectory.Parent == null || binDirectory.Parent.Parent == null)
			{
				return null;
			}

			var engineDirectory = binDirectory.Parent.Parent;

			return engineDirectory.FullName;
		}

		private string GetProjectPath()
		{
			var project = GetProject();
			if(project == null || project.CryEngineParameters == null)
			{
				return null;
			}

			return project.CryEngineParameters.ProjectPath;
		}

		private string GetProjectDirectoryName()
		{
			var project = GetProject();
			if(project == null || project.CryEngineParameters == null)
			{
				return null;
			}

			return Path.GetDirectoryName(project.CryEngineParameters.ProjectPath);
		}

		private string GetAssetPath()
		{
			var projectPath = GetProjectPath();
			if(projectPath == null || !File.Exists(projectPath))
			{
				return null;
			}

			var projectData = File.ReadAllText(projectPath);
			var cryProject = Newtonsoft.Json.JsonConvert.DeserializeObject<CryProjectFile>(projectData);


			var assets = cryProject?.content?.assets;
			if(assets == null || assets.Count == 0)
			{
				return null;
			}
			return Path.Combine(Path.GetDirectoryName(projectPath), assets[0]);
		}

		[CommandHandler(Commands.OpenProjectFolder)]
		public void OnOpenProjectFolder()
		{
			var projectDirectoryName = GetProjectDirectoryName();
			if(string.IsNullOrWhiteSpace(projectDirectoryName))
			{
				LoggingService.LogWarning("Unable to locate the project folder! Make sure the project path is set in project's options.");
				return;
			}

			OpenExplorer(projectDirectoryName);
		}

		[CommandHandler(Commands.OpenEngineFolder)]
		public void OnOpenEngineFolder()
		{
			var engineDirectoryName = GetEngineDirectoryName();
			if(string.IsNullOrWhiteSpace(engineDirectoryName))
			{
				LoggingService.LogWarning("Unable to locate the engine folder! Make sure the launcher path is set in project's options.");
				return;
			}

			OpenExplorer(engineDirectoryName);
		}

		[CommandHandler(Commands.CompileResources)]
		public void CompileResources()
		{
			var engineDirectoryName = GetEngineDirectoryName();
			var projectDirectoryName = GetProjectDirectoryName();
			if(engineDirectoryName != null && projectDirectoryName != null)
			{
				var rcPath = Path.Combine(engineDirectoryName, "Tools", "rc", "rc.exe");
				if(!File.Exists(rcPath))
				{
					LoggingService.LogWarning("Unable to locate the resource compiler at {0}! Make sure the launcher path is set in project's options.", rcPath);
					return;
				}

				var sourceRoot = GetAssetPath();
				if(string.IsNullOrWhiteSpace(sourceRoot))
				{
					LoggingService.LogWarning("Unable to locate the assets folder! Make sure the project path is set in project's options.");
					return;
				}

				var arguments = $"\"*.*\" /threads=8 /sourceRoot={sourceRoot}";

				Process.Start(rcPath, arguments);
			}
		}
	}
}