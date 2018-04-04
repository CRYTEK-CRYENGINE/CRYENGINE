// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;

namespace CryEngine
{
	[System.Flags]
	public enum DynamicLightFlags
	{
		AreaSpecTex = 1,
		Directional,
		BoxProjectedCm = 4,
		Post3DRenderer = 16,
		CastShadowMaps = 32,
		Point = 64,
		Project = 128,
		IgnoresVisAreas = 1024,
		DeferredCubemaps = 2048,
		HasClipVolume = 4096,
		Disabled = 8192,
		AreaLight = 16384,
		UseForSVOGI = 32768,
		/// <summary>
		/// No lighting, used for Flares, beams and such.
		/// </summary>
		Fake = 131072,
		Sun = 262144,
		LM = 524288,
		/// <summary>
		/// Affects only current area/sector.
		/// </summary>
		ThisAreaOnly = 1048576,
		/// <summary>
		/// Ambient light (has name indicates, used for replacing ambient).
		/// </summary>
		Ambient = 2097152,
		/// <summary>
		/// Do not affect height map.
		/// </summary>
		IndoorOnly = 4194304,
		/// <summary>
		/// Affects volumetric fog.
		/// </summary>
		VolumetricFog = 8388608,
		/// <summary>
		/// Add only to  Light Propagation Volume if it's possible.
		/// </summary>
		AttachToSun = 33554432,
		/// <summary>
		/// Add only to  Light Propagation Volume if it's possible.
		/// </summary>
		TrackviewTimescrubbing = 67108864,
		/// <summary>
		/// Affects only volumetric fog.
		/// </summary>
		VolumetricFogOnly = 134217728,
		DiffuseOcclusion = -2147483648,
		LightTypeMask = (Directional | Point | Project | AreaLight)
	}

	public abstract class RenderLight
	{
		/// <summary>
		/// The base <see cref="Color"/> of this <see cref="RenderLight"/>. The alpha value is ignored.
		/// </summary>
		/// <value>The base color.</value>
		public Color BaseColor
		{
			get
			{
				return NativeBaseHandle.m_BaseColor;
			}
			set
			{
				NativeBaseHandle.m_BaseColor = value;
			}
		}

		/// <summary>
		/// The base object matrix of this <see cref="RenderLight"/>.
		/// </summary>
		/// <value>The base object matrix.</value>
		public Matrix3x4 BaseObjMatrix
		{
			get
			{
				return NativeBaseHandle.m_BaseObjMatrix;
			}
			set
			{
				NativeBaseHandle.m_BaseObjMatrix = value;
			}
		}

		/// <summary>
		/// The position of this <see cref="RenderLight"/> in world-space.
		/// </summary>
		/// <value>The world-space position.</value>
		public Vector3 BaseOrigin
		{
			get
			{
				return NativeBaseHandle.m_BaseOrigin;
			}
			set
			{
				NativeBaseHandle.m_BaseOrigin = value;
			}
		}

		/// <summary>
		/// The base specular multiplier.
		/// </summary>
		/// <value>The base specular multiplier.</value>
		public float BaseSpecMult
		{
			get
			{
				return NativeBaseHandle.m_BaseSpecMult;
			}
			set
			{
				NativeBaseHandle.m_BaseSpecMult = value;
			}
		}

		/// <summary>
		/// The <see cref="Color"/> of this <see cref="RenderLight"/>. The alpha is ignored.
		/// </summary>
		/// <value>The color.</value>
		public Color Color
		{
			get
			{
				return NativeBaseHandle.m_Color;
			}
			set
			{
				NativeBaseHandle.m_Color = value;
			}
		}

		public float AreaHeight
		{
			get
			{
				return NativeBaseHandle.m_fAreaHeight;
			}
			set
			{
				NativeBaseHandle.m_fAreaHeight = value;
			}
		}

		public float AreaWidth
		{
			get
			{
				return NativeBaseHandle.m_fAreaWidth;
			}
			set
			{
				NativeBaseHandle.m_fAreaWidth = value;
			}
		}

		public float AttenuationBulbSize
		{
			get
			{
				return NativeBaseHandle.m_fAttenuationBulbSize;
			}
			set
			{
				NativeBaseHandle.m_fAttenuationBulbSize = value;
			}
		}

