// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using CryEngine.Common;

namespace CryEngine
{
	internal class ConsoleVariable
	{
		private class ConsoleVariableItem : IEquatable<ConsoleVariableItem>
		{
			public ICVar m_icVar;
			public ManagedConsoleVariableFunctionDelegate m_managedConsoleVariableDelegate;

			public bool Equals(ConsoleVariableItem item)
			{
				if(item == null) return false;
				var intPtr = ICVar.GetIntPtr(item.m_icVar);
				var myIntPtr = ICVar.GetIntPtr(m_icVar);
				if(intPtr.Equals(myIntPtr))
				{
					return true;
				}

				return false;
			}

			public override bool Equals(object obj)
			{
				if(obj == null) return false;
				var itemObj = obj as ConsoleVariableItem;
				if(itemObj == null)
				{
					return false;
				}

				return Equals(itemObj);
			}

			public override int GetHashCode()
			{
#pragma warning disable RECS0025 // Non-readonly field referenced in 'GetHashCode()'
				var intPtr = ICVar.GetIntPtr(m_icVar);
#pragma warning restore RECS0025 // Non-readonly field referenced in 'GetHashCode()'
				return intPtr.GetHashCode();
			}

			public static bool operator ==(ConsoleVariableItem item1, ConsoleVariableItem item2)
			{
				if(((object)item1 == null) || ((object)item2 == null))
				{
					return Equals(item1, item2);
				}
				return item1.Equals(item2);
			}

			public static bool operator !=(ConsoleVariableItem item1, ConsoleVariableItem item2)
			{
				if(((object)item1 == null) || ((object)item2 == null))
				{
					return !Equals(item1, item2);
				}
				return !(item1.Equals(item2));
			}
		}

		// internal listener for console variable changes in C++
		private delegate void ConsoleVariableFunctionDelegate(IntPtr consoleVariableArg);

		// public listener for console variable changes in C#
		public delegate void ManagedConsoleVariableFunctionDelegate(ICVar icVar);

		public delegate void ManagedSFunctorDelegate();

		private static ConsoleVariableFunctionDelegate s_myDelegate;

		private static IDictionary<string, ConsoleVariableItem> s_variablesDelegates;

		static ConsoleVariable()
		{
			s_myDelegate = OnConsoleVariableChanged;
			s_variablesDelegates = new Dictionary<string, ConsoleVariableItem>();
		}

		~ConsoleVariable()
		{
			s_variablesDelegates.Clear();
			s_variablesDelegates = null;
			s_myDelegate = null;
		}

		public static bool Register(string consoleVariableName, string consoleVariableValue, uint nFlags, string commandHelp, ManagedConsoleVariableFunctionDelegate managedConsoleVariableDelegate, out ICVar newICVar)
		{
			//check if console variable with same name exists
			var existingICVar = Global.gEnv.pConsole.GetCVar(consoleVariableName);
			if(existingICVar != null)
			{
				newICVar = null;
				return false;
			}

			var pointer = Marshal.GetFunctionPointerForDelegate(s_myDelegate);
			NativeInternals.IConsole.AddConsoleVariableString(pointer, consoleVariableName, consoleVariableValue, nFlags, commandHelp);
			newICVar = Global.gEnv.pConsole.GetCVar(consoleVariableName);
			if(newICVar == null) return false;

			var newItem = new ConsoleVariableItem { m_icVar = newICVar, m_managedConsoleVariableDelegate = managedConsoleVariableDelegate };
			s_variablesDelegates.Add(consoleVariableName, newItem);
			return true;
		}

		public static bool Register(string consoleVariableName, long consoleVariableValue, uint nFlags, string commandHelp, ManagedConsoleVariableFunctionDelegate managedConsoleVariableDelegate, out ICVar newICVar)
		{
			//check if console variable with same name exists
			var existingICVar = Global.gEnv.pConsole.GetCVar(consoleVariableName);
			if(existingICVar != null)
			{
				newICVar = null;
				return false;
			}

			var pointer = Marshal.GetFunctionPointerForDelegate(s_myDelegate);
			NativeInternals.IConsole.AddConsoleVariableInt64(pointer, consoleVariableName, consoleVariableValue, nFlags, commandHelp);
			newICVar = Global.gEnv.pConsole.GetCVar(consoleVariableName);
			if(newICVar == null) 
			{
				return false;
			}

			var newItem = new ConsoleVariableItem { m_icVar = newICVar, m_managedConsoleVariableDelegate = managedConsoleVariableDelegate };
			s_variablesDelegates.Add(consoleVariableName, newItem);
			return true;
		}

		/// <summary>
		/// Registers a new control variable with the specified delegate. 
		/// </summary>
		/// <param name="consoleVariableName"></param>
		/// <param name="consoleVariableValue"></param>
		/// <param name="nFlags"></param>
		/// <param name="commandHelp"></param>
		/// <param name="managedConsoleVariableDelegate"></param>
		/// <param name="newICVar"></param>
		/// <returns></returns>
		public static bool Register(string consoleVariableName, int consoleVariableValue, uint nFlags, string commandHelp, ManagedConsoleVariableFunctionDelegate managedConsoleVariableDelegate, out ICVar newICVar)
		{
			//check if console variable with same name exists
			var existingICVar = Global.gEnv.pConsole.GetCVar(consoleVariableName);
			if(existingICVar != null)
			{
				newICVar = null;
				return false;
			}

			var pointer = Marshal.GetFunctionPointerForDelegate(s_myDelegate);
			NativeInternals.IConsole.AddConsoleVariableInt(pointer, consoleVariableName, consoleVariableValue, nFlags, commandHelp);
			newICVar = Global.gEnv.pConsole.GetCVar(consoleVariableName);
			if(newICVar == null) return false;

			var newItem = new ConsoleVariableItem { m_icVar = newICVar, m_managedConsoleVariableDelegate = managedConsoleVariableDelegate };
			s_variablesDelegates.Add(consoleVariableName, newItem);
			return true;
		}

