// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryRenderer/IShader.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IComputeSkinning.h>

struct IRenderNode;
class CCompiledRenderObject;
struct SSectorTextureSet;

//////////////////////////////////////////////////////////////////////////
/// CRenderObject::m_ObjFlags: Flags used by shader pipeline
//////////////////////////////////////////////////////////////////////////
enum ERenderObjectFlags : uint64
{
	FOB_TRANS_ROTATE                = BIT64(0),
	FOB_TRANS_SCALE                 = BIT64(1),
	FOB_TRANS_TRANSLATE             = BIT64(2),
	FOB_RENDER_AFTER_POSTPROCESSING = BIT64(3),
	FOB_OWNER_GEOMETRY              = BIT64(4),
	FOB_MESH_SUBSET_INDICES         = BIT64(5),
	FOB_SELECTED                    = BIT64(6),
	FOB_RENDERER_IDENDITY_OBJECT    = BIT64(7),
	FOB_IN_DOORS                    = BIT64(8),
	FOB_NO_FOG                      = BIT64(9),
	FOB_DECAL                       = BIT64(10),
	FOB_OCTAGONAL                   = BIT64(11),
	FOB_BLEND_WITH_TERRAIN_COLOR    = BIT64(12),
	FOB_POINT_SPRITE                = BIT64(13),
	FOB_SOFT_PARTICLE               = BIT64(14),
	FOB_REQUIRES_RESOLVE            = BIT64(15),
	FOB_UPDATED_RTMASK              = BIT64(16),
	FOB_AFTER_WATER                 = BIT64(17),
	FOB_BENDED                      = BIT64(18),
	FOB_ZPREPASS                    = BIT64(19),
	FOB_INSHADOW                    = BIT64(20),
	FOB_DISSOLVE                    = BIT64(21),
	FOB_MOTION_BLUR                 = BIT64(22),
	FOB_NEAREST                     = BIT64(23), //!< [Rendered in Camera Space]
	FOB_SKINNED                     = BIT64(24),
	FOB_DISSOLVE_OUT                = BIT64(25),
	FOB_DYNAMIC_OBJECT              = BIT64(26),
	FOB_ALLOW_TESSELLATION          = BIT64(27),
	FOB_DECAL_TEXGEN_2D             = BIT64(28),
	FOB_ALPHATEST                   = BIT64(29),  // Careful when moving (used in ObjSort)
	FOB_HAS_PREVMATRIX              = BIT64(30),  // Careful when moving (used in ObjSort)
	FOB_LIGHTVOLUME                 = BIT64(31),
	FOB_TERRAIN_LAYER               = BIT64(32),
	FOB_HUD_REQUIRE_DEPTHTEST       = BIT64(33),

	FOB_TRANS_MASK                  = (FOB_TRANS_ROTATE | FOB_TRANS_SCALE | FOB_TRANS_TRANSLATE),
	FOB_DECAL_MASK                  = (FOB_DECAL | FOB_DECAL_TEXGEN_2D),
	FOB_PARTICLE_MASK               = (FOB_SOFT_PARTICLE | FOB_NO_FOG | FOB_INSHADOW | FOB_NEAREST | FOB_MOTION_BLUR | FOB_LIGHTVOLUME | FOB_ALLOW_TESSELLATION | FOB_IN_DOORS | FOB_AFTER_WATER),

	// WARNING: FOB_MASK_AFFECTS_MERGING must start from 0x10000/bit 16 (important for instancing).
	FOB_MASK_AFFECTS_MERGING_GEOM = (FOB_ZPREPASS | FOB_SKINNED | FOB_BENDED | FOB_DYNAMIC_OBJECT | FOB_ALLOW_TESSELLATION | FOB_NEAREST),
	FOB_MASK_AFFECTS_MERGING      = (FOB_ZPREPASS | FOB_MOTION_BLUR | FOB_HAS_PREVMATRIX | FOB_SKINNED | FOB_BENDED | FOB_INSHADOW | FOB_AFTER_WATER | FOB_DISSOLVE | FOB_DISSOLVE_OUT | FOB_NEAREST | FOB_DYNAMIC_OBJECT | FOB_ALLOW_TESSELLATION),
};

