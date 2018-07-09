// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct ISurfaceType;
struct ISurfaceTypeManager;
class ICrySizer;

#include <CryRenderer/IShader.h> // EEfResTextures

struct SShaderItem;
struct SShaderParam;
struct IShader;
struct IShaderPublicParams;
struct IMaterial;
struct IMaterialManager;
struct IRenderShaderResources;
class CCamera;
struct CMaterialCGF;
struct CRenderChunk;
struct SEfTexModificator;
struct SInputShaderResources;

#include <CryRenderer/Tarray.h>

#include <Cry3DEngine/CGF/CryHeaders.h>  // MAX_SUB_MATERIALS

//! Special names for materials.
#define MTL_SPECIAL_NAME_COLLISION_PROXY         "collision_proxy"
#define MTL_SPECIAL_NAME_COLLISION_PROXY_VEHICLE "nomaterial_vehicle"
#define MTL_SPECIAL_NAME_RAYCAST_PROXY           "raycast_proxy"

enum
{
	MAX_STREAM_PREDICTION_ZONES = 2
};

//////////////////////////////////////////////////////////////////////////
// Description:
//    IMaterial is an interface to the material object, SShaderItem host which is a combination of IShader and SShaderInputResources.
//    Material bind together rendering algorithm (Shader) and resources needed to render this shader, textures, colors, etc...
//    All materials except for pure sub material childs have a unique name which directly represent .mtl file on disk.
//    Ex: "Materials/Fire/Burn"
//    Materials can be created by Sandbox MaterialEditor.
//////////////////////////////////////////////////////////////////////////
enum EMaterialFlags
{
	MTL_FLAG_WIRE                      = 0x0001,         //!< Use wire frame rendering for this material.
	MTL_FLAG_2SIDED                    = 0x0002,         //!< Use 2 Sided rendering for this material.
	MTL_FLAG_ADDITIVE                  = 0x0004,         //!< Use Additive blending for this material.
	MTL_FLAG_DETAIL_DECAL              = 0x0008,         //!< Massive decal technique.
	MTL_FLAG_LIGHTING                  = 0x0010,         //!< Should lighting be applied on this material.
	MTL_FLAG_NOSHADOW                  = 0x0020,         //!< Material do not cast shadows.
	MTL_FLAG_ALWAYS_USED               = 0x0040,         //!< When set forces material to be export even if not explicitly used.
	MTL_FLAG_PURE_CHILD                = 0x0080,         //!< Not shared sub material, sub material unique to his parent multi material.
	MTL_FLAG_MULTI_SUBMTL              = 0x0100,         //!< This material is a multi sub material.
	MTL_FLAG_NOPHYSICALIZE             = 0x0200,         //!< Should not physicalize this material.
	MTL_FLAG_NODRAW                    = 0x0400,         //!< Do not render this material.
	MTL_FLAG_NOPREVIEW                 = 0x0800,         //!< Cannot preview the material.
	MTL_FLAG_NOTINSTANCED              = 0x1000,         //!< Do not instantiate this material.
	MTL_FLAG_COLLISION_PROXY           = 0x2000,         //!< This material is the collision proxy.
	MTL_FLAG_SCATTER                   = 0x4000,         //!< Use scattering for this material.
	MTL_FLAG_REQUIRE_FORWARD_RENDERING = 0x8000,         //!< This material has to be rendered in foward rendering passes (alpha/additive blended).
	MTL_FLAG_NON_REMOVABLE             = 0x10000,        //!< Material with this flag once created are never removed from material manager (Used for decal materials, this flag should not be saved).
	MTL_FLAG_HIDEONBREAK               = 0x20000,        //!< Non-physicalized subsets with such materials will be removed after the object breaks.
	MTL_FLAG_UIMATERIAL                = 0x40000,        //!< Used for UI in Editor. Don't need show it DB.
	MTL_64BIT_SHADERGENMASK            = 0x80000,        //!< ShaderGen mask is remapped.
	MTL_FLAG_RAYCAST_PROXY             = 0x100000,
	MTL_FLAG_REQUIRE_NEAREST_CUBEMAP   = 0x200000,       //!< Materials with alpha blending requires special processing for shadows.
	MTL_FLAG_CONSOLE_MAT               = 0x400000,
	MTL_FLAG_BLEND_TERRAIN             = 0x1000000,
	MTL_FLAG_TRACEABLE_TEXTURE         = 0x2000000,      //!< Diffuse texture keeps low-res copy for raytracing (in decals, for instance)
	MTL_FLAG_REFRACTIVE                = 0x4000000,      //!< Refractive shader or has a sum-material with a refractive shader
};

