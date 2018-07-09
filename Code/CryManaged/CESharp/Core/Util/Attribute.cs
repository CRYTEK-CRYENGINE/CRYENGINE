// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace CryEngine.Attributes
{
	/// <summary>
	/// Defines the path to an icon in the engine assets. Used to give EntityComponents an icon in the Sandbox.
	/// </summary>
	[AttributeUsage(AttributeTargets.Field, AllowMultiple = false, Inherited = false)]
	public sealed class IconPathAttribute : Attribute
	{

		/// <summary>
		/// Defines the path to an icon in the engine assets. Used to give EntityComponents an icon in the Sandbox.
		/// </summary>
		/// <param name="iconPath"></param>
		public IconPathAttribute(string iconPath)
		{
			Path = iconPath;
		}

		/// <summary>
		/// Path to the icon.
		/// </summary>
		public string Path
		{
			get;
			private set;
		}
	}
}

