// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Runtime.InteropServices;
using Microsoft.VisualStudio.Shell.Flavor;

namespace CryEngine.Debugger.Mono
{
	[ComVisible(false)]
	[Guid(CryEngineDebugPackage.CryEngineProjectGuid)]
	public class CryEngineProjectFactory : FlavoredProjectFactoryBase
	{
		private CryEngineDebugPackage package;

		public CryEngineProjectFactory(CryEngineDebugPackage package)
		{
			this.package = package;
		}

		protected override object PreCreateForOuter(IntPtr outerProjectIUnknown)
		{
			return new CryEngineFlavoredProject(package);
		}
	}
}