using System.IO;
using MonoDevelop.Core.Execution;
using MonoDevelop.CryEngine.Projects;

namespace MonoDevelop.CryEngine.Execution
{
	public class CryEngineGameExecutionHandler : NativePlatformExecutionHandler
	{
		public override bool CanExecute(ExecutionCommand command)
		{
			return command is CryEngineGameExecutionCommand;
		}

		public override ProcessAsyncOperation Execute(ExecutionCommand command, OperationConsole console)
		{
			var ceCmd = (CryEngineGameExecutionCommand)command;
			var ceTarget = ceCmd.Target as CryEngineExecutionTarget;

			var launcherPath = ceTarget != null ? ceTarget.LauncherPath : ceCmd.CryEngineParameters.LauncherPath;
			var workingDirectory = Path.GetDirectoryName(launcherPath);

			var arguments = $"-project {ceCmd.CryEngineParameters.ProjectPath}";
			if (ceCmd.CryEngineParameters.CommandArguments != null)
			{
				arguments += " " + ceCmd.CryEngineParameters.CommandArguments;
			}

			if (ceTarget != null && ceTarget.CommandArguments != null)
			{
				arguments += " " + ceTarget.CommandArguments;
			}

			var nativeCmd = new NativeExecutionCommand(launcherPath, arguments , workingDirectory);

			return base.Execute(nativeCmd, console);
		}
	}
}