//////////////////////////////////////////////////////////////////////
// CRenderObject::m_customFlags
enum ERenderObjectCustomFlags : uint16
{
	COB_FADE_CLOAK_BY_DISTANCE         = BIT(0),
	COB_CUSTOM_POST_EFFECT             = BIT(1),
	COB_IGNORE_HUD_INTERFERENCE_FILTER = BIT(2),
	COB_IGNORE_HEAT_AMOUNT             = BIT(3),
	COB_POST_3D_RENDER                 = BIT(4),
	COB_IGNORE_CLOAK_REFRACTION_COLOR  = BIT(5),
	COB_HUD_REQUIRE_DEPTHTEST          = BIT(6),
	COB_CLOAK_INTERFERENCE             = BIT(7),
	COB_CLOAK_HIGHLIGHT                = BIT(8),
	COB_HUD_DISABLEBLOOM               = BIT(9),
	COB_DISABLE_MOTIONBLUR             = BIT(10),
};

//////////////////////////////////////////////////////////////////////////
/// Description:
///	 Interface for the skinnable objects (renderer calls its functions to get the skinning data).
/// should only created by EF_CreateSkinningData
//////////////////////////////////////////////////////////////////////////
struct SSkinningData
{
	uint32                           nNumBones;
	uint32                           nHWSkinningFlags;
	DualQuat*                        pBoneQuatsS;
	compute_skinning::SActiveMorphs* pActiveMorphs;
	uint32                           nNumActiveMorphs;
	JointIdType*                     pRemapTable;
	JobManager::SJobState*           pAsyncJobs;
	SSkinningData*                   pPreviousSkinningRenderData; // used for motion blur
	void*                            pCustomTag;                  //!< Used as a key for instancing with compute skinning SRV.
	uint32                           remapGUID;
	void*                            pCharInstCB;             // used if per char instance cbs are available in renderdll (d3d11+);
	                                                          // members below are for Software Skinning
	void*                            pCustomData;             // client specific data, used for example for sw-skinning on animation side
	SSkinningData**                  pMasterSkinningDataList; // used by the SkinningData for a Character Instance, contains a list of all Skin Instances which need SW-Skinning
	SSkinningData*                   pNextSkinningData;       // List to the next element which needs SW-Skinning
	Vec3                             vecAdditionalOffset;     //!< Contains MeshNode translation and in case of floats with 16bit precision: an additional precision-offset-correction

	IRenderMesh*                     pRenderMesh;
};

//////////////////////////////////////////////////////////////////////////
/// Additional custom data that can be attached to the CRenderObject
//////////////////////////////////////////////////////////////////////////
struct SRenderObjData
{
	uintptr_t                     m_uniqueObjectId;

	SSkinningData*                m_pSkinningData;

	float                         m_fTempVars[10];           // Different useful vars (ObjVal component in shaders)

	const DynArray<SShaderParam>* m_pShaderParams;

	// Optional Terrain Sector Information
	SSectorTextureSet*                m_pTerrainSectorTextureInfo;
	float                             m_fMaxViewDistance;

	uint16                            m_nLightID;

	uint32                            m_nVisionParams;
	uint32                            m_nHUDSilhouetteParams;

	uint32                            m_pLayerEffectParams;  // only used for layer effects

	hidemask                          m_nSubObjHideMask;

	const struct SParticleShaderData* m_pParticleShaderData;  // specific data from the Particle Render Function to the shaders

	uint16                            m_FogVolumeContribIdx;

	uint16                            m_scissorX;
	uint16                            m_scissorY;

	uint16                            m_scissorWidth;
	uint16                            m_scissorHeight;

	uint16                            m_LightVolumeId;

	//@ see ERenderObjectCustomFlags
	uint16 m_nCustomFlags;
	uint8  m_nCustomData;

	uint8  m_nVisionScale;
	int32  m_nLastDeformedFrameId;

	SRenderObjData()
	{
		Init();
	}

	void Init()
	{
		m_nSubObjHideMask = 0;
		m_uniqueObjectId = 0;
		m_nVisionScale = 1;
		m_nVisionParams = 0;
		m_pLayerEffectParams = 0;
		m_nLightID = 0;
		m_LightVolumeId = 0;
		m_pSkinningData = NULL;
		m_scissorX = m_scissorY = m_scissorWidth = m_scissorHeight = 0;
		m_nCustomData = 0;
		m_nCustomFlags = 0;
		m_nHUDSilhouetteParams = m_nVisionParams = 0;
		m_nLastDeformedFrameId = 0;
		m_pShaderParams = NULL;
		m_pTerrainSectorTextureInfo = 0;
		m_fMaxViewDistance = 100000.f;
		m_FogVolumeContribIdx = ~(uint16)0;
	}

