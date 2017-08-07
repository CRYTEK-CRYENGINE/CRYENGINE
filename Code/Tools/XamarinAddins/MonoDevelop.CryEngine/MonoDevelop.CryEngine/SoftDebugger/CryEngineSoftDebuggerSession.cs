using System;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using Mono.Debugging.Client;
using Mono.Debugging.Soft;
using Mono.Debugger.Soft;
using MonoDevelop.Core;
using MonoDevelop.CryEngine.Projects;
using System.Threading;
using System.IO;

namespace MonoDevelop.CryEngine.SoftDebugger
{
	public class CryEngineSoftDebuggerSession : SoftDebuggerSession
	{
		private Process _ceProcess;

		public CryEngineSoftDebuggerSession()
		{
			Adaptor.BusyStateChanged += (sender, args) => SetBusyState(args);
			MonoDevelop.Ide.IdeApp.Exiting += (sender, args) => EndSession();
		}

		protected override bool HandleException(Exception ex)
		{
			if (ex is VMNotSuspendedException)
			{
				return false;
			}

			return base.HandleException(ex);
		}

		private static int GetFirstAvailableRandomPort(int startPort, int stopPort)
		{
			var random = new Random();

			IPGlobalProperties ipGlobalProperties = IPGlobalProperties.GetIPGlobalProperties();
			TcpConnectionInformation[] tcpConnInfoArray = ipGlobalProperties.GetActiveTcpConnections();

			var busyPorts = tcpConnInfoArray.Select(t => t.LocalEndPoint.Port).Where(v => v >= startPort && v <= stopPort).ToArray();

			var firstAvailableRandomPort = Enumerable.Range(startPort, stopPort - startPort).OrderBy(v => random.Next()).FirstOrDefault(p => !busyPorts.Contains(p));

			return firstAvailableRandomPort;
		}

		private int Launch(CryEngineSoftDebuggerStartInfo startInfo)
		{
			if (_ceProcess != null)
			{
				throw new InvalidOperationException("CryEngine already running");
			}

			CryEngineParameters engineParameters = startInfo.ExecuteCommand.CryEngineParameters;
			var ceTarget = startInfo.ExecuteCommand.Target as CryEngineExecutionTarget;

			var launcherPath = ceTarget != null ? ceTarget.LauncherPath : engineParameters.LauncherPath;
			var workingDirectory = Path.GetDirectoryName(launcherPath);

			var arguments = $"-project {engineParameters.ProjectPath}";
			if (engineParameters.CommandArguments != null)
			{
				arguments += " " + engineParameters.CommandArguments;
			}

			if (ceTarget != null && ceTarget.CommandArguments != null)
			{
				arguments += " " + ceTarget.CommandArguments;
			}

			var psi = new ProcessStartInfo(launcherPath)
			{
				Arguments = startInfo.Arguments,
				UseShellExecute = false,
				WorkingDirectory = workingDirectory
			};

			// NOTE: Temporarily C# will only work with the project-based approach (rather than the monolithic in-engine-project approach
			psi.Arguments += arguments;

			var instancePort = GetFirstAvailableRandomPort(17615, 18000);
			psi.Arguments += string.Format(" -monoDebuggerEnable -monoDebuggerPort {0}", instancePort);

			_ceProcess = Process.Start(psi);

			_ceProcess.EnableRaisingEvents = true;
			_ceProcess.Exited += (s, e) => EndSession();

			while (string.IsNullOrEmpty(_ceProcess.MainWindowTitle))
			{
				Thread.Sleep(20);
				_ceProcess.Refresh();
			}

			return instancePort;
		}

		private SoftDebuggerStartInfo PrepareInstanceStartInfo(int instancePort)
		{
			return new SoftDebuggerStartInfo(new SoftDebuggerConnectArgs("CryEngine", IPAddress.Loopback, instancePort));
		}

		protected override void OnRun(DebuggerStartInfo startInfo)
		{
			var ceStartInfo = (CryEngineSoftDebuggerStartInfo)startInfo;

			var instancePort = Launch(ceStartInfo);

			var instanceStartInfo = PrepareInstanceStartInfo(instancePort);

			StartConnecting(instanceStartInfo,5000,20);
		}

		protected override string GetConnectingMessage(DebuggerStartInfo dsi)
		{
			Runtime.RunInMainThread(() => Ide.IdeApp.Workbench.CurrentLayout = "Debug");

			return base.GetConnectingMessage(dsi);
		}

		protected override void EndSession()
		{
			try
			{
				Runtime.RunInMainThread(() => Ide.IdeApp.Workbench.CurrentLayout = "Solution");
				EndProcess();

				base.EndSession();
			}
			catch 
			{
			}
		}

		private void EndProcess()
		{
			Detach();

			try
			{
				_ceProcess?.CloseMainWindow();
			}
			catch
			{
			}
			finally
			{
				_ceProcess = null;
			}
		}

		protected override void OnExit()
		{
			try
			{
				EndProcess();
				base.OnExit();
			}
			catch
			{
			}
		}

		protected override void OnDetach()
		{
			try
			{
				//base.OnDetach(); <-- exception! . .therefore, we will just do nothing
			}
			catch (ObjectDisposedException)
			{
			}
			catch (NullReferenceException)
			{
			}
		}
	}
}