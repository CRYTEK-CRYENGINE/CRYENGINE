// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace CryEngine.Attributes
{
	[AttributeUsage(AttributeTargets.Field, AllowMultiple =false,Inherited =false)]
	public sealed class IconPathAttribute : Attribute
	{

		public IconPathAttribute(string iconPath)
		{
			Path = iconPath;
		}

		public string Path
		{
			get;
			private set;
		}
	}	
}

