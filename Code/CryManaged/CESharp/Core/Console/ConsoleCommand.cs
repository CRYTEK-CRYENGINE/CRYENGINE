// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Container for the arguments of console commands.
	/// </summary>
	public class ConsoleCommandArgumentsHolder
	{
		private readonly List<string> _arguments;

		/// <summary>
		/// The amount of arguments this container can hold.
		/// </summary>
		public int Capacity{ get { return _arguments.Count; } }

		private ConsoleCommandArgumentsHolder(int length)
		{
			_arguments = new List<string>(length);
			for(int i = 0; i < length; ++i)
			{
				_arguments.Add(string.Empty);
			}
		}

		/// <summary>
		/// Set the value of an argument.
		/// </summary>
		/// <param name="index"></param>
		/// <param name="argument"></param>
		public void SetArgument(int index, string argument)
		{
			if(index >= _arguments.Count)
			{
				throw new ArgumentOutOfRangeException(nameof(index));
			}
			_arguments[index] = argument;
		}

		/// <summary>
		/// Get the argument at the specified index.
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		public string GetArgument(int index)
		{
			if(index >= _arguments.Count)
			{
				throw new ArgumentOutOfRangeException(nameof(index));
			}
			return _arguments[index];
		}

		/// <summary>
		/// Returns all arguments in an array.
		/// </summary>
		/// <returns></returns>
		public string[] GetAllArguments()
		{
			var arguments = _arguments.ToArray();
			return arguments;
		}
	}

	/// <summary>
	/// Managed wrapper for console commands that can be invoked through the console.
	/// </summary>
	internal class ConsoleCommand
	{
		/// <summary>
		/// internal listener for console commands in C++
		/// </summary>
		public delegate void ConsoleCommandFunctionDelegate(IntPtr consoleCommandArg);

		/// <summary>
		/// public listener for console commands in C#
		/// </summary>
		public delegate void ManagedConsoleCommandFunctionDelegate(params string[] arguments);

		private static ConsoleCommandFunctionDelegate myDelegate = OnConsoleCommandFunctionInvoked;

		private static IDictionary<string, ManagedConsoleCommandFunctionDelegate> _commandsAndDelegates;

		static ConsoleCommand()
		{
			myDelegate = OnConsoleCommandFunctionInvoked;
			_commandsAndDelegates = new Dictionary<string, ManagedConsoleCommandFunctionDelegate>();
		}

		/// <summary>
		/// Destructor that handles cleaning up all console commands.
		/// </summary>
		~ConsoleCommand()
		{
			//try to unregister
			foreach(string command in _commandsAndDelegates.Keys)
			{
				UnregisterManagedConsoleCommandFunction(command);
			}
			_commandsAndDelegates.Clear();
			_commandsAndDelegates = null;
		}

		/// <summary>
		/// Executes a string on the command line, for example to load a map as a user could in the console. ('map myMap')
		/// </summary>
		/// <param name="command">The command to execute</param>
		/// <param name="silent">Whether or not to output the specified command in the log</param>
		/// <param name="deferred">If false, executes the command immediately, otherwise waits until the next frame.</param>
		public static void Execute(string command, bool silent = false, bool deferred = false)
		{
			Global.gEnv.pConsole.ExecuteString(command, silent, deferred);
		}

		public static bool RegisterManagedConsoleCommandFunction(string command, uint nFlags, string commandHelp, ManagedConsoleCommandFunctionDelegate managedConsoleCmdDelegate)
		{
			bool ret = false;

			//check if current console command exist
			if(!_commandsAndDelegates.ContainsKey(command))
			{
				_commandsAndDelegates.Add(command, managedConsoleCmdDelegate);

				NativeInternals.IConsole.AddConsoleCommandFunction(System.Runtime.InteropServices.Marshal.GetFunctionPointerForDelegate(myDelegate), command, nFlags, commandHelp, true);
				ret = true;
			}
			return ret;
		}

		public static bool UnregisterManagedConsoleCommandFunction(string command)
		{
			bool ret = false;
			if(_commandsAndDelegates.ContainsKey(command))
			{
				ret = _commandsAndDelegates.Remove(command);
				Global.gEnv.pConsole.RemoveCommand(command);
			}
			return ret;
		}

		public static void NotifyManagedConsoleCommand(string command, ConsoleCommandArgumentsHolder argumentsHolder)
		{
			if(argumentsHolder != null)
			{
				ManagedConsoleCommandFunctionDelegate commandDelegate = _commandsAndDelegates[command];
				commandDelegate(argumentsHolder.GetAllArguments());
			}
		}

		private static void OnConsoleCommandFunctionInvoked(IntPtr arg)
		{
			//do nothing
		}
	}
}
