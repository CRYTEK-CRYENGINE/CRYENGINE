using Mono.Addins;
using Mono.Addins.Description;

[assembly: Addin(
	"MonoDevelop.FileUtilities",
	Namespace = "MonoDevelop.FileUtilities",
	Version = "1.0"
)]

[assembly: AddinName("MonoDevelop.FileUtilities")]
[assembly: AddinCategory("IDE extensions")]
[assembly: AddinDescription("File utilities: read only flag clear")]
[assembly: AddinAuthor("Crytek")]