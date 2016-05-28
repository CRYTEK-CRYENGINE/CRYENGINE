// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Linq;
using System.Reflection;

/// <summary>
/// All framework relevant Attributes.
/// </summary>
namespace CryEngine.Attributes
{
	/// <summary>
	/// Use this attribute to hide any Property of Class from any reflection driven inspection.
	/// </summary>
	[AttributeUsage(System.AttributeTargets.All)]
	public class HideFromInspectorAttribute : Attribute
	{
	}

	[AttributeUsage(System.AttributeTargets.Field)]
	public class StringValueAttribute : System.Attribute
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
			Type type = value.GetType();
			FieldInfo fi = type.GetField(value.ToString());
			StringValueAttribute attr = (StringValueAttribute)fi.GetCustomAttributes(typeof(StringValueAttribute), true).FirstOrDefault();
			return attr != null ? attr.Value : String.Empty;
		}
		#endregion
	}
}