	void SetShaderParams(const DynArray<SShaderParam>* pShaderParams)
	{
		m_pShaderParams = pShaderParams;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
	}
};

//////////////////////////////////////////////////////////////////////
///
/// Objects using in shader pipeline
/// Single rendering item, that can be created from 3DEngine and persist across multiple frames
/// It can be compiled into the platform specific efficient rendering compiled object.
///
//////////////////////////////////////////////////////////////////////
struct SRenderObjectAccessThreadConfig
{
	const threadID objAccessorThreadId;
	SRenderObjectAccessThreadConfig() = delete;
	explicit SRenderObjectAccessThreadConfig(threadID tID) : objAccessorThreadId(tID) {}
};


class CRY_ALIGN(16) CRenderObject
{
public:
	enum
	{
		MAX_INSTANCING_ELEMENTS = 800  //!< 4096 Vec4 entries max in DX11 (65536 bytes)
	};

	enum ERenderPassType
	{
		eRenderPass_General  = 0,
		eRenderPass_Shadows  = 1,
		eRenderPass_NumTypes = 2
	};

	// Structure used to pass information about vegetation bending to the shaders.
	struct SBendingData
	{
		float scale;
		float verticalRadius;

		bool operator != (const SBendingData& that) const
		{ return (scale != that.scale) | (verticalRadius != that.verticalRadius); }
	};

	struct SInstanceInfo
	{
		Matrix34     m_Matrix;
		ColorF       m_AmbColor;
		ColorF       m_FogVolumeContribution;

		SBendingData m_Bending;
	};

public:
	ILINE void SetIdentityMatrix()
	{
		for (auto& II : m_II)
		{
			II.m_Matrix = Matrix34::CreateIdentity();
		}
	}

	// The template is used to defer the function compilation as SRenderingPassInfo and gcpRendD3D are not defined at this point
	ILINE void SetMatrix(const Matrix34& mat, const SRenderObjectAccessThreadConfig& roThreadAccessThreadCfg)
	{
		m_II[roThreadAccessThreadCfg.objAccessorThreadId].m_Matrix = mat;
	}

	ILINE void SetAmbientColor(const ColorF& ambColor, const SRenderObjectAccessThreadConfig& roThreadAccessThreadCfg)
	{
		m_II[roThreadAccessThreadCfg.objAccessorThreadId].m_AmbColor = ambColor;
	}

	ILINE void SetBendingData(const SBendingData& vbend, const SRenderObjectAccessThreadConfig& roThreadAccessThreadCfg)
	{
		m_II[roThreadAccessThreadCfg.objAccessorThreadId].m_Bending = vbend;
	}

	ILINE const Matrix34& GetMatrix(const SRenderObjectAccessThreadConfig& roThreadAccessThreadCfg) const
	{
		return m_II[roThreadAccessThreadCfg.objAccessorThreadId].m_Matrix;
	}

	ILINE const ColorF& GetAmbientColor(const SRenderObjectAccessThreadConfig& roThreadAccessThreadCfg) const
	{
		return m_II[roThreadAccessThreadCfg.objAccessorThreadId].m_AmbColor;
	}

	ILINE const SBendingData& GetBendingData(const SRenderObjectAccessThreadConfig& roThreadAccessThreadCfg) const
	{
		return m_II[roThreadAccessThreadCfg.objAccessorThreadId].m_Bending;
	}

	uint64 m_ObjFlags;                 //!< Combination of FOB_ flags.
	uint32 m_Id;

	float m_fAlpha;                    //!< Object alpha.
	float m_fDistance;                 //!< Distance to the object.

	//!< Custom sort value.
	union
	{
		float  m_fSort;
		uint16 m_nSort;
	};

	uint32 m_nRTMask;                  //!< Shader runtime modification flags
	uint16 m_nMDV;                     //!< Vertex modifier flags for Shader.
	uint16 m_nRenderQuality;           //!< 65535 - full quality, 0 - lowest quality, used by CStatObj
	int16 m_nTextureID;                //!< Custom texture id.

	union
	{
		uint8 m_breakableGlassSubFragIndex;
		uint8 m_ParticleObjFlags;
	};
	uint8 m_nClipVolumeStencilRef;     //!< Per instance vis area stencil reference ID
	uint8 m_DissolveRef;               //!< Dissolve value
	uint8 m_RState;                    //!< Render state used for object

	uint32 m_nMaterialLayers;          //!< Which mtl layers active and how much to blend them

