using MonoDevelop.Projects;

namespace MonoDevelop.CryEngine
{
	public class CryEngineParameters
	{
		private const string LauncherPathKey = "CryEngineLauncherPath";
		private const string ProjectPathKey = "CryEngineProjectPath";
		private const string CommandArgumentsKey = "CryEngineCommandArguments";

		public string LauncherPath { get; set; }

		public string ProjectPath { get; set; }

		public string CommandArguments { get; set; }

		public CryEngineParameters(DotNetProject project)
		{
			LauncherPath = project.ProjectProperties.GetValue(LauncherPathKey);
			ProjectPath = project.ProjectProperties.GetValue(ProjectPathKey);
			CommandArguments = project.ProjectProperties.GetValue(CommandArgumentsKey);
		}

		public void Save(DotNetProject project)
		{
			project.ProjectProperties.SetValue(LauncherPathKey, LauncherPath);
			project.ProjectProperties.SetValue(ProjectPathKey, ProjectPath);
			project.ProjectProperties.SetValue(CommandArgumentsKey, CommandArguments);
		}
	}
}