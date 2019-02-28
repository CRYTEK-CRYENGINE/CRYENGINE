// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace CryEngine.Debugging
{
	internal static class ExceptionHandler
	{
		static ExceptionHandler()
		{
			AppDomain.CurrentDomain.UnhandledException += UnhandledExceptionOccurred;
		}

		private static void UnhandledExceptionOccurred(object sender, UnhandledExceptionEventArgs e)
		{
			var exception = e.ExceptionObject as Exception;

			// The CLS doesn't force exceptions to derive from System.Exception
			if(exception == null)
				throw new NotSupportedException("An exception that does not derive from System.Exception was thrown.");

			Display(exception);
		}

		/// <summary>
		/// Displays an exception via an exception form.
		/// </summary>
		/// <param name="ex">The exception that occurred</param>
		public static void Display(Exception ex)
		{
			// Log exception as well
			Log.Error(ex.ToString());

			var form = new ExceptionMessage(ex, false);
			form.ShowDialog();
		}
	}
}
