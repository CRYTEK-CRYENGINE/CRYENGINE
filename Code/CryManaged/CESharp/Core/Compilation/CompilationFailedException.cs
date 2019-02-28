// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace CryEngine.Compilation
{
	/// <summary>
	/// Exception indicating that compiling C# source files has failed.
	/// </summary>
	public class CompilationFailedException : Exception
	{
		/// <summary>
		/// Exception indicating that compiling C# source files has failed.
		/// </summary>
		public CompilationFailedException() { }

		/// <summary>
		/// Exception indicating that compiling C# source files has failed.
		/// </summary>
		/// <param name="errorMessage"></param>
		public CompilationFailedException(string errorMessage)
			: base(errorMessage) { }

		/// <summary>
		/// Exception indicating that compiling C# source files has failed.
		/// </summary>
		/// <param name="errorMessage"></param>
		/// <param name="innerEx"></param>
		public CompilationFailedException(string errorMessage, Exception innerEx)
			: base(errorMessage, innerEx) { }
	}
}
