// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Runtime.InteropServices;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Flavor;
using Microsoft.VisualStudio.Shell.Interop;

namespace CryEngine.Debugger.Mono
{
	internal class CryEngineFlavoredProject : FlavoredProjectBase, IVsProjectFlavorCfgProvider
	{
		private IVsProjectFlavorCfgProvider innerFlavorConfig;
		private CryEngineDebugPackage package;

		public CryEngineFlavoredProject(CryEngineDebugPackage package)
		{
			this.package = package;
		}

		public int CreateProjectFlavorCfg(IVsCfg pBaseProjectCfg, out IVsProjectFlavorCfg ppFlavorCfg)
		{
			IVsProjectFlavorCfg cfg = null;
			ppFlavorCfg = null;

			if (innerFlavorConfig != null)
			{
				GetProperty(VSConstants.VSITEMID_ROOT, (int)__VSHPROPID.VSHPROPID_ExtObject, out object project);

				innerFlavorConfig.CreateProjectFlavorCfg(pBaseProjectCfg, out cfg);
				ppFlavorCfg = new CryEngineDebuggableConfig(cfg, project as EnvDTE.Project);
			}

			if (ppFlavorCfg != null)
				return VSConstants.S_OK;

			return VSConstants.E_FAIL;
		}

		protected override void SetInnerProject(IntPtr innerIUnknown)
		{
			object inner = null;

			inner = Marshal.GetObjectForIUnknown(innerIUnknown);
			innerFlavorConfig = inner as IVsProjectFlavorCfgProvider;

			if (base.serviceProvider == null)
				base.serviceProvider = package;

			base.SetInnerProject(innerIUnknown);
		}

		protected override void Close()
		{
			base.Close();

			LauncherCommands.HideCommands();
		}
	}
}