// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell.Interop;

namespace CryEngine.Debugger.Mono
{
	public static class OutputHandler
	{
		private static CryEngineDebugPackage _package;
		private static IVsOutputWindowPane _pane;

		private static IVsOutputWindowPane Pane
		{
			get
			{
				if(_pane == null)
				{
					_pane = _package.GetOutputPane(VSConstants.OutputWindowPaneGuid.DebugPane_guid, "Output");
				}
				return _pane;
			}
		}

		internal static void Initialize(CryEngineDebugPackage package)
		{
			_package = package;
		}

		internal static void LogMessage(object message)
		{
			Pane?.OutputStringThreadSafe(string.Format("{0}\n", message.ToString()));
		}
	}
}
