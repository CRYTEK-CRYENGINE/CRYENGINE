using Mono.Addins;
using Mono.Addins.Description;

[assembly:Addin (
	"MonoDevelop.CryEngine", 
	Namespace = "MonoDevelop.CryEngine",
	Version = "1.0"
)]

[assembly:AddinName ("CryEngine MonoDevelop addin")]
[assembly:AddinCategory ("Game Development")]
[assembly:AddinDescription ("MonoDevelop.CryEngine")]
[assembly:AddinAuthor ("Crytek")]

[assembly: AddinDependency("::MonoDevelop.Core", MonoDevelop.BuildInfo.Version)]
[assembly: AddinDependency("::MonoDevelop.Ide", MonoDevelop.BuildInfo.Version)]
[assembly: AddinDependency("::MonoDevelop.Debugger", MonoDevelop.BuildInfo.Version)]