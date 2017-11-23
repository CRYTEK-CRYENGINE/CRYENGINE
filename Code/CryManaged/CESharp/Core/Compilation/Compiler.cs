using System;
using System.CodeDom.Compiler;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.IO;
using System.Linq;
using System.Reflection;

using CryEngine;
using System.Text.RegularExpressions;

namespace CryEngine.Compilation
{
	internal static class Compiler
	{
		private static List<Assembly> defaultNetFrameworkAssemblies = new List<Assembly>();

		static Compiler()
		{
			var defaultAssemblyPaths = Directory.GetFiles(Engine.GlobalAssemblyCacheDirectory, "*.dll", SearchOption.AllDirectories);
			defaultNetFrameworkAssemblies.Capacity = defaultAssemblyPaths.Length;

			foreach(var assemblyPath in defaultAssemblyPaths)
			{
				defaultNetFrameworkAssemblies.Add(Assembly.ReflectionOnlyLoadFrom(assemblyPath));
			}
		}

		public static Assembly CompileCSharpSourceFiles(string[] sourceFiles, out string compileException)
		{
			Assembly assembly = null;
			compileException = null;
			try
			{
				assembly = CompileSourceFiles(CodeDomProvider.CreateProvider("CSharp"), sourceFiles);
			}
			catch(CompilationFailedException ex)
			{
				compileException = ex.ToString();
			}
			catch(Exception ex)
			{
				throw (ex);
			}
			return assembly;
		}

		public static void CompileAndLoadFromSourceFiles(CodeDomProvider provider, string searchPattern)
		{
			var sourceFiles = Directory.GetFiles(Engine.DataDirectory, searchPattern, SearchOption.AllDirectories);
			if(sourceFiles.Length == 0)
				return;

			CompileSourceFiles(provider, sourceFiles);
		}

		public static Assembly CompileSourceFiles(CodeDomProvider provider, string[] sourceFiles)
		{
			using(provider)
			{
				var compilerParameters = new CompilerParameters();

				compilerParameters.GenerateExecutable = false;

				// Necessary for stack trace line numbers etc
				compilerParameters.IncludeDebugInformation = true;
				compilerParameters.GenerateInMemory = false;

#if RELEASE
				compilerParameters.GenerateInMemory = true;
				compilerParameters.IncludeDebugInformation = false;
#endif

				if(!compilerParameters.GenerateInMemory)
				{
					var scriptsDir = Path.Combine(Directory.GetCurrentDirectory(), "user", "scripts");
					var dllName = "CRYENGINE.CSharp.dll";
					Directory.CreateDirectory(scriptsDir);
					compilerParameters.OutputAssembly = Path.Combine(scriptsDir, dllName);
				}

				AddReferencedAssembliesForSourceFiles(sourceFiles, ref compilerParameters);

				var results = provider.CompileAssemblyFromFile(compilerParameters, sourceFiles);
				if(!results.Errors.HasErrors && results.CompiledAssembly != null)
				{
					return results.CompiledAssembly;
				}

				string compilationError = string.Format("Compilation failed; {0} errors: ", results.Errors.Count);

				foreach(CompilerError error in results.Errors)
				{
					compilationError += Environment.NewLine;

					if(!error.ErrorText.Contains("(Location of the symbol related to previous error)"))
						compilationError += string.Format("{0}({1},{2}): {3} {4}: {5}", error.FileName, error.Line, error.Column, error.IsWarning ? "warning" : "error", error.ErrorNumber, error.ErrorText);
					else
						compilationError += "    " + error.ErrorText;
				}

				throw new CompilationFailedException(compilationError);
			}
		}

		private static void AddReferencedAssembliesForSourceFiles(IEnumerable<string> sourceFilePaths, ref CompilerParameters compilerParameters)
		{
			// Always reference any currently loaded assembly, such CryEngine.Core.dll.
			var currentDomainAssemblies = AppDomain.CurrentDomain.GetAssemblies().Select(x => x.Location).ToArray();
			compilerParameters.ReferencedAssemblies.AddRange(currentDomainAssemblies);

			var usedNamespaces = new HashSet<string>();

			foreach(var sourceFilePath in sourceFilePaths)
			{
				// Open the script
				using(var stream = new FileStream(sourceFilePath, FileMode.Open))
				{
					GetNamespacesFromStream(stream, ref usedNamespaces);
				}
			}

			var unusedAssemblies = defaultNetFrameworkAssemblies;
			var referencedAssemblies = new List<string>();

			var watch = System.Diagnostics.Stopwatch.StartNew();
			foreach(var foundNamespace in usedNamespaces)
			{
				unusedAssemblies.RemoveAll(assembly => assembly.GetTypes().Any(x =>
				{
					referencedAssemblies.Add(assembly.Location);

					return x.Namespace == foundNamespace;
				}));
			}

			// compilerParameters.ReferencedAssemblies.AddRange(referencedAssemblies.ToArray());

			watch.Stop();
			Log.Always("Took {0}", watch.ElapsedMilliseconds);
		}

		private static void GetNamespacesFromStream(Stream stream, ref HashSet<string> namespaces)
		{
			using(var streamReader = new StreamReader(stream))
			{
				string line;

				while((line = streamReader.ReadLine()) != null)
				{
					//Filter for using statements
					var matches = Regex.Matches(line, @"using ([^;]+);");
					foreach(Match match in matches)
					{
						namespaces.Add(match.Groups[1].Value);
					}
				}
			}
		}
	}
}
