// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   Material.h
//  Version:     v1.00
//  Created:     3/9/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __Material_h__
#define __Material_h__
#pragma once

#include <Cry3DEngine/IMaterial.h>

#if CRY_PLATFORM_DESKTOP
	#define TRACE_MATERIAL_LEAKS
	#define SUPPORT_MATERIAL_EDITING
#endif

class CMaterialLayer : public IMaterialLayer
{
public:
	CMaterialLayer() : m_nRefCount(0), m_nFlags(0)
	{
	}

	virtual ~CMaterialLayer()
	{
		SAFE_RELEASE(m_pShaderItem.m_pShader);
		SAFE_RELEASE(m_pShaderItem.m_pShaderResources);
	}

	virtual void AddRef()
	{
		m_nRefCount++;
	};

	virtual void Release()
	{
		if (--m_nRefCount <= 0)
		{
			delete this;
		}
	}

	virtual void Enable(bool bEnable = true)
	{
		m_nFlags |= (bEnable == false) ? MTL_LAYER_USAGE_NODRAW : 0;
	}

	virtual bool IsEnabled() const
	{
		return (m_nFlags & MTL_LAYER_USAGE_NODRAW) ? false : true;
	}

	virtual void FadeOut(bool bFadeOut = true)
	{
		m_nFlags |= (bFadeOut == false) ? MTL_LAYER_USAGE_FADEOUT : 0;
	}

	virtual bool DoesFadeOut() const
	{
		return (m_nFlags & MTL_LAYER_USAGE_FADEOUT) ? true : false;
	}

	virtual void               SetShaderItem(const IMaterial* pParentMtl, const SShaderItem& pShaderItem);

	virtual const SShaderItem& GetShaderItem() const
	{
		return m_pShaderItem;
	}

	virtual SShaderItem& GetShaderItem()
	{
		return m_pShaderItem;
	}

	virtual void SetFlags(uint8 nFlags)
	{
		m_nFlags = nFlags;
	}

	virtual uint8 GetFlags() const
	{
		return m_nFlags;
	}

	void   GetMemoryUsage(ICrySizer* pSizer);
	size_t GetResourceMemoryUsage(ICrySizer* pSizer);

private:
	uint8       m_nFlags;
	int         m_nRefCount;
	SShaderItem m_pShaderItem;
};

//////////////////////////////////////////////////////////////////////
class CMatInfo : public IMaterial, public stl::intrusive_linked_list_node<CMatInfo>, public Cry3DEngineBase
{
public:
	CMatInfo();
	~CMatInfo();

	void         ShutDown();
	virtual bool IsValid() const;

	virtual void AddRef();
	virtual void Release();

	virtual int  GetNumRefs() { return m_nRefCount; };

	//////////////////////////////////////////////////////////////////////////
	// IMaterial implementation
	//////////////////////////////////////////////////////////////////////////

	virtual IMaterialHelpers& GetMaterialHelpers();
	virtual IMaterialManager* GetMaterialManager();

	virtual void              SetName(const char* pName);
	virtual const char*       GetName() const     { return m_sMaterialName; };

	virtual void              SetFlags(int flags) { m_Flags = flags; };
	virtual int               GetFlags() const    { return m_Flags; };

	// Returns true if this is the default material.
	virtual bool          IsDefault() const;

	virtual int           GetSurfaceTypeId() const { return m_nSurfaceTypeId; };

	virtual void          SetSurfaceType(const char* sSurfaceTypeName);
	virtual ISurfaceType* GetSurfaceType();

	virtual void          SetMatTemplate(const char* sMatTemplate);
	virtual IMaterial*    GetMatTemplate();
	// shader item

	virtual void               SetShaderItem(const SShaderItem& _ShaderItem);
	// [Alexey] EF_LoadShaderItem return value with RefCount = 1, so if you'll use SetShaderItem after EF_LoadShaderItem use Assign function
	virtual void               AssignShaderItem(const SShaderItem& _ShaderItem);

	virtual SShaderItem&       GetShaderItem();
	virtual const SShaderItem& GetShaderItem() const;

	virtual SShaderItem&       GetShaderItem(int nSubMtlSlot);
	virtual const SShaderItem& GetShaderItem(int nSubMtlSlot) const;

	virtual bool               IsStreamedIn(const int nMinPrecacheRoundIds[MAX_STREAM_PREDICTION_ZONES], IRenderMesh* pRenderMesh) const;
	virtual bool               IsStreamedIn(const int nMinPrecacheRoundIds[MAX_STREAM_PREDICTION_ZONES]) const;
	bool                       AreChunkTexturesStreamedIn(CRenderChunk* pChunk, const int nMinPrecacheRoundIds[MAX_STREAM_PREDICTION_ZONES]) const;
	bool                       AreTexturesStreamedIn(const int nMinPrecacheRoundIds[MAX_STREAM_PREDICTION_ZONES]) const;

	//////////////////////////////////////////////////////////////////////////
	virtual bool SetGetMaterialParamFloat(const char* sParamName, float& v, bool bGet);
	virtual bool SetGetMaterialParamVec3(const char* sParamName, Vec3& v, bool bGet);
	virtual void SetTexture(int textureId, int textureSlot);
	virtual void SetSubTexture(int textureId, int subMaterialSlot, int textureSlot);

	virtual void SetCamera(CCamera& cam);

