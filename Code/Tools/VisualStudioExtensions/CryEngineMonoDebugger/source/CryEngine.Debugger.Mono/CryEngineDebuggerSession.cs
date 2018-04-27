// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Diagnostics;
using System.IO;
using Mono.Debugging.Client;
using Mono.Debugging.Soft;

// Process is ambigious between System.Diagnostics and EnvDTE, but we only need one.
using Process = System.Diagnostics.Process;

namespace CryEngine.Debugger.Mono
{
	public enum LauncherType
	{
		GameLauncher,
		Sandbox,
		Server
	}

	public class CryEngineDebuggerSession : SoftDebuggerSession
	{
		private Process _process;
		private RemoteConsole _remoteConsole;

		private string GetLauncherPath(string gameLauncherPath, LauncherType launcher)
		{
			var dir = Path.GetDirectoryName(gameLauncherPath);
			switch (launcher)
			{
				case LauncherType.GameLauncher:
					return Path.Combine(dir, "GameLauncher.exe");
				case LauncherType.Sandbox:
					return Path.Combine(dir, "Sandbox.exe");
				case LauncherType.Server:
					return Path.Combine(dir, "Game_Server.exe");
			}
			return gameLauncherPath;
		}

		protected override void OnRun(DebuggerStartInfo startInfo)
		{
			base.OnRun(startInfo);
			UseOperationThread = true;

			var cryStartInfo = startInfo as CryEngineStartInfo;
			if (cryStartInfo == null)
			{
				throw new ArgumentException(string.Format("The value of {0} is not a valid type of '{1}'.", nameof(startInfo), typeof(CryEngineStartInfo).Name));
			}

			var listenArgs = cryStartInfo.StartArgs as SoftDebuggerConnectArgs;
			var projectData = cryStartInfo.ProjectData;

			var launcherPath = GetLauncherPath(projectData.LauncherPath, cryStartInfo.LauncherType);

			if (!File.Exists(launcherPath))
			{
				throw new InvalidOperationException(string.Format("Did not find a valid launcher at expected location: {0}", launcherPath));
			}

			var remoteConsolePort = NetworkHelper.GetAvailablePort(4600, 4610);

			var arguments = string.Format("-project {0} -monoDebuggerPort {1} {2} -monoSuspend 1 -remoteConsolePort {3}", 
				projectData.ProjectPath, listenArgs.DebugPort, projectData.CommandArguments, remoteConsolePort);

			var processStartInfo = new ProcessStartInfo(launcherPath, arguments)
			{
				UseShellExecute = true
			};

			_process = new Process
			{
				StartInfo = processStartInfo
			};

			_process.Exited += OnProcessExitedExternally;

			_process.Start();

			StartRemoteConsole(remoteConsolePort);
		}

		private void StartRemoteConsole(int port)
		{
			StopRemoteConsole();

			_remoteConsole = new RemoteConsole(port);
			_remoteConsole.SetListener(new CryEngineConsoleListener());
			_remoteConsole.Start();
		}

		private void StopRemoteConsole()
		{
			if (_remoteConsole != null)
			{
				_remoteConsole.Stop();
				_remoteConsole.Dispose();
				_remoteConsole = null;
			}
		}

		private void OnProcessExitedExternally(object sender, EventArgs e)
		{
			StopRemoteConsole();
			Exit();
		}

		protected override void OnExit()
		{
			StopRemoteConsole();
			if (_process != null && !_process.HasExited)
			{
				_process.Kill();
			}
		}
	}
}