#define MTL_FLAGS_SAVE_MASK     (MTL_FLAG_WIRE | MTL_FLAG_2SIDED | MTL_FLAG_ADDITIVE | MTL_FLAG_DETAIL_DECAL | MTL_FLAG_LIGHTING | MTL_FLAG_TRACEABLE_TEXTURE | \
  MTL_FLAG_NOSHADOW | MTL_FLAG_MULTI_SUBMTL | MTL_FLAG_SCATTER | MTL_FLAG_REQUIRE_FORWARD_RENDERING | MTL_FLAG_HIDEONBREAK | MTL_FLAG_UIMATERIAL | MTL_64BIT_SHADERGENMASK | MTL_FLAG_REQUIRE_NEAREST_CUBEMAP | MTL_FLAG_CONSOLE_MAT | MTL_FLAG_BLEND_TERRAIN)

#define MTL_FLAGS_TEMPLATE_MASK (MTL_FLAG_WIRE | MTL_FLAG_2SIDED | MTL_FLAG_ADDITIVE | MTL_FLAG_DETAIL_DECAL | MTL_FLAG_LIGHTING | MTL_FLAG_TRACEABLE_TEXTURE | \
  MTL_FLAG_NOSHADOW | MTL_FLAG_SCATTER | MTL_FLAG_REQUIRE_FORWARD_RENDERING | MTL_FLAG_HIDEONBREAK | MTL_FLAG_UIMATERIAL | MTL_64BIT_SHADERGENMASK | MTL_FLAG_REQUIRE_NEAREST_CUBEMAP | MTL_FLAG_BLEND_TERRAIN)

//! Post-effects flags.
enum EPostEffectFlags
{
	POST_EFFECT_GHOST          = 0x1,
	POST_EFFECT_HOLOGRAM       = 0x2,
	POST_EFFECT_CHAMELEONCLOAK = 0x4,

	POST_EFFECT_MASK           = POST_EFFECT_GHOST | POST_EFFECT_HOLOGRAM
};

//! Bit offsets for shader layer flags.
enum EMaterialLayerFlags : uint32
{
	// Active layers flags
	MTL_LAYER_FROZEN        = 0x0001,
	MTL_LAYER_WET           = 0x0002,
	MTL_LAYER_CLOAK         = 0x0004,
	MTL_LAYER_DYNAMICFROZEN = 0x0008,

	// Usage flags
	MTL_LAYER_USAGE_NODRAW      = 0x0001,       //!< Layer is disabled.
	MTL_LAYER_USAGE_REPLACEBASE = 0x0002,       //!< Replace base pass rendering with layer - optimization.
	MTL_LAYER_USAGE_FADEOUT     = 0x0004,       //!< Layer doesn't render but still causes parent to fade out.

	// Blend offsets.
	MTL_LAYER_BLEND_FROZEN        = 0xff000000,
	MTL_LAYER_BLEND_WET           = 0x00fe0000, //! Bit stolen for cloak dissolve.
	MTL_LAYER_BIT_CLOAK_DISSOLVE  = 0x00010000,
	MTL_LAYER_BLEND_CLOAK         = 0x0000ff00,
	MTL_LAYER_BLEND_DYNAMICFROZEN = 0x000000ff,

	MTL_LAYER_FROZEN_MASK         = 0xff,
	MTL_LAYER_WET_MASK            = 0xfe,       //!< Bit stolen.
	MTL_LAYER_CLOAK_MASK          = 0xff,
	MTL_LAYER_DYNAMICFROZEN_MASK  = 0xff,

	MTL_LAYER_BLEND_MASK          = (MTL_LAYER_BLEND_FROZEN | MTL_LAYER_BLEND_WET | MTL_LAYER_BIT_CLOAK_DISSOLVE | MTL_LAYER_BLEND_CLOAK | MTL_LAYER_BLEND_DYNAMICFROZEN),

