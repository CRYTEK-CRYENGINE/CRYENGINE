// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;
using System.Diagnostics;

namespace CryEngine
{
	/// <summary>
	/// Static wrapper around ILog (gEnv->pLog)
	/// </summary>
	public static class Log
	{
		public static string FileName
		{
			get
			{
				if (Global.gEnv == null || Global.gEnv.pLog == null)
					return null;
				return Global.gEnv.pLog.GetFileName ();
			}
			set
			{
				if (Global.gEnv == null || Global.gEnv.pLog == null)
					return;

				Global.gEnv.pLog.SetFileName (value);
			}
		}

		public static bool BreakOnException { get; set; }

		static Log()
		{
			BreakOnException = false;
		}
		
		public static void Info(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || String.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.Log ("[Mono] " + msg);
		}

		public static void Info(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				Info (format);
			else
				Info(String.Format(format, args));
		}

		public static void Info<T>(string msg)
		{
			Info ("[" + typeof(T).Name + "] " + msg);
		}

		public static void Info<T>(string format, params object[] args)
		{
			Info ("[" + typeof(T).Name + "] " + format, args);
		}

		public static void Always(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || String.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.LogAlways ("[Mono] " + msg);
		}

		public static void Always(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				Always (format);
			else
				Always (String.Format (format, args));
		}

		public static void Always<T>(string msg)
		{
			Always ("[" + typeof(T).Name + "] " + msg);
		}

		public static void Always<T>(string format, params object[] args)
		{
			Always ("[" + typeof(T).Name + "] " + format, args);
		}

		public static void Warning(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || String.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.LogWarning ("[Mono] " + msg);
		}

		public static void Warning(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				Warning (format);
			else
				Warning (String.Format (format, args));
		}

		public static void Warning<T>(string msg)
		{
			Warning ("[" + typeof(T).Name + "] " + msg);
		}

		public static void Warning<T>(string format, params object[] args)
		{
			Warning ("[" + typeof(T).Name + "] " + format, args);
		}

		public static void Error(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || String.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.LogError ("[Mono] " + msg);
		}

		public static void Error(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				Error (format);
			else
				Error (String.Format (format, args));
		}

		public static void Error<T>(string msg)
		{
			Error ("[" + typeof(T).Name + "] " + msg);
		}

		public static void Error<T>(string format, params object[] args)
		{
			Error ("[" + typeof(T).Name + "] " + format, args);
		}

		public static void Plus(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || String.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.LogPlus ("[Mono] " + msg);			
		}

		public static void Plus(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				Plus (format);
			else
				Plus (String.Format (format, args));
		}

		public static void Plus<T>(string msg)
		{
			Plus ("[" + typeof(T).Name + "] " + msg);
		}

		public static void Plus<T>(string format, params object[] args)
		{
			Plus ("[" + typeof(T).Name + "] " + format, args);
		}

		public static void ToConsole(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || String.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.LogToConsole ("[Mono] " + msg);
		}

		public static void ToConsole(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				ToConsole (format);
			else
				ToConsole (String.Format (format, args));
		}

		public static void ToConsole<T>(string msg)
		{
			ToConsole ("[" + typeof(T).Name + "] " + msg);
		}

		public static void ToConsole<T>(string format, params object[] args)
		{
			ToConsole ("[" + typeof(T).Name + "] " + format, args);
		}

		public static void ToConsolePlus(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || String.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.LogToConsolePlus ("[Mono] " + msg);
		}

		public static void ToConsolePlus(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				ToConsolePlus (format);
			else
				ToConsolePlus(String.Format(format, args));
		}

		public static void ToConsolePlus<T>(string msg)
		{
			ToConsolePlus ("[" + typeof(T).Name + "] " + msg);
		}

		public static void ToConsolePlus<T>(string format, params object[] args)
		{
			ToConsolePlus ("[" + typeof(T).Name + "] " + format, args);
		}

		public static void ToFile(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || String.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.LogToFile ("[Mono] " + msg);
		}

		public static void ToFile(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				ToFile (format);
			else
				ToFile(String.Format(format, args));
		}

		public static void ToFile<T>(string msg)
		{
			ToFile ("[" + typeof(T).Name + "] " + msg);
		}

		public static void ToFile<T>(string format, params object[] args)
		{
			ToFile ("[" + typeof(T).Name + "] " + format, args);
		}

		public static void ToFilePlus(string msg)
		{
			if (Global.gEnv == null || Global.gEnv.pLog == null || String.IsNullOrEmpty(msg))
				return;

			Global.gEnv.pLog.LogToFilePlus ("[Mono] " + msg);
		}

		public static void ToFilePlus(string format, params object[] args)
		{
			if (args == null || args.Length < 1)
				ToFilePlus (format);
			else
				ToFilePlus(String.Format(format, args));
		}

		public static void ToFilePlus<T>(string msg)
		{
			ToFilePlus ("[" + typeof(T).Name + "] " + msg);
		}

		public static void ToFilePlus<T>(string format, params object[] args)
		{
			ToFilePlus ("[" + typeof(T).Name + "] " + format, args);
		}

		public static void Exception(Exception ex)
		{
			Exception (ex, null);
		}

		public static void Exception(Exception ex, string format, params object[] args)
		{
			Exception (ex, String.Format (format, args));
		}

		public static void Exception(Exception ex, string customError)
		{
			if (customError != null) {
				Error ("Error: " + customError);
			}
			Error ("Message: " + ex.Message);
			StackTrace st = new StackTrace (ex, true);
			Error ("Stack Trace: " + st.FrameCount);
			for (int i = 0; i < st.FrameCount; i++) {
				Error ("  {0,3}) {1}::{2}() [{3}:{4}]", i, st.GetFrame (i).GetMethod ().DeclaringType.FullName, st.GetFrame (i).GetMethod ().Name, st.GetFrame (i).GetFileLineNumber (), st.GetFrame (i).GetFileName ());
				System.Threading.Thread.Sleep (1);
			}

			if (BreakOnException)
				throw ex;
		}

		public static void Exception<T>(Exception ex)
		{
			Exception<T> (ex, null);
		}

		public static void Exception<T>(Exception ex, string format, params object[] args)
		{
			Exception<T> (ex, String.Format (format, args));
		}

		public static void Exception<T>(Exception ex, string customError = null)
		{
			if (customError != null) {
				Error<T> ("Error: " + customError);
			}
			Error<T> ("Message: " + ex.Message);
			StackTrace st = new StackTrace (ex, true);
			Error<T> ("Stack Trace: " + st.FrameCount);
			for (int i = 0; i < st.FrameCount; i++) {
				Error<T> ("  {0,3}) {1}::{2}() [{3}:{4}]", i, st.GetFrame (i).GetMethod ().DeclaringType.FullName, st.GetFrame (i).GetMethod ().Name, st.GetFrame (i).GetFileLineNumber (), st.GetFrame (i).GetFileName ());
				System.Threading.Thread.Sleep (1);
			}

			if (BreakOnException)
				throw ex;
		}
	}
}