		public float BaseRadius
		{
			get
			{
				return NativeBaseHandle.m_fBaseRadius;
			}
			set
			{
				NativeBaseHandle.m_fBaseRadius = value;
			}
		}

		public float BoxHeight
		{
			get
			{
				return NativeBaseHandle.m_fBoxHeight;
			}
			set
			{
				NativeBaseHandle.m_fBoxHeight = value;
			}
		}

		public float BoxLength
		{
			get
			{
				return NativeBaseHandle.m_fBoxLength;
			}
			set
			{
				NativeBaseHandle.m_fBoxLength = value;
			}
		}

		public float BoxWidth
		{
			get
			{
				return NativeBaseHandle.m_fBoxWidth;
			}
			set
			{
				NativeBaseHandle.m_fBoxWidth = value;
			}
		}

		public float FogRadialLobe
		{
			get
			{
				return NativeBaseHandle.m_fFogRadialLobe;
			}
			set
			{
				NativeBaseHandle.m_fFogRadialLobe = value;
			}
		}

		public float HDRDynamic
		{
			get
			{
				return NativeBaseHandle.m_fHDRDynamic;
			}
			set
			{
				NativeBaseHandle.m_fHDRDynamic = value;
			}
		}

		public DynamicLightFlags Flags
		{
			get
			{
				return (DynamicLightFlags)NativeBaseHandle.m_Flags;
			}
			set
			{
				NativeBaseHandle.m_Flags = (uint)value;
			}
		}

		public float LightFrustumAngle
		{
			get
			{
				return NativeBaseHandle.m_fLightFrustumAngle;
			}
			set
			{
				NativeBaseHandle.m_fLightFrustumAngle = value;
			}
		}

		public float ProjectorNearPlane
		{
			get
			{
				return NativeBaseHandle.m_fProjectorNearPlane;
			}
			set
			{
				NativeBaseHandle.m_fProjectorNearPlane = value;
			}
		}

		public float Radius
		{
			get
			{
				return NativeBaseHandle.m_fRadius;
			}
			set
			{
				NativeBaseHandle.m_fRadius = value;
			}
		}

		public float ShadowBias
		{
			get
			{
				return NativeBaseHandle.m_fShadowBias;
			}
			set
			{
				NativeBaseHandle.m_fShadowBias = value;
			}
		}

		public float ShadowResolutionScale
		{
			get
			{
				return NativeBaseHandle.m_fShadowResolutionScale;
			}
			set
			{
				NativeBaseHandle.m_fShadowResolutionScale = value;
			}
		}

		public float ShadowSlopeBias
		{
			get
			{
				return NativeBaseHandle.m_fShadowSlopeBias;
			}
			set
			{
				NativeBaseHandle.m_fShadowSlopeBias = value;
			}
		}

		public float ShadowUpdateMinRadius
		{
			get
			{
				return NativeBaseHandle.m_fShadowUpdateMinRadius;
			}
			set
			{
				NativeBaseHandle.m_fShadowUpdateMinRadius = value;
			}
		}

		public float TimeScrubbed
		{
			get
			{
				return NativeBaseHandle.m_fTimeScrubbed;
			}
			set
			{
				NativeBaseHandle.m_fTimeScrubbed = value;
			}
		}

		public short Id
		{
			get
			{
				return NativeBaseHandle.m_Id;
			}
			set
			{
				NativeBaseHandle.m_Id = value;
			}
		}

		public byte LensOpticsFrustumAngle
		{
			get
			{
				return NativeBaseHandle.m_LensOpticsFrustumAngle;
			}
			set
			{
				NativeBaseHandle.m_LensOpticsFrustumAngle = value;
			}
		}

		public uint Engine3DUpdateFrameID
		{
			get
			{
				return NativeBaseHandle.m_n3DEngineUpdateFrameID;
			}
			set
			{
				NativeBaseHandle.m_n3DEngineUpdateFrameID = value;
			}
		}

