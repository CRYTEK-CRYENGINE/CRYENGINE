// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;
using System;
using System.Diagnostics;

namespace CryEngine
{
	/// <summary>
	/// Static wrapper around ILog (gEnv->pLog)
	/// </summary>
	public static class Log
	{
		/// <summary>
		/// The path of the file where the log is saved to.
		/// </summary>
		public static string FileName
		{
			get
			{
				if (Global.gEnv == null || Global.gEnv.pLog == null)
					return null;
				return Global.gEnv.pLog.GetFileName();
			}
			set
			{
				if (Global.gEnv == null || Global.gEnv.pLog == null)
					return;

				Global.gEnv.pLog.SetFileName(value);
			}
		}

		/// <summary>
		/// If true, the debugger will break when an exception is thrown.
		/// </summary>
		public static bool BreakOnException { get; set; }

		static Log()
		{
			BreakOnException = false;
		}

		/// <summary>
		/// Log an info message to the console and log.
		/// </summary>
		/// <param name="msg"></param>
		public static void Info(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || string.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.Log("[Mono] " + msg);
		}

		/// <summary>
		/// Log a formatted info message to the console and log. 
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void Info(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				Info(format);
			else
				Info(string.Format(format, args));
		}

		/// <summary>
		/// Log an info message to the console and log, and prefix it with the type name.
		/// Example: <c>[Mono] [Log] My info message.</c>
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="msg"></param>
		public static void Info<T>(string msg)
		{
			Info("[" + typeof(T).Name + "] " + msg);
		}

		/// <summary>
		/// Log a formatted info message to the console and log, and prefix it with the type name.
		/// Example: <c>[Mono] [Log] My info message.</c>
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void Info<T>(string format, params object[] args)
		{
			Info("[" + typeof(T).Name + "] " + format, args);
		}

		/// <summary>
		/// Log a message to the console and log that is always printed.
		/// </summary>
		/// <param name="msg"></param>
		public static void Always(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || string.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.LogAlways("[Mono] " + msg);
		}

		/// <summary>
		/// Log a formatted message to the console and log that is always printed.
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void Always(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				Always(format);
			else
				Always(string.Format(format, args));
		}

		/// <summary>
		/// Log a message to the console and log that is always printed, and prefix it with the type name.
		/// Example: <c>[Mono] [Log] My message.</c>
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="msg"></param>
		public static void Always<T>(string msg)
		{
			Always("[" + typeof(T).Name + "] " + msg);
		}

		/// <summary>
		/// Log a formatted message to the console and log that is always printed, and prefix it with the type name.
		/// Example: <c>[Mono] [Log] My message.</c>
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void Always<T>(string format, params object[] args)
		{
			Always("[" + typeof(T).Name + "] " + format, args);
		}

		/// <summary>
		/// Log a warning message to the console and log.
		/// </summary>
		/// <param name="msg"></param>
		public static void Warning(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || string.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.LogWarning("[Mono] " + msg);
		}

		/// <summary>
		/// Log a formatted warning message to the console and log.
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void Warning(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				Warning(format);
			else
				Warning(string.Format(format, args));
		}

		/// <summary>
		/// Log a warning message to the console and log, and prefix it with the type name.
		/// Example: <c>[Mono] [Log] My warning message.</c>
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="msg"></param>
		public static void Warning<T>(string msg)
		{
			Warning("[" + typeof(T).Name + "] " + msg);
		}

		/// <summary>
		/// Log a formatted warning message to the console and log, and prefix it with the type name.
		/// Example: <c>[Mono] [Log] My warning message.</c>
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void Warning<T>(string format, params object[] args)
		{
			Warning("[" + typeof(T).Name + "] " + format, args);
		}

		/// <summary>
		/// Log an error message to the console and log.
		/// </summary>
		/// <param name="msg"></param>
		public static void Error(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || string.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.LogError("[Mono] " + msg);
		}

		/// <summary>
		/// Log a formatted error message to the console and log.
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void Error(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				Error(format);
			else
				Error(string.Format(format, args));
		}

		/// <summary>
		/// Log an error message to the console and log, and prefix it with the type name.
		/// Example: <c>[Mono] [Log] My error message.</c>
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="msg"></param>
		public static void Error<T>(string msg)
		{
			Error("[" + typeof(T).Name + "] " + msg);
		}

		/// <summary>
		/// Log a formatted error message to the console and log, and prefix it with the type name.
		/// Example: <c>[Mono] [Log] My error message.</c>
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void Error<T>(string format, params object[] args)
		{
			Error("[" + typeof(T).Name + "] " + format, args);
		}

		/// <summary>
		/// Log a message only to the console.
		/// </summary>
		/// <param name="msg"></param>
		public static void ToConsole(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || string.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.LogToConsole("[Mono] " + msg);
		}

