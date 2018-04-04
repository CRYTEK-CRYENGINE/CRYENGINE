using System.Net;
using Mono.Debugging.Soft;

namespace MonoDevelop.CryEngine.SoftDebugger
{
	public class CryEngineSoftDebuggerStartInfo : SoftDebuggerStartInfo
	{
		public CryEngineGameExecutionCommand ExecuteCommand { get; }

		public CryEngineSoftDebuggerStartInfo(CryEngineGameExecutionCommand cmd)
			: base(new SoftDebuggerConnectArgs("CryEngine.Launcher", IPAddress.Loopback,17615))
		{
			ExecuteCommand = cmd;
		}
	}
}