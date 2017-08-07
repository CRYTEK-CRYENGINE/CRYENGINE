// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine
{
	public enum AttributeType
	{
		Boolean,
		Integer,
		Float,
		Color
	}

	public sealed class ParticleAttributes
	{
		public uint Count
		{
			get
			{
				return NativeHandle.GetNumAttributes();
			}
		}
		internal IParticleAttributes NativeHandle { get; private set; }

		internal ParticleAttributes(IParticleAttributes nativeHandle)
		{
			NativeHandle = nativeHandle;
		}

		public uint FindAttributeIdByName(string name)
		{
			return NativeHandle.FindAttributeIdByName(name);
		}

		public bool GetAsBoolean(uint id, bool defaultValue)
		{
			return NativeHandle.GetAsBoolean(id, defaultValue);
		}

		public Color GetAsColor(uint id, Color defaultValue)
		{
			var nativeColor = NativeHandle.GetAsColorF(id, defaultValue);
			return nativeColor == null ? new Color() : new Color(nativeColor.r,
			                                                     nativeColor.g,
			                                                     nativeColor.b,
			                                                     nativeColor.a);
		}

		public float GetAsFloat(uint id, float defaultValue)
		{
			return NativeHandle.GetAsFloat(id, defaultValue);
		}

		public int GetAsInteger(uint id, int defaultValue)
		{
			return NativeHandle.GetAsInteger(id, defaultValue);
		}

		public string GetAttributeName(uint idx)
		{
			return NativeHandle.GetAttributeName(idx);
		}

		public AttributeType GetAttributeType(uint idx)
		{
			return (AttributeType)NativeHandle.GetAttributeType(idx);
		}

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

		public void Reset()
		{
			NativeHandle.Reset();
		}

		public void SetAsBoolean(uint id, bool value)
		{
			NativeHandle.SetAsBoolean(id, value);
		}

		public void SetAsColor(uint id, Color value)
		{
			NativeHandle.SetAsColor(id, value);
		}

		public float SetAsFloat(uint id, float value)
		{
			return NativeHandle.SetAsFloat(id, value);
		}

		public int SetAsInteger(uint id, int value)
		{
			return NativeHandle.SetAsInteger(id, value);
		}

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