	MTL_LAYER_MAX_SLOTS           = 3             //!< Slot count.
};

//! Copy flags.
enum EMaterialCopyFlags
{
	MTL_COPY_DEFAULT  = 0,
	MTL_COPY_NAME     = BIT(0),
	MTL_COPY_TEMPLATE = BIT(1),
	MTL_COPY_TEXTURES = BIT(2),
};

struct IMaterialHelpers
{
	virtual ~IMaterialHelpers() {}

	//////////////////////////////////////////////////////////////////////////
	virtual EEfResTextures FindTexSlot(const char* texName) const = 0;
	virtual const char*    FindTexName(EEfResTextures texSlot) const = 0;
	virtual const char*    LookupTexName(EEfResTextures texSlot) const = 0;
	virtual const char*    LookupTexEnum(EEfResTextures texSlot) const = 0;
	virtual const char*    LookupTexSuffix(EEfResTextures texSlot) const = 0;
	virtual bool           IsAdjustableTexSlot(EEfResTextures texSlot) const = 0;

	//////////////////////////////////////////////////////////////////////////

	// texType in ETEX_Type, e.g. eTT_2D or eTT_Cube.
	virtual const char* GetNameFromTextureType(uint8 texType) const = 0;
	virtual uint8       GetTextureTypeFromName(const char* szName) const = 0;

