// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/IFlares.h>
#include <CryMath/Cry_Vector2.h>
#include "Timeline.h"

#include "XRenderD3D9/GraphicsPipeline/Common/PrimitiveRenderPass.h"
#include "XRenderD3D9/GraphicsPipeline/StandardGraphicsPipeline.h"

class CTexture;
class CShader;
class RootOpticsElement;

class CSoftOcclusionVisiblityFader
{
public:
	CSoftOcclusionVisiblityFader()
		: m_fTargetVisibility(-1.0f)
		, m_fVisibilityFactor(1.0f)
	{
	}

	float UpdateVisibility(const float newTargetVisibility, const float duration);

	TimelineFloat m_TimeLine;
	float         m_fTargetVisibility;
	float         m_fVisibilityFactor;
};

class CFlareSoftOcclusionQuery : public ISoftOcclusionQuery
{
public:
	static const int   s_nIDColMax = 32;
	static const int   s_nIDRowMax = 32;
	static const int   s_nIDMax = s_nIDColMax * s_nIDRowMax;

	static const int   s_nGatherEachSectorWidth = 8;
	static const int   s_nGatherEachSectorHeight = 8;

	static const int   s_nGatherTextureWidth = s_nGatherEachSectorWidth * s_nIDColMax;
	static const int   s_nGatherTextureHeight = s_nGatherEachSectorHeight * s_nIDRowMax;

	static const float s_fSectorWidth;
	static const float s_fSectorHeight;

private:
	static int           s_idCount;
	static char          s_idHashTable[s_nIDMax];
	static unsigned char s_paletteRawCache[s_nIDMax * 4];

	static int           s_ringReadIdx;
	static int           s_ringWriteIdx;
	static int           s_ringSize;

private:
	static int  GenID();
	static void ReleaseID(int id);

public:
	CFlareSoftOcclusionQuery(const uint8 numFaders = 0) :
		m_fOccPlaneWidth(0.02f),
		m_fOccPlaneHeight(0.02f),
		m_PosToBeChecked(0, 0, 0),
		m_fOccResultCache(1),
		m_numVisibilityFaders(numFaders),
		m_pVisbilityFaders(NULL),
		m_refCount(1)
	{
		InitGlobalResources();
		m_nID = GenID();
		if (m_numVisibilityFaders > 0)
			m_pVisbilityFaders = new CSoftOcclusionVisiblityFader[m_numVisibilityFaders];
	}

	~CFlareSoftOcclusionQuery()
	{
		CRY_ASSERT(m_refCount == 0);
		ReleaseID(m_nID);
		m_numVisibilityFaders = 0;
		SAFE_DELETE_ARRAY(m_pVisbilityFaders);
	}

	// Manage multi-thread references
	virtual void AddRef()
	{
		CryInterlockedIncrement(&m_refCount);
	}

	virtual void Release()
	{
		if (CryInterlockedDecrement(&m_refCount) <= 0)
			delete this;
	}

	static void      InitGlobalResources();
	static void      BatchReadResults();
	static void      ReadbackSoftOcclQuery();
	static CTexture* GetOcclusionTex();

	void             GetDomainInTexture(float& out_x0, float& out_y0, float& out_x1, float& out_y1);
	void             GetSectorSize(float& width, float& height);

	struct SOcclusionSectorInfo
	{
		float x0, y0, x1, y1;
		float u0, v0, u1, v1;
		float lineardepth;
	};
	void GetOcclusionSectorInfo(SOcclusionSectorInfo& out_occlusionSector,const SRenderViewInfo& viewInfo);

	void UpdateCachedResults();
	int  GetID()
	{
		return m_nID;
	}
	float GetVisibility() const
	{
		return m_fOccResultCache;
	}
	float       GetOccResult()  const   { return m_fOccResultCache; }
	float       GetDirResult()  const   { return m_fDirResultCache; }
	const Vec2& GetDirVecResult() const { return m_DirVecResultCache; }
	bool        IsVisible() const       { return m_fOccResultCache > 0; }
	void        SetPosToBeChecked(const Vec3& vPos)
	{
		m_PosToBeChecked = vPos;
	}

	CSoftOcclusionVisiblityFader* GetVisibilityFader(const uint8 index) const
	{
		return (m_pVisbilityFaders && index < m_numVisibilityFaders) ? &m_pVisbilityFaders[index] : NULL;
	}

	void         SetOccPlaneSizeRatio(const Vec2& vRatio) { m_fOccPlaneWidth = vRatio.x; m_fOccPlaneHeight = vRatio.y; }
	float        GetOccPlaneWidth() const                 { return m_fOccPlaneWidth; }
	float        GetOccPlaneHeight() const                { return m_fOccPlaneHeight; }

	CTexture*    GetGatherTexture() const;

	static bool  ComputeProjPos(const Vec3& vWorldPos, const Matrix44A& viewMat, const Matrix44A& projMat, Vec3& outProjPos);
	static float ComputeLinearDepth(const Vec3& worldPos, const Matrix44A& cameraMat, float nearDist, float farDist);

private:
	uint8                         m_numVisibilityFaders;
	CSoftOcclusionVisiblityFader* m_pVisbilityFaders;

	int                           m_nID;

	float                         m_fOccResultCache;
	float                         m_fDirResultCache;
	Vec2                          m_DirVecResultCache;

	Vec3                          m_PosToBeChecked;

	float                         m_fOccPlaneWidth;
	float                         m_fOccPlaneHeight;

	volatile int                  m_refCount;
};

class CSoftOcclusionManager
{
public:

	CSoftOcclusionManager();
	~CSoftOcclusionManager();

	void Init();

	void AddSoftOcclusionQuery(CFlareSoftOcclusionQuery* pQuery, const Vec3& vPos);
	CFlareSoftOcclusionQuery* GetSoftOcclusionQuery(int nIndex) const;

	int  GetSize() const { return m_nPos; }
	void Reset();

	bool                      Update(SRenderViewInfo* pViewInfo, int viewInfoCount);

private:

	bool                      PrepareOcclusionPrimitive(CRenderPrimitive& primitive, const CPrimitiveRenderPass& targetPass,const SRenderViewInfo& viewInfo);
	bool                      PrepareGatherPrimitive(CRenderPrimitive& primitive, const CPrimitiveRenderPass& targetPass, SRenderViewInfo* pViewInfo, int viewInfoCount);

	int    m_nPos;
	_smart_ptr<CFlareSoftOcclusionQuery> m_SoftOcclusionQueries[CFlareSoftOcclusionQuery::s_nIDMax];

	CRenderPrimitive m_occlusionPrimitive;
	CRenderPrimitive m_gatherPrimitive;

	CPrimitiveRenderPass m_occlusionPass;
	CPrimitiveRenderPass m_gatherPass;

	buffer_handle_t m_indexBuffer;
	buffer_handle_t m_occlusionVertexBuffer;
	buffer_handle_t m_gatherVertexBuffer;
};