	IRenderNode* m_pRenderNode;        //!< Will define instance id.
	IMaterial* m_pCurrMaterial;        //!< Parent material used for render object.
	CRenderElement* m_pRE;             //!< RenderElement used by this CRenderObject

	// Linked list of compiled objects, one per mesh subset (Chunk).
	CCompiledRenderObject* m_pCompiledObject;

	// Common flags
	bool m_bWasDeleted;                               //!< Object was deleted and in unusable state
	bool m_bPermanent;                                //!< Object is permanent and persistent across multiple frames
	bool m_bInstanceDataDirty[eRenderPass_NumTypes];  //!< Object per instance data dirty and needs to be recompiled, (When only the instance data need recompilation)
	bool m_bAllCompiledValid;                         //!< Set to true when compiled successfully.

	volatile uint32 m_passReadyMask;                  //!< For Persistent Render Objects, This render object will be submitted for filling once for every not ready pass (should be 32 bit for atomic operation to work on it)

	//! Embedded SRenderObjData, optional data carried by CRenderObject
	SRenderObjData m_data;

	// Array of instances, cannot be bigger then MAX_INSTANCING_ELEMENTS
	std::vector<SInstanceInfo> m_Instances;

	uint32 m_editorSelectionID;                            //!< SelectionID for the editor

protected:
	//////////////////////////////////////////////////////////////////////////
	// Double buffered since RT and main/job thread will access it simultaneously. One for RT and one for main/job thread 
	SInstanceInfo m_II[RT_COMMAND_BUF_COUNT];             //!< Per instance data

public:
	//////////////////////////////////////////////////////////////////////////
	// Methods
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	/// Constructor
	//////////////////////////////////////////////////////////////////////////
	CRenderObject()
		: m_Id(~0u)
		  , m_pCompiledObject(nullptr)
	{
		Init();
	}
	~CRenderObject()
	{
	};                   // non virtual destructor!

	//=========================================================================================================

	inline void  Init()
	{
		m_ObjFlags = 0;
		SetInstanceDataDirty(false);

		m_bPermanent = false;
		m_nRenderQuality = 65535;

		m_RState = 0;
		m_fDistance = 0.0f;

		m_nClipVolumeStencilRef = 0;
		m_nMaterialLayers = 0;
		m_DissolveRef = 0;

		m_nMDV = 0;
		m_fSort = 0;

		m_fAlpha = 1.0f;
		m_nTextureID = -1;
		m_pCurrMaterial = nullptr;
		m_pRE = nullptr;

		m_nRTMask = 0;
		m_pRenderNode = NULL;

		m_bWasDeleted = false;
		m_bAllCompiledValid = false;

		m_passReadyMask = 0;
		m_pCompiledObject = nullptr;

		m_Instances.clear();

		m_data.Init();

		m_II[0].m_Matrix.SetIdentity();
		m_II[0].m_AmbColor = Col_White;
		m_II[0].m_Bending = { 0.0f, 0.0f };

		m_II[1] = m_II[0];

		m_editorSelectionID = 0;
	}

	void                    AssignId(uint32 id) { m_Id = id; }

	ILINE void SetInstanceDataDirty(bool dirty = true)
	{
		for (uint32_t i = 0; i < eRenderPass_NumTypes; ++i)
			m_bInstanceDataDirty[i] = dirty;
	}

	ILINE SRenderObjData*         GetObjData()       { return &m_data; }
	ILINE const SRenderObjData*   GetObjData() const { return &m_data; }

	ILINE CRenderElement*         GetRE() const      { return m_pRE; }

	template <typename ObjectAccessor>
	AABB TransformAABB(const AABB &aabb, const Vec3 &cameraPosition, ObjectAccessor &&accessor) const
	{
		auto m = GetMatrix(std::forward<ObjectAccessor>(accessor));
		// Convert from camera space to world space for nearest
		if (m_ObjFlags & FOB_NEAREST)
			m.AddTranslation(cameraPosition);

		return AABB::CreateTransformedAABB(m, aabb);
	}

protected:
	// Disallow copy (potential bugs with PERMANENT objects)
	// alwasy use IRendeer::EF_DuplicateRO if you want a copy
	// of a CRenderObject
	CRenderObject(const CRenderObject &other)
	{
	};
	// CRenderObject& operator= (const CRenderObject& other) = default; // Not supported on vs2012

	void CloneObject(CRenderObject* srcObj)
	{
		*this = *srcObj;
	}

	friend class CRenderer;

};

//! \endcond