	// A subset of ETEX_Type is visible to the user in .mtl files, the material manager, and python scripts.
	// These are called the "exposed texture types".
	virtual bool IsTextureTypeExposed(uint8 texType) const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual bool SetGetMaterialParamFloat(IRenderShaderResources& pShaderResources, const char* sParamName, float& v, bool bGet) const = 0;
	virtual bool SetGetMaterialParamVec3(IRenderShaderResources& pShaderResources, const char* sParamName, Vec3& v, bool bGet) const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetTexModFromXml(SEfTexModificator& pShaderResources, const XmlNodeRef& node) const = 0;
	virtual void SetXmlFromTexMod(const SEfTexModificator& pShaderResources, XmlNodeRef& node) const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetTexturesFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node, const char* szBaseFileName) const = 0;
	virtual void SetXmlFromTextures(const SInputShaderResources& pShaderResources, XmlNodeRef& node, const char* szBaseFileName) const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetVertexDeformFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const = 0;
	virtual void SetXmlFromVertexDeform(const SInputShaderResources& pShaderResources, XmlNodeRef& node) const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetDetailDecalFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const = 0;
	virtual void SetXmlFromDetailDecal(const SInputShaderResources& pShaderResources, XmlNodeRef& node) const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetLightingFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const = 0;
	virtual void SetXmlFromLighting(const SInputShaderResources& pShaderResources, XmlNodeRef& node) const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetShaderParamsFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const = 0;
	virtual void SetXmlFromShaderParams(const SInputShaderResources& pShaderResources, XmlNodeRef& node) const = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void MigrateXmlLegacyData(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////
// Description:
//    IMaterialLayer is group of material layer properties.
//    Each layer is composed of shader item, specific layer textures, lod info, etc
struct IMaterialLayer
{
	// <interfuscator:shuffle>
	virtual ~IMaterialLayer(){}

	//! Reference counting.
	virtual void AddRef() = 0;
	virtual void Release() = 0;

	//! Enable/disable layer usage
	virtual void Enable(bool bEnable = true) = 0;

	//! Check if layer enabled
	virtual bool IsEnabled() const = 0;

	//! Enable/disable fade out
	virtual void FadeOut(bool bFadeOut = true) = 0;

	//! Check if layer fades out
	virtual bool DoesFadeOut() const = 0;

	//! Set shader item
	virtual void SetShaderItem(const IMaterial* pParentMtl, const SShaderItem& pShaderItem) = 0;

	//! Return shader item
	virtual const SShaderItem& GetShaderItem() const = 0;
	virtual SShaderItem&       GetShaderItem() = 0;

	//! Set layer usage flags
	virtual void SetFlags(uint8 nFlags) = 0;

	//! Get layer usage flags
	virtual uint8 GetFlags() const = 0;

	// todo: layer specific textures support.
	// </interfuscator:shuffle>
};

//! Represents an .mtl instance that can be applied to geometry in the scene
struct IMaterial
{
	// TODO: Remove it!
	//! Default texture mapping.
	uint8 m_ucDefautMappingAxis;
	float m_fDefautMappingScale;

	// <interfuscator:shuffle>
	virtual ~IMaterial() {};

	// Reference counting.
	virtual bool              IsValid() const = 0;
	virtual void              AddRef() = 0;
	virtual void              Release() = 0;
	virtual int               GetNumRefs() = 0;

	virtual IMaterialHelpers& GetMaterialHelpers() = 0;
	virtual IMaterialManager* GetMaterialManager() = 0;

	// Material name.
	//! Set material name, (Do not use this directly, to change material name use I3DEngine::RenameMatInfo method).
	virtual void SetName(const char* pName) = 0;

	//! Returns material name.
	virtual const char* GetName() const = 0;

	//! Material flags.
	//! \see EMaterialFlags.
	virtual void SetFlags(int flags) = 0;
	virtual int  GetFlags() const = 0;

	//! Returns true if this is the default material.
	virtual bool IsDefault() const = 0;

	virtual int  GetSurfaceTypeId() const = 0;

	//! Assign a different surface type to this material.
	virtual void          SetSurfaceType(const char* sSurfaceTypeName) = 0;
	virtual ISurfaceType* GetSurfaceType() = 0;

	//! Assign a different surface type to this material.
	virtual void       SetMatTemplate(const char* sMatTemplate) = 0;
	virtual IMaterial* GetMatTemplate() = 0;

	//! Shader item.
	virtual void SetShaderItem(const SShaderItem& _ShaderItem) = 0;

	//! Used to detect the cases when dependent permanent render objects have to be updated
	virtual void IncrementModificationId() = 0;

	//! EF_LoadShaderItem return value with RefCount = 1, so if you'll use SetShaderItem after EF_LoadShaderItem use Assign function.
	virtual void               AssignShaderItem(const SShaderItem& _ShaderItem) = 0;
	virtual SShaderItem&       GetShaderItem() = 0;
	virtual const SShaderItem& GetShaderItem() const = 0;

	//! Returns shader item for correct sub material or for single material.
	//! Even if this is not sub material or nSubMtlSlot is invalid, it will return valid renderable shader item.
	virtual SShaderItem&       GetShaderItem(int nSubMtlSlot) = 0;
	virtual const SShaderItem& GetShaderItem(int nSubMtlSlot) const = 0;

	//! Returns true if streamed in.
	virtual bool IsStreamedIn(const int nMinPrecacheRoundIds[MAX_STREAM_PREDICTION_ZONES], IRenderMesh* pRenderMesh) const = 0;
	virtual bool IsStreamedIn(const int nMinPrecacheRoundIds[MAX_STREAM_PREDICTION_ZONES]) const = 0;

	//! Sub-material access.
	//! Sets number of child sub materials holded by this material.
	virtual void SetSubMtlCount(int numSubMtl) = 0;

	//! Returns number of child sub materials holded by this material.
	virtual int GetSubMtlCount() = 0;

	//! Return sub material at specified index.
	virtual IMaterial* GetSubMtl(int nSlot) = 0;

	//! Assign material to the sub mtl slot.
	//! Must first allocate slots using SetSubMtlCount.
	virtual void SetSubMtl(int nSlot, IMaterial* pMtl) = 0;

	// Layers access.

	//! Returns number of layers in this material.
	virtual void SetLayerCount(uint32 nCount) = 0;

	//! Returns number of layers in this material.
	virtual uint32 GetLayerCount() const = 0;

	//! Set layer at slot id (### MUST ALOCATE SLOTS FIRST ### USING SetLayerCount).
	virtual void SetLayer(uint32 nSlot, IMaterialLayer* pLayer) = 0;

	//! Return active layer.
	virtual const IMaterialLayer* GetLayer(uint8 nLayersMask, uint8 nLayersUsageMask) const = 0;

	//! Return layer at slot id.
	virtual const IMaterialLayer* GetLayer(uint32 nSlot) const = 0;

	//! Create a new layer.
	virtual IMaterialLayer* CreateLayer() = 0;

	//! Always get a valid material.
	//! If not multi material return this material.
	//! If Multi material return Default material if wrong id.
	virtual IMaterial* GetSafeSubMtl(int nSlot) = 0;

	//! Fill an array of integeres representing surface ids of the sub materials or the material itself.
	//! \param pSurfaceIdsTable Pointer to the array of int with size enough to hold MAX_SUB_MATERIALS surface type ids.
	//! \return Number of filled items.
	virtual int FillSurfaceTypeIds(int pSurfaceIdsTable[]) = 0;

	//! Set user data used to link with the Editor.
	virtual void  SetUserData(void* pUserData) = 0;
	virtual void* GetUserData() const = 0;

	virtual bool  SetGetMaterialParamFloat(const char* sParamName, float& v, bool bGet) = 0;
	virtual bool  SetGetMaterialParamVec3(const char* sParamName, Vec3& v, bool bGet) = 0;
	virtual void  SetTexture(int textureId, int textureSlot = EFTT_DIFFUSE) = 0;
	virtual void  SetSubTexture(int textureId, int subMaterialSlot, int textureSlot = EFTT_DIFFUSE) = 0;

	//! Set Optional Camera for material (Used for monitors that look thru camera).
	virtual void   SetCamera(CCamera& cam) = 0;

	virtual void   GetMemoryUsage(ICrySizer* pSizer) const = 0;

	virtual size_t GetResourceMemoryUsage(ICrySizer* pSizer) = 0;

	// Debug routine.
	//! Trace leaking materials by callstack.
	virtual const char* GetLoadingCallstack() = 0;

	//! Requests texture streamer to start loading textures asynchronously.
	virtual void RequestTexturesLoading(const float fMipFactor) = 0;
	//! Force texture streamer to start and finish loading textures asynchronously but within one frame, disregarding mesh visibility etc.
	virtual void ForceTexturesLoading(const float fMipFactor) = 0;
	virtual void ForceTexturesLoading(const int iScreenTexels) = 0;

	virtual void PrecacheMaterial(const float fEntDistance, struct IRenderMesh* pRenderMesh, bool bFullUpdate, bool bDrawNear = false) = 0;

	//! Estimates texture memory usage for this material.
	//! When nMatID is not negative only caluclate for one sub-material.
	virtual int GetTextureMemoryUsage(ICrySizer* pSizer, int nMatID = -1) = 0;

	//! Set & retrieve a material link name.
	//! This value by itself is not used by the material system per-se and hence has no real effect
	//! However it is used on a higher level to tie related materials together, e.g. by procedural breakable glass to determine which material to switch to.
	virtual void                SetMaterialLinkName(const char* name) = 0;
	virtual const char*         GetMaterialLinkName() const = 0;
	virtual void                SetKeepLowResSysCopyForDiffTex() = 0;

	virtual CryCriticalSection& GetSubMaterialResizeLock() = 0;
	virtual void                ActivateDynamicTextureSources(bool activate) = 0;
	// </interfuscator:shuffle>

#if CRY_PLATFORM_WINDOWS
	virtual void LoadConsoleMaterial() = 0;
#endif
};

//! \cond INTERNAL
//! IMaterialManagerListener is a callback interface to listen for special events of material manager, (used by Editor).
struct IMaterialManagerListener
{
	// <interfuscator:shuffle>
	virtual ~IMaterialManagerListener(){}

	//! Called when material manager tries to load a material.
	//! \param NLoadingFlags Zero or a bitwise combination of the flagas defined in ELoadingFlags.
	virtual IMaterial* OnLoadMaterial(const char* sMtlName, bool bForceCreation = false, unsigned long nLoadingFlags = 0) = 0;
	virtual void       OnCreateMaterial(IMaterial* pMaterial) = 0;
	virtual void       OnDeleteMaterial(IMaterial* pMaterial) = 0;
	// </interfuscator:shuffle>
};
//! \endcond

//! IMaterialManager interface provide access to the material manager implemented in 3DEngine.
struct IMaterialManager
{
	//! Loading flags.
	enum ELoadingFlags
	{
		ELoadingFlagsPreviewMode = BIT(0),
	};

	// <interfuscator:shuffle>
	virtual ~IMaterialManager(){}

	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;

	//! Creates a new material object and register it with the material manager
	//! \return Newly created object derived from IMaterial.
	virtual IMaterial* CreateMaterial(const char* sMtlName, int nMtlFlags = 0) = 0;

	//! Rename a material object.
	//! \note Do not use IMaterial::SetName directly.
	//! \param pMtl Pointer to a material object.
	//! \param sNewName New name to assign to the material.
	virtual void RenameMaterial(IMaterial* pMtl, const char* sNewName) = 0;

	//! Finds named material.
	virtual IMaterial* FindMaterial(const char* sMtlName) const = 0;

	//! Loads material.
	//! \param nLoadingFlags Zero or a bitwise combination of the values defined in ELoadingFlags.
	virtual IMaterial* LoadMaterial(const char* sMtlName, bool bMakeIfNotFound = true, bool bNonremovable = false, unsigned long nLoadingFlags = 0) = 0;

	//! Loads material from xml.
	virtual IMaterial* LoadMaterialFromXml(const char* sMtlName, XmlNodeRef mtlNode) = 0;

	//! Clone single material or multi sub material.
	//! \param nSubMtl When negative all sub materials of MultiSubMtl are cloned, if positive only specified slot is cloned.
	virtual IMaterial* CloneMaterial(IMaterial* pMtl, int nSubMtl = -1) = 0;

	//! Copy single material.
	virtual void CopyMaterial(IMaterial* pMtlSrc, IMaterial* pMtlDest, EMaterialCopyFlags flags) = 0;

	//! Clone MultiSubMtl material.
	//! \param sSubMtlName Name of the sub-material to clone, if NULL all submaterial are cloned.
	virtual IMaterial* CloneMultiMaterial(IMaterial* pMtl, const char* sSubMtlName = 0) = 0;

	//! Associate a special listener callback with material manager inside 3DEngine.
	//! This listener callback is used primerly by the editor.
	virtual void SetListener(IMaterialManagerListener* pListener) = 0;

	//! Retrieve a default engine material.
	virtual IMaterial* GetDefaultMaterial() = 0;

	//! Retrieve a default engine material for terrain layer
	virtual IMaterial* GetDefaultTerrainLayerMaterial() = 0;

	//! Retrieve a default engine material with material layers presets.
	virtual IMaterial* GetDefaultLayersMaterial() = 0;

	//! Retrieve a default engine material for drawing helpers.
	virtual IMaterial* GetDefaultHelperMaterial() = 0;

	//! Retrieve surface type by name.
	virtual ISurfaceType* GetSurfaceTypeByName(const char* sSurfaceTypeName, const char* sWhy = NULL) = 0;
	virtual int           GetSurfaceTypeIdByName(const char* sSurfaceTypeName, const char* sWhy = NULL) = 0;

	//! Retrieve surface type by unique surface type id.
	virtual ISurfaceType* GetSurfaceType(int nSurfaceTypeId, const char* sWhy = NULL) = 0;

	//! Retrieve interface to surface type manager.
	virtual ISurfaceTypeManager* GetSurfaceTypeManager() = 0;

	//! Get IMaterial pointer for a material referenced by a .cgf file.
	//! \param nLoadingFlags Zero, or a bitwise combination of the enum items from ELoadingFlags.
	virtual IMaterial* LoadCGFMaterial(const char* szMaterialName, const char* szCgfFilename, unsigned long nLoadingFlags = 0) = 0;

	//! For statistics - call once to get the count (pData==0), again to get the data(pData!=0).
	virtual void GetLoadedMaterials(IMaterial** pData, uint32& nObjCount) const = 0;

	//! Sets a suffix that will be appended onto every requested material to try and find an alternative version.
	//! \param pSuffix Set this to null to stop returning alternates.
	virtual void SetAltMaterialSuffix(const char* pSuffix) = 0;

	//! Updates material data in the renderer.
	virtual void RefreshMaterialRuntime() = 0;

	//// Forcing to create ISurfaceTypeManager
	//virtual void CreateSurfaceTypeManager() = 0;
	//// Forcing to destroy ISurfaceTypeManager
	//virtual void ReleaseSurfaceTypeManager() = 0;

	// </interfuscator:shuffle>
};