		/// <summary>
		/// Log a formatted message only to the console.
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void ToConsole(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				ToConsole(format);
			else
				ToConsole(string.Format(format, args));
		}

		/// <summary>
		/// Log a message only to the console and prefix it with the name of the type.
		/// Example: <c>[Mono] [Log] My console message.</c>
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="msg"></param>
		public static void ToConsole<T>(string msg)
		{
			ToConsole("[" + typeof(T).Name + "] " + msg);
		}

		/// <summary>
		/// Log a formatted message only to the console and prefix it with the name of the type.
		/// Example: <c>[Mono] [Log] My console message.</c>
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void ToConsole<T>(string format, params object[] args)
		{
			ToConsole("[" + typeof(T).Name + "] " + format, args);
		}

		/// <summary>
		/// Log a message only to the log file.
		/// </summary>
		/// <param name="msg"></param>
		public static void ToFile(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || string.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.LogToFile("[Mono] " + msg);
		}

		/// <summary>
		/// Log a formatted message only to the log file.
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void ToFile(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				ToFile(format);
			else
				ToFile(string.Format(format, args));
		}

		/// <summary>
		/// Log a message only to the log file and prefix it with the name of the type.
		/// Example: <c>[Mono] [Log] My log file message.</c>
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="msg"></param>
		public static void ToFile<T>(string msg)
		{
			ToFile("[" + typeof(T).Name + "] " + msg);
		}

		/// <summary>
		/// Log a formatted message only to the log file and prefix it with the name of the type.
		/// Example: <c>[Mono] [Log] My log file message.</c>
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void ToFile<T>(string format, params object[] args)
		{
			ToFile("[" + typeof(T).Name + "] " + format, args);
		}

		/// <summary>
		/// Log an exception to the console and log.
		/// </summary>
		/// <param name="ex"></param>
		public static void Exception(Exception ex)
		{
			Exception(ex, null);
		}

		/// <summary>
		/// Log an exception to the console and log with a formatted custom error message.
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <param name="ex"></param>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void Exception(Exception ex, string format, params object[] args)
		{
			Exception(ex, string.Format(format, args));
		}

		/// <summary>
		/// Log an exception to the console and log with a custom error message.
		/// </summary>
		/// <param name="ex"></param>
		/// <param name="customError"></param>
		public static void Exception(Exception ex, string customError)
		{
			if (customError != null)
			{
				Error("Error: " + customError);
			}
			Error("Message: " + ex.Message);
			var st = new StackTrace(ex, true);
			Error("Stack Trace: " + st.FrameCount);
			for (int i = 0; i < st.FrameCount; i++)
			{
				Error("  {0,3}) {1}::{2}() [{3}:{4}]", i, st.GetFrame(i).GetMethod().DeclaringType.FullName, st.GetFrame(i).GetMethod().Name, st.GetFrame(i).GetFileLineNumber(), st.GetFrame(i).GetFileName());
				System.Threading.Thread.Sleep(1);
			}

			if (BreakOnException)
				throw ex;
		}

		/// <summary>
		/// Log an exception to the console and log and prefix it with the name of the type.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="ex"></param>
		public static void Exception<T>(Exception ex)
		{
			Exception<T>(ex, null);
		}

		/// <summary>
		/// Log an exception to the console and log with a formatted custom error message and prefix it with the name of the type.
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="ex"></param>
		/// <param name="format"></param>
		/// <param name="args"></param>
		public static void Exception<T>(Exception ex, string format, params object[] args)
		{
			Exception<T>(ex, string.Format(format, args));
		}

		/// <summary>
		/// Log an exception to the console and log with a custom error message and prefix it with the name of the type.
		/// For more information on formatting refer to <see cref="string.Format(string, object[])"/>.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="ex"></param>
		/// <param name="customError"></param>
		public static void Exception<T>(Exception ex, string customError = null)
		{
			if (customError != null)
			{
				Error<T>("Error: " + customError);
			}
			Error<T>("Message: " + ex.Message);
			var st = new StackTrace(ex, true);
			Error<T>("Stack Trace: " + st.FrameCount);
			for (int i = 0; i < st.FrameCount; i++)
			{
				Error<T>("  {0,3}) {1}::{2}() [{3}:{4}]", i, st.GetFrame(i).GetMethod().DeclaringType.FullName, st.GetFrame(i).GetMethod().Name, st.GetFrame(i).GetFileLineNumber(), st.GetFrame(i).GetFileName());
				System.Threading.Thread.Sleep(1);
			}

			if (BreakOnException)
				throw ex;
		}

		/// <summary>
		/// Log current stack trace
		/// </summary>
		public static void StackTrace()
		{
			Always(Environment.StackTrace);
		}
	}
}
