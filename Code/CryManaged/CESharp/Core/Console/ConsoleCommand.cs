using System;
using System.Collections.Generic;
using CryEngine.Common;

namespace CryEngine
{
	public class ConsoleCommandArgumentsHolder
	{
		private readonly List<string> m_arguments;

		private ConsoleCommandArgumentsHolder(int length)
		{
			m_arguments = new List<string>(length);
		}

		public void SetArgument(int index, string argument)
		{
			if(index >= m_arguments.Capacity)
			{
				return;
			}
			m_arguments.Add(argument);
		}

		public string GetArgument(int index)
		{
			if(index >= m_arguments.Capacity)
			{
				return null;
			}
			return m_arguments[index];
		}

		public string[] GetAllArguments()
		{
			var arguments = m_arguments.ToArray();
			return arguments;
		}
	}

	public class ConsoleCommand
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

		private static IDictionary<string, ManagedConsoleCommandFunctionDelegate> m_commandsAndDelegates;

		static ConsoleCommand()
		{
			myDelegate = OnConsoleCommandFunctionInvoked;
			m_commandsAndDelegates = new Dictionary<string, ManagedConsoleCommandFunctionDelegate>();
		}

		~ConsoleCommand()
		{
			//try to unregister
			foreach(string command in m_commandsAndDelegates.Keys)
			{
				UnregisterManagedConsoleCommandFunction(command);
			}
			m_commandsAndDelegates.Clear();
			m_commandsAndDelegates = null;
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
			if(!m_commandsAndDelegates.ContainsKey(command))
			{
				m_commandsAndDelegates.Add(command, managedConsoleCmdDelegate);

				NativeInternals.IConsole.AddConsoleCommandFunction(System.Runtime.InteropServices.Marshal.GetFunctionPointerForDelegate(myDelegate), command, nFlags, commandHelp, true);
				ret = true;
			}
			return ret;
		}

		public static bool UnregisterManagedConsoleCommandFunction(string command)
		{
			bool ret = false;
			if(m_commandsAndDelegates.ContainsKey(command))
			{
				ret = m_commandsAndDelegates.Remove(command);
				Global.gEnv.pConsole.RemoveCommand(command);
			}
			return ret;
		}

		public static void NotifyManagedConsoleCommand(string command, ConsoleCommandArgumentsHolder argumentsHolder)
		{
			if(argumentsHolder != null)
			{
				ManagedConsoleCommandFunctionDelegate commandDelegate = m_commandsAndDelegates[command];
				commandDelegate(argumentsHolder.GetAllArguments());
			}
		}

		private static void OnConsoleCommandFunctionInvoked(IntPtr arg)
		{
			//do nothing
		}
	}
}
