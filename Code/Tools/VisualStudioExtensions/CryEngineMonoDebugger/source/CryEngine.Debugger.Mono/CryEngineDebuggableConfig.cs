// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Runtime.InteropServices;
using EnvDTE;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.OLE.Interop;
using Microsoft.VisualStudio.Shell.Interop;
using Mono.Debugging.Soft;
using Mono.Debugging.VisualStudio;

namespace CryEngine.Debugger.Mono
{
	internal class CryEngineDebuggableConfig : IVsDebuggableProjectCfg, IVsProjectFlavorCfg, IVsProjectCfgDebugTargetSelection
	{
		private const string DebugTargetGuidString = "6E87CFAD-6C05-4adf-9CD7-3B7943875B7C";

		private static readonly uint[] DebugTargetCommandIds = new uint[] { 0x02001, 0x02002, 0x02003 };
		private static readonly string[] DebugTargetNames = new string[] { "Debug GameLauncher", "Debug Sandbox", "Debug Server" };
		private static readonly Guid DebugTargetGuid = new Guid(DebugTargetGuidString);

		private static LauncherType _launcherType = LauncherType.GameLauncher;
		private static IVsDebugTargetSelectionService _debugService;

		private IVsProjectFlavorCfg _baseProjectCfg;
		private Project _baseProject;
		
		public CryEngineDebuggableConfig(IVsProjectFlavorCfg pBaseProjectCfg, Project project)
		{
			_baseProject = project;
			_baseProjectCfg = pBaseProjectCfg;
		}

		public int Close()
		{
			if (_baseProjectCfg != null)
			{
				_baseProjectCfg.Close();
				_baseProjectCfg = null;
			}

			return VSConstants.S_OK;
		}

		private int GetAvailablePort()
		{
			const int startPort = 17615;
			const int stopPort = 18000;
			var random = new Random(DateTime.Now.Millisecond);

			IPGlobalProperties ipGlobalProperties = IPGlobalProperties.GetIPGlobalProperties();
			TcpConnectionInformation[] tcpConnInfoArray = ipGlobalProperties.GetActiveTcpConnections();

			var busyPorts = tcpConnInfoArray.Select(t => t.LocalEndPoint.Port).Where(v => v >= startPort && v <= stopPort).ToArray();

			var firstAvailableRandomPort = Enumerable.Range(startPort, stopPort - startPort).OrderBy(v => random.Next()).FirstOrDefault(p => !busyPorts.Contains(p));

			return firstAvailableRandomPort;
		}

		/// <summary>
		/// Called when the application is run with debugging support. Otherwise it will just launch.
		/// </summary>
		/// <param name="grfLaunch"></param>
		/// <returns></returns>
		public int DebugLaunch(uint grfLaunch)
		{
			var port = NetworkHelper.GetAvailablePort(17615, 18000);

			var startArgs = new SoftDebuggerConnectArgs("CRYENGINE", IPAddress.Loopback, port)
			{
				MaxConnectionAttempts = 10
			};

			var startInfo = new CryEngineStartInfo(startArgs, null, _baseProject, CryEngineProjectData.GetProjectData(_baseProject), _launcherType);
			var session = new CryEngineDebuggerSession();

			IDebugLauncher debugLauncher = new CryEngineDebugLauncher();
			var progress = new Progress<string>();
			progress.ProgressChanged += OnProgressChanged;
			var launcher = new MonoDebuggerLauncher(progress, debugLauncher);

			launcher.StartSession(startInfo, session);

			return VSConstants.S_OK;
		}

		private void OnProgressChanged(object sender, string e)
		{
			OutputHandler.LogMessage(string.Format("[MonoInternal] {0}", e));
		}

		public int QueryDebugLaunch(uint grfLaunch, out int pfCanLaunch)
		{
			pfCanLaunch = 1;
			return VSConstants.S_OK;
		}

		public int get_CfgType(ref Guid iidCfg, out IntPtr ppCfg)
		{
			ppCfg = IntPtr.Zero;

			try
			{
				if (iidCfg == typeof(IVsDebuggableProjectCfg).GUID)
					ppCfg = Marshal.GetComInterfaceForObject(this, typeof(IVsDebuggableProjectCfg));
				else if ((ppCfg == IntPtr.Zero) && (this._baseProjectCfg != null))
					return this._baseProjectCfg.get_CfgType(ref iidCfg, out ppCfg);
			}
			catch (InvalidCastException)
			{
			}

			return VSConstants.S_OK;
		}

