// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Flags that can be used to set the parameters of dynamic lights.
	/// </summary>
	[System.Flags]
	public enum DynamicLightFlags
	{
		/// <summary>
		/// Flag for AreaSpecTex.
		/// </summary>
		AreaSpecTex = eDynamicLightFlags.DLF_AREA_SPEC_TEX,
		/// <summary>
		/// Flag that indicates this is a directional light.
		/// </summary>
		Directional = eDynamicLightFlags.DLF_DIRECTIONAL,
		/// <summary>
		/// Flag for box projected cubemaps.
		/// </summary>
		BoxProjectedCm = eDynamicLightFlags.DLF_BOX_PROJECTED_CM,
		/// <summary>
		/// Flag for post 3d renderer light.
		/// </summary>
		Post3DRenderer = eDynamicLightFlags.DLF_POST_3D_RENDERER,
		/// <summary>
		/// Flag that indicates if this light should cast shadows.
		/// </summary>
		CastShadowMaps = eDynamicLightFlags.DLF_CASTSHADOW_MAPS,
		/// <summary>
		/// Flag that indicates this light is a point light.
		/// </summary>
		Point = eDynamicLightFlags.DLF_POINT,
		/// <summary>
		/// Flag that indicates this light is a projector.
		/// </summary>
		Project = eDynamicLightFlags.DLF_PROJECT,
		/// <summary>
		/// Flag for ignoring Vis Areas.
		/// </summary>
		IgnoresVisAreas = eDynamicLightFlags.DLF_IGNORES_VISAREAS,
		/// <summary>
		/// Flag that indicates that the cubemaps are deferred.
		/// </summary>
		DeferredCubemaps = eDynamicLightFlags.DLF_DEFERRED_CUBEMAPS,
		/// <summary>
		/// Flag that indicates this light has a clip volume.
		/// </summary>
		HasClipVolume = eDynamicLightFlags.DLF_HAS_CLIP_VOLUME,
		/// <summary>
		/// Flag that indicate that this light is disabled.
		/// </summary>
		Disabled = eDynamicLightFlags.DLF_DISABLED,
		/// <summary>
		/// Flag that indicates that this light is an area light.
		/// </summary>
		AreaLight = eDynamicLightFlags.DLF_AREA_LIGHT,
		/// <summary>
		/// Flag that indicates that this light is used for SVOGI.
		/// </summary>
		UseForSVOGI = eDynamicLightFlags.DLF_USE_FOR_SVOGI,
		/// <summary>
		/// No lighting, used for Flares, beams and such.
		/// </summary>
		Fake = eDynamicLightFlags.DLF_FAKE,
		/// <summary>
		/// Flag that indicates that this light is the sun.
		/// </summary>
		Sun = eDynamicLightFlags.DLF_SUN,
		// TODO Add summary for DLF_LM.
		/// <summary>
		/// 
		/// </summary>
		LM = eDynamicLightFlags.DLF_LM,
		/// <summary>
		/// Affects only current area/sector.
		/// </summary>
		ThisAreaOnly = eDynamicLightFlags.DLF_THIS_AREA_ONLY,
		/// <summary>
		/// Ambient light (as name indicates, used for replacing ambient).
		/// </summary>
		Ambient = eDynamicLightFlags.DLF_AMBIENT,
		/// <summary>
		/// Does not affect height map.
		/// </summary>
		IndoorOnly = eDynamicLightFlags.DLF_INDOOR_ONLY,
		/// <summary>
		/// Affects volumetric fog.
		/// </summary>
		VolumetricFog = eDynamicLightFlags.DLF_VOLUMETRIC_FOG,
		/// <summary>
		/// Add only to Light Propagation Volume if it's possible.
		/// </summary>
		AttachToSun = eDynamicLightFlags.DLF_ATTACH_TO_SUN,
		/// <summary>
		/// Add only to Light Propagation Volume if it's possible.
		/// </summary>
		TrackviewTimescrubbing = eDynamicLightFlags.DLF_TRACKVIEW_TIMESCRUBBING,
		/// <summary>
		/// Affects only volumetric fog.
		/// </summary>
		VolumetricFogOnly = eDynamicLightFlags.DLF_VOLUMETRIC_FOG_ONLY,
		/// <summary>
		/// Mask with the various types of light that are available.
		/// </summary>
		LightTypeMask = (Directional | Point | Project | AreaLight)
	}

	/// <summary>
	/// Base class for a RenderLight.
	/// </summary>
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

		/// <summary>
		/// The area height of this <see cref="RenderLight"/>.
		/// </summary>
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

		/// <summary>
		/// The area width of this <see cref="RenderLight"/>.
		/// </summary>
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

		/// <summary>
		/// The bulb size of the attenuation.
		/// </summary>
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

		/// <summary>
		/// The clipping radius.
		/// </summary>
		public float ClipRadius
		{
			get
			{
				return NativeBaseHandle.m_fClipRadius;
			}
			set
			{
				NativeBaseHandle.m_fClipRadius = value;
			}
		}

		/// <summary>
		/// The environment probe box height of this <see cref="RenderLight"/>.
		/// </summary>
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

		/// <summary>
		/// The environment probe box length of this <see cref="RenderLight"/>.
		/// </summary>
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

		/// <summary>
		/// The environment probe box width of this <see cref="RenderLight"/>.
		/// </summary>
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

		/// <summary>
		/// The blend ratio of two radial lobe for volumetric fog.
		/// </summary>
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

		/// <summary>
		/// The flags of this <see cref="RenderLight"/>.
		/// </summary>
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

		/// <summary>
		/// The light frustrum angle of the projector.
		/// </summary>
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

		/// <summary>
		/// The near plane of the projector.
		/// </summary>
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

		/// <summary>
		/// The radius of the light.
		/// </summary>
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

		/// <summary>
		/// The shadow bias of the shadow map.
		/// </summary>
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

		/// <summary>
		/// The resolution scale of the shadow map.
		/// </summary>
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

		/// <summary>
		/// The slope bias of the shadow map.
		/// </summary>
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

		/// <summary>
		/// The minimal update radius of the shadow map.
		/// </summary>
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

		/// <summary>
		/// The time the light was scrubbed.
		/// </summary>
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

		/// <summary>
		/// The ID of the light.
		/// </summary>
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

		/// <summary>
		/// Frustum angle ranging from 0 to 255. The range will be adjusted from 0 to 360 when used.
		/// </summary>
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

		/// <summary>
		/// Frame ID of the Engine3D update.
		/// </summary>
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

		/// <summary>
		/// The animation speed of the light.
		/// </summary>
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

		/// <summary>
		/// The maximum attenuation falloff of the environment probe.
		/// </summary>
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

		/// <summary>
		/// The ID of the entity.
		/// </summary>
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

		/// <summary>
		/// The phase of the light.
		/// </summary>
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

		/// <summary>
		/// The style of the light.
		/// </summary>
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

		/// <summary>
		/// The minimum shadow resolution of the shadow map.
		/// </summary>
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

		/// <summary>
		/// The update ratio of the shadow map.
		/// </summary>
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

		/// <summary>
		/// The sorting priority of the environemt probe.
		/// </summary>
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

		/// <summary>
		/// The object matrix of the projector.
		/// </summary>
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

		/// <summary>
		/// The world space position of the <see cref="RenderLight"/>.
		/// </summary>
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

		/// <summary>
		/// The extents of the environment probe.
		/// </summary>
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

		/// <summary>
		/// The projection matrix of the projector.
		/// </summary>
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

		/// <summary>
		/// The index of the shadow mask of the shadow map.
		/// </summary>
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

		/// <summary>
		/// The height of the <see cref="RenderLight"/>.
		/// </summary>
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

		/// <summary>
		/// Optional name of the light source.
		/// </summary>
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

		/// <summary>
		/// The specular multiplier.
		/// </summary>
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

		/// <summary>
		/// The width of the <see cref="RenderLight"/>.
		/// </summary>
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

		/// <summary>
		/// Scissor parameters (2d extent).
		/// </summary>
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

		/// <summary>
		/// Scissor parameters (2d extent).
		/// </summary>
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

		[SerializeValue]
		internal SRenderLight NativeBaseHandle { get; private set; }

		internal RenderLight(SRenderLight nativeHandle)
		{
			NativeBaseHandle = nativeHandle;
		}

		/// <summary>
		/// Safely acquires the data of this <see cref="RenderLight"/>.
		/// </summary>
		public void AcquireResources()
		{
			NativeBaseHandle.AcquireResources();
		}

		/// <summary>
		/// Safely releases the data of this <see cref="RenderLight"/>.
		/// </summary>
		public void DropResources()
		{
			NativeBaseHandle.DropResources();
		}
	}
}
