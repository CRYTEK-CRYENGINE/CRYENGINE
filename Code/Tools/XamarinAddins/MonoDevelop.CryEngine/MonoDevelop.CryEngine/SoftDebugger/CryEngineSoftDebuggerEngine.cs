using Mono.Debugging.Client;
using MonoDevelop.Core.Execution;
using MonoDevelop.Debugger;

namespace MonoDevelop.CryEngine.SoftDebugger
{
	public class CryEngineSoftDebuggerEngine : DebuggerEngineBackend
	{
		public override bool CanDebugCommand(ExecutionCommand cmd)
		{
			return cmd is CryEngineGameExecutionCommand;
		}

		public override DebuggerStartInfo CreateDebuggerStartInfo(ExecutionCommand cmd)
		{
			var executeCmd = (CryEngineGameExecutionCommand)cmd;

			return new CryEngineSoftDebuggerStartInfo(executeCmd);
		}

		public override DebuggerSession CreateSession()
		{
			return new CryEngineSoftDebuggerSession();
		}
	}
}