		public byte AnimationSpeed
		{
			get
			{
				return NativeBaseHandle.m_nAnimSpeed;
			}
			set
			{
				NativeBaseHandle.m_nAnimSpeed = value;
			}
		}

		public byte AttenuationFalloffMax
		{
			get
			{
				return NativeBaseHandle.m_nAttenFalloffMax;
			}
			set
			{
				NativeBaseHandle.m_nAttenFalloffMax = value;
			}
		}

		public EntitySystem.EntityId EntityId
		{
			get
			{
				return NativeBaseHandle.m_nEntityId;
			}
			set
			{
				NativeBaseHandle.m_nEntityId = value;
			}
		}

		public byte LightPhase
		{
			get
			{
				return NativeBaseHandle.m_nLightPhase;
			}
			set
			{
				NativeBaseHandle.m_nLightPhase = value;
			}
		}

		public byte LightStyle
		{
			get
			{
				return NativeBaseHandle.m_nLightStyle;
			}
			set
			{
				NativeBaseHandle.m_nLightStyle = value;
			}
		}

		public ushort ShadowMinResolution
		{
			get
			{
				return NativeBaseHandle.m_nShadowMinResolution;
			}
			set
			{
				NativeBaseHandle.m_nShadowMinResolution = value;
			}
		}

		public ushort ShadowUpdateRatio
		{
			get
			{
				return NativeBaseHandle.m_nShadowUpdateRatio;
			}
			set
			{
				NativeBaseHandle.m_nShadowUpdateRatio = value;
			}
		}

		public byte SortPriority
		{
			get
			{
				return NativeBaseHandle.m_nSortPriority;
			}
			set
			{
				NativeBaseHandle.m_nSortPriority = value;
			}
		}

		public Matrix3x4 ObjectMatrix
		{
			get
			{
				return NativeBaseHandle.m_ObjMatrix;
			}
			set
			{
				NativeBaseHandle.m_ObjMatrix = value;
			}
		}

		public Vector3 Position
		{
			get
			{
				return NativeBaseHandle.m_Origin;
			}
			set
			{
				NativeBaseHandle.m_Origin = value;
			}
		}

		public Vector3 ProbeExtents
		{
			get
			{
				return NativeBaseHandle.m_ProbeExtents;
			}
			set
			{
				NativeBaseHandle.m_ProbeExtents = value;
			}
		}

		public Matrix4x4 ProjectionMatrix
		{
			get
			{
				return NativeBaseHandle.m_ProjMatrix;
			}
			set
			{
				NativeBaseHandle.m_ProjMatrix = value;
			}
		}

		public byte ShadowMaskIndex
		{
			get
			{
				return NativeBaseHandle.m_ShadowMaskIndex;
			}
			set
			{
				NativeBaseHandle.m_ShadowMaskIndex = value;
			}
		}

		public short Height
		{
			get
			{
				return NativeBaseHandle.m_sHeight;
			}
			set
			{
				NativeBaseHandle.m_sHeight = value;
			}
		}

		public string Name
		{
			get
			{
				return NativeBaseHandle.m_sName;
			}
			set
			{
				NativeBaseHandle.m_sName = value;
			}
		}

		public float SpecularMultiplier
		{
			get
			{
				return NativeBaseHandle.m_SpecMult;
			}
			set
			{
				NativeBaseHandle.m_SpecMult = value;
			}
		}

		public short Width
		{
			get
			{
				return NativeBaseHandle.m_sWidth;
			}
			set
			{
				NativeBaseHandle.m_sWidth = value;
			}
		}

		public short X
		{
			get
			{
				return NativeBaseHandle.m_sX;
			}
			set
			{
				NativeBaseHandle.m_sX = value;
			}
		}

		public short Y
		{
			get
			{
				return NativeBaseHandle.m_sY;
			}
			set
			{
				NativeBaseHandle.m_sY = value;
			}
		}

		internal SRenderLight NativeBaseHandle { get; private set; }

		internal RenderLight(SRenderLight nativeHandle)
		{
			NativeBaseHandle = nativeHandle;
		}

		public void AcquireResources()
		{
			NativeBaseHandle.AcquireResources();
		}

		public void DropResources()
		{
			NativeBaseHandle.DropResources();
		}
	}
}
