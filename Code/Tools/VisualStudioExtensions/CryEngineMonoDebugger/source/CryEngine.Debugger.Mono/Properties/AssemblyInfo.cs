using System.Reflection;
using System.Runtime.InteropServices;

[assembly: AssemblyTitle("CryEngine.Debugger.Mono")]
[assembly: AssemblyProduct("CryEngine.Debugger.Mono")]
[assembly: AssemblyDescription("Extension that enables debugging CRYENGINE C# projects.")]
#if DEBUG
[assembly: AssemblyConfiguration("Debug")]
#else
[assembly: AssemblyConfiguration("Release")]
#endif
[assembly: AssemblyCompany("Crytek GmbH")]
[assembly: AssemblyCopyright("Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.")]
[assembly: AssemblyTrademark("")]
[assembly: AssemblyCulture("")]

[assembly: ComVisible(false)]
[assembly: AssemblyVersion("5.5.0.1")]
[assembly: AssemblyFileVersion("5.5.0.1")]
