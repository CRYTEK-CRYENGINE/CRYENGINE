// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Linq;


// All framework relevant Attributes.
namespace CryEngine.Attributes
{
	/// <summary>
	/// Use this attribute to hide any Property of Class from any reflection driven inspection.
	/// </summary>
	[AttributeUsage(AttributeTargets.All)]
	[Obsolete]
	public class HideFromInspectorAttribute : Attribute
	{
	}

	[AttributeUsage(AttributeTargets.Field)]
	public class StringValueAttribute : Attribute
	{
		#region Properties
		public string Value { get; private set; }
		#endregion

		#region Constructors
		public StringValueAttribute(string value)
		{
			Value = value;
		}
		#endregion

		#region Methods
		public static string GetStringValue(Enum value)
		{
			var type = value.GetType();
			var fi = type.GetField(value.ToString());
			var attr = (StringValueAttribute)fi.GetCustomAttributes(typeof(StringValueAttribute), true).FirstOrDefault();
			return attr != null ? attr.Value : string.Empty;
		}
		#endregion
	}
}
