using EnvDTE;
using Microsoft.Build.Construction;

namespace CryEngine.Debugger.Mono
{
	public struct CryEngineProjectData
	{
		public string LauncherPath { get; set; }
		public string ProjectPath { get; set; }
		public string CommandArguments { get; set; }

		public static CryEngineProjectData GetProjectData(Project envProject)
		{
			const string launcherKey = "CryEngineLauncherPath";
			const string projectKey = "CryEngineProjectPath";
			const string argsKey = "CryEngineCommandArguments";

			ProjectRootElement root = ProjectRootElement.Open(envProject.FullName);
			var proj = new Microsoft.Build.Evaluation.Project(root);

			var projectData = new CryEngineProjectData
			{
				LauncherPath = proj.GetPropertyValue(launcherKey),
				ProjectPath = proj.GetPropertyValue(projectKey),
				CommandArguments = proj.GetPropertyValue(argsKey)
			};

			Microsoft.Build.Evaluation.ProjectCollection.GlobalProjectCollection.UnloadProject(proj);

			return projectData;
		}
	}

}
