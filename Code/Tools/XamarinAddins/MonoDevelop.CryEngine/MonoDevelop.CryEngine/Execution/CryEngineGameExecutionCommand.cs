using System;
using MonoDevelop.Core.Execution;

namespace MonoDevelop.CryEngine
{
	public class CryEngineGameExecutionCommand : ExecutionCommand
	{
		public string CryProjectFile { get; set; }
		public CryEngineParameters CryEngineParameters { get; set; }
	}
}