	//////////////////////////////////////////////////////////////////////////
	// Sub materials.
	//////////////////////////////////////////////////////////////////////////
	virtual void       SetSubMtlCount(int numSubMtl);
	virtual int        GetSubMtlCount() { return m_subMtls.size(); }
	virtual IMaterial* GetSubMtl(int nSlot)
	{
		if (m_subMtls.empty() || !(m_Flags & MTL_FLAG_MULTI_SUBMTL))
			return 0; // Not Multi material.
		if (nSlot >= 0 && nSlot < (int)m_subMtls.size())
			return m_subMtls[nSlot];
		else
			return 0;
	}
	virtual void       SetSubMtl(int nSlot, IMaterial* pMtl);
	virtual void       SetUserData(void* pUserData);
	virtual void*      GetUserData() const;

	virtual IMaterial* GetSafeSubMtl(int nSlot);
	virtual CMatInfo*  Clone(CMatInfo* pParentOfClonedMtl);
	virtual void       Copy(IMaterial* pMtlDest, EMaterialCopyFlags flags);

	virtual void       ActivateDynamicTextureSources(bool activate);

	//////////////////////////////////////////////////////////////////////////
	// Layers
	//////////////////////////////////////////////////////////////////////////
	virtual void                  SetLayerCount(uint32 nCount);
	virtual uint32                GetLayerCount() const;
	virtual void                  SetLayer(uint32 nSlot, IMaterialLayer* pLayer);
	virtual const IMaterialLayer* GetLayer(uint8 nLayersMask, uint8 nLayersUsageMask) const;
	virtual const IMaterialLayer* GetLayer(uint32 nSlot) const;
	virtual IMaterialLayer*       CreateLayer();

	// Fill int table with surface ids of sub materials.
	// Return number of filled items.
	int            FillSurfaceTypeIds(int pSurfaceIdsTable[]);

	virtual void   GetMemoryUsage(ICrySizer* pSizer) const;

	virtual size_t GetResourceMemoryUsage(ICrySizer* pSizer);

	void           UpdateShaderItems();
	void           RefreshShaderResourceConstants();

	virtual void IncrementModificationId() final { m_nModificationId++; }
	uint32 GetModificationId() const { return m_nModificationId; }

	//////////////////////////////////////////////////////////////////////////

	// Check for specific rendering conditions (forward rendering/nearest cubemap requirement)
	bool IsForwardRenderingRequired();
	bool IsNearestCubemapRequired();

	//////////////////////////////////////////////////////////////////////////
	// Debug routines
	//////////////////////////////////////////////////////////////////////////
	virtual const char* GetLoadingCallstack();  // trace leaking materials by callstack

	virtual void        RequestTexturesLoading(const float fMipFactor);
	virtual void        ForceTexturesLoading(const float fMipFactor);
	virtual void        ForceTexturesLoading(const int iScreenTexels);

	virtual void        PrecacheMaterial(const float fEntDistance, struct IRenderMesh* pRenderMesh, bool bFullUpdate, bool bDrawNear = false);
	void                PrecacheTextures(const float fMipFactor, const int nFlags, bool bFullUpdate);
	void                PrecacheTextures(const int iScreenTexels, const int nFlags, bool bFullUpdate);
	void                PrecacheChunkTextures(const float fInstanceDistance, const int nFlags, CRenderChunk* pRenderChunk, bool bFullUpdate);

	virtual int         GetTextureMemoryUsage(ICrySizer* pSizer, int nSubMtlSlot = -1);
	virtual void        SetKeepLowResSysCopyForDiffTex();

	virtual void        SetMaterialLinkName(const char* name);
	virtual const char* GetMaterialLinkName() const;

#if CRY_PLATFORM_WINDOWS
	virtual void LoadConsoleMaterial();
#endif

	virtual CryCriticalSection& GetSubMaterialResizeLock();
public:
	//////////////////////////////////////////////////////////////////////////
	// for debug purposes
	//////////////////////////////////////////////////////////////////////////
#ifdef TRACE_MATERIAL_LEAKS
	string m_sLoadingCallstack;
#endif

private:

	void UpdateMaterialFlags();

private:
	friend class CMatMan;
	friend class CMaterialLayer;

	//////////////////////////////////////////////////////////////////////////
	string m_sMaterialName;
	string m_sUniqueMaterialName;

	// Id of surface type assigned to this material.
	int m_nSurfaceTypeId;

	//! Number of references to this material.
	volatile int m_nRefCount;
	//! Material flags.
	//! @see EMatInfoFlags
	int         m_Flags;

	bool m_bDeleted;
	bool m_bDeletePending;

	SShaderItem m_shaderItem;

	// Used to detect the cases when dependent permanent render objects have to be updated
	uint32 m_nModificationId;

	//! Array of Sub materials.
	typedef DynArray<_smart_ptr<CMatInfo>> SubMtls;
	SubMtls m_subMtls;

#ifdef SUPPORT_MATERIAL_EDITING
	// User data used by Editor.
	void*  m_pUserData;

	string m_sMaterialLinkName;
	// name of mat templalte material
	string m_sMatTemplate;
#endif

	//! Material layers
	typedef std::vector<_smart_ptr<CMaterialLayer>> MatLayers;
	MatLayers* m_pMaterialLayers;

	//! Used for material layers
	mutable CMaterialLayer* m_pActiveLayer;

	struct SStreamingPredictionZone
	{
		int   nRoundId      : 31;
		int   bHighPriority : 1;
		float fMinMipFactor;
	} m_streamZoneInfo[2];

#if defined(ENABLE_CONSOLE_MTL_VIZ)
public:
	static void RegisterConsoleMatCVar();
	static bool IsConsoleMatModeEnabled() { return ms_useConsoleMat >= 0; }

private:
	static int            ms_useConsoleMat;
	_smart_ptr<IMaterial> m_pConsoleMtl;
#endif
};

#endif // __Material_h__
