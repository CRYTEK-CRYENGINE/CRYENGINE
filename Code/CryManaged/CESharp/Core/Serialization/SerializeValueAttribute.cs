using System;

namespace CryEngine
{
	/// <summary>
	/// Marks a property or field to be serialized when reloading the C# plugins.
	/// Values with this attribute are not shown in the Properties panel of the Sandbox.
	/// For serializing and showing values in the Sandbox use the <see cref="EntityPropertyAttribute"/> instead.
	/// </summary>
	[AttributeUsage(AttributeTargets.Property | AttributeTargets.Field)]
	public class SerializeValueAttribute : Attribute
	{
	}
}