		public static bool Register(string consoleVariableName, float consoleVariableValue, uint nFlags, string commandHelp, ManagedConsoleVariableFunctionDelegate managedConsoleVariableDelegate, out ICVar newICVar)
		{
			//check if console variable with same name exists
			var existingICVar = Global.gEnv.pConsole.GetCVar(consoleVariableName);
			if(existingICVar != null)
			{
				newICVar = null;
				return false;
			}

			var pointer = Marshal.GetFunctionPointerForDelegate(s_myDelegate);
			NativeInternals.IConsole.AddConsoleVariableFloat(pointer, consoleVariableName, consoleVariableValue, nFlags, commandHelp);
			newICVar = Global.gEnv.pConsole.GetCVar(consoleVariableName);
			if(newICVar == null)
			{
				return false;
			}

			var newItem = new ConsoleVariableItem { m_icVar = newICVar, m_managedConsoleVariableDelegate = managedConsoleVariableDelegate };
			s_variablesDelegates.Add(consoleVariableName, newItem);
			return true;
		}

		/// <summary>
		/// Unregistered an existing control variable, This unregisters from both control variables that are both registered natively and via managed
		/// </summary>
		/// <param name="consoleVariable">Will be set to null if unregistered successfully</param>
		/// <returns>true if control variable is unregistered successfully</returns>
		public static bool UnRegister(ref ICVar consoleVariable)
		{
			if(consoleVariable == null)
			{
				return false;
			}
			bool unregistered = false;
			//check if registered via C# 
			var consoleVariableName = consoleVariable.GetName();
			if(s_variablesDelegates.ContainsKey(consoleVariableName))
			{
				ConsoleVariableItem consoleVariableItem = null;
				s_variablesDelegates.TryGetValue(consoleVariableName, out consoleVariableItem);
				unregistered = s_variablesDelegates.Remove(consoleVariableName);
				if(unregistered)
				{
					consoleVariableItem.m_icVar = null;
					consoleVariableItem.m_managedConsoleVariableDelegate = null;
				}
			}
			// likelihood that console variable is preregistered via C++ and not C#
			Global.gEnv.pConsole.UnregisterVariable(consoleVariableName);
			unregistered |= (Global.gEnv.pConsole.GetCVar(consoleVariableName) == null);
			if(unregistered)
			{
				consoleVariable = null;
			}
			return unregistered;
		}

		/// <summary>
		/// Changes the control variable callback to the specified delegate.
		/// Note: Only control variables that are registered via managed class CryEngine.ConsoleVariable can be changed. Control Variables that are predefined in system cannot be changed.
		/// </summary>
		/// <param name="icVar"></param>
		/// <param name="managedConsoleVariableDelegate"></param>
		/// <returns></returns>
		public static bool SetManagedConsoleVariableFunctionDelegate(ICVar icVar, ManagedConsoleVariableFunctionDelegate managedConsoleVariableDelegate)
		{
			var consoleVariableName = icVar.GetName();
			if(!s_variablesDelegates.ContainsKey(consoleVariableName))
			{
				return false;
			}

			//change the console variable item to map to new delegate, the c++ callback "OnConsoleVariableChanged(IntPtr consoleVariableArg)" remains unchanged
			ConsoleVariableItem consoleVariableItem = null;
			s_variablesDelegates.TryGetValue(consoleVariableName, out consoleVariableItem);
			consoleVariableItem.m_managedConsoleVariableDelegate = managedConsoleVariableDelegate;
			return true;
		}

		/// <summary>
		/// Retrieves the managed control variable callback delegate. Only for control variables that are registered via managed 
		/// </summary>
		/// <param name="icVar">Any control variable that is registered via managed</param>
		/// <param name="managedConsoleVariableDelegate">the managed control variable delegate if function returns true</param>
		/// <returns>true if the control variable exists</returns>
		public static bool GetManagedConsoleVariableFunctionDelegate(ICVar icVar, out ManagedConsoleVariableFunctionDelegate managedConsoleVariableDelegate)
		{
			var consoleVariableName = icVar.GetName();
			managedConsoleVariableDelegate = null;
			if(!s_variablesDelegates.ContainsKey(consoleVariableName))
			{
				return false;
			}
			managedConsoleVariableDelegate = s_variablesDelegates[consoleVariableName].m_managedConsoleVariableDelegate;
			return true;
		}

		private static void OnConsoleVariableChanged(IntPtr consoleVariableArg)
		{
			var foundValue = s_variablesDelegates.First(item => ICVar.GetIntPtr(item.Value.m_icVar) == consoleVariableArg);
			foundValue.Value.m_managedConsoleVariableDelegate?.Invoke(foundValue.Value.m_icVar);
		}
	}
}