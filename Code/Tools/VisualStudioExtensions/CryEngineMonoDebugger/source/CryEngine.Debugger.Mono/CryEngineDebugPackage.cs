// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Runtime.InteropServices;
using Microsoft.VisualStudio.Shell;
using Mono.Debugging.Client;

namespace CryEngine.Debugger.Mono
{
	[PackageRegistration(UseManagedResourcesOnly = true)]
	[InstalledProductRegistration("#110", "#112", "1.0", IconResourceID = 400)] // Info on this package for Help/About
	[Guid(PackageGuidString)]
	[ProvideProjectFactory(typeof(CryEngineProjectFactory), "CryEngine.Debugger.Mono", null, "csproj", "csproj", "", LanguageVsTemplate = "CSharp")]
	[SuppressMessage("StyleCop.CSharp.DocumentationRules", "SA1650:ElementDocumentationMustBeSpelledCorrectly", Justification = "pkgdef, VS and vsixmanifest are valid VS terms")]
	[ProvideMenuResource("Menus.ctmenu", 1)]
	public sealed class CryEngineDebugPackage : Package
	{
		public const string PackageGuidString = "5E1711F6-3C97-41B2-96F0-4F05BE71EB19";
		public const string CryEngineProjectGuid = "E7562513-36BA-4D11-A927-975E5375ED10";

		protected override void Initialize()
		{
			base.Initialize();

			RegisterProjectFactory(new CryEngineProjectFactory(this));

			OutputHandler.Initialize(this);
			if(System.Diagnostics.Debugger.IsAttached)
			{
				DebuggerLoggingService.CustomLogger = new DebugOutputLogger();
			}
			else
			{
				DebuggerLoggingService.CustomLogger = new FileLogger();
			}

			LauncherCommands.Initialize(this);
		}
	}

	class NullLogger : ICustomLogger
	{
		public string GetNewDebuggerLogFilename() => null;

		public void LogAndShowException(string message, Exception ex) { }

		public void LogError(string message, Exception ex) { }

		public void LogMessage(string messageFormat, params object[] args) { }
	}

	public class DebugOutputLogger : ICustomLogger
	{
		public string GetNewDebuggerLogFilename() => null;
		
		public void LogAndShowException(string message, Exception ex)
		{
			Debug.WriteLine(string.Format("[CryEngine.Mono.Debugger] [Exception] {0}", message));
			Debug.WriteLine(ex.ToString());
			Debug.WriteLine(ex.StackTrace);
		}

		public void LogError(string message, Exception ex)
		{
			Debug.WriteLine(string.Format("[CryEngine.Mono.Debugger] [Error] {0}", message));
			Debug.WriteLine(ex.ToString());
			Debug.WriteLine(ex.StackTrace);
		}

		public void LogMessage(string messageFormat, params object[] args)
		{
			var message = string.Format(messageFormat, args);
			Debug.WriteLine(string.Format("[CryEngine.Mono.Debugger] {0}", message));
		}
	}

	public class FileLogger : ICustomLogger
	{
		const string LogFilename = "log.txt";

		private bool _initialized = false;

		public string GetNewDebuggerLogFilename() => LogFilename;

		public void LogAndShowException(string message, Exception ex)
		{
			Debug.WriteLine(string.Format("Writing log message to: {0}", Path.Combine(Directory.GetCurrentDirectory(), LogFilename)));
			string text = string.Format("[CryEngine.Mono.Debugger] [Exception] {0}{3}{1}{3}{2}", message, ex.ToString(), ex.StackTrace, Environment.NewLine);
			WriteText(text);
		}

		public void LogError(string message, Exception ex)
		{
			Debug.WriteLine(string.Format("Writing log message to: {0}", Path.Combine(Directory.GetCurrentDirectory(), LogFilename)));
			string text = string.Format("[CryEngine.Mono.Debugger] [Error] {0}{3}{1}{3}{2}", message, ex.ToString(), ex.StackTrace, Environment.NewLine);
			WriteText(text);
		}

		public void LogMessage(string messageFormat, params object[] args)
		{
			Debug.WriteLine(string.Format("Writing log message to: {0}", Path.Combine(Directory.GetCurrentDirectory(), LogFilename)));
			var message = string.Format(messageFormat, args);
			WriteText(string.Format("[CryEngine.Mono.Debugger] {0}", message));
		}

		private void WriteText(string text)
		{
			if(!_initialized)
			{
				Debug.WriteLine(string.Format("Writing log message to: {0}", Path.Combine(Directory.GetCurrentDirectory(), LogFilename)));
				_initialized = true;
			}
			
			using(var fileStream = File.AppendText(LogFilename))
			{
				fileStream.Write(string.Format("{0}{1}", text, Environment.NewLine));
			}
		}
	}
}