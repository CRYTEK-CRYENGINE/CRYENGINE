using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

// General Information about an assembly is controlled through the following 
// set of attributes. Change these attribute values to modify the information
// associated with an assembly.
[assembly: AssemblyTitle("CRYENGINE C# Core")]
[assembly: AssemblyDescription("The main C# interface for the CRYENGINE.")]
[assembly: AssemblyConfiguration("")]
[assembly: AssemblyCompany("Crytek GmbH")]
[assembly: AssemblyProduct("CryEngine.Properties")]
[assembly: AssemblyCopyright("Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.")]
[assembly: AssemblyTrademark("")]
[assembly: AssemblyCulture("")]

// Setting ComVisible to false makes the types in this assembly not visible 
// to COM components.  If you need to access a type in this assembly from 
// COM, set the ComVisible attribute to true on that type.
[assembly: ComVisible(false)]

// The following GUID is for the ID of the typelib if this project is exposed to COM
[assembly: Guid("eac3faf3-2c18-4572-98f1-bb48b90d5512")]

// Version information for an assembly consists of the following four values:
//
//      Major Version
//      Minor Version 
//      Build Number
//      Revision
//
[assembly: AssemblyVersion("5.5.0.0")]
[assembly: AssemblyFileVersion("5.5.0.0")]

// Expose the internal methods to the test assembly.
[assembly: InternalsVisibleTo("CryEngine.Core.Tests")]
