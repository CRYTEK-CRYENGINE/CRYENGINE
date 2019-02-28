// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Defines the types <see cref="ParticleAttributes"/> can reference.
	/// </summary>
	public enum AttributeType
	{
		/// <summary>
		/// Specifies the type to be <see cref="bool"/>
		/// </summary>
		Boolean,
		/// <summary>
		/// Specifies the type to be <see cref="int"/>
		/// </summary>
		Integer,
		/// <summary>
		/// Specifies the type to be <see cref="float"/>
		/// </summary>
		Float,
		/// <summary>
		/// Specifies the type to be <see cref="CryEngine.Color"/>
		/// </summary>
		Color
	}

	/// <summary>
	/// Managed wrapper for the IParticleAttributes interface.
	/// </summary>
	public sealed class ParticleAttributes
	{
		/// <summary>
		/// The amount of attributes on this instance.
		/// </summary>
		public uint Count
		{
			get
			{
				return NativeHandle.GetNumAttributes();
			}
		}

		[SerializeValue]
		internal IParticleAttributes NativeHandle { get; private set; }

		internal ParticleAttributes(IParticleAttributes nativeHandle)
		{
			NativeHandle = nativeHandle;
		}

		/// <summary>
		/// Find an attribute by name.
		/// </summary>
		/// <param name="name"></param>
		/// <returns></returns>
		public uint FindAttributeIdByName(string name)
		{
			return NativeHandle.FindAttributeIdByName(name);
		}

		/// <summary>
		/// Get the <see cref="bool"/> value of an attribute.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="defaultValue"></param>
		/// <returns></returns>
		public bool GetAsBoolean(uint id, bool defaultValue)
		{
			return NativeHandle.GetAsBoolean(id, defaultValue);
		}

		/// <summary>
		/// Get the <see cref="Color"/> value of an attribute.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="defaultValue"></param>
		/// <returns></returns>
		public Color GetAsColor(uint id, Color defaultValue)
		{
			var nativeColor = NativeHandle.GetAsColorF(id, defaultValue);
			return nativeColor == null ? new Color() : new Color(nativeColor.r,
																 nativeColor.g,
																 nativeColor.b,
																 nativeColor.a);
		}

		/// <summary>
		/// Get the <see cref="float"/> value of an attribute.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="defaultValue"></param>
		/// <returns></returns>
		public float GetAsFloat(uint id, float defaultValue)
		{
			return NativeHandle.GetAsFloat(id, defaultValue);
		}

		/// <summary>
		/// Get the <see cref="int"/> value of an attribute.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="defaultValue"></param>
		/// <returns></returns>
		public int GetAsInteger(uint id, int defaultValue)
		{
			return NativeHandle.GetAsInteger(id, defaultValue);
		}

		/// <summary>
		/// Get the <see cref="bool"/> value of an attribute.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="defaultValue"></param>
		/// <returns></returns>
		public bool GetValue(uint id, bool defaultValue)
		{
			return NativeHandle.GetAsBoolean(id, defaultValue);
		}

		/// <summary>
		/// Get the <see cref="Color"/> value of an attribute.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="defaultValue"></param>
		/// <returns></returns>
		public Color GetValue(uint id, Color defaultValue)
		{
			var nativeColor = NativeHandle.GetAsColorF(id, defaultValue);
			return nativeColor == null ? new Color() : new Color(nativeColor.r,
																 nativeColor.g,
																 nativeColor.b,
																 nativeColor.a);
		}

		/// <summary>
		/// Get the <see cref="float"/> value of an attribute.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="defaultValue"></param>
		/// <returns></returns>
		public float GetValue(uint id, float defaultValue)
		{
			return NativeHandle.GetAsFloat(id, defaultValue);
		}

		/// <summary>
		/// Get the <see cref="int"/> value of an attribute.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="defaultValue"></param>
		/// <returns></returns>
		public int GetValue(uint id, int defaultValue)
		{
			return NativeHandle.GetAsInteger(id, defaultValue);
		}

		/// <summary>
		/// Get the name of the attribute with the specified <paramref name="idx"/>.
		/// </summary>
		/// <param name="idx"></param>
		/// <returns></returns>
		public string GetAttributeName(uint idx)
		{
			return NativeHandle.GetAttributeName(idx);
		}

		/// <summary>
		/// Get the type of the attribute with specified <paramref name="idx"/>.
		/// </summary>
		/// <param name="idx"></param>
		/// <returns></returns>
		public AttributeType GetAttributeType(uint idx)
		{
			return (AttributeType)NativeHandle.GetAttributeType(idx);
		}

		/// <summary>
		/// Resets this instance. The values are set to the values of <paramref name="copySource"/> unless it is null.
		/// </summary>
		/// <param name="copySource"></param>
		public void Reset(ParticleAttributes copySource)
		{
			if(copySource == null)
			{
				Reset();
			}
			else
			{
				NativeHandle.Reset(copySource.NativeHandle);
			}
		}

		/// <summary>
		/// Resets this instance.
		/// </summary>
		public void Reset()
		{
			NativeHandle.Reset();
		}

		/// <summary>
		/// Set the <see cref="bool"/> value of this instance.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="value"></param>
		public void SetAsBoolean(uint id, bool value)
		{
			NativeHandle.SetAsBoolean(id, value);
		}

		/// <summary>
		/// Set the <see cref="Color"/> value of this instance.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="value"></param>
		public void SetAsColor(uint id, Color value)
		{
			NativeHandle.SetAsColor(id, (ColorF)value);
		}

		/// <summary>
		/// Set the <see cref="float"/> value of this instance.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="value"></param>
		public void SetAsFloat(uint id, float value)
		{
			NativeHandle.SetAsFloat(id, value);
		}

		/// <summary>
		/// Set the <see cref="int"/> value of this instance.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="value"></param>
		public void SetAsInteger(uint id, int value)
		{
			NativeHandle.SetAsInteger(id, value);
		}

		/// <summary>
		/// Set the <see cref="bool"/> value of this instance.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="value"></param>
		public void SetValue(uint id, bool value)
		{
			NativeHandle.SetAsBoolean(id, value);
		}

		/// <summary>
		/// Set the <see cref="Color"/> value of this instance.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="value"></param>
		public void SetValue(uint id, Color value)
		{
			NativeHandle.SetAsColor(id, (ColorF)value);
		}

		/// <summary>
		/// Set the <see cref="float"/> value of this instance.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="value"></param>
		public void SetValue(uint id, float value)
		{
			NativeHandle.SetAsFloat(id, value);
		}

		/// <summary>
		/// Set the <see cref="int"/> value of this instance.
		/// </summary>
		/// <param name="id"></param>
		/// <param name="value"></param>
		public void SetValue(uint id, int value)
		{
			NativeHandle.SetAsInteger(id, value);
		}

		/// <summary>
		/// Transfer the values of this instance into the <paramref name="receiver"/>.
		/// </summary>
		/// <param name="receiver"></param>
		public void TransferInto(ParticleAttributes receiver)
		{
			if(receiver == null)
			{
				throw new ArgumentNullException(nameof(receiver));
			}

			NativeHandle.TransferInto(receiver.NativeHandle);
		}
	}
}
