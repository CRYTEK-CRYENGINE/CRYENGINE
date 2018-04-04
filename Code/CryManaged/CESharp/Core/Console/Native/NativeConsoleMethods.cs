using System;
using System.Runtime.CompilerServices;

namespace CryEngine.NativeInternals
{
	public static class IConsole
	{
		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void AddConsoleCommandFunction(IntPtr consoleCommandFunctionPtr, string functionName, uint nFlags, string sHelp, bool isManaged);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void AddConsoleVariableString(IntPtr consoleVariableFunctionPtr, string variableName, string variableValue, uint nFlags, string sHelp);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void AddConsoleVariableInt64(IntPtr consoleVariableFunctionPtr, string variableName, long variableValue, uint nFlags, string sHelp);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void AddConsoleVariableInt(IntPtr consoleVariableFunctionPtr, string variableName, int variableValue, uint nFlags, string sHelp);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void AddConsoleVariableFloat(IntPtr consoleVariableFunctionPtr, string variableName, float variableValue, uint nFlags, string sHelp);

	}
}