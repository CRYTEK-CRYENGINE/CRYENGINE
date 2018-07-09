// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace CryEngine
{
	/// <summary>
	/// Exposes the specified method to Schematyc
	/// </summary>
	[AttributeUsage(AttributeTargets.Method)]
	public sealed class SchematycMethodAttribute : Attribute
	{
		/// <summary>
		/// Exposes the specified method to Schematyc
		/// </summary>
		public SchematycMethodAttribute() { }
	}
}