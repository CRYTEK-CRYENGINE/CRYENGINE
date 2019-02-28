// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;

namespace CryEngine
{
	public enum GeomType
	{
		None,
		BoundingBox,
		Physics,
		Render
	}

	public enum GeomForm
	{
		Vertices,
		Edges,
		Surface,
		Volume
	}

	public sealed class SpawnParameters
	{
		public string AudioRtpc
		{
			get
			{
				return NativeHandle.audioRtpc.c_str();
			}
			set
			{
				NativeHandle.audioRtpc = new CryString(value);
			}
		}

		public bool CountPerUnit
		{
			get
			{
				return NativeHandle.bCountPerUnit;
			}
			set
			{
				NativeHandle.bCountPerUnit = value;
			}
		}

		public bool EnableAudio
		{
			get
			{
				return NativeHandle.bEnableAudio;
			}
			set
			{
				NativeHandle.bEnableAudio = value;
			}
		}

		public bool Nowhere
		{
			get
			{
				return NativeHandle.bNowhere;
			}
			set
			{
				NativeHandle.bNowhere = value;
			}
		}

		public bool bPrime
		{
			get
			{
				return NativeHandle.bPrime;
			}
			set
			{
				NativeHandle.bPrime = value;
			}
		}

		public bool bRegisterByBBox
		{
			get
			{
				return NativeHandle.bRegisterByBBox;
			}
			set
			{
				NativeHandle.bRegisterByBBox = value;
			}
		}

		public GeomForm eAttachForm
		{
			get
			{
				return (GeomForm)NativeHandle.eAttachForm;
			}
			set
			{
				NativeHandle.eAttachForm = (EGeomForm)value;
			}
		}

		public GeomType AttachType
		{
			get
			{
				return (GeomType)NativeHandle.eAttachType;
			}
			set
			{
				NativeHandle.eAttachType = (EGeomType)value;
			}
		}

		public float CountScale
		{
			get
			{
				return NativeHandle.fCountScale;
			}
			set
			{
				NativeHandle.fCountScale = value;
			}
		}

		public float PulsePeriod
		{
			get
			{
				return NativeHandle.fPulsePeriod;
			}
			set
			{
				NativeHandle.fPulsePeriod = value;
			}
		}

		public float SizeScale
		{
			get
			{
				return NativeHandle.fSizeScale;
			}
			set
			{
				NativeHandle.fSizeScale = value;
			}
		}

		public float SpeedScale
		{
			get
			{
				return NativeHandle.fSpeedScale;
			}
			set
			{
				NativeHandle.fSpeedScale = value;
			}
		}

		public float Strength
		{
			get
			{
				return NativeHandle.fStrength;
			}
			set
			{
				NativeHandle.fStrength = value;
			}
		}

		public float TimeScale
		{
			get
			{
				return NativeHandle.fTimeScale;
			}
			set
			{
				NativeHandle.fTimeScale = value;
			}
		}

		public int Seed
		{
			get
			{
				return NativeHandle.nSeed;
			}
			set
			{
				NativeHandle.nSeed = value;
			}
		}

		[SerializeValue]
		internal SpawnParams NativeHandle { get; private set; }

		internal SpawnParameters(SpawnParams nativeHandle)
		{
			NativeHandle = nativeHandle;
		}
	}
}
