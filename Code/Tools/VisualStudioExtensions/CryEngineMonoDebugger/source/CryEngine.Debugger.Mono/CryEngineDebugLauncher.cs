// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.IO;
using System.Runtime.Remoting;
using System.Runtime.Serialization.Formatters.Binary;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Mono.Debugging.Soft;
using Mono.Debugging.VisualStudio;

namespace CryEngine.Debugger.Mono
{
	class CryEngineDebugLauncher : IDebugLauncher
	{
		public bool StartDebugger(SoftDebuggerSession session, StartInfo startInfo)
		{
			IVsDebugger4 service = ServiceProvider.GlobalProvider.GetService(typeof(SVsShellDebugger)) as IVsDebugger4;
			if (service == null)
			{
				return false;
			}

			SessionMarshalling sessionMarshalling = new SessionMarshalling(session, startInfo);
			VsDebugTargetInfo4 debugTargetInfo4 = new VsDebugTargetInfo4
			{
				dlo = 1U,
				bstrExe = string.Format("CRYENGINE - {0}", startInfo.StartupProject.Name),
				bstrCurDir = "",
				bstrArg = null,
				bstrRemoteMachine = null,
				fSendToOutputWindow = 0,
				guidPortSupplier = Guids.PortSupplierGuid,
				guidLaunchDebugEngine = Guids.EngineGuid,
				bstrPortName = "Mono"
			};

			using (MemoryStream memoryStream = new MemoryStream())
			{
				BinaryFormatter binaryFormatter = new BinaryFormatter();
				ObjRef objRef = RemotingServices.Marshal(sessionMarshalling);
				binaryFormatter.Serialize(memoryStream, objRef);
				debugTargetInfo4.bstrOptions = Convert.ToBase64String(memoryStream.ToArray());
			}

			try
			{
				VsDebugTargetProcessInfo[] pLaunchResults = new VsDebugTargetProcessInfo[1];
				service.LaunchDebugTargets4(1U, new VsDebugTargetInfo4[1]{ debugTargetInfo4 }, pLaunchResults);
				return true;
			}
			catch
			{
				throw;
			}
		}
	}
}