		public int EnumOutputs(out IVsEnumOutputs ppIVsEnumOutputs)
		{
			throw new NotImplementedException();
		}

		public int get_BuildableProjectCfg(out IVsBuildableProjectCfg ppIVsBuildableProjectCfg)
		{
			throw new NotImplementedException();
		}

		public int get_CanonicalName(out string pbstrCanonicalName)
		{
			throw new NotImplementedException();
		}

		public int get_DisplayName(out string pbstrDisplayName)
		{
			throw new NotImplementedException();
		}

		public int get_IsDebugOnly(out int pfIsDebugOnly)
		{
			throw new NotImplementedException();
		}

		public int get_IsPackaged(out int pfIsPackaged)
		{
			throw new NotImplementedException();
		}

		public int get_IsReleaseOnly(out int pfIsReleaseOnly)
		{
			throw new NotImplementedException();
		}

		public int get_IsSpecifyingOutputSupported(out int pfIsSpecifyingOutputSupported)
		{
			throw new NotImplementedException();
		}

		public int get_Platform(out Guid pguidPlatform)
		{
			throw new NotImplementedException();
		}

		public int get_ProjectCfgProvider(out IVsProjectCfgProvider ppIVsProjectCfgProvider)
		{
			throw new NotImplementedException();
		}

		public int get_RootURL(out string pbstrRootURL)
		{
			throw new NotImplementedException();
		}

		public int get_TargetCodePage(out uint puiTargetCodePage)
		{
			throw new NotImplementedException();
		}

		public int get_UpdateSequenceNumber(ULARGE_INTEGER[] puliUSN)
		{
			throw new NotImplementedException();
		}

		public int OpenOutput(string szOutputCanonicalName, out IVsOutput ppIVsOutput)
		{
			throw new NotImplementedException();
		}

		public bool HasDebugTargets(IVsDebugTargetSelectionService pDebugTargetSelectionService, out Array pbstrSupportedTargetCommandIDs)
		{
			_debugService = pDebugTargetSelectionService;

			var targets = new string[DebugTargetCommandIds.Length];
			for (int i = 0; i < DebugTargetCommandIds.Length; ++i)
			{
				var commandId = DebugTargetCommandIds[i];
				targets[i] = string.Join(":", DebugTargetGuid, commandId);
			}

			pbstrSupportedTargetCommandIDs = targets;
			return true;
		}

		public Array GetDebugTargetListOfType(Guid guidDebugTargetType, uint debugTargetTypeId)
		{
			if(guidDebugTargetType != DebugTargetGuid)
			{
				return new string[0];
			}

			for (int i = 0; i < DebugTargetCommandIds.Length; ++i)
			{
				if(debugTargetTypeId == DebugTargetCommandIds[i])
				{
					return new string[] { DebugTargetNames[i] };
				}
			}
			
			return new string[0];
		}

		public void GetCurrentDebugTarget(out Guid pguidDebugTargetType, out uint pDebugTargetTypeId, out string pbstrCurrentDebugTarget)
		{
			pguidDebugTargetType = DebugTargetGuid;
			pDebugTargetTypeId = DebugTargetCommandIds[(int)_launcherType];
			pbstrCurrentDebugTarget = DebugTargetNames[(int)_launcherType];
		}

		public void SetCurrentDebugTarget(Guid guidDebugTargetType, uint debugTargetTypeId, string bstrCurrentDebugTarget)
		{
			if (guidDebugTargetType != DebugTargetGuid)
			{
				return;
			}

			for (int i = 0; i < DebugTargetCommandIds.Length; ++i)
			{
				if(debugTargetTypeId == DebugTargetCommandIds[i])
				{
					_launcherType = (LauncherType)i;
					_debugService?.UpdateDebugTargets();
					break;
				}
			}
		}

		public static void SetCurrentLauncherTarget(LauncherType launcherType)
		{
			_launcherType = launcherType;
			_debugService?.UpdateDebugTargets();
		}
	}
}